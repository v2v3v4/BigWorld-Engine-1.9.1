/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_named_lock.hpp"

#include "mysql_wrapper.hpp"
#include "mysql_notprepared.hpp"

namespace MySQL
{

// -----------------------------------------------------------------------------
// Section: Functions
// -----------------------------------------------------------------------------
/**
 * 	This function obtains a named lock. Returns true if it was successful,
 * 	false if it failed i.e. lock already held by someone else.
 */
bool obtainNamedLock( MySql& connection, const std::string& lockName )
{
	std::stringstream ss;
	ss << "SELECT GET_LOCK( '" << lockName << "', 0 )";

	MySqlUnPrep::Statement getLockStmt( connection, ss.str() );
	int result;
	MySqlUnPrep::Bindings bindings;
	bindings << result;
	getLockStmt.bindResult( bindings );

	connection.execute( getLockStmt );

	return (getLockStmt.fetch() && result);
}

/**
 * 	This function releases a named lock (acquired by obtainNamedLock()).
 */
void releaseNamedLock( MySql& connection, const std::string& lockName )
{
	std::stringstream ss;
	ss << "SELECT RELEASE_LOCK( '" << lockName << "')";

	connection.execute( ss.str() );
}

// -----------------------------------------------------------------------------
// Section: NamedLock
// -----------------------------------------------------------------------------
NamedLock::NamedLock( MySql& connection, const std::string& lockName,
		bool shouldLock ) :
	connection_( connection ), lockName_( lockName ),
	isLocked_( false )
{
	if (shouldLock && !this->lock())
	{
		throw Exception( lockName_ );
	}
}

NamedLock::~NamedLock()
{
	this->unlock();
}

bool NamedLock::lock()
{
	if (!isLocked_ && obtainNamedLock( connection_, lockName_ ))
	{
		isLocked_ = true;
		return true;
	}

	return false;
}

bool NamedLock::unlock()
{
	if (isLocked_)
	{
		if (!connection_.hasFatalError())
		{
			// If we have a connection error then the lock is automatically
			// released anyway.
			releaseNamedLock( connection_, lockName_ );
		}
		isLocked_ = false;
		return true;
	}

	return false;
}



// -----------------------------------------------------------------------------
// Section: NamedLock::Exception
// -----------------------------------------------------------------------------
NamedLock::Exception::Exception( const std::string& lockName )
{
	std::stringstream ss;
	ss << "Failed to obtain lock named '" << lockName << "'";
	errMsg_ = ss.str();
}

}	// namespace MySQL
