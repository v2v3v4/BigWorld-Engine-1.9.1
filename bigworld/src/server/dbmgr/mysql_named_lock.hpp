/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_NAMEDLOCK_HPP
#define MYSQL_NAMEDLOCK_HPP

#include <string>

class MySql;

namespace MySQL
{

bool obtainNamedLock( MySql& connection, const std::string& lockName );
void releaseNamedLock( MySql& connection, const std::string& lockName );

/**
 * 	This class obtains and releases an named lock.
 */
class NamedLock
{
public:
	class Exception: public std::exception
	{
	public:
		Exception( const std::string& lockName );
		~Exception() throw() {}
		const char * what() const throw()	{ return errMsg_.c_str(); }
	private:
		std::string errMsg_;
	};

	NamedLock( MySql& connection, const std::string& lockName,
			bool shouldLock = true );
	~NamedLock();

	bool lock();
	bool unlock();

	const std::string& lockName() const 	{ return lockName_; }
	bool isLocked() const 					{ return isLocked_; }

private:
	MySql&		connection_;
	std::string lockName_;
	bool		isLocked_;
};

}	// namespace MySQL

#endif 	// MYSQL_NAMEDLOCK_HPP
