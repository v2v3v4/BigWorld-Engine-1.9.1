/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_wrapper.hpp"

#include "mysql_prepared.hpp"
#include "mysql_notprepared.hpp"
#include "db_config.hpp"

#include "cstdmf/binary_stream.hpp"

#include <mysql/errmsg.h>

DECLARE_DEBUG_COMPONENT( 0 );

// -----------------------------------------------------------------------------
// Section: Utility functions
// -----------------------------------------------------------------------------
/**
 *	This function converts a MYSQL_TIME to Unix epoch time.
 */
time_t convertMySqlTimeToEpoch(  const MYSQL_TIME& mysqlTime )
{
	tm ctime;
	ctime.tm_year = mysqlTime.year - 1900;
	ctime.tm_mon = mysqlTime.month - 1;
	ctime.tm_mday = mysqlTime.day;
	ctime.tm_hour = mysqlTime.hour;
	ctime.tm_min = mysqlTime.minute;
	ctime.tm_hour = mysqlTime.hour;
	ctime.tm_sec = mysqlTime.second;

	// Init other fields to default
	ctime.tm_wday = -1;
	ctime.tm_yday = -1;
	ctime.tm_isdst = -1;

	return timegm( &ctime );
}

// -----------------------------------------------------------------------------
// Section: class MySqlTypeTraits
// -----------------------------------------------------------------------------
const std::string MySqlTypeTraits<std::string>::TINYBLOB( "TINYBLOB" );
const std::string MySqlTypeTraits<std::string>::BLOB( "BLOB" );
const std::string MySqlTypeTraits<std::string>::MEDIUMBLOB( "MEDIUMBLOB" );
const std::string MySqlTypeTraits<std::string>::LONGBLOB( "LONGBLOB" );

// -----------------------------------------------------------------------------
// Section: class MySql
// -----------------------------------------------------------------------------

MySql::MySql( const DBConfig::Connection& connectInfo ) :
// initially set all pointers to 0 so that we can see where we got to
// should an error occur
	sql_(0),
	inTransaction_(false)
{
	try
	{
		sql_ = mysql_init( 0 );
		if (!sql_)
			this->throwError( this->sql_ );
		if (!mysql_real_connect( sql_, connectInfo.host.c_str(),
				connectInfo.username.c_str(), connectInfo.password.c_str(),
				connectInfo.database.c_str(), connectInfo.port, NULL, 0 ))
			this->throwError( this->sql_ );
	}
	catch (std::exception& e)
	{
		ERROR_MSG( "MySql::MySql: %s\n", e.what() );
		if (sql_) mysql_close( sql_ );
		throw;
	}
}

MySql::~MySql()
{
	MF_ASSERT( !inTransaction_ );
	mysql_close( sql_ );
}

namespace MySqlUtils
{
	inline unsigned int getErrno( MYSQL* connection )
	{
		return mysql_errno( connection );
	}

	inline unsigned int getErrno( MYSQL_STMT* statement )
	{
		return mysql_stmt_errno( statement );
	}
}

template <class MYSQLOBJ>
void MySql::throwError( MYSQLOBJ* failedObj )
{
	unsigned int mySqlErrno = MySqlUtils::getErrno( failedObj );
//	DEBUG_MSG( "MySql::throwError: error=%d\n", mySqlErrno );
	switch (mySqlErrno)
	{
		case ER_LOCK_DEADLOCK:
		case ER_LOCK_WAIT_TIMEOUT:
			throw MySqlRetryTransactionException( failedObj );
			break;
		case ER_DUP_ENTRY:
			throw MySqlDuplicateEntryException( failedObj );
			break;
		case CR_SERVER_GONE_ERROR:
		case CR_SERVER_LOST:
			{
				MySqlError serverGone( failedObj );
				this->onFatalError( serverGone );
				throw serverGone;
			}
			break;
		default:
			throw MySqlError( failedObj );
			break;
	}
}

void MySql::execute( const std::string& statement, BinaryOStream * resdata )
{
	//DEBUG_MSG("MySqlTransaction::execute: %s\n", statement.c_str());
	if (mysql_real_query( this->sql_, statement.c_str(), statement.length() ))
		this->throwError( this->sql_ );
	MYSQL_RES * result = mysql_store_result( this->sql_ );
	if (result)
	{
		if (resdata != NULL)
		{
			int nrows = mysql_num_rows( result );
			int nfields = mysql_num_fields( result );
			(*resdata) << nrows << nfields;
			MYSQL_ROW arow;
			while ((arow = mysql_fetch_row( result )) != NULL)
			{
				unsigned long *lengths = mysql_fetch_lengths(result);
				for (int i = 0; i < nfields; i++)
				{
					if (arow[i] == NULL)
						(*resdata) << "NULL";
					else
						resdata->appendString(arow[i],lengths[i]);
				}
			}
		}
		mysql_free_result( result );
	}
}

void MySql::execute( MySqlUnPrep::Statement& statement )
{
	statement.giveResult( 0 );
	std::string query = statement.getQuery( this->sql_ );
	// DEBUG_MSG("MySqlTransaction::execute: %s\n", query.c_str());
	if (mysql_real_query( this->sql_, query.c_str(), query.length() ))
		this->throwError( this->sql_ );
	MYSQL_RES * result = mysql_store_result( this->sql_ );
	statement.giveResult( result );
}

void MySql::execute( MySqlPrep::Statement& stmt )
{
	if (mysql_stmt_execute( stmt.get() ))
		this->throwError( stmt.get() );

	if (mysql_stmt_store_result( stmt.get() ))
		this->throwError( stmt.get() );
}

// This is the non-exception version of execute().
int MySql::query( const std::string & statement )
{
	int errorNum =
			mysql_real_query( sql_, statement.c_str(), statement.length() );

	if ((errorNum == CR_SERVER_GONE_ERROR) || (errorNum == CR_SERVER_LOST))
	{
		this->onFatalError( MySqlError( sql_ ) );
	}

	return errorNum;
}


/**
 * 	This function returns the list of table names that matches the specified
 * 	pattern.
 */
void MySql::getTableNames( std::vector<std::string>& tableNames,
							const char * pattern )
{
	tableNames.clear();

	MYSQL_RES * pResult = mysql_list_tables( sql_, pattern );
	if (pResult)
	{
		tableNames.reserve( mysql_num_rows( pResult ) );

		MYSQL_ROW row;
		while ((row = mysql_fetch_row( pResult )) != NULL)
		{
			unsigned long *lengths = mysql_fetch_lengths(pResult);
			tableNames.push_back( std::string( row[0], lengths[0] ) );
		}
		mysql_free_result( pResult );
	}
}

// -----------------------------------------------------------------------------
// Section: class MySqlBuffer
// -----------------------------------------------------------------------------
void MySqlBuffer::printTruncateError( unsigned long size,
			unsigned long capacity )
{
	// Can't do this in the header file due to dependency on
	// DECLARE_DEBUG_COMPONENT()
	ERROR_MSG( "MySqlBuffer::set: truncating data of size %lu to %lu\n",
		size, capacity );
}

BinaryIStream& operator>>( BinaryIStream& strm, MySqlBuffer& buffer )
{
	int len = strm.readStringLength();
	buffer.set( strm.retrieve( len ), len );

	return strm;
}

BinaryOStream& operator<<( BinaryOStream& strm, const MySqlBuffer& buffer )
{
	const char * pBuf = (const char *) buffer.get();
	if (!pBuf) throw std::runtime_error("string is null");
	strm.appendString( pBuf, buffer.size_ );

	return strm;
}


// -----------------------------------------------------------------------------
// Section: class MySqlTableMetadata
// -----------------------------------------------------------------------------
MySqlTableMetadata::MySqlTableMetadata( MySql& connection, 
		const std::string tableName ) :
	result_( mysql_list_fields( connection.get(), tableName.c_str(), "%" ) )
{
	if (result_)
	{
		numFields_ = mysql_num_fields(result_);
		fields_ = mysql_fetch_fields(result_);
	}
	else
	{
		numFields_ = 0;
		fields_ = NULL;
	}
}

MySqlTableMetadata::~MySqlTableMetadata()
{
	if (result_)
	{
		mysql_free_result( result_ );
	}
}
