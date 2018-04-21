/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_TABLE_HPP
#define MYSQL_TABLE_HPP

#include "network/basictypes.hpp"

#include "mysql_wrapper.hpp" 

#include <mysql/mysql.h>

// -----------------------------------------------------------------------------
// Section: Forward declarations
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// Section: Useful constants
// -----------------------------------------------------------------------------
// __kyl__ (20/7/2005) For some reason I can't find a #define for these but they
// are well known to MySQL developers.
enum MySQLLimits
{
	MySQLMaxTableNameLen = NAME_LEN,
	MySQLMaxColumnNameLen = NAME_LEN,
	MySQLMaxDbNameLen = NAME_LEN,
	MySQLMaxIndexNameLen = NAME_LEN,
	MySQLMaxInnoDbIndexLen = 767,		// From documentation
	MySQLMaxMyIsamDbIndexLen = 1000		// From documentation
};

enum BWMySQLLimits
{
	BWMySQLMaxTypeNameLen = 64,
	BWMySQLMaxLogOnNameLen = 255,
	BWMySQLMaxLogOnPasswordLen = 255,
	BWMySQLMaxNamePropertyLen = 255
};

#define TABLE_NAME_PREFIX				"tbl"
#define	DEFAULT_SEQUENCE_COLUMN_NAME	"value"
#define DEFAULT_SEQUENCE_TABLE_NAME		"values"
#define ID_COLUMN_NAME					"id"
#define PARENTID_COLUMN_NAME			"parentID"
#define GAME_TIME_COLUMN_NAME			"gameTime"
#define TIMESTAMP_COLUMN_NAME			"timestamp"

const std::string ID_COLUMN_NAME_STR( ID_COLUMN_NAME );
const std::string PARENTID_COLUMN_NAME_STR( PARENTID_COLUMN_NAME );
const std::string TIMESTAMP_COLUMN_NAME_STR( TIMESTAMP_COLUMN_NAME );

// Arbitrary number used to determine our buffer size.
static const unsigned long MAX_SECONDARY_DB_LOCATION_LENGTH = 4096;

/**
 *	This is an interface into a column mapping i.e. something that maps to one
 * 	columns in a MySql table.
 */
struct IMySqlColumnMapping
{
	struct IVisitor
	{
		// NOTE: For efficiency reasons the IMySqlColumnMapping passed to the
		// visitor may be a temporary on the stack i.e. you should not store
		// its address.
		virtual bool onVisitColumn( IMySqlColumnMapping& column )= 0;
	};

	// The following enum values are stored in bigworldTableMetadata.idx.
	// If you change their numerical values, you must update upgradeDatabase().
	enum IndexType
	{
		IndexTypeNone 		= 0,
		IndexTypePrimary	= 1,
		IndexTypeName		= 2,
		IndexTypeParentID	= 3
	};

	struct Type
	{
		enum_field_types	fieldType;
		bool				isUnsignedOrBinary;	// Dual use field
		uint				length;
		std::string			defaultValue;
		std::string			onUpdateCmd;
		bool				isAutoIncrement;

		Type( enum_field_types type = MYSQL_TYPE_NULL, 
				bool isUnsignedOrBin = false, uint len = 0, 
				const std::string defVal = std::string(),
				bool isAutoInc = false ) :
			fieldType( type ), isUnsignedOrBinary( isUnsignedOrBin ),
			length( len ), defaultValue( defVal ), 
			isAutoIncrement( isAutoInc )
		{}
		Type( const MYSQL_FIELD& field ) :
			fieldType( field.type), 
			isUnsignedOrBinary( deriveIsUnsignedOrBinary( field ) ), 
			length( field.length ), 
			defaultValue( field.def ? field.def : std::string() ),
			isAutoIncrement( field.flags & AUTO_INCREMENT_FLAG )
		{
			if (this->fieldType == MYSQL_TYPE_BLOB)
				this->fieldType = MySqlTypeTraits<std::string>::colType( this->length );
		}

		// Only applies to integer fields
		bool isUnsigned() const			{ return isUnsignedOrBinary; }
		void setIsUnsigned( bool val )	{ isUnsignedOrBinary = val; }

		// Only applies to string or blob fields
		bool isBinary()	const			{ return isUnsignedOrBinary; }
		void setIsBinary( bool val ) 	{ isUnsignedOrBinary = val; }

		std::string getAsString( MySql& connection, IndexType idxType ) const;
		std::string getDefaultValueAsString( MySql& connection ) const;
		bool isDefaultValueSupported() const;
		bool isStringType() const;
		bool isSimpleNumericalType() const;

		bool operator==( const Type& other ) const;
		bool operator!=( const Type& other ) const
		{	return !this->operator==( other );	}

		static bool deriveIsUnsignedOrBinary( const MYSQL_FIELD& field );


	private:
		void adjustBlobTypeForSize();
	};

	// Gets info about the column.
	virtual const std::string& 	getColumnName() const = 0;
	virtual void getColumnType( Type& type ) const = 0; 
	virtual IndexType getColumnIndexType() const = 0;
	virtual bool hasBinding() const = 0;

	// Adds the bindings for this column to MySqlBindings
	virtual void addSelfToBindings( MySqlBindings& bindings ) = 0;
};

const IMySqlColumnMapping::Type PARENTID_COLUMN_TYPE( MYSQL_TYPE_LONGLONG );
const IMySqlColumnMapping::Type ID_COLUMN_TYPE( MYSQL_TYPE_LONGLONG, false, 
		0, std::string(), true );	// Auto-increment column

/**
 *	This is a IMySqlColumnMapping for an ID column
 */
struct IMySqlIDColumnMapping : public IMySqlColumnMapping
{
	struct IVisitor
	{
		// NOTE: For efficiency reasons the IMySqlColumnMapping passed to the
		// visitor may be a temporary on the stack i.e. you should not store
		// its address.
		virtual bool onVisitIDColumn( IMySqlIDColumnMapping& column )= 0;
	};

	// Gets the buffer for the id column.
	virtual DatabaseID& getIDBuffer() = 0;
};

/**
 *	This is an interface into a table mapping i.e. something that maps to a
 * 	MySql table.
 */
struct IMySqlTableMapping
{
	struct IVisitor
	{
		virtual bool onVisitTable( IMySqlTableMapping& table ) = 0;
	};

	virtual const std::string& getTableName() const = 0;
	// Visits all columns except the ID column
	virtual bool visitColumnsWith( IMySqlColumnMapping::IVisitor& visitor ) = 0;
	// Visit the ID column
	virtual bool visitIDColumnWith( IMySqlIDColumnMapping::IVisitor& visitor ) = 0;
	virtual bool visitSubTablesWith( IVisitor& visitor ) = 0;

	// Since our column bindings map to only one row in the table, we can
	// optionally have a row buffer for operations that sets/retrieve multiple
	// rows of data. Data must be moved to/from the bindings prior/after an
	// operation using setBoundData()/addBoundData()
	// NOTE: Currently setBoundData()/addBoundData() doesn't include the id
	// or parentID columns. When using setBoundData(), the id and parentID
	// columns will have to be set manually.
	struct IRowBuffer
	{
		virtual void addBoundData() = 0;
		virtual void setBoundData( int row ) = 0;
		virtual int getNumRows() const = 0;
		virtual void clear() = 0;
	};
	virtual IRowBuffer* getRowBuffer() = 0;

	// Determines whether this table has any sub-tables.
	bool hasSubTables();
	// Returns the ID column buffer.
	DatabaseID& getIDColumnBuffer();
};

// For some reason making this a local class of
// IMySqlTableMapping::hasSubTables() causes a GCC internal compiler error.
struct hasSubTables_SubTableVisitor : public IMySqlTableMapping::IVisitor
{
	virtual bool onVisitTable( IMySqlTableMapping& table )
	{
		return false;
	}
};
inline bool IMySqlTableMapping::hasSubTables()
{
	hasSubTables_SubTableVisitor visitor;

	return !this->visitSubTablesWith( visitor );
}

struct getIDColumnBuffer_IDColVisitor : public IMySqlIDColumnMapping::IVisitor
{
	DatabaseID*	pIDColBuf;
	getIDColumnBuffer_IDColVisitor() : pIDColBuf( NULL ) {}
	virtual bool onVisitIDColumn( IMySqlIDColumnMapping& column )
	{
		pIDColBuf = &column.getIDBuffer();
		return false;
	}
};
inline DatabaseID& IMySqlTableMapping::getIDColumnBuffer()
{
	getIDColumnBuffer_IDColVisitor visitor;
	this->visitIDColumnWith( visitor );
	return *visitor.pIDColBuf;
}

/**
 *	This helper function visits the table and all it's sub-tables using the
 * 	provided visitor i.e. visitor.onVisitTable() will be called for
 * 	this table and all it's sub-tables.
 */
// For some reason, making this a local class of visitSubTablesRecursively()
// causes a GCC internal compiler error.
struct visitSubTablesRecursively_Visitor : public IMySqlTableMapping::IVisitor
{
	IMySqlTableMapping::IVisitor& origVisitor_;
public:
	visitSubTablesRecursively_Visitor( IMySqlTableMapping::IVisitor& origVisitor ) :
		origVisitor_( origVisitor )
	{}
	virtual bool onVisitTable( IMySqlTableMapping& table )
	{
		return origVisitor_.onVisitTable( table ) &&
				table.visitSubTablesWith( *this );
	}
};
inline bool visitSubTablesRecursively( IMySqlTableMapping& table,
		IMySqlTableMapping::IVisitor& visitor )
{
	visitSubTablesRecursively_Visitor proxyVisitor( visitor );
	return proxyVisitor.onVisitTable( table );
}

/**
 *	This class is used to pass an addition argument to
 * 	IMySqlTableMapping::IVisitor.
 */
template <class VISITOR, class ARG>
class TableVisitorArgPasser : public IMySqlTableMapping::IVisitor
{
	VISITOR&	origVisitor_;
	ARG&		arg_;
public:
	TableVisitorArgPasser( VISITOR& origVisitor, ARG& arg ) :
		origVisitor_( origVisitor ), arg_( arg )
	{}
	virtual bool onVisitTable( IMySqlTableMapping& table )
	{
		return origVisitor_.onVisitTable( table, arg_ );
	}
};

/**
 *	This class is used to pass an addition argument to
 * 	IMySqlColumnMapping::IVisitor.
 */
template <class VISITOR, class ARG>
class ColumnVisitorArgPasser : public IMySqlColumnMapping::IVisitor
{
	VISITOR&	origVisitor_;
	ARG&		arg_;
public:
	ColumnVisitorArgPasser( VISITOR& origVisitor, ARG& arg ) :
		origVisitor_( origVisitor ), arg_( arg )
	{}
	virtual bool onVisitColumn( IMySqlColumnMapping& column )
	{
		return origVisitor_.onVisitColumn( column, arg_ );
	}
};

/**
 * 	This class provides simple pass-through implementation of the
 * 	IMySqlColumnMapping interface. It is meant to be a temporary stack variable
 * 	that references the "real" column (which may not be derived from
 * 	IMySqlColumnMapping due to historical or other reasons).
 */
template < class BOUNDTYPE >
class MySqlColumnMappingAdapter : public IMySqlColumnMapping
{
	const std::string& 	name_;
	const Type&			columnType_;
	IndexType			indexType_;
	BOUNDTYPE&			bindBuffer_;

public:
	MySqlColumnMappingAdapter( const std::string& name,
			const Type& columnType, IndexType indexType,
			BOUNDTYPE& bindBuffer ) :
		name_( name ), columnType_( columnType ), indexType_( indexType ),
		bindBuffer_( bindBuffer )
	{}

	BOUNDTYPE& getBindBuffer()
	{
		return bindBuffer_;
	}

	// IMySqlColumnMapping overrides
	virtual const std::string& getColumnName() const
	{
		return name_;
	}
	virtual void getColumnType( Type& type ) const
	{
		type = columnType_;
	}
	virtual IndexType getColumnIndexType() const
	{
		return indexType_;
	}
	virtual bool hasBinding() const
	{
		return true;
	}
	virtual void addSelfToBindings( MySqlBindings& bindings )
	{
		bindings << bindBuffer_;
	}
};

/**
 *	Specialisation of MySqlColumnMappingAdapter for an ID column
 */
class MySqlIDColumnMappingAdapter : public MySqlColumnMappingAdapter< DatabaseID >,
									public IMySqlIDColumnMapping
{
	typedef MySqlColumnMappingAdapter< DatabaseID > BaseClass;
public:
	MySqlIDColumnMappingAdapter( DatabaseID& idBinding ) :
		BaseClass( ID_COLUMN_NAME_STR, ID_COLUMN_TYPE,
				IndexTypePrimary, idBinding )
	{}

	// IMySqlColumnMapping overrides
	virtual const std::string& getColumnName() const
	{
		return BaseClass::getColumnName();
	}
	virtual void getColumnType( Type& type ) const
	{
		BaseClass::getColumnType( type ); 
	}
	virtual IndexType getColumnIndexType() const
	{
		return BaseClass::getColumnIndexType();
	}
	virtual bool hasBinding() const
	{
		return true;
	}
	virtual void addSelfToBindings( MySqlBindings& bindings )
	{
		BaseClass::addSelfToBindings( bindings );
	}

	// IMySqlIDColumnMapping overrides
	virtual DatabaseID& getIDBuffer()
	{
		return BaseClass::getBindBuffer();
	}
};

#endif /*MYSQL_TABLE_HPP*/
