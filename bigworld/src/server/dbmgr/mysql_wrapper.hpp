/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_WRAPPER_HPP
#define MYSQL_WRAPPER_HPP

#include "cstdmf/stdmf.hpp"
#include "cstdmf/debug.hpp"

#include <stdexcept>
#include <mysql/mysql.h>
#include <mysql/mysqld_error.h>

// Forward declarations.
class BinaryOStream;
class BinaryIStream;
namespace MySqlUnPrep
{
	class Statement;
	class Bindings;
}
namespace MySqlPrep
{
	class Statement;
	class Bindings;
}
namespace DBConfig
{
	struct Connection;
}

// Constants
#define MYSQL_ENGINE_TYPE "InnoDB"

time_t convertMySqlTimeToEpoch(  const MYSQL_TIME& mysqlTime );

// This traits class is used for mapping C types to MySQL types.
template < class CTYPE > struct MySqlTypeTraits
{
	// static const enum enum_field_types	colType = MYSQL_TYPE_LONG;
};
template <> struct MySqlTypeTraits<int8>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_TINY;
};
template <> struct MySqlTypeTraits<uint8>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_TINY;
};
template <> struct MySqlTypeTraits<int16>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_SHORT;
};
template <> struct MySqlTypeTraits<uint16>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_SHORT;
};
template <> struct MySqlTypeTraits<int32>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_LONG;
};
template <> struct MySqlTypeTraits<uint32>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_LONG;
};
template <> struct MySqlTypeTraits<int64>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_LONGLONG;
};
template <> struct MySqlTypeTraits<uint64>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_LONGLONG;
};
template <> struct MySqlTypeTraits<float>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_FLOAT;
};
template <> struct MySqlTypeTraits<double>
{
	static const enum enum_field_types	colType = MYSQL_TYPE_DOUBLE;
};
// Slightly dodgy specialisation for std::string. Basically it maps to
// different sized BLOBs. colType and colTypeStr are actually functions
// instead of constants.
template <> struct MySqlTypeTraits<std::string>
{
	typedef MySqlTypeTraits<std::string> THIS;

	static const enum enum_field_types colType( int maxColWidth )
	{
		if (maxColWidth < 1<<8)
			return MYSQL_TYPE_TINY_BLOB;
		else if (maxColWidth < 1<<16)
			return MYSQL_TYPE_BLOB;
		else if (maxColWidth < 1<<24)
			return MYSQL_TYPE_MEDIUM_BLOB;
		else
			return MYSQL_TYPE_LONG_BLOB;
	}
	static const std::string TINYBLOB;
	static const std::string BLOB;
	static const std::string MEDIUMBLOB;
	static const std::string LONGBLOB;
	static const std::string colTypeStr( int maxColWidth )
	{
		switch ( THIS::colType( maxColWidth ) )
		{
			case MYSQL_TYPE_TINY_BLOB:
				return TINYBLOB;
			case MYSQL_TYPE_BLOB:
				return BLOB;
			case MYSQL_TYPE_MEDIUM_BLOB:
				return MEDIUMBLOB;
			case MYSQL_TYPE_LONG_BLOB:
				return LONGBLOB;
			default:
				break;
		}
		return NULL;
	}
};

// exception class for this little MySQL wrapper library
class MySqlError : public std::exception
{
public:
	MySqlError( MYSQL * mysql ) : err_( mysql_error(mysql) ) {}
	MySqlError( MYSQL_STMT * stmt ) : err_( mysql_stmt_error(stmt) ) {}
	~MySqlError() throw() {}
	const char * what() const throw() { return err_.c_str(); }

private:
	const std::string err_;
};

class MySqlRetryTransactionException : public MySqlError
{
public:
	MySqlRetryTransactionException( MYSQL * mysql ) : MySqlError( mysql ) {}
	MySqlRetryTransactionException( MYSQL_STMT * stmt ) : MySqlError( stmt ) {}
};

class MySqlDuplicateEntryException : public MySqlError
{
public:
	MySqlDuplicateEntryException( MYSQL * mysql ) : MySqlError( mysql ) {}
	MySqlDuplicateEntryException( MYSQL_STMT * stmt ) : MySqlError( stmt ) {}
};

// Represents a MySql result cursor.
class MySqlResult
{
	MYSQL_RES& 		handle_;
	my_ulonglong	numRows_;
	unsigned int	numFields_;

	MYSQL_ROW 		curRow_;
	unsigned long *	curRowLengths_;

public:
	MySqlResult( MYSQL_RES& handle ) : handle_( handle ),
		numRows_( mysql_num_rows( &handle_ ) ),
		numFields_( mysql_num_fields( &handle_ ) )
	{}

	~MySqlResult()
	{
		mysql_free_result( &handle_ );
	}

	my_ulonglong	getNumRows() const		{ return numRows_; }
	unsigned int	getNumFields() const 	{ return numFields_; }

	// Advance to the next row. Returns false if there is no next row.
	bool getNextRow()
	{
		curRow_ = mysql_fetch_row( &handle_ );
		if (curRow_)
			curRowLengths_ = mysql_fetch_lengths( &handle_ );

		return curRow_ != NULL;
	}

	// Gets the n-th field from the current row.
	const char* getField( unsigned int n )		{ return curRow_[n]; }
	// Gets the length of the n-th field of the current row.
	unsigned long getFieldLen( unsigned int n )	{ return curRowLengths_[n]; }
};

// represents a MySQL server connection
class MySql
{
public:
	MySql( const DBConfig::Connection& connectInfo );
	~MySql();

	MYSQL * get() { return sql_; }

	void execute( const std::string & statement,
		BinaryOStream * pResData = NULL );
	void execute( MySqlUnPrep::Statement& stmt );
	void execute( MySqlPrep::Statement& stmt );
	int query( const std::string & statement );
	MYSQL_RES* storeResult()	{ return mysql_store_result( sql_ ); }
	bool ping()					{ return mysql_ping( sql_ ) == 0; }
	void getTableNames( std::vector<std::string>& tableNames,
						const char * pattern );
	my_ulonglong insertID()		{ return mysql_insert_id( sql_ ); }
	my_ulonglong affectedRows()	{ return mysql_affected_rows( sql_ ); }
	const char* info()			{ return mysql_info( sql_ ); }
	const char* getLastError()	{ return mysql_error( sql_ ); }
	unsigned int getLastErrorNum() { return mysql_errno( sql_ ); }

	struct Lock
	{
		MySql&	sql;

		Lock(MySql& mysql) : sql(mysql)
		{
			MF_ASSERT(!sql.inTransaction_);
			sql.inTransaction_ = true;
		}
		~Lock()
		{
			MF_ASSERT(sql.inTransaction_);
			sql.inTransaction_ = false;
		}
	private:
		Lock(const Lock&);
		Lock& operator=(const Lock&);
	};

	bool hasFatalError() const	{	return !fatalErrorStr_.empty();	}
	const std::string& getFatalErrorStr() const	{	return fatalErrorStr_;	}
	void onFatalError(const std::exception& e)	{ fatalErrorStr_ = e.what(); }

private:
	template <class MYSQLOBJ>
	void throwError( MYSQLOBJ* failedObj );

	MySql( const MySql& );
	void operator=( const MySql& );

	MYSQL * sql_;
	bool inTransaction_;
	std::string fatalErrorStr_;
};

// a single transaction
// will rollback any changes when it leaves scope if commit() isn't
// called
class MySqlTransaction
{
public:
	MySqlTransaction( MySql& sql ) : lock_(sql), committed_(false)
	{
		lock_.sql.execute( "START TRANSACTION" );
	}
	MySqlTransaction( MySql& sql, int& errorNum ) :
		lock_(sql), committed_(false)
	{
		errorNum = this->query( "START TRANSACTION" );
	}
	~MySqlTransaction()
	{
		if ( !committed_ && !lock_.sql.hasFatalError() )
		{
			// Can't let exception escape from destructor otherwise terminate()
			// will be called.
			try
			{
				lock_.sql.execute( "ROLLBACK" );
			}
			catch (std::exception& e)
			{
				lock_.sql.onFatalError( e );
			}
		}
	}

	MySql& get() { return lock_.sql; }

	void execute( MySqlUnPrep::Statement& stmt )
	{
		MF_ASSERT( !committed_ );
		lock_.sql.execute( stmt );
	}
	void execute( MySqlPrep::Statement& stmt )
	{
		MF_ASSERT( !committed_ );
		lock_.sql.execute( stmt );
	}
	void execute( const std::string& stmt, BinaryOStream * resdata = NULL )
	{
		MF_ASSERT( !committed_ );
		lock_.sql.execute( stmt, resdata );
	}
	int query( const std::string & statement )
	{ return lock_.sql.query( statement ); }
	MYSQL_RES* storeResult()	{ return lock_.sql.storeResult(); }
	my_ulonglong insertID()		{ return lock_.sql.insertID(); }
	my_ulonglong affectedRows()	{ return lock_.sql.affectedRows(); }
	const char* info()			{ return lock_.sql.info();	}
	const char* getLastError()	{ return lock_.sql.getLastError(); }
	unsigned int getLastErrorNum() { return lock_.sql.getLastErrorNum(); }
	bool shouldRetry() { return (this->getLastErrorNum() == ER_LOCK_DEADLOCK); }

	void commit()
	{
		MF_ASSERT( !committed_ );
		lock_.sql.execute( "COMMIT" );
		committed_ = true;
	}

private:
	MySqlTransaction( const MySqlTransaction& );
	void operator=( const MySqlTransaction& );

	MySql::Lock		lock_;
	bool			committed_;
};

// this class is used to represent values that might also be
// null
template <class T>
class MySqlValueWithNull
{
public:
	MySqlValueWithNull() : isNull_(1) {}
	MySqlValueWithNull( const T& x ) : value_(x), isNull_(0) {}

	// this is the public interface
	void nullify() { isNull_ = 1; }
	void valuefy() { isNull_ = 0; }
	void set( const T& x ) { value_ = x; isNull_ = 0; }
	const T * get() const { return isNull_? 0 : &value_; }
	T& getBuf() { return value_; }

protected:
	template <class TYPE>
	friend MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings&, MySqlValueWithNull<TYPE>& );

	T value_;
	my_bool isNull_;
};

class MySqlTimestampNull : public MySqlValueWithNull< MYSQL_TIME >
{
public:
	typedef MySqlValueWithNull< MYSQL_TIME > BaseClass;
	MySqlTimestampNull() {}
	MySqlTimestampNull( const MYSQL_TIME& x ) : BaseClass(x) {}
private:
	friend MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings&, MySqlTimestampNull& );
};

// this class is used to represent binary data from MySQL
// can be NULL
class MySqlBuffer
{
public:
	MySqlBuffer( unsigned long capacity, enum_field_types type = FIELD_TYPE_BLOB ) :
		type_( type ),
		buffer_( new char[capacity] ),
		size_( 0 ),
		capacity_( capacity ),
		isNull_( 1 )
	{
	}

	~MySqlBuffer()
	{
		delete[] buffer_;
	}

	unsigned long size() const
	{
		return isNull_? 0 : size_;
	}

	unsigned long capacity() const
	{
		return capacity_;
	}

	void set( const void * ptr, unsigned long size )
	{
		if (size > capacity_)
		{
			printTruncateError( size, capacity_ );
			size = capacity_;
		}

		memcpy( buffer_, ptr, size );
		size_ = size;
		isNull_ = 0;
	}

	void setString( const std::string& s )
	{
		set( s.data(), s.length() );
	}

	std::string getString() const
	{
		const char * p = (const char *) this->get();
		if (!p) throw std::runtime_error("string is null");
		return std::string( p, size_ );
	}

	void nullify()
	{
		isNull_ = 1;
	}

	const void * get() const
	{
		return isNull_? 0 : buffer_;
	}

	bool isNull() const
	{
		return isNull_? true : false;
	}

private:
	friend MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& binding, MySqlBuffer& buffer );
	friend MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& binding, MySqlBuffer& buffer );
	friend BinaryIStream& operator>>( BinaryIStream& strm, MySqlBuffer& buffer );
	friend BinaryOStream& operator<<( BinaryOStream& strm, const MySqlBuffer& buffer );

	MySqlBuffer( const MySqlBuffer& );
	MySqlBuffer& operator=( const MySqlBuffer& );

	static void printTruncateError( unsigned long size,
			unsigned long capacity );

	enum_field_types type_;
	char * buffer_;
	unsigned long size_;
	unsigned long capacity_;
	my_bool isNull_;
};
BinaryIStream& operator>>( BinaryIStream& strm, MySqlBuffer& buffer );
BinaryOStream& operator<<( BinaryOStream& strm, const MySqlBuffer& buffer );

/**
 *	This class initialises itself to a MySQL escaped string from any string.
 */
class MySqlEscapedString
{
public:
	MySqlEscapedString( MySql& connection, const std::string& string ) :
		escapedString_( new char[ string.length() * 2 + 1 ] )
	{
		mysql_real_escape_string( connection.get(), escapedString_,
				string.data(), string.length() );
	}
	~MySqlEscapedString()
	{
		delete [] escapedString_;
	}

	operator char*()	{ return escapedString_; }

private:
	char *	escapedString_;
};

/**
 *	This class retrieves the fields and indexes of a table.
 */
class MySqlTableMetadata
{
public:
	MySqlTableMetadata( MySql& connection, const std::string tableName );
	~MySqlTableMetadata();

	bool isValid() const 				{ return result_; }
	unsigned int getNumFields() const	{ return numFields_; }
	const MYSQL_FIELD& getField( unsigned int index ) const
	{ return fields_[index]; }

private:
	MYSQL_RES*		result_;
	unsigned int 	numFields_;
	MYSQL_FIELD* 	fields_;
};

// The following classes should be API compatible between the prepared
// and unprepared versions since they get mapped to a common name
// depending on whether USE_MYSQL_PREPARED_STATEMENTS is defined.
#ifdef USE_MYSQL_PREPARED_STATEMENTS
	#include "mysql_prepared.hpp"
	typedef	MySqlPrep::Bindings		MySqlBindings;
	typedef MySqlPrep::Statement	MySqlStatement;
#else
	#include "mysql_notprepared.hpp"
	typedef	MySqlUnPrep::Bindings	MySqlBindings;
	typedef MySqlUnPrep::Statement	MySqlStatement;
#endif

/**
 *	Runs query in a transaction.
 */
template <class QUERY>
bool wrapInTransaction( MySql& connection, QUERY& query )
{
	bool retry;
	do
	{
		retry = false;
		try
		{
			MySqlTransaction transaction( connection );

			query.execute( transaction.get() );

			transaction.commit();
		}
		catch (MySqlRetryTransactionException& e)
		{
			retry = true;
		}
		catch (std::exception& e)
		{
			query.setExceptionStr( e.what() );
			return false;
		}
	} while (retry);

	return true;
};

#endif
