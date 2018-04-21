/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mysql_notprepared.hpp"
#include "mysql_wrapper.hpp"

#include "cstdmf/debug.hpp"
#include "cstdmf/binary_stream.hpp"

#include <sstream>

DECLARE_DEBUG_COMPONENT(0);

namespace MySqlUnPrep
{
	// -----------------------------------------------------------------------------
	// Section: class MySqlUnPrep::Statement
	// -----------------------------------------------------------------------------

	Statement::Statement( MySql&,const std::string& stmt ) :
		resultSet_(0)
	{
		using namespace std;

		if ( stmt == "" )
			return;

		string::const_iterator last = stmt.begin();
		for ( string::const_iterator i = stmt.begin();
				i != stmt.end(); ++i )
		{
			if (*i == '?')
			{
				queryParts_.push_back( string(last,i) );
				last = i+1;
			}
		}
		queryParts_.push_back( string(last,stmt.end()) );
	}

	Statement::~Statement()
	{
		this->giveResult( NULL );
	}

	void Statement::bindParams( const Bindings& bindings )
	{
		if (queryParts_.size())
		{
			MF_ASSERT( bindings.size() == queryParts_.size()-1 );
			params_ = bindings;
		}
	}

	void Statement::bindResult( const Bindings& bindings )
	{
		if (queryParts_.size())
			results_ = bindings;
	}

	std::string Statement::getQuery( MYSQL * sql )
	{
		using namespace std;

		if (!queryParts_.size())
			throw std::runtime_error( "no such query exists" );

		ostringstream os;
		// __kyl__ (1/2/2006) Luckly BigWorld doesn't have a DOUBLE data type,
		// otherwise precision should be 16. Or we should do some work to set
		// the precision correctly for each data type.
		os.precision(8);
		vector<string>::const_iterator pQueryPart = queryParts_.begin();
		BindColumnPtr * ppBinding = params_.get();

		os << *pQueryPart;
		while (++pQueryPart != queryParts_.end())
		{
			(*ppBinding)->addValueToStream( os, sql );
			++ppBinding;
			os << *pQueryPart;
		}

		return os.str();
	}

	void Statement::giveResult( MYSQL_RES * resultSet )
	{
		if (resultSet_)
			mysql_free_result( resultSet_ );
		resultSet_ = resultSet;
		if (resultSet_)
		{
			if (mysql_num_fields( resultSet_ ) != results_.size())
			{
				WARNING_MSG( "MySqlUnPrep::Statement::giveResult: "
						"size mismatch; got %i fields, but expected %zu\n",
						mysql_num_fields( resultSet_ ), results_.size() );
				mysql_free_result( resultSet_ );
				resultSet_ = 0;
			}
		}
	}

	bool Statement::fetch()
	{
		if (!resultSet_)
			throw std::runtime_error( "error fetching results" );
		MYSQL_ROW row = mysql_fetch_row( resultSet_ );
		unsigned long * lengths = mysql_fetch_lengths( resultSet_ );
		if (!row)
			return false;
		BindColumnPtr * ppBinding = results_.get();
		for (size_t i=0; i<results_.size(); ++i)
			ppBinding[i]->getValueFromString( row[i], lengths[i] );
		return true;
	}
}	// namespace MySqlUnPrep

namespace
{
	struct BindBuffer : public MySqlUnPrep::BindColumn
	{
		BindBuffer( MySqlBuffer * x ) : x_(x) {}
		void addValueToStream( std::ostream& os, MYSQL * sql )
		{
			if ( x_->isNull() )
				os << "NULL";
			else
			{
				char * buffer = new char[1 + 2*x_->size()];
				mysql_real_escape_string( sql, buffer, (const char*)x_->get(), x_->size() );
				os << '\'' << buffer << '\'';
				delete[] buffer;
			}
		}
		void getValueFromString( const char * str, int len )
		{
			if (str)
				x_->set( str, len );
			else
				x_->nullify();
		}
		MySqlBuffer * x_;
	};
}

MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b, MySqlBuffer& x )
{
	b.attach( new BindBuffer(&x) );
	return b;
}
