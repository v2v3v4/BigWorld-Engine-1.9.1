/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_prepared.hpp"
#include "mysql_wrapper.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT(0);

namespace MySqlPrep
{
	// -----------------------------------------------------------------------------
	// Section: class MySqlPrep::Statement
	// -----------------------------------------------------------------------------

	Statement::Statement( MySql& con, const std::string& stmt ) : 
		stmt_( mysql_stmt_init( con.get() ) )
	{
		if ( !stmt_ )
			throw MySqlError( con.get() );

		if (mysql_stmt_prepare( stmt_, stmt.c_str(), stmt.length() ))
			throw MySqlError( stmt_ );

		if (stmt_)
			meta_ = mysql_stmt_result_metadata( stmt_ );
		else
			meta_ = 0;
	}

	Statement::~Statement()
	{
		if (meta_) mysql_free_result( meta_ );
		if (stmt_) mysql_stmt_close( stmt_ );
	}

	void Statement::bindParams( const Bindings& bindings )
	{
		if (stmt_)
		{
			MF_ASSERT( bindings.size() == this->paramCount() );
			paramBindings_ = bindings;
			if (mysql_stmt_bind_param( stmt_, paramBindings_.get() ))
				throw MySqlError( stmt_ );
		}
	}

	void Statement::bindResult( const Bindings& bindings )
	{
		if (stmt_)
		{
			MF_ASSERT( bindings.size() == this->resultCount() );
			resultBindings_ = bindings;
			if (mysql_stmt_bind_result( stmt_, resultBindings_.get() ))
				throw MySqlError( stmt_ );
		}
	}
}	// namespace MySqlPrep
