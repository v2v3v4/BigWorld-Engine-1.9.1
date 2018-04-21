/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "Python.h"		// See http://docs.python.org/api/includes.html

#include "mysql_typemapping.hpp"
#include "database.hpp"
#include "db_entitydefs.hpp"
#include "mysql_notprepared.hpp"
#include "resmgr/xml_section.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/base64.h"
#include "cstdmf/memory_stream.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/unique_id.hpp"
#include "server/bwconfig.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/pickler.hpp"
#include "entitydef/entity_description_map.hpp"
#include "entitydef/data_types.hpp"

#include <vector>
#include <set>
#include <map>
#include <stack>

DECLARE_DEBUG_COMPONENT( 0 );

// -----------------------------------------------------------------------------
// Section: useful typedefs
// -----------------------------------------------------------------------------
#define PRIMARY_INDEX_NAME 	"PRIMARY"
#define PARENTID_INDEX_NAME	"parentIDIndex"

const std::string PRIMARY_INDEX_NAME_STR( PRIMARY_INDEX_NAME );
const std::string PARENTID_INDEX_NAME_STR( PARENTID_INDEX_NAME );

// -----------------------------------------------------------------------------
// Section: Table meta data
// -----------------------------------------------------------------------------

namespace
{
	/**
	 *	This function generates an index name based on column name. The name
	 * 	of the index isn't really all that important but it's nice to have some
	 * 	consistency.
	 */
	std::string generateIndexName( const std::string& colName )
	{
		std::string::size_type underscorePos = colName.find( '_' );
		return (underscorePos == std::string::npos) ?
			colName + "Index" :
			colName.substr( underscorePos + 1 ) + "Index";
	}

	/**
	 * 	This structure contains all the buffers for the result of a "SHOW INDEX"
	 * 	query.
	 */
	struct IndexInfoBuffers
	{
		MySqlBuffer				tableName;
		int						nonUnique;
		MySqlBuffer				keyName;
		int						indexSeq;
		MySqlBuffer				columnName;
		MySqlBuffer				collation;
		int						cardinality;
		MySqlValueWithNull<int>	subPart;
		MySqlBuffer				packed;
		MySqlBuffer				nullable;
		MySqlBuffer				indexType;
		MySqlBuffer				comment;

		// Constructor. Add ourself to the given bindings.
		IndexInfoBuffers( MySqlBindings& bindings ) :
			tableName( MySQLMaxTableNameLen ),
			keyName( MySQLMaxIndexNameLen ),
			columnName( MySQLMaxColumnNameLen ),
			// __kyl__ (24/9/2007) Had a good guess as to the size of these
			// fields.
			collation( 64 ),
			packed( 64 ),
			nullable( 16 ),
			indexType( 64 ),
			comment( 256 )
		{
			bindings << tableName << nonUnique << keyName << indexSeq
					<< columnName << collation << cardinality << subPart
					<< packed << nullable << indexType << comment;
		}
	};

}

/**
 *	This class executes the "SHOW INDEX" query on a table and stores the
 * 	list of index names
 */
// NOTE: Can't be in anonymous namespace because forward declared in header
class TableIndices
{
	// Maps column names to index name.
	typedef std::map< std::string, std::string > ColumnToIndexMap;
	ColumnToIndexMap	colToIndexMap_;

public:
	TableIndices( MySql& connection, const std::string& tableName );

	const std::string* getIndexName( const std::string& colName ) const
	{
		ColumnToIndexMap::const_iterator found = colToIndexMap_.find(colName);
		return (found != colToIndexMap_.end()) ? &found->second : NULL;
	}
};

/**
 *	Constructor. Retrieves the index information for the given table.
 */
TableIndices::TableIndices( MySql& connection, const std::string& tableName )
{
	// Get index info
	std::stringstream query;
	query << "SHOW INDEX FROM " << tableName;
	MySqlStatement getIndexesStmt( connection, query.str() );

	MySqlBindings bindings;
	IndexInfoBuffers buf( bindings );
	getIndexesStmt.bindResult( bindings );

	connection.execute( getIndexesStmt );
	while ( getIndexesStmt.fetch() )
	{
//		DEBUG_MSG( "Found index %s.%s.%s\n", tableName.c_str(),
//				buf.columnName.getString().c_str(),
//				buf.keyName.getString().c_str() );
		// Build column name to index name map. Assume no multi-column index.
		colToIndexMap_[ buf.columnName.getString() ] = buf.keyName.getString();
	}
}


/**
 * 	Constructor. Initialises from a MYSQL_FIELD and TableIndices.
 */
TableMetaData::ColumnInfo::ColumnInfo( const MYSQL_FIELD& field,
		const TableIndices& indices ) :
	columnType( field ), indexType( deriveIndexType( field, indices ) )
{}

/**
 * Default constructor. Required for insertion into std::map
 */
TableMetaData::ColumnInfo::ColumnInfo() :
	columnType( MYSQL_TYPE_NULL, false, 0, std::string() ),
	indexType( IMySqlColumnMapping::IndexTypeNone )
{}

bool TableMetaData::ColumnInfo::operator==( const ColumnInfo& other ) const
{
	return (this->columnType == other.columnType) &&
			(this->indexType == other.indexType);
}

/**
 *	Returns the correct IMySqlColumnMapping::IndexType based on the information
 * 	in MYSQL_FIELD and TableIndices.
 */
IMySqlColumnMapping::IndexType TableMetaData::ColumnInfo::deriveIndexType(
		const MYSQL_FIELD& field, const TableIndices& indices )
{
	if (field.flags & PRI_KEY_FLAG)
	{
		return IMySqlColumnMapping::IndexTypePrimary;
	}
	else if (field.flags & UNIQUE_KEY_FLAG)
	{
		const std::string* pIndexName = indices.getIndexName( field.name );
		MF_ASSERT( pIndexName );
		if (*pIndexName == generateIndexName( field.name ))
		{
			// One of ours
			return IMySqlColumnMapping::IndexTypeName;
		}
		else
		{
			WARNING_MSG( "TableMetaData::ColumnInfo::deriveIndexType: "
					"Found unknown unique index %s for column %s\n",
					pIndexName->c_str(), field.name );
		}
	}
	else if (field.flags & MULTIPLE_KEY_FLAG)
	{
		const std::string* pIndexName = indices.getIndexName( field.name );
		MF_ASSERT( pIndexName );
		if (*pIndexName == PARENTID_INDEX_NAME_STR)
		{
			return IMySqlColumnMapping::IndexTypeParentID;
		}
		else
		{
			WARNING_MSG( "TableMetaData::ColumnInfo::deriveIndexType: "
					"Found unknown multiple key index %s for column %s\n",
					pIndexName->c_str(), field.name );
		}
	}

	return IMySqlColumnMapping::IndexTypeNone;
}

/**
 *	Retrieves all the names of entity tables currently in the database.
 *
 * 	@param	This parameter receives the list of tables.
 */
void TableMetaData::getEntityTables( StrSet& tables, MySql& connection )
{
	MySqlUnPrep::Statement stmtGetTables( connection,
			"SHOW TABLES LIKE '"TABLE_NAME_PREFIX"_%'" );

	MySqlBuffer 			bufferTableName( MySQLMaxTableNameLen );
	MySqlUnPrep::Bindings	bindings;
	bindings << bufferTableName;
	stmtGetTables.bindResult( bindings );

	connection.execute( stmtGetTables );
	while ( stmtGetTables.fetch() )
	{
		tables.insert( bufferTableName.getString() );
	}
}

/**
 * 	Retrieves meta data of all the columns for a given table.
 *
 * 	@param	columns	This output parameter receives the list of columns.
 * 	The key is the column name and the data is the column type.
 * 	@param	tableName	The name of the table to get the columns for.
 */
void TableMetaData::getTableColumns( TableMetaData::NameToColInfoMap& columns,
		MySql& connection, const std::string& tableName )
{
	MySqlTableMetadata	tableMetadata( connection, tableName );
	if (tableMetadata.isValid())	// table exists
	{
		TableIndices tableIndices( connection, tableName );
		for (unsigned int i = 0; i < tableMetadata.getNumFields(); i++)
		{
			const MYSQL_FIELD& field = tableMetadata.getField( i );
			columns[ field.name ] =
				TableMetaData::ColumnInfo( field, tableIndices );
		}
	}
}

// -----------------------------------------------------------------------------
// Section: BigWorld meta data
// -----------------------------------------------------------------------------
/**
 *	Constructor. Can only be called after initSpecialBigWorldTables().
 *
 * 	@param	connection	The connection to MySQL server.
 */
BigWorldMetaData::BigWorldMetaData( MySql& connection ) :
	connection_( connection ),
	stmtGetEntityTypeID_( connection_,
			"SELECT bigworldID FROM bigworldEntityTypes WHERE name=?" ),
	stmtSetEntityTypeID_( connection_,
			"UPDATE bigworldEntityTypes SET bigworldID=? WHERE name=?" ),
	stmtAddEntityType_( connection_,
			"INSERT INTO bigworldEntityTypes (typeID, bigworldID, name)"
			"VALUES (NULL, ?, ?)" ),
	stmtRemoveEntityType_( connection_,
			"DELETE FROM bigworldEntityTypes WHERE name=?" ),
	bufferTypeName_( BWMySQLMaxTypeNameLen ),
	bufferInteger_( -1 )
{
	MySqlBindings b;

	b.clear();
	b << bufferTypeName_;
	stmtGetEntityTypeID_.bindParams( b );

	b.clear();
	b << bufferInteger_;
	stmtGetEntityTypeID_.bindResult( b );

	b.clear();
	b << bufferInteger_ << bufferTypeName_;
	stmtSetEntityTypeID_.bindParams( b );

	b.clear();
	b << bufferInteger_ << bufferTypeName_;
	stmtAddEntityType_.bindParams( b );

	b.clear();
	b << bufferTypeName_;
	stmtRemoveEntityType_.bindParams( b );
}

/**
 * 	This method retrieves the EntityTypeID associated with the entity name
 * 	from our meta information.
 *
 *  @param	entityName	The entity type name.
 */
EntityTypeID BigWorldMetaData::getEntityTypeID( const std::string& entityName )
{
	bufferTypeName_.setString( entityName );
	connection_.execute( stmtGetEntityTypeID_ );

	EntityTypeID typeID = INVALID_TYPEID;
	if (stmtGetEntityTypeID_.resultRows() > 0)
	{
		MF_ASSERT(stmtGetEntityTypeID_.resultRows() == 1);

		stmtGetEntityTypeID_.fetch();
		typeID = bufferInteger_;
	}
	return typeID;
}

/**
 * 	This method sets the EntityTypeID associated with the entity name
 * 	into our meta information.
 *
 *  @param	entityName	The entity type name.
 * 	@param	typeID		The entity type ID.
 */
void BigWorldMetaData::setEntityTypeID( const std::string& entityName,
		EntityTypeID typeID	)
{
	bufferInteger_ = typeID;
	bufferTypeName_.setString( entityName );
	connection_.execute( stmtSetEntityTypeID_ );
}

/**
 * 	This method adds an EntityTypeID-entity name mapping into
 * 	our meta information.
 *
 *  @param	entityName	The entity type name.
 * 	@param	typeID		The entity type ID.
 */
void BigWorldMetaData::addEntityType( const std::string& entityName,
		EntityTypeID typeID )
{
	bufferInteger_ = typeID;
	bufferTypeName_.setString( entityName );
	connection_.execute( stmtAddEntityType_ );
}

/**
 * 	This method removes an EntityTypeID-entity name mapping from
 * 	our meta information.
 *
 *  @param	entityName	The entity type name.
 */
void BigWorldMetaData::removeEntityType( const std::string& entityName )
{
	bufferTypeName_.setString( entityName );
	connection_.execute( stmtRemoveEntityType_ );
}


// -----------------------------------------------------------------------------
// Section: Entity table manipulation functions
// -----------------------------------------------------------------------------
namespace
{
	/**
	 * 	This is a utility function which will separate columns into new
	 * 	(needs to be added), old (needs to be deleted) or out of date (needs
	 * 	to be updated).
	 *
	 * 	@param	oldColumns	On input, this parameter contains the existing
	 * 	columns. On output, this parameter will contain those columns that
	 * 	require removal.
	 * 	@param	newColumns	On input, this parameter contains the desired
	 * 	columns. On output, this parameter will contain those columns that
	 * 	require addition.
	 * 	@param	updatedColumns	On input, this parameter should be empty. On
	 * 	output, this parameter contains the columns whose type needs changing.
	 */
	void classifyColumns( TableMetaData::NameToColInfoMap& oldColumns,
		TableMetaData::NameToColInfoMap& newColumns,
		TableMetaData::NameToUpdatedColInfoMap& updatedColumns )
	{
		typedef TableMetaData::NameToColInfoMap 		Columns;
		typedef TableMetaData::NameToUpdatedColInfoMap 	UpdatedColumns;

		for ( Columns::iterator oldCol = oldColumns.begin();
				oldCol != oldColumns.end(); /*++oldCol*/ )
		{
			Columns::iterator newCol = newColumns.find( oldCol->first );
			if (newCol != newColumns.end())
			{
				if (newCol->second != oldCol->second)
				{
					updatedColumns.insert(
							UpdatedColumns::value_type( newCol->first,
									TableMetaData::UpdatedColumnInfo(
											newCol->second, oldCol->second )));
				}

				TableMetaData::NameToColInfoMap::iterator curOldCol = oldCol;
				++oldCol;
				oldColumns.erase( curOldCol );
				newColumns.erase( newCol );
			}
			else
			{
				++oldCol;
			}
		}
	}

	/**
	 *	Wrapper for createEntityTableIndex() that takes a MySql connection.
	 */
	void createEntityTableIndex( MySqlTransaction& transaction,
		const std::string& tblName, const std::string& colName,
		const TableMetaData::ColumnInfo& colInfo )
	{
		createEntityTableIndex( transaction.get(), tblName, colName, colInfo );
	}
} // anonymous namespace

	/**
	 *	This function creates an index on the given column in the given table
	 * 	according to colInfo.indexType. Most of the time this would be
	 * 	IndexTypeNone.
	 */
	void createEntityTableIndex( MySql& connection,
		const std::string& tblName, const std::string& colName,
		const TableMetaData::ColumnInfo& colInfo )
	{
		switch (colInfo.indexType)
		{
			case IMySqlColumnMapping::IndexTypeNone:
				break;
			case IMySqlColumnMapping::IndexTypePrimary:
				// A bit dodgy, but this is created when we create the table
				// and it cannot be added or deleted afterwards.
				break;
			case IMySqlColumnMapping::IndexTypeName:
				{
					// __kyl__ (24/7/2006) Super dodgy way of working out the
					// size of the index. If it is a VARCHAR field then we use
					// the size of the field. If it is any other type of field,
					// then we make the index size 255.
					const char * indexLengthConstraint = "";
					if (colInfo.columnType.fieldType != MYSQL_TYPE_VAR_STRING)
					{	// Not VARCHAR field. Set index length to 255.
						indexLengthConstraint = "(255)";
					}
					std::string indexName = generateIndexName( colName );

					try
					{
						connection.execute( "CREATE UNIQUE INDEX " + indexName +
							" ON " + tblName + " (" + colName +
							indexLengthConstraint + ")" );
					}
					catch (...)
					{
						ERROR_MSG( "createEntityTableIndex: Failed to create "
								"name index on column '%s.%s'. Please ensure "
								"all that values in the column are unique "
								"before attempting to create a name index.\n",
								tblName.c_str(), colName.c_str() );
						throw;
					}
				}
				break;
			case IMySqlColumnMapping::IndexTypeParentID:
				connection.execute( "CREATE INDEX "PARENTID_INDEX_NAME" ON " +
						tblName + " (" + colName + ")" );
				break;
			default:
				CRITICAL_MSG( "createEntityTableIndex: Unknown index type %d\n",
						colInfo.indexType );
				break;
		}
	}

namespace
{
	/**
	 *	This function deletes an index in the given table according to
	 * 	indexType. This is the evil twin of createEntityTableIndex().
	 */
	void removeEntityTableIndex( MySql& connection,
		const std::string& tblName, const std::string& colName,
		IMySqlColumnMapping::IndexType indexType )
	{

		try
		{
			switch (indexType)
			{
				case IMySqlColumnMapping::IndexTypeNone:
					break;
				case IMySqlColumnMapping::IndexTypePrimary:
					// Can't delete primary index.
					break;
				case IMySqlColumnMapping::IndexTypeName:
					{
						std::string indexName = generateIndexName( colName );

						connection.execute( "ALTER TABLE " + tblName +
								" DROP INDEX " + indexName );
					}
					break;
				case IMySqlColumnMapping::IndexTypeParentID:
					connection.execute( "ALTER TABLE " + tblName +
							" DROP INDEX "PARENTID_INDEX_NAME );
					break;
				default:
					CRITICAL_MSG( "removeEntityTableIndex: Unknown index type "
							"%d\n", indexType );
					break;
			}
		}
		catch (std::exception& e)
		{
			// Shouldn't really happen, but it's not fatal so we shouldn't
			// die.
			ERROR_MSG( "removeEntityTableIndex: %s\n", e.what() );
		}
	}

	/**
	 *	This function sets the value of a string column for all rows in the table.
	 */
	void setColumnValue( MySql& con, const std::string& tableName,
			const std::string& columnName, const std::string& columnValue )
	{
		std::stringstream ss;
		ss << "UPDATE " << tableName << " SET " << columnName
				<< "='" << MySqlEscapedString( con, columnValue ) << '\'';
		con.execute( ss.str() );
	}

	/**
	 * 	This visitor collects information about the columns of a table.
	 */
	class ColumnsCollector : public IMySqlColumnMapping::IVisitor,
							public IMySqlIDColumnMapping::IVisitor
	{
		TableMetaData::NameToColInfoMap	columns_;

	public:
		// IMySqlColumnMapping::IVisitor overrides
		virtual bool onVisitColumn( IMySqlColumnMapping& column )
		{
			TableMetaData::ColumnInfo& colInfo =
					columns_[ column.getColumnName() ];
			column.getColumnType( colInfo.columnType );
			colInfo.indexType = column.getColumnIndexType();

			return true;
		}
		// IMySqlIDColumnMapping::IVisitor overrides
		virtual bool onVisitIDColumn( IMySqlIDColumnMapping& column )
		{
			return this->onVisitColumn( column );
		}

		TableMetaData::NameToColInfoMap& getColumnsInfo()	{ return columns_; }
	};

	/**
	 * 	This visitor collects information about the tables required by an
	 * 	entity type and checks whether they match the tables in the database.
	 */
	class TableInspector: public IMySqlTableMapping::IVisitor
	{
	public:
		TableInspector( MySql& connection ) :
			connection_( connection ), isSynced_( true )
		{}

		MySql& connection()		{ return connection_; }

		bool deleteUnvisitedTables();

		// Returns whether the tables required by the entity definitions match
		// the tables
		bool isSynced() const	{ return isSynced_; }

		// IMySqlTableMapping::IVisitor overrides
		virtual bool onVisitTable( IMySqlTableMapping& table );

	protected:
		// Interface for derived class
		virtual bool onNeedNewTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& columns ) = 0;
		virtual bool onNeedUpdateTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& obsoleteColumns,
				const TableMetaData::NameToColInfoMap& newColumns,
				const TableMetaData::NameToUpdatedColInfoMap& updatedColumns )
				= 0;
		virtual bool onNeedDeleteTables( const StrSet& tableNames ) = 0;

	protected:
		MySql& 	connection_;

	private:
		bool	isSynced_;
		StrSet 	visitedTables_;
	};

	// IMySqlTableMapping::IVisitor override.
	bool TableInspector::onVisitTable( IMySqlTableMapping& table )
	{
		ColumnsCollector colCol;
		table.visitIDColumnWith( colCol );
		table.visitColumnsWith( colCol );

		// Alias for required columns.
		TableMetaData::NameToColInfoMap& newColumns( colCol.getColumnsInfo() );
		const std::string& tableName( table.getTableName() );

		// Check it is not duplicate table
		if (!visitedTables_.insert( tableName ).second)
		{
			throw std::runtime_error( "table " + tableName +
					" requested twice" );
		}

		// Get existing table columns.
		TableMetaData::NameToColInfoMap oldColumns;
		TableMetaData::getTableColumns( oldColumns, connection_, tableName );

		if (oldColumns.size() == 0)	// No existing table
		{
			if (!this->onNeedNewTable( tableName, colCol.getColumnsInfo() ))
			{
				isSynced_ = false;
			}
		}
		else
		{
			// Check difference between required and actual columns
			TableMetaData::NameToUpdatedColInfoMap updatedColumns;
			classifyColumns( oldColumns, newColumns, updatedColumns );

			if ((oldColumns.size() + newColumns.size() + updatedColumns.size())
					> 0)
			{
				if (!this->onNeedUpdateTable( tableName, oldColumns, newColumns,
						updatedColumns ))
				{
					isSynced_ = false;
				}
			}
		}

		return true;	// Continue to visit the next table.
	}

	/**
	 *	This function removes the tables in the database that are were not
	 * 	visited.
	 */
	bool TableInspector::deleteUnvisitedTables()
	{
		StrSet obsoleteTables;
		{
			StrSet existingTables;
			TableMetaData::getEntityTables( existingTables, connection_ );

			std::set_difference( existingTables.begin(), existingTables.end(),
						visitedTables_.begin(), visitedTables_.end(),
						std::insert_iterator<StrSet>( obsoleteTables,
								obsoleteTables.begin() ) );
		}

		bool isDeleted = true;

		if (obsoleteTables.size() > 0)
		{
			isDeleted = this->onNeedDeleteTables( obsoleteTables );
			if (!isDeleted)
			{
				isSynced_ = false;
			}
		}

		return isDeleted;
	}

	/**
	 *	This specialisation of TableInspector simply prints out the differences
	 * 	between the required tables and the tables in the database.
	 */
	class TableValidator : public TableInspector
	{
	public:
		TableValidator( MySql& connection ) : TableInspector( connection ) {}

	protected:
		// Override interface methods
		virtual bool onNeedNewTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& columns );
		virtual bool onNeedUpdateTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& obsoleteColumns,
				const TableMetaData::NameToColInfoMap& newColumns,
				const TableMetaData::NameToUpdatedColInfoMap& updatedColumns );
		virtual bool onNeedDeleteTables( const StrSet& tableNames );
	};

	bool TableValidator::onNeedNewTable( const std::string& tableName,
			const TableMetaData::NameToColInfoMap& columns )
	{
		INFO_MSG("\tRequire table %s\n", tableName.c_str());

		return false;	// We didn't create the table.
	}

	bool TableValidator::onNeedUpdateTable(
			const std::string& tableName,
			const TableMetaData::NameToColInfoMap& obsoleteColumns,
			const TableMetaData::NameToColInfoMap& newColumns,
			const TableMetaData::NameToUpdatedColInfoMap& updatedColumns )
	{
		for ( TableMetaData::NameToColInfoMap::const_iterator i =
				newColumns.begin(); i != newColumns.end(); ++i )
		{
			INFO_MSG( "\tNeed to add column %s into table %s\n",
					i->first.c_str(), tableName.c_str() );
		}
		for ( TableMetaData::NameToColInfoMap::const_iterator i =
			obsoleteColumns.begin(); i != obsoleteColumns.end(); ++i )
		{
			INFO_MSG( "\tNeed to delete column %s from table %s\n",
					i->first.c_str(), tableName.c_str() );
		}
		for ( TableMetaData::NameToUpdatedColInfoMap::const_iterator i =
				updatedColumns.begin(); i != updatedColumns.end(); ++i )
		{
			const char * indexedStr = (i->second.indexType ==
					IMySqlColumnMapping::IndexTypeNone) ?
					"non-indexed" : "indexed";
//			std::string oldcolumnTypeStr =
//					i->second.oldColumnType.getAsString( this->get() );

			INFO_MSG( "\tNeed to update column %s in table %s to "
					"%s (%s)\n", i->first.c_str(), tableName.c_str(),
//					i->second.oldColumnType.getAsString( this->get() ).c_str(),
					i->second.columnType.getAsString( connection_,
							i->second.indexType ).c_str(),
					indexedStr );
		}

		return false;	// We didn't update the table
	}

	bool TableValidator::onNeedDeleteTables( const StrSet& tableNames )
	{
		for ( StrSet::const_iterator i = tableNames.begin();
				i != tableNames.end(); ++i )
		{
			INFO_MSG( "Need to remove table %s\n", i->c_str() );
		}

		return false;	// We didn't delete the tables.
	}

	class TableInitialiser : public TableInspector
	{
	public:
		TableInitialiser( MySql& con ) : TableInspector( con ) {}

	protected:
		// Override interface methods
		virtual bool onNeedNewTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& columns );
		virtual bool onNeedUpdateTable( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& obsoleteColumns,
				const TableMetaData::NameToColInfoMap& newColumns,
				const TableMetaData::NameToUpdatedColInfoMap& updatedColumns );
		virtual bool onNeedDeleteTables( const StrSet& tableNames );

	private:
		void addNewColumns( const std::string& tableName,
				const TableMetaData::NameToColInfoMap& columns,
				bool shouldPrintInfo );
	};

	// Override interface methods
	bool TableInitialiser::onNeedNewTable( const std::string& tableName,
			const TableMetaData::NameToColInfoMap& columns )
	{
		INFO_MSG("\tCreating table %s\n", tableName.c_str());
		connection_.execute( "CREATE TABLE IF NOT EXISTS " + tableName +
				" (id BIGINT AUTO_INCREMENT, PRIMARY KEY idKey (id)) "
				"ENGINE="MYSQL_ENGINE_TYPE );
		// __kyl__ (28/7/2005) We can't create a table with no columns so
		// we create one with the id column even though it may not be
		// needed. We'll delete the id column later on.
		bool deleteIDCol = false;
		TableMetaData::NameToColInfoMap newColumns( columns );
		TableMetaData::NameToColInfoMap::iterator idIter =
				newColumns.find( ID_COLUMN_NAME );
		if (idIter != newColumns.end())
		{
			newColumns.erase( idIter );
		}
		else
		{
			deleteIDCol = true;
		}

		// TODO: Incorporate columns into CREATE TABLE instead of adding
		// them one by one.
		this->addNewColumns( tableName, newColumns, false );

		// delete unnecessary ID column that we created table with.
		if (deleteIDCol)
		{
			connection_.execute( "ALTER TABLE " + tableName + " DROP COLUMN "
					ID_COLUMN_NAME );
		}

		return true;	// We've created the table.
	}

	bool TableInitialiser::onNeedUpdateTable( const std::string& tableName,
			const TableMetaData::NameToColInfoMap& obsoleteColumns,
			const TableMetaData::NameToColInfoMap& newColumns,
			const TableMetaData::NameToUpdatedColInfoMap& updatedColumns )
	{
		this->addNewColumns( tableName, newColumns, true );

		// TODO: Do this with one statement instead of issuing one for each
		// column.
		// Delete obsolete columns
		for ( TableMetaData::NameToColInfoMap::const_iterator
				i = obsoleteColumns.begin(); i != obsoleteColumns.end(); ++i )
		{
			INFO_MSG( "\tDeleting column %s from table %s\n",
						i->first.c_str(), tableName.c_str() );
			removeEntityTableIndex( connection_, tableName, i->first,
					i->second.indexType );
			connection_.execute( "ALTER TABLE " + tableName + " DROP COLUMN "
					+ i->first );
		}

		// Update changed columns
		for ( TableMetaData::NameToUpdatedColInfoMap::const_iterator i =
				updatedColumns.begin(); i != updatedColumns.end(); ++i )
		{
			std::string columnTypeStr =
					i->second.columnType.getAsString( connection_,
							i->second.indexType );
//				std::string oldcolumnTypeStr =
//						i->second.oldColumnType.getAsString( this->get() );

			INFO_MSG( "\tUpdating type of column %s in table %s to %s "
					"(%sindexed)\n",
					i->first.c_str(), tableName.c_str(),
					// oldcolumnTypeStr.c_str(),
					columnTypeStr.c_str(),
					(i->second.indexType == IMySqlColumnMapping::IndexTypeNone) ?
						"non-": "" );

			removeEntityTableIndex( connection_, tableName, i->first,
					i->second.oldIndexType );

			connection_.execute( "ALTER TABLE " + tableName + " MODIFY COLUMN "
					+ i->first + " " + columnTypeStr );

			createEntityTableIndex( connection_, tableName, i->first,
					i->second );
		}

		return true;	// We've updated the table
	}

	bool TableInitialiser::onNeedDeleteTables( const StrSet& tableNames )
	{
		for ( StrSet::const_iterator i = tableNames.begin();
				i != tableNames.end(); ++i )
		{
			INFO_MSG( "\tDeleting table %s\n", i->c_str() );
			connection_.execute( "DROP TABLE " + *i );
		}

		return true;	// We've deleted the tables.
	}

	// Adds columns into the table
	void TableInitialiser::addNewColumns( const std::string& tableName,
			const TableMetaData::NameToColInfoMap& columns,
			bool shouldPrintInfo )
	{
		for ( TableMetaData::NameToColInfoMap::const_iterator
				i = columns.begin(); i != columns.end(); ++i )
		{
			if (shouldPrintInfo)
			{
				INFO_MSG( "\tAdding column %s into table %s\n",
							i->first.c_str(), tableName.c_str() );
			}

			connection_.execute( "ALTER TABLE " + tableName + " ADD COLUMN "
					+ i->first + " " +
					i->second.columnType.getAsString( connection_,
							i->second.indexType ) );
			createEntityTableIndex( connection_, tableName, i->first,
					i->second );

			if (!i->second.columnType.isDefaultValueSupported())
			{
				// We have to manually set value of column
				setColumnValue( connection_, tableName, i->first,
						i->second.columnType.defaultValue );
			}
		}
	}

	/**
	 *	This class collects the names and ID of entity types and updates
	 * 	the bigworldEntityTypes table.
	 */
	class TypesCollector
	{
	public:
		TypesCollector( MySql& connection ) : metaData_( connection ), types_()
		{}

		void addType( EntityTypeID bigworldID, const std::string& name );

		void deleteUnwantedTypes();

	private:
		BigWorldMetaData	metaData_;
		StrSet 				types_;
	};

	/**
	 *	Let us know about an entity type in the entity definitions.
	 */
	void TypesCollector::addType( EntityTypeID bigworldID,
			const std::string& name )
	{
		bool inserted = types_.insert( name ).second;
		if (!inserted)
			throw std::runtime_error( "type " + name + " requested twice" );

		EntityTypeID typeID = metaData_.getEntityTypeID( name );
		if ( typeID == INVALID_TYPEID )
			metaData_.addEntityType( name, bigworldID );
		else if ( typeID != bigworldID )
			metaData_.setEntityTypeID( name, bigworldID );
	}

	/**
	 *	Remove rows in bigworldEntityTypes that are no longer in the current
	 * 	entity definitions. i.e. those that were not added using addType().
	 */
	void TypesCollector::deleteUnwantedTypes()
	{
		std::string cleanupTypes = "DELETE FROM bigworldEntityTypes WHERE 1=1";
		for ( StrSet::const_iterator i = types_.begin(); i != types_.end(); ++i )
		{
			cleanupTypes += " AND name != '" + *i + "'";
		}
		metaData_.connection().execute( cleanupTypes );
	}
}

// -----------------------------------------------------------------------------
// Section: class SimpleTableCollector
// -----------------------------------------------------------------------------

/**
 *	IMySqlTableMapping::IVisitor override.
 */
bool SimpleTableCollector::onVisitTable( IMySqlTableMapping& table )
{
	ColumnsCollector colCol;
	table.visitIDColumnWith( colCol );
	table.visitColumnsWith( colCol );

	tables_[table.getTableName()] = colCol.getColumnsInfo();

	return true;
}

/**
 * 	Adds the tables from the other SimpleTableCollector into this one.
 */
SimpleTableCollector& SimpleTableCollector::operator+=(
		const SimpleTableCollector& rhs )
{
	for ( NewTableDataMap::const_iterator i = rhs.tables_.begin();
			i != rhs.tables_.end(); ++i )
	{
		tables_.insert( *i );
	}
	return *this;
}

// -----------------------------------------------------------------------------
// Section: property mappings
// -----------------------------------------------------------------------------

namespace
{
	/**
	 *	This method gets the default value section for the child type based on
	 * 	the parent type's default value section. If it cannot find the section,
	 * 	then it uses the child's default section.
	 */
	DataSectionPtr getChildDefaultSection( const DataType& childType,
			const std::string childName, DataSectionPtr pParentDefault )
	{
		DataSectionPtr pChildDefault = (pParentDefault) ?
				pParentDefault->openSection( childName ) :
				DataSectionPtr( NULL );
		if (!pChildDefault)
		{
			pChildDefault = childType.pDefaultSection();
		}

		return pChildDefault;
	}

	/**
	 * 	This method gets the default value section for the DataDescription.
	 */
	DataSectionPtr getDefaultSection( const DataDescription& dataDesc )
	{
		DataSectionPtr pDefaultSection = dataDesc.pDefaultSection();
		if (!pDefaultSection)
		{
			pDefaultSection = dataDesc.dataType()->pDefaultSection();
		}
		return pDefaultSection;
	}

	/**
	 *	This class helps to build names for table columns. Introduced when
	 *	due to nested properties. It tries to achieve the following:
	 *		- Table names are fully qualified.
	 *		- Column names are relative to the current table.
	 */
	class Namer
	{
		typedef std::vector< std::string >			Strings;
		typedef std::vector< Strings::size_type >	Levels;

		std::string tableNamePrefix_;
		Strings		names_;
		Levels		tableLevels_;

	public:
		Namer( const std::string& entityName,
				const std::string& tableNamePrefix ) :
			tableNamePrefix_( tableNamePrefix ), names_( 1, entityName ),
			tableLevels_( 1, 1 )
		{}

		Namer( const Namer& existing, const std::string& propName, bool isTable ) :
			tableNamePrefix_( existing.tableNamePrefix_ ),
			names_( existing.names_ ), tableLevels_( existing.tableLevels_ )
		{
			if ( propName.empty() )
				names_.push_back( isTable ? DEFAULT_SEQUENCE_TABLE_NAME :
									DEFAULT_SEQUENCE_COLUMN_NAME  );
			else
				names_.push_back( propName );
			if (isTable)
				tableLevels_.push_back( names_.size() );
		}

		std::string buildColumnName( const std::string& prefix,
									const std::string& propName ) const
		{
			std::string suffix =
				(propName.empty()) ? DEFAULT_SEQUENCE_COLUMN_NAME : propName;
			// TRACE_MSG( "buildColumnName: %s\n", this->buildName( prefix, suffix, tableLevels_.back() ).c_str() );
			return this->buildName( prefix, suffix, tableLevels_.back() );
		}

		std::string buildTableName( const std::string& propName ) const
		{
			std::string suffix =
				(propName.empty()) ? DEFAULT_SEQUENCE_TABLE_NAME : propName;
			// TRACE_MSG( "buildTableName: %s\n", this->buildName( tableNamePrefix_, suffix, tableLevels_.back() ).c_str() );
			return this->buildName( tableNamePrefix_, suffix, 0 );
		}

	private:
		std::string buildName( const std::string& prefix,
							const std::string& suffix,
							Strings::size_type startIdx ) const
		{
			std::string name = prefix;
			for ( Strings::size_type i = startIdx; i < names_.size(); ++i )
			{
				name += '_';
				name += names_[i];
			}
			if (!suffix.empty())
			{
				name += '_';
				name += suffix;
			}
			return name;
		}
	};

	// for UserData, we want a single property mapping to support (possible)
	// lots and lots of different properties; the CompositePropertyMapping
	// handles this (UserData types are the only users of this class... 'tho
	// really the whole type should be encapsualted in one of these)
	class CompositePropertyMapping : public PropertyMapping
	{
	public:
		CompositePropertyMapping( const std::string& propName ) :
			PropertyMapping( propName )
		{}

		void addChild( PropertyMappingPtr child )
		{
			if (!child)
			{
				ERROR_MSG( "CompositePropertyMapping::addChild: "
						"child is null (ignoring)\n" );
				return;
			}
			children_.push_back( child );
		}

		PropertyMappingPtr::ConstProxy getChild( int idx ) const
		{
			return children_[idx];
		}

		PropertyMappingPtr getChild( int idx )
		{
			return children_[idx];
		}

		int getNumChildren() const
		{
			return int(children_.size());
		}

		virtual void prepareSQL( MySql& con )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).prepareSQL( con );
			}
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).streamToBound( strm );
			}
		}
		virtual void boundToStream( BinaryOStream & strm ) const
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).boundToStream( strm );
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			// Assume that data is stored on the stream in the same order as
			// the bindings are created in PyUserTypeBinder::bind().
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).defaultToStream( strm );
			}
		}

		virtual void defaultToBound()
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).defaultToBound();
			}
		}

		virtual bool hasTable() const
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				if ((*ppChild)->hasTable())
					return true;
			}
			return false;
		}

		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).updateTable( transaction, parentID );
			}
		}

		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).getTableData( transaction, parentID );
			}
		}

		virtual void deleteChildren( MySqlTransaction& t, DatabaseID databaseID )
		{
			for (Children::iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				(**ppChild).deleteChildren( t, databaseID );
			}
		}

		virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				if (!(**ppChild).visitParentColumns( visitor ))
					return false;
			}
			return true;
		}

		virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
		{
			for (Children::const_iterator ppChild = children_.begin();
					ppChild != children_.end(); ++ppChild)
			{
				if (!(**ppChild).visitTables( visitor ))
					return false;
			}
			return true;
		}

	protected:
		typedef std::vector<PropertyMappingPtr> Children;

		class SequenceBuffer : public PropertyMapping::ISequenceBuffer
		{
		protected:
			typedef std::vector< PropertyMapping::ISequenceBuffer* > SequenceBuffers;

			auto_container< SequenceBuffers > childBuffers_;

		public:
			SequenceBuffer( const CompositePropertyMapping::Children& children )
			{
				childBuffers_.container.reserve( children.size() );
				for ( CompositePropertyMapping::Children::const_iterator i =
					  children.begin(); i < children.end(); ++i )
				{
					childBuffers_.container.push_back(
						(*i)->createSequenceBuffer() );
				}
			}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				for ( int i = 0; i < numElems; ++i )
				{
					for ( SequenceBuffers::iterator j =
						  childBuffers_.container.begin();
						  j < childBuffers_.container.end(); ++j )
					{
						(*j)->streamToBuffer( 1, strm );
					}
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				for ( SequenceBuffers::const_iterator j =
						childBuffers_.container.begin();
						j < childBuffers_.container.end(); ++j )
				{
					(*j)->bufferToStream( strm, idx );
				}
			}

			virtual void bufferToBound( PropertyMapping& binding,
				                        int idx ) const
			{
				CompositePropertyMapping& compositeProp =
					static_cast<CompositePropertyMapping&>(binding);

				int i = 0;
				for ( SequenceBuffers::const_iterator childBuf =
					  childBuffers_.container.begin();
					  childBuf < childBuffers_.container.end();
				      ++i, ++childBuf )
				{
					(*childBuf)->bufferToBound( *compositeProp.getChild(i),
						                        idx );
				}
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				const CompositePropertyMapping& compositeProp =
					static_cast<const CompositePropertyMapping&>(binding);

				int i = 0;
				for ( SequenceBuffers::const_iterator childBuf =
					  childBuffers_.container.begin();
					  childBuf < childBuffers_.container.end();
				      ++i, ++childBuf )
				{
					(*childBuf)->boundToBuffer( *compositeProp.getChild(i) );
				}
			}

			virtual int	getNumElems() const
			{
				return (childBuffers_.container.size()) ?
					childBuffers_.container[0]->getNumElems() : 0;
			}

			virtual void reset()
			{
				for ( SequenceBuffers::iterator i =
					  childBuffers_.container.begin();
					  i < childBuffers_.container.end(); ++i )
				{
					(*i)->reset();
				}
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new SequenceBuffer( children_ );	}

	protected:
		Children children_;
	};

	typedef SmartPointer<CompositePropertyMapping> CompositePropertyMappingPtr;
	// UserTypeMapping maps USER_TYPE in MySQL. It's a CompositePropertyMapping
	// with special handling for serialisation.
	class UserTypeMapping : public CompositePropertyMapping
	{
	public:
		UserTypeMapping( const std::string& propName )
			: CompositePropertyMapping( propName )
		{}

		virtual void streamToBound( BinaryIStream& strm )
		{
			std::string encStr;
			strm >> encStr;
			MemoryIStream encStrm( const_cast<char*>(encStr.c_str()),
				encStr.length() );
			CompositePropertyMapping::streamToBound( encStrm );
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			MemoryOStream encStrm;
			CompositePropertyMapping::boundToStream( encStrm );
			// __kyl__ (18/5/2005) We are assuming that this is how a
			// std::string gets serialised.
			strm.appendString( reinterpret_cast<const char *>(encStrm.data()),
				encStrm.size() );
		}

	private:
		// Override CompositePropertyMapping::SequenceBuffer stream methods.
		class UserPropSequenceBuffer : public SequenceBuffer
		{
		public:
			UserPropSequenceBuffer( const CompositePropertyMapping::Children& children )
				: SequenceBuffer( children )
			{}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				for ( int i = 0; i < numElems; ++i )
				{
					std::string encStr;
					strm >> encStr;
					MemoryIStream encStrm(
						const_cast<char*>(encStr.c_str()), encStr.length() );
					SequenceBuffer::streamToBuffer( 1, encStrm );
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				MemoryOStream encStrm;
				SequenceBuffer::bufferToStream( encStrm, idx );
				// __kyl__ (18/5/2005) We are assuming that this is how a
				// std::string gets serialised.
				strm.appendString( reinterpret_cast<const char *>(
									encStrm.data()), encStrm.size() );
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new UserPropSequenceBuffer( children_ );	}
	};

	// UserTypeMapping maps CLASS and FIXED_DICT types into MySQL columns and
	// tables. It's basically a CompositePropertyMapping with support for object
	// being null.
	// NOTE: Theoretically ClassMapping should not derive from
	// IMySqlColumnMapping since it consists of 0 or more columns, but this
	// just makes the implementation easier.
	class ClassMapping : public CompositePropertyMapping,
			private IMySqlColumnMapping
	{
		bool		allowNone_;
		std::string	colName_;
		uint8		hasProps_;

	public:
		ClassMapping( const Namer& namer, const std::string& propName,
					bool allowNone )
			: CompositePropertyMapping( propName ), allowNone_(allowNone),
			colName_( allowNone_ ? namer.buildColumnName( "fm", propName ) : "" ),
			hasProps_(1)
		{}

		virtual void streamToBound( BinaryIStream& strm )
		{
			if (allowNone_)
				strm >> hasProps_;
			if (hasProps_)
				CompositePropertyMapping::streamToBound( strm );
			else
				this->defaultToBound();
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (allowNone_)
				strm << hasProps_;
			if (hasProps_)
				CompositePropertyMapping::boundToStream( strm );
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			if (allowNone_)
				strm << uint8(0) ;
			else
				CompositePropertyMapping::defaultToStream( strm );
		}

		virtual void defaultToBound()
		{
			this->setHasProps( 0 );
			CompositePropertyMapping::defaultToBound();
		}

		bool  isAllowNone() const			{ return allowNone_; }
		uint8 getHasProps() const			{ return hasProps_; }
		void setHasProps( uint8 hasProps )	{ hasProps_ = hasProps; }

		virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
		{
			if (allowNone_)
			{
				if (!visitor.onVisitColumn( *this ))
					return false;
			}
			return CompositePropertyMapping::visitParentColumns( visitor );
		}

		// IMySqlColumnMapping overrides
		// These functions are only called when allowNone is true.
		virtual const std::string& getColumnName() const
		{
			return colName_;
		}
		virtual void getColumnType( Type& type ) const
		{
			type.fieldType = MYSQL_TYPE_TINY;
			type.setIsUnsigned( true );
			type.defaultValue = "1";
		}
		virtual IndexType getColumnIndexType() const
		{
			return IndexTypeNone;
		}
		virtual bool hasBinding() const
		{
			return true;
		}
		virtual void addSelfToBindings( MySqlBindings& bindings )
		{
			// Don't need to check allowNone_ since we don't give out our
			// IMySqlColumnMapping interface unless allowNone_ is true.
			bindings << hasProps_;
		}

	private:
		// Override CompositePropertyMapping::SequenceBuffer stream methods.
		// This sequence buffer only works if allowNone is true.
		class ClassPropSequenceBuffer : public SequenceBuffer
		{
			// Because class props can be null,
			// ClassPropSequenceBuffer::bufferToStream( strm, i ) doesn't
			// correspond to CompositePropertyMapping::SequenceBuffer::
			// bufferToStream( strm, i ). We need to calculate a new 'i'
			// (say 'j') by subtracting the number null values between 0 and i.
			// nonNullIdxs_ is the precomputed 'j' so that j = nonNullIdxs_[i];
			// And if the i-th value is null, then
			// nonNullIdxs_[i] == nonNullIdxs_[i+1];
			std::vector<int> nonNullIdxs_;

		public:
			ClassPropSequenceBuffer( const CompositePropertyMapping::Children& children )
				: SequenceBuffer( children ), nonNullIdxs_( 1, 0 )
			{}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				uint8 hasProps;
				for ( int i = 0; i < numElems; ++i )
				{
					strm >> hasProps;
					if (hasProps)
					{
						nonNullIdxs_.push_back( nonNullIdxs_.back() + 1 );
						SequenceBuffer::streamToBuffer( 1, strm );
					}
					else
					{
						nonNullIdxs_.push_back( nonNullIdxs_.back() );
					}
				}
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				int	realIdx = nonNullIdxs_[idx];
				uint8 hasProps = (realIdx < nonNullIdxs_[idx + 1]) ? 1 : 0;
				strm << hasProps;
				if (hasProps)
					SequenceBuffer::bufferToStream( strm, realIdx );
			}

			virtual void bufferToBound( PropertyMapping& binding,
				                        int idx ) const
			{
				ClassMapping& classMapping =
					static_cast<ClassMapping&>(binding);
				int	realIdx = nonNullIdxs_[idx];
				if (realIdx < nonNullIdxs_[idx + 1])
				{
					classMapping.setHasProps( 1 );
					SequenceBuffer::bufferToBound( binding, realIdx );
				}
				else
				{
					classMapping.defaultToBound();
				}
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				const ClassMapping& classMapping =
					static_cast<const ClassMapping&>(binding);
				if (classMapping.getHasProps())
				{
					SequenceBuffer::boundToBuffer( binding );
					nonNullIdxs_.push_back( nonNullIdxs_.back() + 1);
				}
				else
				{
					nonNullIdxs_.push_back( nonNullIdxs_.back() );
				}
			}

			virtual int	getNumElems() const
			{
				return int(nonNullIdxs_.size() - 1);
			}

			virtual void reset()
			{
				nonNullIdxs_.resize(1);
				SequenceBuffer::reset();
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer() const
		{
			return (allowNone_) ? new ClassPropSequenceBuffer( children_ ) :
				// Can use simpler sequence buffer if none is disallowed.
				CompositePropertyMapping::createSequenceBuffer();
		}

	};

	// map sequences to tables
	class SequenceMapping : public PropertyMapping, public IMySqlTableMapping,
							private IMySqlTableMapping::IRowBuffer
	{
	public:
		SequenceMapping( const Namer& namer, const std::string& propName,
			PropertyMappingPtr child, int size = 0 ) :
			PropertyMapping( propName ),
			tblName_( namer.buildTableName( propName ) ),
			child_(child), size_(size), pBuffer_( 0 ), childHasTable_(false)
		{}

		~SequenceMapping()
		{
			delete pBuffer_;
		}

		const PropertyMapping& getChildMapping() const	{	return *child_;	}

		bool isFixedSize() const	{ return size_ != 0; }
		int getFixedSize() const	{ return size_;	}

		virtual void prepareSQL( MySql& con )
	   	{
			// NOTE: child->createSequenceBuffer() can't be initialised in the
			// constructor because UserTypeMapping doesn't not have its
			// children set up yet.
			pBuffer_ = child_->createSequenceBuffer();
			if (!pBuffer_)
				ERROR_MSG( "Persistence to MySQL is not supported for the "
						   "type of sequence used by '%s'.", propName().c_str() );

			childHasTable_ = child_->hasTable();

			std::string stmt;
			MySqlBindings b;

			CommaSepColNamesBuilder colNamesBuilder( *child_ );
			std::string	childColNames = colNamesBuilder.getResult();
			int			childNumColumns = colNamesBuilder.getCount();
			MF_ASSERT( childHasTable_ || (childNumColumns > 0) );

			ColumnsBindingsBuilder	childColumnsBindings( *child_ );

			stmt = "SELECT ";
			if (childHasTable_)
				stmt += "id";
		   	if (childNumColumns)
			{
				if (childHasTable_)
					stmt += ",";
				stmt += childColNames;
			}
			stmt += " FROM " + tblName_ + " WHERE parentID=? ORDER BY id";
			pSelect_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			if (childHasTable_)
				b << childID_;
			b << childColumnsBindings.getBindings();
			pSelect_->bindResult( b );
			b.clear();
			b << queryID_;
			pSelect_->bindParams( b );

			stmt = "SELECT id FROM " + tblName_ + " WHERE parentID=? ORDER BY "
					"id FOR UPDATE";
			pSelectChildren_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			b << childID_;
			pSelectChildren_->bindResult( b );
			b.clear();
			b << queryID_;
			pSelectChildren_->bindParams( b );

			stmt = "INSERT INTO " + tblName_ + " (parentID";
			if (childNumColumns)
				stmt += "," + childColNames;
		   	stmt += ") VALUES (" +
				buildCommaSeparatedQuestionMarks( 1 + childNumColumns ) + ")";
			pInsert_.reset( new MySqlStatement( con, stmt ) );

			stmt = "UPDATE " + tblName_ + " SET parentID=?";
			if (childNumColumns)
			{
				CommaSepColNamesBuilderWithSuffix
						updateColNamesBuilder( *child_, "=?" );
				std::string	updateCols = updateColNamesBuilder.getResult();

				stmt += "," + updateCols;
			}
			stmt += " WHERE id=?";
			pUpdate_.reset( new MySqlStatement( con, stmt ) );

			b.clear();
			b << queryID_;
			b << childColumnsBindings.getBindings();
			pInsert_->bindParams( b );
			b << childID_;
			pUpdate_->bindParams( b );

			stmt = "DELETE FROM " + tblName_ + " WHERE parentID=?";
			pDelete_.reset( new MySqlStatement( con, stmt ) );
			stmt += " AND id >= ?";
			pDeleteExtra_.reset( new MySqlStatement( con, stmt ) );
			b.clear();
			b << queryID_;
			pDelete_->bindParams( b );
			b << childID_;
			pDeleteExtra_->bindParams( b );

			child_->prepareSQL( con );
		}

		// Get the number of elements present in the stream.
		int getNumElemsFromStrm( BinaryIStream& strm ) const
		{
			if (this->isFixedSize())
				return this->getFixedSize();

			int numElems;
			strm >> numElems;
			return numElems;
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			int numElems = this->getNumElemsFromStrm( strm );

			if (pBuffer_)
			{
				pBuffer_->reset();
				pBuffer_->streamToBuffer( numElems, strm );
			}
			else
			{	// Sequence type not supported. Skip over data in stream.
				for ( int i = 0; i < numElems; ++i )
					child_->streamToBound( strm );
			}
		}

		int setNumElemsInStrm( BinaryOStream & strm, int numElems ) const
		{
			if (this->isFixedSize())
				return this->getFixedSize();

			strm << numElems;
			return numElems;
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (pBuffer_)
			{
				int numAvailElems = pBuffer_->getNumElems();
				int numElems = setNumElemsInStrm( strm, numAvailElems );
				int numFromBuf = std::min( numElems, numAvailElems );
				for ( int i = 0; i < numFromBuf; ++i )
				{
					pBuffer_->bufferToStream( strm, i );
				}
				for ( int i = numFromBuf; i < numElems; ++i )
				{
					child_->defaultToStream( strm );
				}
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			if (this->isFixedSize())
			{
				int numElems = this->getFixedSize();
				strm << numElems;
				for ( int i = 0; i < numElems; ++i )
				{
					child_->defaultToStream( strm );
				}
			}
			else
			{
				strm << int(0);
			}
		}

		virtual void defaultToBound()
		{
			if (pBuffer_)
				pBuffer_->reset();
		}

		virtual bool hasTable() const	{	return true;	}

		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			if (pBuffer_)
			{
				int numElems = pBuffer_->getNumElems();
				if (numElems == 0)	// Optimisation for empty arrays
				{
					this->deleteChildren( transaction, parentID );
				}
				else
				{
					// my_ulonglong affectedRows = 0;

					queryID_ = parentID;
					transaction.execute( *pSelectChildren_ );
					int numRows = pSelectChildren_->resultRows();
					int numUpdates = std::min( numRows, numElems );

					// Update existing rows
					for ( int i = 0; i < numUpdates; ++i )
					{
						pSelectChildren_->fetch();
						pBuffer_->bufferToBound( *child_, i );
						transaction.execute( *pUpdate_ );

						// affectedRows += transaction.affectedRows();

						if (childHasTable_)
						{
							child_->updateTable( transaction, childID_ );
						}
					}

					// Delete any extra rows (i.e. array has shrunk).
					if (pSelectChildren_->fetch())
					{
						transaction.execute( *pDeleteExtra_ );
						// affectedRows += transaction.affectedRows();
						if (childHasTable_)
						{
							do
							{
								child_->deleteChildren( transaction, childID_ );
							} while ( pSelectChildren_->fetch() );
						}
					}
					// Insert any extra rows (i.e. array has grown)
					else if (numElems > numRows)
					{
						// TODO: Multi-row insert in one statement.
						for (int i = numRows; i < numElems; ++i)
						{
							pBuffer_->bufferToBound( *child_, i );
							transaction.execute( *pInsert_ );
							// affectedRows += transaction.affectedRows();
							if (childHasTable_)
							{
								DatabaseID insertID = transaction.insertID();
								child_->updateTable( transaction, insertID );
							}
						}
					}
					// DEBUG_MSG( "SequenceMapping::updateTable: table=%s, "
					//		"affectedRows=%d\n", tblName_.c_str(),
					//		int(affectedRows) );
				}
			}
		}

		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID )
		{
			if (pBuffer_)
			{
				pBuffer_->reset();

				queryID_ = parentID;
				transaction.execute( *pSelect_ );
				int numElems = pSelect_->resultRows();

				for ( int i = 0; i < numElems; ++i )
				{
					pSelect_->fetch();
					if (childHasTable_)
						child_->getTableData( transaction, childID_ );
					pBuffer_->boundToBuffer( *child_ );
				}
			}
		}

		virtual void deleteChildren( MySqlTransaction& t, DatabaseID databaseID )
		{
			queryID_ = databaseID;
			if (childHasTable_)
			{
				t.execute( *pSelectChildren_ );
				while ( pSelectChildren_->fetch() )
				{
					child_->deleteChildren( t, childID_ );
				}
			}
			t.execute( *pDelete_ );
		}

		PropertyMapping::ISequenceBuffer* swapChildSeqBuffer(
			PropertyMapping::ISequenceBuffer* pBuffer )
		{
			MF_ASSERT( pBuffer_ );
			PropertyMapping::ISequenceBuffer* pCurBuf = pBuffer_;
			pBuffer_ = pBuffer;
			return pCurBuf;
		}

		virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
		{
			// We don't add any columns to our parent's table.
			return true;
		}
		virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
		{
			return visitor.onVisitTable( *this );
		}

		// IMySqlTableMapping overrides
		virtual const std::string& getTableName() const
		{	return tblName_;	}
		virtual bool visitColumnsWith( IMySqlColumnMapping::IVisitor& visitor )
		{
			MySqlColumnMappingAdapter< DatabaseID > parentIdColumn(
					PARENTID_COLUMN_NAME_STR, PARENTID_COLUMN_TYPE,
					IMySqlColumnMapping::IndexTypeParentID, queryID_ );
			if (!visitor.onVisitColumn( parentIdColumn ))
				return false;

			return child_->visitParentColumns( visitor );
		}
		virtual bool visitIDColumnWith( IMySqlIDColumnMapping::IVisitor& visitor )
		{
			MySqlIDColumnMappingAdapter idColumn( childID_ );
			return visitor.onVisitIDColumn( idColumn );
		}
		virtual bool visitSubTablesWith( IMySqlTableMapping::IVisitor& visitor )
		{
			return child_->visitTables( visitor );
		}
		virtual IMySqlTableMapping::IRowBuffer* getRowBuffer()
		{
			// For convenience, we inherited from IMySqlTableMapping::IRowBuffer
			// so we can pretend to be a row buffer without having to wrap
			// pBuffer_ with another class.
			return (pBuffer_) ? this : NULL;
		}

		// IMySqlTableMapping::IRowBuffer overrides
		virtual void addBoundData()
		{
			pBuffer_->boundToBuffer( *child_ );
		}
		virtual void setBoundData( int row )
		{
			pBuffer_->bufferToBound( *child_, row );
		}
		virtual int getNumRows() const	{ return pBuffer_->getNumElems(); }
		virtual void clear()			{ pBuffer_->reset(); }

	private:
		class SequenceBuffer : public PropertyMapping::ISequenceBuffer
		{
			typedef std::vector< PropertyMapping::ISequenceBuffer* > SequenceBuffers;

			const SequenceMapping& mapping_;
			auto_container< SequenceBuffers > childBuffers_;
			int	numUsed_;
			mutable int swappedIdx_;

		public:
			SequenceBuffer(const SequenceMapping& mapping)
				: mapping_(mapping), numUsed_(0), swappedIdx_(-1)
			{
				childBuffers_.container.push_back(
					mapping_.getChildMapping().createSequenceBuffer() );
			}

			virtual void streamToBuffer( int numElems, BinaryIStream& strm )
			{
				int numRequired = numUsed_ + numElems;
				for ( int i = int(childBuffers_.container.size());
						i < numRequired; ++i )
				{
					childBuffers_.container.push_back(
						mapping_.getChildMapping().createSequenceBuffer() );
				}

				for ( int i = numUsed_; i < numRequired; ++i )
				{
					int numChildElems = mapping_.getNumElemsFromStrm( strm );
					childBuffers_.container[i]->
						streamToBuffer( numChildElems, strm );
				}
				numUsed_ = numRequired;
				swappedIdx_ = -1;
			}

			virtual void bufferToStream( BinaryOStream& strm, int idx ) const
			{
				MF_ASSERT( swappedIdx_ < 0 );
				PropertyMapping::ISequenceBuffer* pChildSeqBuf =
					childBuffers_.container[idx];
				int numAvail = pChildSeqBuf->getNumElems();
				int numElems = mapping_.setNumElemsInStrm( strm, numAvail );
				int numFromBuf = std::min( numElems, numAvail );
				for ( int i = 0; i < numFromBuf; ++i )
				{
					pChildSeqBuf->bufferToStream( strm, i );
				}
				for ( int i = numFromBuf; i < numElems; ++i )
				{
					mapping_.getChildMapping().defaultToStream( strm );
				}
			}

			virtual void bufferToBound( PropertyMapping& binding,
										int idx ) const
			{
				MF_ASSERT( idx < numUsed_ );
				// We actually swap the buffer with the bindings instead of
				// copying it to the bindings.
				if (swappedIdx_ == idx)
					return;	// Data already there.

				SequenceBuffer* pThis = const_cast<SequenceBuffer*>(this);
				SequenceMapping& seqMapping = static_cast<SequenceMapping&>(binding);
				PropertyMapping::ISequenceBuffer* pPrevBuffer =
					seqMapping.swapChildSeqBuffer( childBuffers_.container[idx] );

				// Remember the index of the swapped buffer so that we can
				// swap them back later on.
				if (swappedIdx_ >= 0)
				{
					// We swapped the buffer previously so pPrevBuffer must be
					// the buffer that was at swappedIdx_. Restore it.
					pThis->childBuffers_.container[idx] =
							pThis->childBuffers_.container[swappedIdx_];
					pThis->childBuffers_.container[swappedIdx_] = pPrevBuffer;
			}
				else
				{
					pThis->childBuffers_.container[idx] = pPrevBuffer;
				}
				swappedIdx_ = idx;
			}

			virtual void boundToBuffer( const PropertyMapping& binding )
			{
				MF_ASSERT( swappedIdx_ < 0 );
				if (numUsed_ == int(childBuffers_.container.size()))
				{
					childBuffers_.container.push_back(
						mapping_.getChildMapping().createSequenceBuffer() );
				}

				SequenceMapping& seqMapping = const_cast<SequenceMapping&>(
					static_cast<const SequenceMapping&>(binding) );
				childBuffers_.container[numUsed_] =
					seqMapping.swapChildSeqBuffer( childBuffers_.container[numUsed_] );
				++numUsed_;
			}

			virtual int	getNumElems() const
			{
				return numUsed_;
			}

			virtual void reset()
			{
				numUsed_ = 0;
				swappedIdx_ = -1;
				for ( SequenceBuffers::iterator i = childBuffers_.container.begin();
					  i < childBuffers_.container.end(); ++i )
				{
					(*i)->reset();
				}
			}
		};

	public:
		virtual ISequenceBuffer* createSequenceBuffer()	const
		{
			return new SequenceBuffer(*this);
		}

	private:

		std::string tblName_;
		PropertyMappingPtr child_;
		int	size_;
		ISequenceBuffer* pBuffer_;
		DatabaseID queryID_;
		DatabaseID childID_;
		bool childHasTable_;

		// auto_ptr's so we can delay instantiation
		std::auto_ptr<MySqlStatement> pSelect_;
		std::auto_ptr<MySqlStatement> pSelectChildren_;
		std::auto_ptr<MySqlStatement> pDelete_;
		std::auto_ptr<MySqlStatement> pDeleteExtra_;
		std::auto_ptr<MySqlStatement> pInsert_;
		std::auto_ptr<MySqlStatement> pUpdate_;
	};

	/**
	 *	Utility class used by various PropertyMapping classes to implement
	 *	their PropertyMapping::ISequenceBuffer.
	 */
	template < class STRM_TYPE, class MAPPING_TYPE >
	class PrimitiveSequenceBuffer : public PropertyMapping::ISequenceBuffer
	{
		typedef	std::vector< STRM_TYPE > Buffer;
		Buffer	buffer_;

	public:
		PrimitiveSequenceBuffer()
			: buffer_()
		{}

		virtual void streamToBuffer( int numElems,
				                        BinaryIStream& strm )
		{
			typename Buffer::size_type numUsed = buffer_.size();
			buffer_.resize( buffer_.size() + numElems );

			for ( typename Buffer::iterator pVal = buffer_.begin() + numUsed;
				  pVal < buffer_.end(); ++pVal )
				strm >> *pVal;
		}

		virtual void bufferToStream( BinaryOStream& strm, int idx ) const
		{
			strm << buffer_[idx];
		}

		// Set binding with value of element number "idx" in the buffer.
		virtual void bufferToBound( PropertyMapping& binding,
				                    int idx ) const
		{
			static_cast<MAPPING_TYPE&>(binding).setValue( buffer_[idx] );
		}

		virtual void boundToBuffer( const PropertyMapping& binding )
		{
			buffer_.push_back( static_cast<const MAPPING_TYPE&>(binding).getValue() );
		}

		virtual int	getNumElems() const	{ return int(buffer_.size());	}

		virtual void reset()	{	buffer_.clear();	}
	};

	template <class STRM_NUM_TYPE>
	class NumMapping : public PropertyMapping, public IMySqlColumnMapping
	{
	public:
		NumMapping( const std::string& propName,
				DataSectionPtr pDefaultValue ) :
			PropertyMapping( propName ),
			colName_( propName ),
			defaultValue_(0)
		{
			if (pDefaultValue)
				defaultValue_ = pDefaultValue->as<STRM_NUM_TYPE>();
		}

		NumMapping( const Namer& namer, const std::string& propName,
				DataSectionPtr pDefaultValue ) :
			PropertyMapping( propName ),
			colName_( namer.buildColumnName( "sm", propName ) ),
			defaultValue_(0)
		{
			if (pDefaultValue)
				defaultValue_ = pDefaultValue->as<STRM_NUM_TYPE>();
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			STRM_NUM_TYPE i;
			strm >> i;
			value_.set(i);
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			const STRM_NUM_TYPE* pi = value_.get();
			if (pi)
				strm << *pi;
			else
				strm << defaultValue_;
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			strm << defaultValue_;
		}

		virtual void defaultToBound()
		{
			this->setValue( defaultValue_ );
		}

		virtual bool hasTable() const	{	return false;	}
		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type
		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type

		virtual void prepareSQL( MySql& ) {}
		virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new PrimitiveSequenceBuffer< STRM_NUM_TYPE, NumMapping< STRM_NUM_TYPE > >();	}

		void setValue( STRM_NUM_TYPE val )	{	value_.set( val );	}
		STRM_NUM_TYPE getValue() const
		{
			const STRM_NUM_TYPE* pNum = value_.get();
			return (pNum) ? *pNum : defaultValue_;
		}

		virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
		{
			return visitor.onVisitColumn( *this );
		}
		virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
		{
			return true;
		}

		// IMySqlColumnMapping overrides
		virtual const std::string& getColumnName() const
		{
			return colName_;
		}
		virtual void getColumnType( Type& type ) const
		{
			type.fieldType = MySqlTypeTraits<STRM_NUM_TYPE>::colType;
			type.setIsUnsigned( !std::numeric_limits<STRM_NUM_TYPE>::is_signed );
			type.defaultValue = StringConv::toStr( defaultValue_ );
		}
		virtual IndexType getColumnIndexType() const
		{
			return IndexTypeNone;
		}
		virtual bool hasBinding() const
		{
			return true;
		}
		virtual void addSelfToBindings( MySqlBindings& bindings )
		{
			bindings << value_;
		}

	private:
		std::string colName_;
		MySqlValueWithNull<STRM_NUM_TYPE> value_;
		STRM_NUM_TYPE defaultValue_;
	};

	template <class Vec, int DIM>
	class VectorMapping : public PropertyMapping
	{
	public:
		VectorMapping( const Namer& namer, const std::string& propName,
					DataSectionPtr pDefaultValue ) :
			PropertyMapping( propName ),
			colNameTemplate_( namer.buildColumnName( "vm_%i", propName ) ),
			defaultValue_()
		{
			if (pDefaultValue)
				defaultValue_ = pDefaultValue->as<Vec>();
		}

		bool isNull() const
		{
			for ( int i=0; i<DIM; i++ )
				if (!value_[i].get())
					return true;
			return false;
		}

		virtual void streamToBound( BinaryIStream& strm )
		{
			Vec v;
			strm >> v;
			this->setValue(v);
		}

		virtual void boundToStream( BinaryOStream & strm ) const
		{
			if (!this->isNull())
			{
				Vec v;
				for ( int i = 0; i < DIM; ++i )
					v[i] = *value_[i].get();
				strm << v;
			}
			else
			{
				strm << defaultValue_;
			}
		}

		virtual void defaultToStream( BinaryOStream & strm ) const
		{
			strm << defaultValue_;
		}

		virtual void defaultToBound()
		{
			this->setValue( defaultValue_ );
		}

		virtual bool hasTable() const	{	return false;	}
		virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type
		virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}	// No child table for this type

		virtual void prepareSQL( MySql& ) {}
		virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

		virtual ISequenceBuffer* createSequenceBuffer() const
		{	return new PrimitiveSequenceBuffer< Vec, VectorMapping< Vec, DIM > >();	}

		void setValue( const Vec& v )
		{
			for ( int i = 0; i < DIM; ++i )
				value_[i].set(v[i]);
		}
		Vec getValue() const
		{
			if (this->isNull())
				return defaultValue_;

			Vec	v;
			for ( int i = 0; i < DIM; ++i )
				v[i] = *value_[i].get();
			return v;
		}

		virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
		{
			char buffer[512];
			for ( int i=0; i<DIM; i++ )
			{
				bw_snprintf( buffer, sizeof(buffer),
					colNameTemplate_.c_str(), i );
				std::string colName( buffer );
				IMySqlColumnMapping::Type colType( MYSQL_TYPE_FLOAT , false, 0,
						StringConv::toStr( defaultValue_[i] ) );
				MySqlColumnMappingAdapter< MySqlValueWithNull<float> >
						column( colName, colType,
								IMySqlColumnMapping::IndexTypeNone, value_[i] );
				if (!visitor.onVisitColumn( column ))
					return false;
			}
			return true;
		}
		virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
		{
			return true;
		}

	private:
		std::string colNameTemplate_;
		MySqlValueWithNull<float> value_[DIM];
		Vec defaultValue_;
	};
}

// __kyl__ (28/9/2005) StringLikeMapping can't be in anonymous namespace because
// it is forward declared in the header file.
class StringLikeMapping : public PropertyMapping, public IMySqlColumnMapping
{
public:
	StringLikeMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length ) :
		PropertyMapping( propName ),
		colName_( namer.buildColumnName( "sm", propName ) ),
		value_(length), isNameIndex_( isNameIndex ), defaultValue_()
	{}

	void getString( std::string& str ) const	{	str = value_.getString();	}
	void setString( const std::string& str )	{	value_.setString(str);		}

	virtual void streamToBound( BinaryIStream& strm )
	{
		strm >> value_;
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		if (!value_.isNull())
		{
			strm << value_;
		}
		else
		{
			strm << defaultValue_;
		}
	}

	virtual void defaultToStream( BinaryOStream & strm ) const
	{
		strm << defaultValue_;
	}

	virtual void defaultToBound()
	{
		this->setValue( defaultValue_ );
	}

	virtual bool hasTable() const	{	return false;	}
	virtual void updateTable( MySqlTransaction& transaction,
		DatabaseID parentID ) {}	// No child table for this type
	virtual void getTableData( MySqlTransaction& transaction,
		DatabaseID parentID ) {}	// No child table for this type

	virtual void prepareSQL( MySql& ) {}
	virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

	virtual ISequenceBuffer* createSequenceBuffer() const
	{	return new PrimitiveSequenceBuffer< std::string, StringLikeMapping >();	}

	void setValue( const std::string& str )	{	value_.setString(str);	}
	std::string getValue() const
	{
		if (value_.isNull())
			return defaultValue_;
		else
			return value_.getString();
	}

	virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
	{
		return visitor.onVisitColumn( *this );
	}
	virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
	{
		return true;
	}

	// IMySqlColumnMapping overrides
	virtual const std::string& getColumnName() const
	{
		return colName_;
	}
	virtual void getColumnType( Type& type ) const
	{
		type.fieldType =
				MySqlTypeTraits<std::string>::colType( value_.capacity() );
		if (type.fieldType == MYSQL_TYPE_LONG_BLOB)
		{
			// Can't put string > 16MB onto stream.
			CRITICAL_MSG( "StringLikeMapping::StringLikeMapping: Property '%s' "
					"has DatabaseLength %lu that exceeds the maximum supported "
					"length of 16777215\n", this->propName().c_str(),
					value_.capacity() );
		}

		// Derived classes must set type.setIsBinary().

		type.defaultValue = defaultValue_;
	}
	virtual IndexType getColumnIndexType() const
	{
		return (isNameIndex_) ? IndexTypeName : IndexTypeNone;
	}
	virtual bool hasBinding() const
	{
		return true;
	}
	virtual void addSelfToBindings( MySqlBindings& bindings )
	{
		bindings << value_;
	}

	std::string colName_;
	MySqlBuffer value_;
	bool		isNameIndex_;
	std::string	defaultValue_;
};

namespace
{

class TimestampMapping : public PropertyMapping, public IMySqlColumnMapping
{
public:
	TimestampMapping() : PropertyMapping( TIMESTAMP_COLUMN_NAME ) {}

	// No binding for this type (auto updated by MySQL)
	virtual void streamToBound( BinaryIStream& strm ) {}
	virtual void boundToStream( BinaryOStream & strm ) const {}
	virtual void defaultToStream( BinaryOStream & strm ) const {}
	virtual void defaultToBound() {}
	virtual void addSelfToBindings( MySqlBindings& bindings ) {}

	// No child or parent tables for this type
	virtual bool hasTable() const	{	return false;	}
	virtual void updateTable( MySqlTransaction& transaction,
		DatabaseID parentID ) {}
	virtual void getTableData( MySqlTransaction& transaction,
		DatabaseID parentID ) {}
	virtual void deleteChildren( MySqlTransaction&, DatabaseID ) {}

	// Not a sequence, so not needed
	virtual void prepareSQL( MySql& ) {}
	virtual ISequenceBuffer* createSequenceBuffer() const
	{ 	return NULL; }

	virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
	{	return visitor.onVisitColumn( *this );	}
	virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
	{	return true;	}

	// IMySqlColumnMapping overrides
	virtual const std::string& getColumnName() const
	{	return TIMESTAMP_COLUMN_NAME_STR; }
	virtual void getColumnType( Type& type ) const
	{	type.fieldType = MYSQL_TYPE_TIMESTAMP;
		type.defaultValue = "CURRENT_TIMESTAMP";
		type.onUpdateCmd = "CURRENT_TIMESTAMP";
	}

	virtual IndexType getColumnIndexType() const { return IndexTypeNone; }

	virtual bool hasBinding() const
	{	return false;	}
};

/**
 *	This class maps the STRING type to the database.
 */
class StringMapping : public StringLikeMapping
{
public:
	StringMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ) :
		StringLikeMapping( namer, propName, isNameIndex, length )
	{
		if (pDefaultValue)
		{
			defaultValue_ = pDefaultValue->as<std::string>();

			if (defaultValue_.length() > value_.capacity())
			{
				defaultValue_.resize( value_.capacity() );
				WARNING_MSG( "StringMapping::StringMapping: Default value for "
						"property %s has been truncated to '%s'\n",
						propName.c_str(), defaultValue_.c_str() );
			}
		}
	}

	virtual void getColumnType( Type& type ) const
	{
		StringLikeMapping::getColumnType( type );

		// __kyl__ (24/7/2006) Special handling of STRING < 255 characters
		// because this is how we magically pass the size of the name index
		// field. If type is not VARCHAR then index size is assumed to be
		// 255 (see createEntityTableIndex()).
		if (value_.capacity() < 256)
		{
			type.fieldType = MYSQL_TYPE_VAR_STRING;
			type.length = value_.capacity();
		}

		type.setIsBinary( false );
	}
};

/**
 *	This class maps the BLOB type to the database.
 */
class BlobMapping : public StringLikeMapping
{
public:
	BlobMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ) :
		StringLikeMapping( namer, propName, isNameIndex, length )
	{
		if (pDefaultValue)
		{
			BlobMapping::decodeSection( defaultValue_, pDefaultValue );

			if (defaultValue_.length() > value_.capacity())
			{
				defaultValue_.resize( value_.capacity() );
				WARNING_MSG( "BlobMapping::BlobMapping: Default value for "
						"property %s has been truncated\n",
						propName.c_str() );
			}
		}
	}

	virtual void getColumnType( Type& type ) const
	{
		StringLikeMapping::getColumnType( type );

		type.setIsBinary( true );
	}

	// This method gets the section data as a base64 encoded string
	// and decodes it, placing the result in output.
	static void decodeSection( std::string& output, DataSectionPtr pSection )
	{
		output = pSection->as<std::string>();
		int len = output.length();
		if (len <= 256)
		{
			// Optimised for small strings.
			char decoded[256];
			int length = Base64::decode( output, decoded, 256 );
			output.assign(decoded, length);
		}
		else
		{
			char * decoded = new char[ len ];
			int length = Base64::decode( output, decoded, len );
			output.assign(decoded, length);
			delete [] decoded;
		}
	}
};

/**
 *	This class maps the PYTHON type to the database.
 */
class PythonMapping : public StringLikeMapping
{
public:
	PythonMapping( const Namer& namer, const std::string& propName,
			bool isNameIndex, int length, DataSectionPtr pDefaultValue ) :
		StringLikeMapping( namer, propName, isNameIndex, length )
	{
		if (pDefaultValue)
			defaultValue_ = pDefaultValue->as<std::string>();

		if (defaultValue_.length() == 0)
		{
			defaultValue_ = PythonMapping::getPickler().pickle( Py_None );
		}
		else if (PythonDataType_IsExpression( defaultValue_ ))
		{
			PythonMapping::pickleExpression( defaultValue_, defaultValue_ );
		}
		else
		{
			BlobMapping::decodeSection( defaultValue_, pDefaultValue );
		}

		if (defaultValue_.length() > value_.capacity())
		{
			WARNING_MSG( "PythonMapping::PythonMapping: Default value for "
					"property %s is too big to fit inside column. Defaulting"
					"to None\n", propName.c_str() );
			defaultValue_ = PythonMapping::getPickler().pickle( Py_None );
			if (defaultValue_.length() > value_.capacity())
			{
				CRITICAL_MSG( "PythonMapping::PythonMapping: Even None cannot"
						"fit inside column. Please increase DatabaseSize of"
						"property %s\n", propName.c_str() );
			}
		}

	}

	virtual void getColumnType( Type& type ) const
	{
		StringLikeMapping::getColumnType( type );

		type.setIsBinary( true );
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		// An empty string is not a valid PYTHON stream
		if (!value_.isNull() && value_.size() > 0)
		{
			strm << value_.getString();
		}
		else
		{
			strm << defaultValue_;
		}
	}

private:
	// This method evaluates expr as a Python expression, pickles the
	// resulting object and stores it in output.
	static void pickleExpression( std::string& output, const std::string& expr )
	{
		PyObjectPtr pResult( Script::runString( expr.c_str(), false ),
				PyObjectPtr::STEAL_REFERENCE );

		PyObject* 	pToBePickled = pResult ? pResult.getObject() : Py_None;

		output = PythonMapping::getPickler().pickle( pToBePickled );
	}

	static Pickler & getPickler()
	{
		static Pickler pickler;

		return pickler;
	}
};

/**
 *	Maps a UniqueID into MySQL. This is a base class to properties that
 * 	stores a UniqueID into the database istead of the actual object data.
 */
class UniqueIDMapping : public PropertyMapping, public IMySqlColumnMapping
{
public:
	UniqueIDMapping( const Namer& namer, const std::string& propName,
			DataSectionPtr pDefaultValue ) :
		PropertyMapping( propName ),
		colName_( namer.buildColumnName( "sm", propName ) ),
		value_( sizeof(uint32) * 4 )
	{
		if (pDefaultValue)
			defaultValue_ = UniqueID( pDefaultValue->asString() );
	}

	// IMySqlColumnMapping overrides
	virtual const std::string& getColumnName() const
	{
		return colName_;
	}
	virtual void getColumnType( Type& type ) const
	{
		type.fieldType = MYSQL_TYPE_STRING;
		type.setIsBinary( true );
		type.length = value_.capacity();

		MySqlBuffer defaultValue( value_.capacity() );
		UniqueIDMapping::setBuffer( defaultValue, defaultValue_ );
		type.defaultValue = defaultValue.getString();
	}
	virtual IndexType getColumnIndexType() const
	{
		return IndexTypeNone;
	}
	virtual bool hasBinding() const
	{
		return true;
	}
	virtual void addSelfToBindings( MySqlBindings& bindings )
	{
		bindings << value_;
	}

	static void setBuffer( MySqlBuffer& buf, const UniqueID& uniqueID )
	{
		uint32 id[4];

		id[0] = uniqueID.getA();
		id[1] = uniqueID.getB();
		id[2] = uniqueID.getC();
		id[3] = uniqueID.getD();

		buf.set( id, sizeof(id) );
	}

	void setValue( const UniqueID& uniqueID )
	{
		UniqueIDMapping::setBuffer( value_, uniqueID );
	}

	UniqueID getValue() const
	{
		const uint32* id = reinterpret_cast<const uint32*>( value_.get() );
		if (id)
			return UniqueID( id[0], id[1], id[2], id[3] );
		else
			return  defaultValue_;
	}

	virtual void streamToBound( BinaryIStream & strm )
	{
		UniqueID uniqueId;
		strm >> uniqueId;

		this->setValue( uniqueId );
	}

	virtual void boundToStream( BinaryOStream & strm ) const
	{
		strm << this->getValue();
	}

	virtual void defaultToStream( BinaryOStream & strm ) const
	{
		strm << defaultValue_;
	}

	virtual void defaultToBound()
	{
		this->setValue( defaultValue_ );
	}

	virtual void prepareSQL( MySql& ) {}
	virtual bool hasTable() const	{ return false; }
	virtual void updateTable( MySqlTransaction& transaction,
			DatabaseID parentID ) {}
	virtual void getTableData( MySqlTransaction& transaction,
			DatabaseID parentID ) {}
	virtual void deleteChildren( MySqlTransaction&, DatabaseID parentID ) {}

	virtual ISequenceBuffer* createSequenceBuffer() const
	{
		return new PrimitiveSequenceBuffer< UniqueID, UniqueIDMapping >();
	}

	virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor )
	{
		return visitor.onVisitColumn( *this );
	}
	virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor )
	{
		return true;
	}

private:
	std::string	colName_;
	UniqueID 	defaultValue_;
	MySqlBuffer value_;
};

/**
 *	This class maps a UDO_REF property into the database
 */
class UDORefMapping : public UniqueIDMapping
{
public:
	UDORefMapping( const Namer& namer, const std::string& propName,
			DataSectionPtr pDefaultValue ) :
		UniqueIDMapping( namer, propName, getGuidSection( pDefaultValue ) )
	{}

private:
	static DataSectionPtr getGuidSection( DataSectionPtr pParentSection )
	{
		return (pParentSection) ? pParentSection->openSection( "guid" ) : NULL;
	}
};

}

namespace
{
	/**
	 *  this class allows scripts to specify how a datasection should be bound to sql tables
	 *  it then allows createUserTypeMapping to pull out a PropertyMappingPtr to apply this
	 */
	class PyUserTypeBinder : public PyObjectPlus
	{
		Py_Header( PyUserTypeBinder, PyObjectPlus )

	public:
		PyUserTypeBinder( const Namer& namer, const std::string& propName,
			DataSectionPtr pDefaultValue, PyTypePlus * pType = &s_type_ ) :
			PyObjectPlus( pType )
		{
			// Don't add extra naming level if user prop is unnamed
			// i.e. inside a sequence.
			if (propName.empty())
				tables_.push( Context( new UserTypeMapping( propName ),
									namer, pDefaultValue ) );
			else
				tables_.push( Context( new UserTypeMapping( propName ),
									namer, propName, false,
									pDefaultValue ) );
		}

		// start building a child table
		void beginTable( const std::string& name );
		PY_AUTO_METHOD_DECLARE( RETVOID, beginTable, ARG( std::string, END ) );
		// finish building that table (works like a stack)
		bool endTable();
		PY_AUTO_METHOD_DECLARE( RETOK, endTable, END );
		// bind a column into the current table, of some *BigWorld* type,
		// and in a DataSection under propName
		bool bind( const std::string& propName, const std::string& typeName,
				int databaseLength );
		PY_AUTO_METHOD_DECLARE( RETOK, bind,
				ARG( std::string, ARG( std::string, OPTARG( int, 255, END ) ) ) );

		// this method lets createUserTypeMapping figure out its return value
		PropertyMappingPtr getResult();

		PyObject * pyGetAttribute( const char * attr )
		{
			PY_GETATTR_STD();
			return PyObjectPlus::pyGetAttribute( attr );
		}

	private:
		struct Context
		{
			CompositePropertyMappingPtr	pCompositeProp;
			Namer						namer;
			DataSectionPtr				pDefaultValue;

			Context( CompositePropertyMappingPtr pProp,
					const Namer& inNamer,
					const std::string& propName, bool isTable,
					DataSectionPtr pDefault ) :
				pCompositeProp(pProp), namer( inNamer, propName, isTable ),
				pDefaultValue(pDefault)
			{}

			Context( CompositePropertyMappingPtr pProp,
					const Namer& inNamer,
					DataSectionPtr pDefault ) :
				pCompositeProp(pProp), namer( inNamer ),
				pDefaultValue(pDefault)
			{}

		};
		std::stack<Context> tables_;

		const Context& curContext()
		{
			MF_ASSERT( tables_.size() );
			return tables_.top();
		}
	};

	// forward decl
	PropertyMappingPtr createPropertyMapping( const Namer& namer,
			const std::string& propName, const DataType& type,
			int databaseLength, DataSectionPtr pDefaultValue,
			bool isNameIndex = false );

	bool PyUserTypeBinder::bind( const std::string& propName,
			const std::string& typeName, int databaseLength )
	{
		const Context& context = curContext();
		// see what the default value for this element is then
		// this should logically be done by CompositePropertyMapping,
		// but its addChild method wants a constructed ProperyMapping
		// (the default value for a subtable is always the empty sequence)
		DataSectionPtr pPropDefault;
		if (context.pDefaultValue)
			pPropDefault = context.pDefaultValue->openSection( propName );

		// Create type object, before we can create property mapping.
		std::stringstream typeStrm;
		typeStrm << "<Type>" << typeName << "</Type>";
		XMLSectionPtr pXMLTypeSection =
			XMLSection::createFromStream( std::string(),  typeStrm );
		DataSectionPtr pTypeSection( pXMLTypeSection.getObject() );
		DataTypePtr pType = DataType::buildDataType( pTypeSection );
		if (pType.exists())
		{
			// add it to the table on the 'top' of the stack
			context.pCompositeProp->addChild(
				createPropertyMapping( context.namer, propName, *pType,
										databaseLength, pPropDefault ) );
		}
		else
		{
			ERROR_MSG( "PyUserTypeBinder::bind: Invalid type name %s.\n",
						typeName.c_str() );
			PyErr_SetString( PyExc_TypeError, typeName.c_str() );
		}
		return pType.exists();
	}

	void PyUserTypeBinder::beginTable( const std::string& propName )
	{
		const Context& context = curContext();
		DataSectionPtr pPropDefault;
		if (context.pDefaultValue)
			pPropDefault = context.pDefaultValue->openSection( propName );
		CompositePropertyMappingPtr pChild( new CompositePropertyMapping( propName ) );
		PropertyMappingPtr pSequence( new SequenceMapping( context.namer,
			propName, pChild ) );
        context.pCompositeProp->addChild( pSequence );
		tables_.push( Context( pChild, context.namer, propName, true,
							   pPropDefault ) );
	}

	bool PyUserTypeBinder::endTable()
	{
		bool isOK = ( tables_.size() > 1 );
		if ( isOK )
			tables_.pop();
		else
			PyErr_SetString( PyExc_RuntimeError, "No matching beginTable." );
		return isOK;
	}

	PropertyMappingPtr PyUserTypeBinder::getResult()
	{
		if ( tables_.size() == 1 )
			return PropertyMappingPtr( curContext().pCompositeProp.getObject() );
		else
			return PropertyMappingPtr();
	}

	typedef SmartPointer<PyUserTypeBinder> PyUserTypeBinderPtr;

}

PY_TYPEOBJECT( PyUserTypeBinder )

PY_BEGIN_METHODS( PyUserTypeBinder )
	PY_METHOD( beginTable )
	PY_METHOD( endTable )
	PY_METHOD( bind )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PyUserTypeBinder )
PY_END_ATTRIBUTES()

namespace
{
	/**
	 *	This method creates a UserTypeMapping for a USER_TYPE property.
	 */
	PropertyMappingPtr createUserTypeMapping( const Namer& namer,
			const std::string& propName, const UserDataType& type,
			DataSectionPtr pDefaultValue )
	{
		const std::string& moduleName = type.moduleName();
		const std::string& instanceName = type.instanceName();

		// get the module
		PyObjectPtr pModule(
				PyImport_ImportModule(
					const_cast< char * >( moduleName.c_str() ) ),
				PyObjectPtr::STEAL_REFERENCE );
		if (!pModule)
		{
			ERROR_MSG( "createUserTypeMapping: unable to import %s from %s\n",
					instanceName.c_str(), moduleName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}

		// get the object
		PyObjectPtr pObject(
				PyObject_GetAttrString( pModule.getObject(),
					const_cast<char*>( instanceName.c_str() ) ) );
		if (!pObject)
		{
			ERROR_MSG( "createUserTypeMapping: unable to import %s from %s\n",
					instanceName.c_str(), moduleName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}
		else
		{
			Py_DECREF( pObject.getObject() );
		}

		// create our binder class
		PyUserTypeBinderPtr pBinder(
			new PyUserTypeBinder( namer, propName, pDefaultValue )
								  , true );

		// call the method
		PyObjectPtr pResult(
				PyObject_CallMethod( pObject.getObject(), "bindSectionToDB",
					"O", pBinder.getObject() ) );

		if (!pResult)
		{
			ERROR_MSG( "createUserTypeMapping: (%s.%s) bindSectionToDB failed\n",
					moduleName.c_str(), instanceName.c_str() );
			PyErr_Print();
			return PropertyMappingPtr();
		}

		// pull out the result
		PropertyMappingPtr pTypeMapping = pBinder->getResult();
		if (!pTypeMapping.exists())
		{
			ERROR_MSG( "createUserTypeMapping: (%s.%s) bindSectionToDB missing "
						"endTable\n", moduleName.c_str(), instanceName.c_str() );
		}

		return pTypeMapping;
	}

	/**
	 *	This method creates a ClassMapping for a CLASS or FIXED_DICT property.
	 */
	template <class DATATYPE>
	ClassMapping* createClassTypeMapping( const Namer& classNamer,
			const std::string& propName, const DATATYPE& type,
			int databaseLength, DataSectionPtr pDefaultValue )
	{
		ClassMapping* pClassMapping =
			new ClassMapping( classNamer, propName, type.allowNone() );

		Namer childNamer( classNamer, propName, false );
		// Don't add extra level of naming if we are inside a sequence.
		const Namer& namer = (propName.empty()) ? classNamer : childNamer;

		const ClassDataType::Fields& fields = type.getFields();

		for ( ClassDataType::Fields::const_iterator i = fields.begin();
				i < fields.end(); ++i )
		{
			if (i->isPersistent_)
			{
				DataSectionPtr pPropDefault = getChildDefaultSection(
						*(i->type_), i->name_, pDefaultValue );
				PropertyMappingPtr pMemMapping =
					createPropertyMapping( namer, i->name_, *(i->type_),
											i->dbLen_, pPropDefault );
				if (pMemMapping.exists())
					pClassMapping->addChild( pMemMapping );
			}
		}

		return pClassMapping;
	}

	/**
	 *	This method creates the correct PropertyMapping-derived class for a
	 * 	property.
	 */
	PropertyMappingPtr createPropertyMapping( const Namer& namer,
			const std::string& propName, const DataType& type,
			int databaseLength, DataSectionPtr pDefaultValue,
			bool isNameIndex )
	{
		PropertyMappingPtr pResult;

		const SequenceDataType* pSeqType;
		const ClassDataType* pClassType;
		const UserDataType* pUserType;
		const FixedDictDataType* pFixedDictType;

		if ((pSeqType = dynamic_cast<const SequenceDataType*>(&type)))
		{
			// TODO: Is it possible to specify the default for an ARRAY or TUPLE
			// to have more than one element:
			//		<Default>
			//			<item> 1 </item>
			//			<item> 2 </item>
			//			<item> 3 </item>
			//		</Default>
			// Currently, when adding a new ARRAY/TUPLE to an entity, all
			// existing entities in the database will default to having no
			// elements. When creating a new entity, it will default to the
			// specified default.
			// TODO: For ARRAY/TUPLE of FIXED_DICT, there is an additional
			// case where a new property is added to the FIXED_DICT. Then
			// all existing elements in the database will need a default value
			// for the new property. Currently we use the default value for the
			// child type (as opposed to array type) so we don't have to handle
			// complicated cases where default value for the array doesn't
			// have the same number of elements as the existing arrays in the
			// database.

			// Use the type default value for the child. This is
			// mainly useful when adding new properties to an ARRAY of
			// FIXED_DICT. The new column will have the specified default value.
			const DataType& childType = pSeqType->getElemType();
			DataSectionPtr 	pChildDefault = childType.pDefaultSection();

			PropertyMappingPtr childMapping =
				createPropertyMapping( Namer( namer, propName, true ),
										std::string(), childType,
										databaseLength, pChildDefault );

			if (childMapping.exists())
				pResult = new SequenceMapping( namer, propName, childMapping,
												pSeqType->getSize() );
		}
		else if ((pFixedDictType = dynamic_cast<const FixedDictDataType*>(&type)))
		{
			pResult = createClassTypeMapping( namer, propName,
							*pFixedDictType, databaseLength, pDefaultValue );
		}
		else if ((pClassType = dynamic_cast<const ClassDataType*>(&type)))
		{
			pResult = createClassTypeMapping( namer, propName, *pClassType,
							    		databaseLength, pDefaultValue );
		}
		else if ((pUserType = dynamic_cast<const UserDataType*>(&type)))
		{
			pResult = createUserTypeMapping( namer, propName, *pUserType,
											pDefaultValue );
			if (!pResult.exists())
				// Treat same as parse error i.e. stop DbMgr. This is to prevent
				// altering tables (particularly dropping columns) due to
				// a simple scripting error.
				throw std::runtime_error( "Unable to bind USER_TYPE to database" );
		}
		else
		{
			const MetaDataType * pMetaType = type.pMetaDataType();
			MF_ASSERT(pMetaType);
			const char * metaName = pMetaType->name();
			if (strcmp( metaName, "UINT8" ) == 0)
				pResult = new NumMapping< uint8 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "UINT16" ) == 0)
				pResult = new NumMapping< uint16 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "UINT32" ) == 0)
				pResult = new NumMapping< uint32 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "UINT64" ) == 0)
				pResult = new NumMapping< uint64 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "INT8" ) == 0)
				pResult = new NumMapping< int8 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "INT16" ) == 0)
				pResult = new NumMapping< int16 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "INT32" ) == 0)
				pResult = new NumMapping< int32 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "INT64" ) == 0)
				pResult = new NumMapping< int64 >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "FLOAT32" ) == 0)
				pResult = new NumMapping< float >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "FLOAT64" ) == 0)
				pResult = new NumMapping< double >( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "VECTOR2" ) == 0)
				pResult = new VectorMapping<Vector2,2>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "VECTOR3" ) == 0)
				pResult = new VectorMapping<Vector3,3>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "VECTOR4" ) == 0)
				pResult = new VectorMapping<Vector4,4>( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "STRING" ) == 0)
				pResult = new StringMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue );
			else if (strcmp( metaName, "PYTHON" ) == 0)
				pResult = new PythonMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue );
			else if (strcmp( metaName, "BLOB" ) == 0)
				pResult = new BlobMapping( namer, propName, isNameIndex, databaseLength, pDefaultValue );
			else if (strcmp( metaName, "PATROL_PATH" ) == 0)
				pResult = new UniqueIDMapping( namer, propName, pDefaultValue );
			else if (strcmp( metaName, "UDO_REF" ) == 0)
				pResult = new UDORefMapping( namer, propName, pDefaultValue );
		}

		if (!pResult.exists())
			WARNING_MSG( "createPropertyMapping: don't know how to "
					"persist property %s of type %s to the database; ignoring.\n",
					propName.c_str(), type.typeName().c_str() );

		return pResult;
	}

	/**
	 *	Upgrades the database from 1.9 pre-release to 1.9
	 */
	void upgradeDatabase1_9NonNull( MySql& db )
	{
		// Don't print out something to confuse customers. 99% will go directly
		// from 1.8 to 1.9
		// INFO_MSG( "Upgrading database tables from 1.9 pre-release to 1.9\n" );

		// Drop version column from space data related tables.
		INFO_MSG( "Dropping column 'version' from tables bigworldSpaces and "
				"bigworldSpaceData\n" );
		db.execute( "ALTER TABLE bigworldSpaces DROP COLUMN version" );
		db.execute( "ALTER TABLE bigworldSpaceData DROP COLUMN version" );

		// Convert some tables to InnoDB
		INFO_MSG( "Converting tables bigworldSpaces, bigworldSpaceData and "
				"bigworldGameTime tables to use InnoDB engine\n" );
		db.execute( "ALTER TABLE bigworldSpaces ENGINE=InnoDB" );
		db.execute( "ALTER TABLE bigworldSpaceData ENGINE=InnoDB" );
		db.execute( "ALTER TABLE bigworldGameTime ENGINE=InnoDB" );

		// Adding index to id column of bigworldSpaceData
		INFO_MSG( "Adding index to id column of bigworldSpaceData\n" );
		db.execute( "ALTER TABLE bigworldSpaceData ADD INDEX (id)" );

		// Update version number
		INFO_MSG( "\tUpdating version number\n" );
		std::stringstream	ss;
		ss << "UPDATE bigworldInfo SET version=" << DBMGR_CURRENT_VERSION;
		db.execute( ss.str() );
	}

	/**
	 *	Upgrades the database from 1.9 pre-pre-release to pre-release-1.9
	 */
	void upgradeDatabase1_9Snapshot( MySql& db, const EntityDefs& entityDefs )
	{
		// Don't print out something to confuse customers. 99% will go directly
		// from 1.8 to 1.9
		// INFO_MSG( "Upgrading database tables from 1.9 pre-release to 1.9\n" );
		MySqlTransaction transaction(db);

		// Get table meta data from entity definition.
		TypeMappings propMappings;
		createEntityPropMappings( propMappings, entityDefs, TABLE_NAME_PREFIX );

		SimpleTableCollector entityTableCollector;
		for ( EntityTypeID e = 0; e < entityDefs.getNumEntityTypes(); ++e )
		{
			if (!entityDefs.isValidEntityType( e ))
				continue;

			PropertyMappings& properties = propMappings[e];
			const EntityDescription& entDes = entityDefs.getEntityDescription(e);

			MySqlEntityMapping entityMapping( entDes, properties );

			// Collect tables for this entity type.
			visitSubTablesRecursively( entityMapping, entityTableCollector );
		}
		const SimpleTableCollector::NewTableDataMap& entityTables =
				entityTableCollector.getTables();

		// Get list of entity tables from database
		StrSet	tableNames;
		TableMetaData::getEntityTables( tableNames, db );

		// Add "NOT NULL" specification to all entity data columns.
		for (StrSet::const_iterator pTblName = tableNames.begin();
				pTblName != tableNames.end(); ++pTblName )
		{
			INFO_MSG( "Adding \"NOT NULL\" specification to columns in "
					"table %s\n", pTblName->c_str() );

			// Get column meta data from database.
			TableMetaData::NameToColInfoMap columns;
			getTableColumns( columns, db, *pTblName );

			// Get column meta data from entity definition
			SimpleTableCollector::NewTableDataMap::const_iterator itColumnsDef =
					entityTables.find( *pTblName );
			const TableMetaData::NameToColInfoMap* pColumnsDef =
					(itColumnsDef != entityTables.end()) ?
							&(itColumnsDef->second) : NULL;
			if (!pColumnsDef)
			{
				WARNING_MSG( "upgradeDatabase1_9Snapshot: Cannot find matching "
						"entity definition for table %s. Default values for "
						"columns won't be set correctly\n", pTblName->c_str() );
			}

			std::stringstream	ss;
			ss << "ALTER TABLE " << *pTblName;
			for ( TableMetaData::NameToColInfoMap::const_iterator pColInfo =
					columns.begin(); pColInfo != columns.end(); ++pColInfo )
			{
				// Modifying id column will fail, so skip it.
				if (pColInfo->first == ID_COLUMN_NAME_STR)
					continue;

				// Firstly, set all existing NULL values to the default value.
				// If we don't do this, they will default to 0 or empty string.
				// But can only do this if we have default value from
				// entity definition.
				const TableMetaData::ColumnInfo* pDefColInfo = NULL;
				if (pColumnsDef)
				{
					TableMetaData::NameToColInfoMap::const_iterator itDefColInfo
							= pColumnsDef->find( pColInfo->first );
					if (itDefColInfo != pColumnsDef->end())
						pDefColInfo = &itDefColInfo->second;
				}

				if (pDefColInfo)
				{
					std::string	defaultValue =
						pDefColInfo->columnType.getDefaultValueAsString( db );
					if (!defaultValue.empty())
					{
						std::stringstream	setNull;
						setNull << "UPDATE " << *pTblName << " SET "
							<< pColInfo->first << '=' << defaultValue
							<< " WHERE " << pColInfo->first << " IS NULL";
						// Set NULL values to default value
						// DEBUG_MSG( "%s\n", setNull.str().c_str() );
						transaction.execute( setNull.str() );
					}
				}
				// Don't issue warning if the entire table definition is not
				// found.
				else if (pColumnsDef)
				{
					WARNING_MSG( "upgradeDatabase1_9Snapshot: Cannot find "
							"default value for column %s.%s. Existing NULL "
							"values will be set to default value of MySQL "
							"type (not BigWorld type)\n",
							pTblName->c_str(), pColInfo->first.c_str() );
				}

				if (pColInfo != columns.begin())
					ss << ',';
				ss << " MODIFY COLUMN " << pColInfo->first << ' ';
				// Use entity definition if possible.
				if (pDefColInfo)
				{
					ss << pDefColInfo->columnType.getAsString( db,
							pDefColInfo->indexType );
				}
				else
				{
					ss << pColInfo->second.columnType.getAsString( db,
							pColInfo->second.indexType );
				}
			}

			// Finally, update table definition with "NON-NULL" and
			// default value.
			// DEBUG_MSG( "%s\n", ss.str().c_str() );
			transaction.execute( ss.str() );
		}

		// Remove bigworldTableMetadata table.
		INFO_MSG( "\tRemoving bigworldTableMetadata table\n" );
		transaction.execute( "DROP TABLE bigworldTableMetadata" );

		// Update version number stored in database.
		// Now done in upgradeDatabase1_9NonNull()
//		INFO_MSG( "\tUpdating version number\n" );
//		std::stringstream	ss;
//		ss << "UPDATE bigworldInfo SET version=" << DBMGR_CURRENT_VERSION;
//		transaction.execute( ss.str() );

		transaction.commit();
	}

	/**
	 *	Upgrades the database from 1.8 to 1.9 pre-release
	 */
	void upgradeDatabase1_8( MySql& db )
	{
		MySqlTransaction transaction(db);

		INFO_MSG( "Upgrading database tables from 1.8 to 1.9\n" );

		// Update version number stored in database.
		// Now done in upgradeDatabase1_9Snapshot()
//		INFO_MSG( "\tUpdating version number\n" );
//		std::stringstream	ss;
//		ss << "UPDATE bigworldInfo SET version=" << DBMGR_CURRENT_VERSION;
//		transaction.execute( ss.str() );

		// We've added a column to bigworldInfo
		INFO_MSG( "\tAdding snapshotTime column to bigworldInfo\n" );
		transaction.execute( "ALTER TABLE bigworldInfo ADD COLUMN "
				"(snapshotTime TIMESTAMP NULL)" );

		transaction.commit();
	}

	/**
	 *	Upgrades the database from 1.7 to 1.8
	 */
	void upgradeDatabase1_7( MySql& db, const EntityDefs& entityDefs )
	{
		const std::string& defaultNameProperty =
				entityDefs.getDefaultNameProperty();
		if (defaultNameProperty.empty())
			throw std::runtime_error( "Upgrade failed because "
				"dbMgr/nameProperty is not set. dbMgr/nameProperty must be set "
				"to the name property that was used to create this database." );

		INFO_MSG( "Upgrading database tables from 1.7 to 1.8\n" );

		MySqlTransaction transaction(db);

		// Update version number stored in database.
		// Now done in upgradeDatabase1_8()
//		INFO_MSG( "\tUpdating version number\n" );
//		std::stringstream	ss;
//		ss << "UPDATE bigworldInfo SET version=" << DBMGR_CURRENT_VERSION;
//		transaction.execute( ss.str() );

		// Add the idx column to bigworldTableMetadata
		INFO_MSG( "\tAdding idx column to bigworldTableMetadata\n" );
		transaction.execute( "ALTER TABLE bigworldTableMetadata ADD COLUMN idx "
				"INT NOT NULL" );
		// Set the index column correctly.
		transaction.execute( "UPDATE bigworldTableMetadata SET idx=1 WHERE "
				"col='id'" );
		transaction.execute( "UPDATE bigworldTableMetadata SET idx=3 WHERE "
				"col='parentID'" );
		// The name column is a bit more tricky because sub-tables may have
		// a column with the same name as the default name property. Only
		// top level tables have an index on the name property though.
		if ( entityDefs.getNumEntityTypes() > 0 )
		{
			std::stringstream stmtStrm;
			stmtStrm << " UPDATE bigworldTableMetadata SET idx=2 WHERE "
					"col='sm_" << defaultNameProperty << "' AND FIELD(tbl";

			for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes();
					++typeID )
			{
				const EntityDescription& entDes =
					entityDefs.getEntityDescription( typeID );
				stmtStrm << ", '"TABLE_NAME_PREFIX"_" << entDes.name() << "'";
			}
			stmtStrm << ") > 0";

			transaction.execute( stmtStrm.str() );
		}

		// Previously the name index was always called "nameIndex". In
		// 1.8 the name of the index is made up of the column name + "Index".
		// This is to ease changing of the name property because we could
		// temporarily have two columns being the name column as we add the
		// new name column before deleting the old name column. Also looking
		// to the future where we may support multiple name indexes.
		INFO_MSG( "\tRecreating indexes using new index names\n" );
		for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes();
				++typeID )
		{
			const EntityDescription& entDes =
					entityDefs.getEntityDescription( typeID );
			const DataDescription* pDataDes =
					entDes.findProperty( defaultNameProperty );
			if (pDataDes != NULL)
			{
				std::string tblName( TABLE_NAME_PREFIX"_" );
				tblName += entDes.name();

				DEBUG_MSG( "Recreating index for table %s\n", tblName.c_str() );

				try
				{
					transaction.execute( "ALTER TABLE " + tblName +
										" DROP INDEX nameIndex" );
				}
				catch (std::exception& e )
				{
					ERROR_MSG( "upgradeDatabase: %s\n", e.what() );
				}
				TableMetaData::ColumnInfo colInfo;
				// createEntityTableIndex() needs to know whether it is a
				// VARCHAR or not. Index column must be some sort of string
				// so we can use the databaseLength to fudge this.
				colInfo.columnType.fieldType =
						(pDataDes->databaseLength() < 1<<16) ?
								MYSQL_TYPE_VAR_STRING : MYSQL_TYPE_BLOB;
				colInfo.indexType = IMySqlColumnMapping::IndexTypeName;
				createEntityTableIndex( transaction, tblName,
						"sm_" + defaultNameProperty, colInfo );
			}
		}

		transaction.commit();
	}

	/**
	 *	Upgrades the database from a previous version
	 */
	void upgradeDatabase( MySql& db, uint32 version,
			const EntityDefs& entityDefs )
	{
		if (version == DBMGR_VERSION_1_7)
		{
			upgradeDatabase1_7( db, entityDefs );
			version = DBMGR_VERSION_1_8;
		}

		if (version == DBMGR_VERSION_1_8)
		{
			upgradeDatabase1_8( db );
			version = DBMGR_VERSION_1_9_SNAPSHOT;
		}

		if (version == DBMGR_VERSION_1_9_SNAPSHOT)
		{
			upgradeDatabase1_9Snapshot( db, entityDefs );
			version = DBMGR_VERSION_1_9_NON_NULL;
		}

		MF_ASSERT( version = DBMGR_VERSION_1_9_NON_NULL );
		upgradeDatabase1_9NonNull( db );
	}

}

// -----------------------------------------------------------------------------
// Section: helper functions
// -----------------------------------------------------------------------------

/**
 *	Visits all entity tables with visitor and collects list of current entity
 * 	types.
 */
bool visitPropertyMappings( const EntityDefs& entityDefs,
		TypeMappings& propertyMappings, TableInspector& visitor )
{
	TypesCollector typesCollector( visitor.connection() );

	for ( EntityTypeID ent = 0; ent < entityDefs.getNumEntityTypes(); ++ent )
	{
		// Skip over "invalid" entity types e.g. client-only entities.
		if (!entityDefs.isValidEntityType( ent ))
			continue;

		PropertyMappings& properties = propertyMappings[ent];
		const EntityDescription& entDes = entityDefs.getEntityDescription(ent);

		// Create/check tables for this entity type
		MySqlEntityMapping entityMapping( entDes, properties );
		visitSubTablesRecursively( entityMapping, visitor );

		typesCollector.addType( ent, entDes.name() );

		if (properties.size() == 0)
			INFO_MSG( "%s does not have persistent properties.\n",
					  entDes.name().c_str() );
	}

	if (visitor.deleteUnvisitedTables())
	{
		typesCollector.deleteUnwantedTypes();
	}

	return visitor.isSynced();
}

/**
 *	Creates, if necessary, all the entity tables i.e. those tables that store
 * 	entity data.
 */
bool initEntityTables( MySql& con, const EntityDefs& entityDefs,
						uint32 version, bool shouldSyncTablesToDefs )
{
	// Create the PropertyMappings for each entity type.
	TypeMappings	types;
	createEntityPropMappings( types, entityDefs, TABLE_NAME_PREFIX );

	if (version != DBMGR_CURRENT_VERSION)
		upgradeDatabase( con, version, entityDefs );

	INFO_MSG( "\tsyncTablesToDefs = %s\n",
				(shouldSyncTablesToDefs) ? "True" : "False" );

	bool isSynced;
	if (shouldSyncTablesToDefs)
	{
		// Create/Update the tables based on the TypeMappings
		TableInitialiser tableInitialiser( con );
		isSynced = visitPropertyMappings( entityDefs, types, tableInitialiser );
	}
	else
	{
		// Check that tables matches entity definitions
		TableValidator tableValidator( con );
		isSynced = visitPropertyMappings( entityDefs, types, tableValidator );
	}

	return isSynced;
}

std::string buildCommaSeparatedQuestionMarks( int num )
{
	if (num <= 0)
		return std::string();

	std::string list;
	list.reserve( (num * 2) - 1 );
	list += '?';

	for ( int i=1; i<num; i++ )
	{
		list += ",?";
	}
	return list;
}

namespace
{
	std::string createInsertStatement( const std::string& tbl,
			const PropertyMappings& properties )
	{
		std::string stmt = "INSERT INTO " + tbl + " (";
		CommaSepColNamesBuilder colNames( properties );
		stmt += colNames.getResult();
		int numColumns = colNames.getCount();
		stmt += ") VALUES (";

		stmt += buildCommaSeparatedQuestionMarks( numColumns );
		stmt += ')';

		return stmt;
	}

	std::string createUpdateStatement( const std::string& tbl,
			const PropertyMappings& properties )
	{
		std::string stmt = "UPDATE " + tbl + " SET ";
		CommaSepColNamesBuilderWithSuffix colNames( properties, "=?" );
		stmt += colNames.getResult();
		if (colNames.getCount() == 0)
			return std::string();
		stmt += " WHERE id=?";

		return stmt;
	}

	std::string createSelectStatement( const std::string& tbl,
			const PropertyMappings& properties, const std::string& where, bool getID )
	{
		std::string stmt = "SELECT ";
		if (getID) stmt += "id,";
		CommaSepColNamesBuilder colNames( properties );
		stmt += colNames.getResult();
		if (getID && (colNames.getCount() == 0))
			stmt.resize(stmt.length() - 1);	// remove comma
		stmt += " FROM " + tbl;

		if (where.length())
			stmt += " WHERE " + where;

		return stmt;
	}

	std::string createDeleteStatement( const std::string& tbl,
			const std::string& where )
	{
		std::string stmt = "DELETE FROM " + tbl;

		if (where.length())
			stmt += " WHERE " + where;

		return stmt;
	}
}

// -----------------------------------------------------------------------------
// Section: class MySqlEntityMapping
// -----------------------------------------------------------------------------
/**
 * 	Constructor.
 */
MySqlEntityMapping::MySqlEntityMapping( const EntityDescription& entityDesc,
		PropertyMappings& properties, const std::string& tableNamePrefix ) :
	entityDesc_( entityDesc ),
	tableName_( tableNamePrefix + "_" + entityDesc.name() ),
	properties_( properties )
{}

/**
 * 	Gets the type ID of the entity type associated with this entity mapping.
 */
EntityTypeID MySqlEntityMapping::getTypeID() const
{
	return entityDesc_.index();
}

/**
 * 	IMySqlTableMapping override. Visit all our columns, except the ID column.
 */
bool MySqlEntityMapping::visitColumnsWith( IMySqlColumnMapping::IVisitor& visitor )
{
	for ( PropertyMappings::iterator iter = properties_.begin();
			iter != properties_.end(); ++iter )
	{
		if (!(*iter)->visitParentColumns( visitor ))
			return false;
	}

	return true;
}

/**
 * 	IMySqlTableMapping override. Visit our ID column.
 */
bool MySqlEntityMapping::visitIDColumnWith(	IMySqlIDColumnMapping::IVisitor& visitor )
{
	MySqlIDColumnMappingAdapter idColumn( boundDbID_ );
	return visitor.onVisitIDColumn( idColumn );
}

/**
 * 	IMySqlTableMapping override. Visit all our sub-tables.
 */
bool MySqlEntityMapping::visitSubTablesWith( IMySqlTableMapping::IVisitor& visitor )
{
	for ( PropertyMappings::iterator iter = properties_.begin();
			iter != properties_.end(); ++iter )
	{
		if (!(*iter)->visitTables( visitor ))
			return false;
	}

	return true;
}


// -----------------------------------------------------------------------------
// Section: class MySqlEntityTypeMapping
// -----------------------------------------------------------------------------

MySqlEntityTypeMapping::MySqlEntityTypeMapping( MySql& con,
		const std::string& tableNamePrefix, const EntityDescription& desc,
		PropertyMappings& properties, const std::string& nameProperty ) :
	MySqlEntityMapping( desc, properties, tableNamePrefix ),
	insertStmt_( con, createInsertStatement( MySqlEntityMapping::getTableName(),
											properties ) ),
	pUpdateStmt_( NULL ),
	pSelectNamedStmt_( NULL ),
	pSelectNamedForIDStmt_( NULL ),
	pSelectIDForNameStmt_( NULL ),
	selectIDForIDStmt_( con, "SELECT id FROM " +
						MySqlEntityMapping::getTableName() + " WHERE id=?" ),
	pSelectIDStmt_( NULL ),
	deleteIDStmt_( con, createDeleteStatement(
						MySqlEntityMapping::getTableName(), "id=?" ) ),
	propsNameMap_(),
	pNameProp_(0)
{
	MySqlBindings b;

	const std::string&	tableName = this->getTableName();
	if (properties.size() > 0)
	{
		for ( PropertyMappings::iterator prop = properties.begin();
			  prop != properties.end(); ++prop )
		{
			(*prop)->prepareSQL( con );
		}

		ColumnsBindingsBuilder propertyBindings( properties );

		// Create prop name to PropertyMapping map
		for ( PropertyMappings::const_iterator i = properties.begin();
			i != properties.end(); ++i )
		{
			PropertyMappingPtr pMapping = *i;
			propsNameMap_[pMapping->propName()] = pMapping.getObject();
		}

		// Cache fixed properties so we don't have to always go look for them.
		fixedCellProps_[ CellPositionIdx ] = getPropMapByName( "position" );
		fixedCellProps_[ CellDirectionIdx ] = getPropMapByName( "direction" );
		fixedCellProps_[ CellSpaceIDIdx ] = getPropMapByName( "spaceID" );

		fixedMetaProps_[ GameTimeIdx ] =
			getPropMapByName( GAME_TIME_COLUMN_NAME );
		fixedMetaProps_[ TimestampIdx ] =
			getPropMapByName( TIMESTAMP_COLUMN_NAME );

		// Cache name property (if we have one)
		if (!nameProperty.empty())
		{
			PropertyMapping* pNameProp = getPropMapByName( nameProperty );
			if (pNameProp)
				pNameProp_ = static_cast< StringLikeMapping* >( pNameProp );
		}

		std::string updateStmtStr = createUpdateStatement( tableName, properties );
		if (!updateStmtStr.empty())
		{
			pUpdateStmt_.reset( new MySqlStatement( con, updateStmtStr ) );
			pSelectIDStmt_.reset( new MySqlStatement( con,
					createSelectStatement( tableName, properties, "id=?", false ) ) );
		}
		// ELSE table has no columns apart from id. e.g. entity with only
		// ARRAY properties.

		b.clear();
		b << propertyBindings.getBindings();
		insertStmt_.bindParams( b );

		if (pSelectIDStmt_.get())
			pSelectIDStmt_->bindResult( b );

		if (pUpdateStmt_.get())
		{
			b << this->getDBIDBuf();
			pUpdateStmt_->bindParams( b );
		}

		b.clear();
		b << this->getDBIDBuf();

		if (pSelectIDStmt_.get())
			pSelectIDStmt_->bindParams( b );

		if (pNameProp_)
		{
			pSelectNamedStmt_.reset( new MySqlStatement( con,
				createSelectStatement( tableName, properties, "sm_"+nameProperty+"=?", true ) ) );

			pSelectNamedForIDStmt_.reset( new MySqlStatement( con,
				"SELECT id FROM " + tableName + " WHERE sm_" + nameProperty + "=?" ) );

			pSelectIDForNameStmt_.reset( new MySqlStatement( con,
				"SELECT sm_" + nameProperty + " FROM " + tableName + " WHERE id=?" ) );

			b.clear();
			pNameProp_->addSelfToBindings(b);

			pSelectNamedStmt_->bindParams( b );
			pSelectNamedForIDStmt_->bindParams( b );
			pSelectIDForNameStmt_->bindResult( b );

			b.clear();
			b << this->getDBIDBuf();

			pSelectIDForNameStmt_->bindParams( b );
			pSelectNamedForIDStmt_->bindResult( b );

			b << propertyBindings.getBindings();
			pSelectNamedStmt_->bindResult( b );
		}
	}
	else
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
			fixedCellProps_[i] = 0;
		for ( int i = 0; i < NumFixedMetaProps; ++i )
			fixedMetaProps_[i] = 0;
	}

	b.clear();
	b << this->getDBIDBuf();
	selectIDForIDStmt_.bindParams( b );
	selectIDForIDStmt_.bindResult( b );

	deleteIDStmt_.bindParams( b );

	std::stringstream strmStmt;
	strmStmt << "SELECT typeID FROM bigworldEntityTypes WHERE bigworldID=";
	strmStmt << this->getTypeID();
	MySqlStatement stmtGetID( con, strmStmt.str() );
	b.clear();
	b << mappedType_;
	stmtGetID.bindResult( b );
	MySqlTransaction t( con );
	t.execute( stmtGetID );
	stmtGetID.fetch();
	t.commit();
}

/**
 *	This method checks that the entity with the given DBID exists in the
 *	database.
 *
 *	@return	True if it exists.
 */
bool MySqlEntityTypeMapping::checkExists( MySqlTransaction& transaction,
	DatabaseID dbID )
{
	this->setDBID( dbID );
	transaction.execute( selectIDForIDStmt_ );

	return selectIDForIDStmt_.resultRows() > 0;
}

/**
 * This method returns the database ID of the entity given its name.
 *
 * @param	transaction	Transaction to use when querying the database.
 * @param	name		The name of the entity.
 * @return	The database ID of the entity. Returns 0 if the entity
 * doesn't exists or if the entity doesn't have a name index.
 */
DatabaseID MySqlEntityTypeMapping::getDbID( MySqlTransaction& transaction,
	const std::string& name )
{
	if (pNameProp_)
	{
		pNameProp_->setString( name );
		transaction.execute( *pSelectNamedForIDStmt_ );

		if (pSelectNamedForIDStmt_->resultRows())
		{
			pSelectNamedForIDStmt_->fetch();
			return this->getDBID();
		}
	}

	return 0;
}

/**
 * This method returns the name of the entity given its database Id.
 *
 * @param	transaction	Transaction to use when querying the database.
 * @param	dbID		The database ID of the entity.
 * @param	name		Returns the name of the entity here.
 * @return	True if the entity exists and have a name.
 */
bool MySqlEntityTypeMapping::getName( MySqlTransaction& transaction,
	DatabaseID dbID, std::string& name )
{
	if (pNameProp_)
	{	// Entity has name
		this->setDBID( dbID );
		transaction.execute( *pSelectIDForNameStmt_ );

		if (pSelectIDForNameStmt_->resultRows())
		{
			pSelectIDForNameStmt_->fetch();
			pNameProp_->getString(name);
			return true;
		}
	}

	return false;
}

/**
 *	This method retrieves the entity data into MySQL bindings by database ID.
 *
 *	@param	transaction	Transaction to use when updating the database.
 *	@param	dbID		The database ID of the entity.
 *	@param	name		Output parameter. Name of the entity.
 *	@return	True if the entity exists.
 */
bool MySqlEntityTypeMapping::getPropsByID( MySqlTransaction& transaction,
										   DatabaseID dbID, std::string& name )
{
	bool isOK = true;
	if (pSelectIDStmt_.get())
	{
		this->setDBID( dbID );
		isOK = this->getPropsImpl( transaction, *pSelectIDStmt_ );
		if (isOK && pNameProp_)
			pNameProp_->getString(name);
	}

	return isOK;
}

/**
 *	This method retrieves the entity data into MySQL bindings by name.
 *
 *	@param	transaction	Transaction to use when updating the database.
 *	@param	name		Name of the entity.
 *	@return	The database ID of the entity if successful. Returns 0 if the
 *	entity doesn't exist or doesn't have a name index.
 */
DatabaseID MySqlEntityTypeMapping::getPropsByName( MySqlTransaction& transaction,
	const std::string& name )
{
	if (pNameProp_)
	{
		pNameProp_->setString( name );
		if (this->getPropsImpl( transaction, *pSelectNamedStmt_.get() ))
			return this->getDBID();
	}

	return 0;
}

/**
 *	Common implementation for getPropsByID() and getPropsByName().
 */
bool MySqlEntityTypeMapping::getPropsImpl( MySqlTransaction& transaction,
	MySqlStatement& stmt )
{
	transaction.execute( stmt );

	bool hasData = stmt.resultRows();
	if (hasData)
	{
		stmt.fetch();

		// Get child tables data
		PropertyMappings& properties = this->getPropertyMappings();
		for ( PropertyMappings::iterator i = properties.begin();
			i != properties.end(); ++i )
		{
			(*i)->getTableData( transaction, this->getDBID() );
		}
	}

	return hasData;
}

bool MySqlEntityTypeMapping::deleteWithID( MySqlTransaction & t, DatabaseID id )
{
	this->setDBID( id );
	t.execute( deleteIDStmt_ );
	if (t.affectedRows() > 0)
	{
		MF_ASSERT( t.affectedRows() == 1 );
		// Delete any child table entries
		PropertyMappings& properties = this->getPropertyMappings();
		for ( PropertyMappings::iterator i = properties.begin();
				i != properties.end(); ++i )
		{
			(*i)->deleteChildren( t, id );
		}
		return true;
	}

	return false;
	// TODO: Check that deleting the highest id is ok in whatever kind
	// of tables we are using ... the MySQL docs are not very clear on this
	// issue and we really don't want to reuse DatabaseIDs.
}


/**
 *	This visitor class is used by MySqlEntityTypeMapping::streamToBound()
 *	to read entity data from a stream.
 */
class MySqlBindStreamReader : public IDataDescriptionVisitor
{
	MySqlEntityTypeMapping& entityTypeMap_;
	BinaryIStream & stream_;

public:
	MySqlBindStreamReader( MySqlEntityTypeMapping& entityTypeMap,
		   BinaryIStream & stream ) :
		entityTypeMap_( entityTypeMap ),
		stream_( stream )
	{}

	// Override method from IDataDescriptionVisitor
	virtual bool visit( const DataDescription& propDesc );
};


bool MySqlBindStreamReader::visit( const DataDescription& propDesc )
{
	// __kyl__ TODO: Get rid of name lookup - use entity extras?
	PropertyMapping* pPropMap =
		entityTypeMap_.getPropMapByName(propDesc.name());

	// TRACE_MSG( "TypeMapStreamVisitor::readProp: property=%s\n", propDesc.name().c_str() );

	if (pPropMap)
	{
		pPropMap->streamToBound( stream_ );
	}
	else
	{
		// This is probably because the property is non-persistent.
		// Read from stream and discard
		WARNING_MSG( "MySqlBindStreamReader::visit: Ignoring value for "
					"property %s\n", propDesc.name().c_str() );
		propDesc.createFromStream( stream_, false );
	}

	return true;
}


/**
 *	This visitor class is used by MySqlEntityTypeMapping::streamToBound()
 *	to write entity data to a stream.
 */
class MySqlBindStreamWriter : public IDataDescriptionVisitor
{
	MySqlEntityTypeMapping& entityTypeMap_;
	BinaryOStream & stream_;

public:
	MySqlBindStreamWriter( MySqlEntityTypeMapping& entityTypeMap,
		   BinaryOStream & stream ) :
		entityTypeMap_( entityTypeMap ),
		stream_( stream )
	{}

	// Override method from IDataDescriptionVisitor
	virtual bool visit( const DataDescription& propDesc );
};

bool MySqlBindStreamWriter::visit( const DataDescription& propDesc )
{
	PropertyMapping* pPropMap =
		entityTypeMap_.getPropMapByName( propDesc.name() );
	// TRACE_MSG( "TypeMapStreamWriter::visit: property=%s\n", propDesc.name().c_str() );
	if (pPropMap)
	{
		pPropMap->boundToStream( stream_ );
	}
	else
	{
		// This is probably because the property is non-persistent.
		// Write default value into stream.
		WARNING_MSG( "MySqlBindStreamWriter::writeProp: Making up default "
					"value for property %s\n", propDesc.name().c_str() );
		PyObjectPtr pDefaultVal = propDesc.pInitialValue();
		propDesc.addToStream( pDefaultVal.getObject(), stream_, false );
	}

	return true;
}

/**
 *	This method streams off entity data and meta data into MySQL bindings.
 */
void MySqlEntityTypeMapping::streamToBound( BinaryIStream& strm )
{
	this->streamEntityPropsToBound( strm );
	this->streamMetaPropsToBound( strm );
}

/**
 * 	This method streams off entity data into the MySQL bindings.
 */
void MySqlEntityTypeMapping::streamEntityPropsToBound( BinaryIStream& strm )
{
	MySqlBindStreamReader		visitor( *this, strm );
	const EntityDescription&	desc = this->getEntityDescription();
	desc.visit( EntityDescription::BASE_DATA | EntityDescription::CELL_DATA |
			EntityDescription::ONLY_PERSISTENT_DATA, visitor );

	// Set data bindings for non-configurable cell properties.
	if (desc.hasCellScript())
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
		{
			fixedCellProps_[i]->streamToBound( strm );
		}
	}
}

/**
 * 	This method streams off entity meta data into the MySQL bindings.
 */
void MySqlEntityTypeMapping::streamMetaPropsToBound( BinaryIStream& strm )
{
	for ( int i = 0; i < NumFixedMetaProps; ++i )
	{
		fixedMetaProps_[i]->streamToBound( strm );
	}
}

/**
 *	This method transfers the data already in MySQL bindings into the stream.
 *	Entity data must be already set in bindings e.g. via getPropsByID() or
 *	getPropsByName().
 *
 *	If pPasswordOverride is not NULL and if the entity has as STRING or BLOB
 *	property called "password", pPasswordOverride will be written to the
 *	stream instead of the value of the "password" property.
 */
void MySqlEntityTypeMapping::boundToStream( BinaryOStream& strm,
	const std::string* pPasswordOverride  )
{
	if (pPasswordOverride)
	{
		// Set bound value of "password" property. But only if it is a STRING
		// or BLOB property.
		PropertyMapping* pPropMap = this->getPropMapByName( "password" );
		if (dynamic_cast<StringMapping*>(pPropMap) ||
			dynamic_cast<BlobMapping*>(pPropMap) )
		{
			dynamic_cast<StringLikeMapping*>(pPropMap)->setValue(
					*pPasswordOverride );
		}
	}

	MySqlBindStreamWriter 		visitor( *this, strm );
	const EntityDescription&	desc = this->getEntityDescription();
	desc.visit( EntityDescription::CELL_DATA | EntityDescription::BASE_DATA |
			EntityDescription::ONLY_PERSISTENT_DATA, visitor );

	// Write non-configurable cell properties into stream.
	if (desc.hasCellScript())
	{
		for ( int i = 0; i < NumFixedCellProps; ++i )
		{
			fixedCellProps_[i]->boundToStream( strm );
		}
	}

	// __kyl__ (21/8/2008) Disabling streaming on of meta properties since
	// no one is using them at the moment. This does mean that streamToBound()
	// and boundToStream() are not completely symmetrical.
//	for ( int i = 0; i < NumFixedMetaProps; ++i )
//	{
//		fixedMetaProps_[i]->boundToStream( strm );
//	}
}

/**
 * This method insert a new entity into the database.
 * Entity data must be already set in bindings e.g. via streamToBound().
 *
 * @param	transaction	Transaction to use when updating the database.
 * @return	The database ID of the newly inserted entity.
 */
DatabaseID MySqlEntityTypeMapping::insertNew( MySqlTransaction& transaction )
{
	transaction.execute( insertStmt_ );
	DatabaseID dbID = transaction.insertID();

	// Update child tables.
	PropertyMappings& properties = this->getPropertyMappings();
	for ( PropertyMappings::iterator i = properties.begin();
		i != properties.end(); ++i )
	{
		(*i)->updateTable( transaction, dbID );
	}

	return dbID;
}

/**
 * This method updates an existing entity's properties in the database.
 * Entity data must be already set in bindings e.g. via streamToBound() and
 * setDBID()
 *
 * @param	transaction	Transaction to use when updating the database.
 * @return	Returns true if the entity was updated. False if the entity
 * doesn't exist.
 */
bool MySqlEntityTypeMapping::update( MySqlTransaction& transaction )
{
	// Update main table for this entity.
	bool isOK = true;
	if (pUpdateStmt_.get())
	{
		transaction.execute( *pUpdateStmt_ );

		// __kyl__ (24/5/2005) Can't actually use transaction.affectedRows()
		// because if the new entity data is the same as the old entity data,
		// then transaction.affectedRows() will return 0.
		const char* infoStr = transaction.info();
		// infoStr should be "Rows matched: %d Changed: %d Warnings: %d"
		if ( (infoStr) && (atol( infoStr + 14 ) == 1) )
		{
			// Update child tables.
			PropertyMappings& properties = this->getPropertyMappings();
			for ( PropertyMappings::iterator i = properties.begin();
				i != properties.end(); ++i )
			{
				(*i)->updateTable( transaction, this->getDBID() );
			}
		}
		else
		{
			isOK = false;
		}
	}

	return isOK;
}

// -----------------------------------------------------------------------------
// Section: MySqlTypeMapping
// -----------------------------------------------------------------------------

MySqlTypeMapping::MySqlTypeMapping( MySql& con, const EntityDefs& entityDefs,
		const char * tableNamePrefix ) :
	mappings_(),
	stmtAddLogOn_( con, "INSERT INTO bigworldLogOns (databaseID, typeID, objectID, ip, port, salt) "
						"VALUES (?,?,?,?,?,?) ON DUPLICATE KEY UPDATE "
						"objectID=VALUES(objectID), ip=VALUES(ip), port=VALUES(port), salt=VALUES(salt)" ),
	stmtRemoveLogOn_( con, "DELETE FROM bigworldLogOns WHERE databaseID=? AND typeID=?" ),
	stmtGetLogOn_( con, "SELECT objectID, ip, port, salt FROM bigworldLogOns "
				"WHERE databaseID=? and typeID=?" ),
	stmtSetLogOnMapping_( con, "REPLACE INTO bigworldLogOnMapping (logOnName, password, typeID, recordName) "
				"VALUES (?,?,?,?)" ),
	stmtGetLogOnMapping_( con, "SELECT m.password, t.bigworldID, m.recordName "
				"FROM bigworldLogOnMapping m, bigworldEntityTypes t "
				"WHERE m.logOnName=? and m.typeID=t.typeID" ),
	boundLogOnName_(BWMySQLMaxLogOnNameLen),
	boundPassword_(BWMySQLMaxLogOnPasswordLen),
	boundRecordName_(BWMySQLMaxNamePropertyLen)
{
	createEntityMappings( mappings_, entityDefs, tableNamePrefix, con );

	MySqlBindings b;

	b.clear();
	b << boundDatabaseID_ << boundTypeID_;
	stmtRemoveLogOn_.bindParams( b );
	stmtGetLogOn_.bindParams( b );

	b << boundOptEntityID_ << boundAddress_ << boundPort_ << boundSalt_;
	stmtAddLogOn_.bindParams( b );

	b.clear();
	b << boundOptEntityID_ << boundAddress_ << boundPort_ << boundSalt_;
	stmtGetLogOn_.bindResult( b );

	b.clear();
	this->addLogonMappingBindings( b );
	stmtSetLogOnMapping_.bindParams( b );

	b.clear();
	b << boundPassword_ << boundTypeID_ << boundRecordName_;
	stmtGetLogOnMapping_.bindResult( b );
	b.clear();
	b << boundLogOnName_;
	stmtGetLogOnMapping_.bindParams( b );
}

MySqlTypeMapping::~MySqlTypeMapping()
{
	this->clearMappings();
}

/**
 *	This method replaces the clears current entity mappings.
 */
void MySqlTypeMapping::clearMappings()
{
	for ( MySqlEntityTypeMappings::iterator i = mappings_.begin();
			i < mappings_.end(); ++i )
	{
		delete *i;
	}
	mappings_.clear();
}

bool MySqlTypeMapping::hasNameProp( EntityTypeID typeID ) const
{
	return mappings_[typeID]->hasNameProp();
}

DatabaseID MySqlTypeMapping::getEntityDbID( MySqlTransaction& transaction,
	EntityTypeID typeID, const std::string& name )
{
	return mappings_[typeID]->getDbID( transaction, name );
}

bool MySqlTypeMapping::getEntityName( MySqlTransaction& transaction,
	EntityTypeID typeID, DatabaseID dbID, std::string& name )
{
	return mappings_[typeID]->getName( transaction, dbID, name );
}

bool MySqlTypeMapping::checkEntityExists( MySqlTransaction& transaction,
	EntityTypeID typeID, DatabaseID dbID )
{
	return mappings_[typeID]->checkExists( transaction, dbID );
}

bool MySqlTypeMapping::getEntityToBound( MySqlTransaction& transaction,
	EntityDBKey& ekey )
{
	if (ekey.dbID)
	{
		return mappings_[ekey.typeID]->getPropsByID( transaction,
			ekey.dbID, ekey.name );
	}
	else
	{
		ekey.dbID = mappings_[ekey.typeID]->getPropsByName( transaction,
			ekey.name );
		return ekey.dbID != 0;
	}
}

void MySqlTypeMapping::boundToStream( EntityTypeID typeID,
	BinaryOStream& entityDataStrm, const std::string* pPasswordOverride )
{
	mappings_[typeID]->boundToStream( entityDataStrm, pPasswordOverride );
}

bool MySqlTypeMapping::deleteEntityWithID(
		MySqlTransaction& t, EntityTypeID typeID, DatabaseID dbID )
{
	return mappings_[typeID]->deleteWithID( t, dbID );
}

/**
 *	This method stores all the data necessary for setLogOnMapping() into
 *	our bindings.
 */
void MySqlTypeMapping::logOnMappingToBound( const std::string& logOnName,
	const std::string& password, EntityTypeID typeID,
	const std::string& recordName )
{
	boundLogOnName_.setString( logOnName );
	boundPassword_.setString( password );
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundRecordName_.setString( recordName );
}

/**
 *	This method adds a log on mapping into our log on mapping table.
 */
void MySqlTypeMapping::setLogOnMapping( MySqlTransaction& transaction  )
{
	transaction.execute( stmtSetLogOnMapping_ );
}

/**
 *	This method gets the log on mapping for the given logOnName.
 */
bool MySqlTypeMapping::getLogOnMapping( MySqlTransaction& t, const std::string& logOnName,
		std::string& password, EntityTypeID& typeID, std::string& recordName )
{
	boundLogOnName_.setString( logOnName );
	t.execute( stmtGetLogOnMapping_ );
	if (stmtGetLogOnMapping_.fetch())
	{
		if (boundPassword_.isNull())
			password.clear();
		else
			password = boundPassword_.getString();
		typeID = boundTypeID_;
		recordName = boundRecordName_.getString();
		return true;
	}
	else
	{
		return false;
	}
}

bool MySqlTypeMapping::getLogOnRecord( MySqlTransaction& t, EntityTypeID typeID,
		DatabaseID dbID, EntityMailBoxRef& ref )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	t.execute( stmtGetLogOn_ );

	if (stmtGetLogOn_.fetch())
	{
		ref.id = *boundOptEntityID_.get();
		ref.addr.ip = htonl( *boundAddress_.get() );
		ref.addr.port = htons( *boundPort_.get() );
		ref.addr.salt = *boundSalt_.get();
		return true;
	}

	return false;
}

/**
 *	This method sets the MySQL bindings for an entity data update operation.
 */
void MySqlTypeMapping::streamToBound( EntityTypeID typeID, DatabaseID dbID,
									  BinaryIStream& entityDataStrm )
{
	MySqlEntityTypeMapping& mapping = *mappings_[typeID];
	mapping.streamToBound( entityDataStrm );
	mapping.setDBID( dbID );
}

/**
 *	This method sets the MySQL bindings for a log on record update.
 */
void MySqlTypeMapping::logOnRecordToBound( EntityTypeID typeID, DatabaseID dbID,
		const EntityMailBoxRef& baseRef )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	this->baseRefToBound( baseRef );
}

/**
 *	This method sets the MySQL bindings for a base mailbox add/update operation.
 */
void MySqlTypeMapping::baseRefToBound( const EntityMailBoxRef& baseRef )
{
	boundOptEntityID_.set( baseRef.id );
	boundAddress_.set( ntohl( baseRef.addr.ip ) );
	boundPort_.set( ntohs( baseRef.addr.port ) );
	boundSalt_.set( baseRef.addr.salt );
}

DatabaseID MySqlTypeMapping::newEntity( MySqlTransaction& transaction,
									    EntityTypeID typeID )
{
	return mappings_[typeID]->insertNew( transaction );
}

bool MySqlTypeMapping::updateEntity( MySqlTransaction& transaction,
									 EntityTypeID typeID )
{
	return mappings_[typeID]->update( transaction );
}

/**
 *	This methods stores new base mailbox for the given entity in the database.
 * 	If a base mailbox for the entity already exists, it is updated.
 *	Base mailbox data must be already set in bindings e.g. via streamToBound().
 *
 *	This method may be called from another thread. Do not use globals.
 */
void MySqlTypeMapping::addLogOnRecord( MySqlTransaction& transaction,
									   EntityTypeID typeID, DatabaseID dbID )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	transaction.execute( stmtAddLogOn_ );
}

/**
 *	This method remove the base mailbox for a given entity from the database.
 *
 *	This method may be called from another thread. Do not use globals.
 */
void MySqlTypeMapping::removeLogOnRecord( MySqlTransaction& t,
		EntityTypeID typeID, DatabaseID dbID )
{
	boundTypeID_ = mappings_[typeID]->getDatabaseTypeID();
	boundDatabaseID_ = dbID;
	t.execute( stmtRemoveLogOn_ );
}


/**
 *	Accessor to bigworldLogOnMapping bindings so that we can re-use them.
 */
void MySqlTypeMapping::addLogonMappingBindings( MySqlBindings& bindings )
{
	bindings << boundLogOnName_ << boundPassword_ << boundTypeID_
			<< boundRecordName_;
}

/**
 *	Accessor to bigworldLogOn bindings so that we can re-use them.
 */
void MySqlTypeMapping::addLogonRecordBindings( MySqlBindings& bindings )
{
	bindings << boundDatabaseID_ << boundTypeID_ << boundOptEntityID_
			<< boundAddress_ << boundPort_ << boundSalt_;
}

// -----------------------------------------------------------------------------
// Section: free functions
// -----------------------------------------------------------------------------
void createEntityPropMappings( TypeMappings& types,
							const EntityDefs& entityDefs,
							const std::string& tableNamePrefix )
{
	// walk through the properties of each entity type and create a property mapping
	// class instance for it... we'll use these to generate the statements that we'll
	// need later on
	for ( EntityTypeID ent = 0; ent < entityDefs.getNumEntityTypes(); ++ent )
	{
		types.push_back( PropertyMappings() );

		if (!entityDefs.isValidEntityType( ent ))
			// Note that even for invalid entity types we need an blank entry
			// in types since we access by offset into the array.
			continue;

		PropertyMappings& properties = types.back();
		const EntityDescription& entDes = entityDefs.getEntityDescription(ent);

		const std::string& nameProperty = entityDefs.getNameProperty(ent);
		Namer namer( entDes.name(), tableNamePrefix );
		for ( unsigned int i = 0; i < entDes.propertyCount(); ++i )
		{
			DataDescription * dataDes = entDes.property( i );
			if (dataDes->isPersistent())
			{
				DataType * dataType = dataDes->dataType();

				bool isNameProperty = !nameProperty.empty() &&
							(nameProperty == dataDes->name());
				PropertyMappingPtr prop =
					createPropertyMapping( namer, dataDes->name(),
							*dataType, dataDes->databaseLength(),
							getDefaultSection( *dataDes ), isNameProperty );
				if (prop.exists())
					properties.push_back( prop );
			}
		}

		if (entDes.hasCellScript())
		{
			//setup proper default value for position and direction
			Vector3 defaultVec(0,0,0);

			DataSectionPtr pDefaultValue = new XMLSection( "position" );
			pDefaultValue->setVector3(defaultVec);

			properties.push_back(
				new VectorMapping<Vector3,3>( namer, "position", pDefaultValue ) );

			pDefaultValue = new XMLSection( "direction" );
			pDefaultValue->setVector3(defaultVec);
			properties.push_back(
				new VectorMapping<Vector3,3>( namer, "direction", pDefaultValue ) );

			pDefaultValue = new XMLSection( "spaceID" );
			pDefaultValue->setInt( 0 );
			properties.push_back(
				new NumMapping<int32>( namer, "spaceID", pDefaultValue ) );
		}

		DataSectionPtr pDefaultValue = new XMLSection( GAME_TIME_COLUMN_NAME );
		pDefaultValue->setInt( 0 );
		properties.push_back(
			new NumMapping<TimeStamp>( GAME_TIME_COLUMN_NAME, pDefaultValue ) );

		properties.push_back( new TimestampMapping() );
	}
}

/**
 * 	This function creates MySqlEntityTypeMappings from the given
 * 	PropertyMappings.
 */
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	TypeMappings& propMappings, const EntityDefs& entityDefs,
	const std::string& tableNamePrefix, MySql& connection )
{
	for ( EntityTypeID typeID = 0; typeID < entityDefs.getNumEntityTypes(); ++typeID )
	{
		if (entityDefs.isValidEntityType( typeID ))
		{
			const EntityDescription& entDes =
				entityDefs.getEntityDescription( typeID );
			entityMappings.push_back( new MySqlEntityTypeMapping( connection,
									tableNamePrefix,entDes,
									propMappings[typeID],
									entityDefs.getNameProperty( typeID ) ) );
		}
		else
		{
			entityMappings.push_back( NULL );
		}
	}
}

/**
 * 	This function creates MySqlEntityTypeMappings from the given entity
 * 	definitions.
 */
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	const EntityDefs& entityDefs, const std::string& tableNamePrefix,
	MySql& connection )
{
	TypeMappings propMappings;
	createEntityPropMappings( propMappings, entityDefs, tableNamePrefix );

	createEntityMappings( entityMappings, propMappings, entityDefs,
		tableNamePrefix, connection );
}
