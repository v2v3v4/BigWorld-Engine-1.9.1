/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_PREPARED_HPP
#define MYSQL_PREPARED_HPP

#include "mysql_wrapper.hpp"

#include <mysql/mysql.h>
#include <vector>

namespace MySqlPrep
{
	// a set of bound values for a MySQL statement
	class Bindings
	{
	public:
		typedef std::vector<MYSQL_BIND>::size_type size_type;

	public:
		Bindings& attach( MYSQL_BIND& binding )
		{
			bindings_.push_back( binding );
			return *this;
		}

		std::vector<MYSQL_BIND>::size_type size() const
		{
			return bindings_.size();
		}

		bool empty() const
		{
			return bindings_.empty();
		}

		void clear()
		{
			bindings_.clear();
		}

		MYSQL_BIND * get()
		{
			return &bindings_[0];
		}

		void reserve( size_type size )
		{
			bindings_.reserve( size );
		}

		Bindings& operator+=( const Bindings& rhs )
		{
			bindings_.insert( bindings_.end(), rhs.bindings_.begin(),
					rhs.bindings_.end() );
			return *this;
		}

	private:
		std::vector<MYSQL_BIND> bindings_;
	};

	// a previously prepared statement
	class Statement
	{
	public:
		Statement( MySql& con, const std::string& stmt );
		~Statement();

		MYSQL_STMT * get() { return stmt_; }

		uint paramCount() { return mysql_stmt_param_count( stmt_ ); }
		uint resultCount() { return meta_? mysql_num_fields( meta_ ) : 0; }
		int resultRows() { return mysql_stmt_num_rows( stmt_ ); }
		void bindParams( const Bindings& bindings );
		void bindResult( const Bindings& bindings );

		bool fetch()
		{
			switch (mysql_stmt_fetch( stmt_ ))
			{
			case 0:
				return true;
			case MYSQL_NO_DATA:
				return false;
			default:
				throw MySqlError( stmt_ );
			}
		}

	private:
		Statement( const Statement& );
		void operator=( const Statement& );

		MYSQL_STMT * stmt_;
		Bindings paramBindings_;
		Bindings resultBindings_;
		MYSQL_RES * meta_;
	};

}	// namepace MySqlPrep

// NOTE: although we write binding<<value, we always
// take a reference to the value being put on... so don't deallocate
// the value, or allow a temporary to be passed in

inline MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& lhs,
		MySqlPrep::Bindings& rhs )
{
	lhs += rhs;
	return lhs;
}

template < class TYPE >
inline MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& binding, TYPE& x )
{
	MYSQL_BIND b;
	memset( &b, 0, sizeof(b) );
	b.buffer_type = MySqlTypeTraits<TYPE>::colType;
	b.is_unsigned = !std::numeric_limits<TYPE>::is_signed;
	b.buffer      = reinterpret_cast<char*>( &x );
	b.is_null     = NULL;
	return binding.attach( b );
}

template < class TYPE >
inline MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& binding, MySqlValueWithNull<TYPE>& x )
{
	MYSQL_BIND b;
	memset( &b, 0, sizeof(b) );
	b.buffer_type = MySqlTypeTraits<TYPE>::colType;
	b.is_unsigned = !std::numeric_limits<TYPE>::is_signed;
	b.buffer      = reinterpret_cast<char*>( &x.value_ );
	b.is_null     = &x.isNull_;
	return binding.attach( b );
}

inline MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& binding, MySqlTimestampNull& x )
{
	MYSQL_BIND b;
	memset( &b, 0, sizeof(b) );
	b.buffer_type = MYSQL_TYPE_TIMESTAMP;
	b.is_unsigned = false;
	b.buffer      = reinterpret_cast<char*>( &x.value_ );
	b.is_null     = &x.isNull_;
	return binding.attach( b );
}

inline MySqlPrep::Bindings& operator<<( MySqlPrep::Bindings& binding, MySqlBuffer& buffer )
{
	MYSQL_BIND b;
	memset( &b, 0, sizeof(b) );
	b.buffer_type   = buffer.type_;
	b.buffer        = buffer.buffer_;
	b.buffer_length = buffer.capacity_;
	b.length        = &buffer.size_;
	b.is_null       = &buffer.isNull_;
	return binding.attach( b );
}

#endif	// #ifndef MYSQL_PREPARED_HPP
