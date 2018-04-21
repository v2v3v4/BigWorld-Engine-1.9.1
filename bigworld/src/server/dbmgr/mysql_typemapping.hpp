/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_TYPEMAPPING_HPP
#define MYSQL_TYPEMAPPING_HPP

#include "mysql_wrapper.hpp"
#include "mysql_table.hpp"
#include "idatabase.hpp"

#include "cstdmf/smartpointer.hpp"
#include <vector>
#include <set>

class EntityDescription;
class MySqlEntityTypeMapping;
class StringLikeMapping;
class TableIndices;
typedef std::map<std::string, std::string> StrStrMap;
typedef std::set<std::string> StrSet;
typedef std::vector<MySqlEntityTypeMapping*> MySqlEntityTypeMappings;

// __kyl__ (6/9/2005) Should only update CURRENT_VERSION if there are changes
// in the database that requires upgrading.
// static const uint32	DBMGR_CURRENT_VERSION	= 0;	// Pre-1.7
static const uint32	DBMGR_VERSION_1_7		= 1;		// 1.7
static const uint32	DBMGR_VERSION_1_8		= 2;		// 1.8
static const uint32	DBMGR_VERSION_1_9_SNAPSHOT = 3;		// 1.9 snapshot
static const uint32	DBMGR_VERSION_1_9_NON_NULL = 4;		// 1.9 non-null columns
static const uint32	DBMGR_CURRENT_VERSION	= 5;
static const uint32	DBMGR_OLDEST_SUPPORTED_VERSION	= 1;

bool initEntityTables( MySql& con, const EntityDefs& entityDefs,
		uint32 version, bool shouldSyncTablesToDefs );

namespace TableMetaData
{
	struct ColumnInfo
	{
		IMySqlColumnMapping::Type		columnType;
		IMySqlColumnMapping::IndexType	indexType;

		ColumnInfo( const MYSQL_FIELD& field, const TableIndices& indices );
		ColumnInfo();

		bool operator==( const ColumnInfo& other ) const;
		bool operator!=( const ColumnInfo& other ) const
		{	return !this->operator==( other );	}

	private:
		static IMySqlColumnMapping::IndexType deriveIndexType(
				const MYSQL_FIELD& field, const TableIndices& indices );
	};

	// Map of column name to ColumnInfo.
	typedef std::map< std::string, ColumnInfo > NameToColInfoMap;

	struct UpdatedColumnInfo : public ColumnInfo
	{
		// IMySqlColumnMapping::Type		oldColumnType;
		IMySqlColumnMapping::IndexType	oldIndexType;

		UpdatedColumnInfo( const ColumnInfo& newCol, const ColumnInfo& oldCol )
			:  ColumnInfo( newCol ), // oldColumnType( oldCol.columnType ),
			oldIndexType( oldCol.indexType )
		{}
	};
	// Map of column name to UpdatedColumnInfo.
	typedef std::map< std::string, UpdatedColumnInfo > NameToUpdatedColInfoMap;

	void getEntityTables( StrSet& tables, MySql& connection );
	void getTableColumns( TableMetaData::NameToColInfoMap& columns,
			MySql& connection, const std::string& tableName );
};

/**
 *	This class is used to access tables that stores entity meta data.
 */
class BigWorldMetaData
{
	MySql&				connection_;
	MySqlStatement		stmtGetEntityTypeID_;
	MySqlStatement		stmtSetEntityTypeID_;
	MySqlStatement		stmtAddEntityType_;
	MySqlStatement		stmtRemoveEntityType_;

	MySqlBuffer bufferTypeName_;
	int			bufferInteger_;
public:
	BigWorldMetaData( MySql& connection );

	MySql& connection()		{ return connection_; }

	EntityTypeID getEntityTypeID( const std::string& entityName );
	void setEntityTypeID( const std::string& entityName, EntityTypeID typeID );
	void addEntityType( const std::string& entityName, EntityTypeID typeID );
	void removeEntityType( const std::string& entityName );
};

/**
 *	This class is used to visit all the entity properties so that it can
 * 	collect all the tables and columns needed. It simply accumulates all the
 * 	required tables.
 */
class SimpleTableCollector : public IMySqlTableMapping::IVisitor
{
public:
	typedef std::map< std::string, TableMetaData::NameToColInfoMap >
			NewTableDataMap;
	NewTableDataMap	tables_;

	virtual ~SimpleTableCollector()	{}

	const NewTableDataMap& getTables() const
	{
		return tables_;
	}

	SimpleTableCollector& operator+=( const SimpleTableCollector& rhs );

	// IMySqlTableMapping::IVisitor override.
	virtual bool onVisitTable( IMySqlTableMapping& table );
};

void createEntityTableIndex( MySql& connection,
	const std::string& tblName, const std::string& colName,
	const TableMetaData::ColumnInfo& colInfo );

/**
 *	This is the base class for classes that maps BigWorld types to MySQL tables
 * 	and columsn.
 */
class PropertyMapping : public ReferenceCount
{
public:
	PropertyMapping( const std::string& propName ) :
		propName_(propName)
	{}
	virtual ~PropertyMapping() {}

	// after that is called, and the initialisation is complete,
	// we can safely create SQL statements on those tables (which we need to
	// do for sequences), so we do that here
	virtual void prepareSQL( MySql& ) = 0;

	// which property do we map to in a DataSection
	const std::string& propName() const	{ return propName_; }
	// Set our bound value from the stream. Must be consistent with
	// streaming done in DataType. e.g. DataType::createFromStream().
	virtual void streamToBound( BinaryIStream & strm ) = 0;
	// Put our bound value into the stream. Must be consistent with
	// streaming done in DataType. e.g. DataType::addToStream().
	virtual void boundToStream( BinaryOStream & strm ) const = 0;
	// Puts the default value into the stream.
	virtual void defaultToStream( BinaryOStream & strm ) const = 0;
	// Sets the bindings to the default value.
	virtual void defaultToBound() = 0;

	// This method returns true if the type or any of its children
	// stores data in additional table(s).
	virtual bool hasTable() const = 0;
	// Updates any child tables associated with the property.
	// e.g. sequences have their own tables. Bindings must be set
	// (e.g. via streamToBound()) prior to calling this method
	// otherwise crap data will be written to the database.
	virtual void updateTable( MySqlTransaction& transaction,
		DatabaseID parentID ) = 0;
	// Gets data for any child tables into bindings.
	virtual void getTableData( MySqlTransaction& transaction,
		DatabaseID parentID ) = 0;

	// This method lets the visitor visits all the columns that we're adding
	// to our parent's table.
	virtual bool visitParentColumns( IMySqlColumnMapping::IVisitor& visitor ) = 0;
	// This method lets the visitor visits all the tables that we're adding
	// i.e. all our parent's sub-tables.
	virtual bool visitTables( IMySqlTableMapping::IVisitor& visitor ) = 0;

	// Types that can be an element in a sequence must implement
	// createSequenceBuffer() which returns an ISequenceBuffer specific
	// to that type.
	struct ISequenceBuffer
	{
		virtual ~ISequenceBuffer() {};

		// Deserialise from the stream and store value in buffer.
		// If this is called multiple times without calling reset()
		// values should be accumulated instead of overwritten.
		virtual void streamToBuffer( int numElems,
										BinaryIStream& strm ) = 0;
		// Put the idx-th buffer value into the stream.
		virtual void bufferToStream( BinaryOStream& strm, int idx ) const = 0;
		// Set binding with value of element number "idx" in the buffer.
		virtual void bufferToBound( PropertyMapping& binding,
									int idx ) const = 0;
		// Add binding value into buffer.
		virtual void boundToBuffer( const PropertyMapping& binding ) = 0;
		virtual int	getNumElems() const = 0;
		// Resets buffer to "empty".
		virtual void reset() = 0;
	};
	virtual ISequenceBuffer* createSequenceBuffer() const = 0;

	// and this method performs a cascading delete on any child tables
	virtual void deleteChildren( MySqlTransaction&, DatabaseID parentID ) = 0;

private:
	std::string propName_;
};

typedef SmartPointer<PropertyMapping> PropertyMappingPtr;
typedef std::vector<PropertyMappingPtr> PropertyMappings;
typedef std::vector<PropertyMappings> TypeMappings;

/**
 *	This helper class builds a comma separated list string of column names,
 * 	with a suffix attached to the name.
 */
class CommaSepColNamesBuilder : public IMySqlColumnMapping::IVisitor,
								public IMySqlIDColumnMapping::IVisitor
{
protected:
	std::stringstream	commaSepColumnNames_;
	int 				count_;

	CommaSepColNamesBuilder() : count_( 0 ) {}

public:
	// Constructor for a single PropertyMapping
	CommaSepColNamesBuilder( PropertyMapping& property ) : count_( 0 )
	{
		property.visitParentColumns( *this );
	}

	// Constructor for many PropertyMappings
	CommaSepColNamesBuilder( const PropertyMappings& properties ) : count_( 0 )
	{
		for ( PropertyMappings::const_iterator it = properties.begin();
				it != properties.end(); ++it )
		{
			(*it)->visitParentColumns( *this );
		}
	}

	// Constructor for a IMySqlTableMapping
	CommaSepColNamesBuilder( IMySqlTableMapping& table, bool visitIDCol ) :
		count_( 0 )
	{
		if (visitIDCol)
			table.visitIDColumnWith( *this );
		table.visitColumnsWith( *this );
	}

	std::string getResult() const	{ return commaSepColumnNames_.str(); }
	int 		getCount() const	{ return count_; }

	// IMySqlColumnMapping::IVisitor override
	bool onVisitColumn( IMySqlColumnMapping& column )
	{
		if (column.hasBinding())
		{
			if (count_ > 0)
				commaSepColumnNames_ << ',';
			commaSepColumnNames_ << column.getColumnName();

			++count_;
		}

		return true;
	}

	// IMySqlIDColumnMapping::Visitor override
	bool onVisitIDColumn( IMySqlIDColumnMapping& column )
	{
		return this->onVisitColumn( column );
	}
};

/**
 *	This helper class builds a comma separated list string of column names,
 * 	with a suffix attached to the name.
 */
class CommaSepColNamesBuilderWithSuffix : public CommaSepColNamesBuilder
{
	typedef ColumnVisitorArgPasser< CommaSepColNamesBuilderWithSuffix,
			const std::string > SuffixPasser;

public:
	// Constructor for many PropertyMappings
	CommaSepColNamesBuilderWithSuffix( const PropertyMappings& properties,
			const std::string& suffix = std::string() )
	{
		// Passes suffix to the onVisitColumn() callback
		SuffixPasser proxyVisitor( *this, suffix );
		for ( PropertyMappings::const_iterator it = properties.begin();
				it != properties.end(); ++it )
		{
			(*it)->visitParentColumns( proxyVisitor );
		}
	}

	// Constructor for a single PropertyMapping
	CommaSepColNamesBuilderWithSuffix( PropertyMapping& property,
			const std::string& suffix = std::string() )
	{
		// Passes suffix to the onVisitColumn() callback
		SuffixPasser proxyVisitor( *this, suffix );
		property.visitParentColumns( proxyVisitor );
	}

	// Called by SuffixPasser
	bool onVisitColumn( IMySqlColumnMapping& column,
			const std::string& suffix )
	{
		bool shouldContinue = CommaSepColNamesBuilder::onVisitColumn( column );
		if (column.hasBinding())
		{
			commaSepColumnNames_ << suffix;
		}

		return shouldContinue;
	}
};

/**
 *	This helper class adds all the column bindings from properties into a
 * 	MySqlBindings.
 */
class ColumnsBindingsBuilder : public IMySqlColumnMapping::IVisitor,
								public IMySqlIDColumnMapping::IVisitor
{
	MySqlBindings	bindings_;

public:
	// Constructor for a single PropertyMapping
	ColumnsBindingsBuilder( PropertyMapping& property )
	{
		property.visitParentColumns( *this );
	}

	// Constructor for many PropertyMappings
	ColumnsBindingsBuilder( const PropertyMappings& properties )
	{
		for ( PropertyMappings::const_iterator it = properties.begin();
				it != properties.end(); ++it )
		{
			(*it)->visitParentColumns( *this );
		}
	}

	// Constructor for a IMySqlTableMapping
	ColumnsBindingsBuilder( IMySqlTableMapping& table )
	{
		table.visitIDColumnWith( *this );
		table.visitColumnsWith( *this );
	}

	MySqlBindings& getBindings() { return bindings_; }

	// IMySqlColumnMapping::IVisitor override
	virtual bool onVisitColumn( IMySqlColumnMapping& column )
	{
		if (column.hasBinding())
		{
			column.addSelfToBindings( bindings_ );
		}
		return true;
	}

	// IMySqlIDColumnMapping::Visitor override
	bool onVisitIDColumn( IMySqlIDColumnMapping& column )
	{
		return this->onVisitColumn( column );
	}
};

std::string buildCommaSeparatedQuestionMarks( int num );

void createEntityPropMappings( TypeMappings& types,
							const EntityDefs& entityDefs,
							const std::string& tableNamePrefix );
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	TypeMappings& propMappings, const EntityDefs& entityDefs,
	const std::string& tableNamePrefix, MySql& connection );
void createEntityMappings( MySqlEntityTypeMappings& entityMappings,
	const EntityDefs& entityDefs, const std::string& tableNamePrefix,
	MySql& connection );


/**
 * 	This class contains the bindings for an entity type.
 */
class MySqlEntityMapping : public IMySqlTableMapping
{
	const EntityDescription& 	entityDesc_;
	std::string					tableName_;
	PropertyMappings			properties_;

	DatabaseID					boundDbID_;

public:
	MySqlEntityMapping( const EntityDescription& entityDesc,
			PropertyMappings& properties,
			const std::string& tableNamePrefix = TABLE_NAME_PREFIX );
	virtual ~MySqlEntityMapping() {};

	const EntityDescription& getEntityDescription() const { return entityDesc_; }
	const PropertyMappings& getPropertyMappings() const	{ return properties_; }
	PropertyMappings& getPropertyMappings() { return properties_; }

	DatabaseID 	getDBID() const		{ return boundDbID_; }
	DatabaseID& getDBIDBuf()		{ return boundDbID_; }
	void setDBID( DatabaseID dbID )	{ boundDbID_ = dbID; }

	EntityTypeID getTypeID() const;

	// IMySqlTableMapping overrides
	virtual const std::string& getTableName() const
	{	return tableName_;	}
	virtual bool visitColumnsWith( IMySqlColumnMapping::IVisitor& visitor );
	virtual bool visitIDColumnWith( IMySqlIDColumnMapping::IVisitor& visitor );
	virtual bool visitSubTablesWith( IMySqlTableMapping::IVisitor& visitor );
	virtual IRowBuffer* getRowBuffer() { return NULL; }	// no row buffer.
};


/**
 *	This class implements the typical BigWorld operations on entities - for
 * 	a single entity type.
 */
class MySqlEntityTypeMapping : public MySqlEntityMapping
{
public:

	MySqlEntityTypeMapping( MySql& conneciton,
		const std::string& tableNamePrefix, const EntityDescription& desc,
		PropertyMappings& properties, const std::string& nameProperty );

	// Basic database operations.
	DatabaseID getDbID( MySqlTransaction& transaction,
		const std::string& name );
	bool getName( MySqlTransaction& transaction, DatabaseID dbID,
		std::string& name );
	bool checkExists( MySqlTransaction& transaction, DatabaseID dbID );
	bool deleteWithID( MySqlTransaction& t, DatabaseID id );

	// Inserting an entity
	void streamToBound( BinaryIStream& strm );
	void streamEntityPropsToBound( BinaryIStream& strm );
	void streamMetaPropsToBound( BinaryIStream& strm );
	DatabaseID insertNew( MySqlTransaction& transaction );
	bool update( MySqlTransaction& transaction );

	// Retrieving an entity
	bool getPropsByID( MySqlTransaction& transaction, DatabaseID dbID,
		std::string& name );
	DatabaseID getPropsByName( MySqlTransaction& transaction,
		const std::string& name );
	void boundToStream( BinaryOStream& strm, const std::string* pPasswordOverride );

	// get the index of the entity type as mapped by the database
	int getDatabaseTypeID() const	{ return mappedType_;	}
	// Whether this entity has a name property i.e. dbMgr/nameProperty
	bool hasNameProp() const		{ return pNameProp_ != 0;	}

	PropertyMapping* getPropMapByName( const std::string& name )
	{
		std::map< std::string, PropertyMapping*>::iterator propMapIter =
			propsNameMap_.find(name);
		return ( propMapIter != propsNameMap_.end() )
					? propMapIter->second : 0;
	}

	const PropertyMapping* getPropMapByName( const std::string& name ) const
	{
		return const_cast<MySqlEntityTypeMapping*>(this)->getPropMapByName( name );
	}

private:
	MySqlStatement insertStmt_;
	std::auto_ptr<MySqlStatement> pUpdateStmt_;
	std::auto_ptr<MySqlStatement> pSelectNamedStmt_;
	std::auto_ptr<MySqlStatement> pSelectNamedForIDStmt_;
	std::auto_ptr<MySqlStatement> pSelectIDForNameStmt_;
	MySqlStatement selectIDForIDStmt_;
	std::auto_ptr<MySqlStatement> pSelectIDStmt_;
	MySqlStatement deleteIDStmt_;
	std::map< std::string, PropertyMapping* > propsNameMap_;

	// Non-configurable properties.
	// Enums must be in the order that these properties are stored in the stream.
	enum FixedCellProp
	{
		CellPositionIdx,
		CellDirectionIdx,
		CellSpaceIDIdx,
		NumFixedCellProps
	};
	enum FixedMetaProp
	{
		GameTimeIdx,
		TimestampIdx,
		NumFixedMetaProps
	};
	PropertyMapping* fixedCellProps_[ NumFixedCellProps ];
	PropertyMapping* fixedMetaProps_[ NumFixedMetaProps ];
	StringLikeMapping* pNameProp_;

	// we cache what the EntityTypeID is in the database
	int mappedType_;

	bool getPropsImpl( MySqlTransaction& transaction, MySqlStatement& stmt );
};

class MySqlTypeMapping
{
public:
	MySqlTypeMapping( MySql& con, const EntityDefs& entityDefs,
		const char * tableNamePrefix = TABLE_NAME_PREFIX );
	~MySqlTypeMapping();

	void clearMappings();

	MySqlEntityTypeMappings& getEntityMappings() { return mappings_; }
	MySqlEntityTypeMapping* getEntityMapping( EntityTypeID typeID )
	{ return mappings_[typeID]; }

	bool hasNameProp( EntityTypeID typeID ) const;
	DatabaseID getEntityDbID( MySqlTransaction& transaction,
		EntityTypeID typeID, const std::string& name );
	bool getEntityName( MySqlTransaction& transaction,
		EntityTypeID typeID, DatabaseID dbID, std::string& name );
	bool checkEntityExists( MySqlTransaction& transaction,
		EntityTypeID typeID, DatabaseID dbID );
	bool deleteEntityWithID( MySqlTransaction&, EntityTypeID, DatabaseID );

	// Transfer data into MySQL bindings, ready for DB operation.
	void streamToBound( EntityTypeID typeID, DatabaseID dbID,
		BinaryIStream& entityDataStrm );
	void logOnRecordToBound( EntityTypeID typeID, DatabaseID dbID,
			const EntityMailBoxRef& baseRef );
	void baseRefToBound( const EntityMailBoxRef& baseRef );
	void logOnMappingToBound( const std::string& logOnName,
		const std::string& password, EntityTypeID typeID,
		const std::string& recordName );
	std::string getBoundLogOnName() const {	return boundLogOnName_.getString(); }

	// DB operations that use the entity data already in MySQL bindings.
	DatabaseID newEntity( MySqlTransaction& transaction, EntityTypeID typeID );
	bool updateEntity( MySqlTransaction& transaction, EntityTypeID typeID );
	void addLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID );
	void removeLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID );
	void setLogOnMapping( MySqlTransaction& transaction );

	// DB operations that retrieve entity data from database into MySQL bindings.
	bool getEntityToBound( MySqlTransaction& transaction, EntityDBKey& ekey );

	// Operations that returns the entity data already in MySQL bindings.
	void boundToStream( EntityTypeID typeID, BinaryOStream& entityDataStrm,
		const std::string* pPasswordOverride );

	// Old-style operations that store stuff into bindings, execute DB query
	// and return results in one swell swoop.
	bool getLogOnRecord( MySqlTransaction&, EntityTypeID, DatabaseID,
		EntityMailBoxRef& );
	bool getLogOnMapping( MySqlTransaction&, const std::string& logOnName,
			std::string& password, EntityTypeID& typeID, std::string& recordName );

	// Accessor our bindings so that they can be re-used.
	void addLogonMappingBindings( MySqlBindings& bindings );
	void addLogonRecordBindings( MySqlBindings& bindings );

private:
	MySqlEntityTypeMappings	mappings_;
	MySqlStatement stmtAddLogOn_;
	MySqlStatement stmtRemoveLogOn_;
	MySqlStatement stmtGetLogOn_;

	MySqlStatement stmtSetLogOnMapping_;
	MySqlStatement stmtGetLogOnMapping_;

	int boundTypeID_;
	DatabaseID boundDatabaseID_;
	EntityID boundEntityID_;
	MySqlValueWithNull<EntityID> boundOptEntityID_;
	MySqlValueWithNull<uint32> boundAddress_;
	MySqlValueWithNull<uint16> boundPort_;
	MySqlValueWithNull<uint16> boundSalt_;

	MySqlBuffer boundLogOnName_;
	MySqlBuffer boundPassword_;
	MySqlBuffer boundRecordName_;
};

#endif
