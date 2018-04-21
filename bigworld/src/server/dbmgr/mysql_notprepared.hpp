/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MYSQL_NOTPREPARED_HPP
#define MYSQL_NOTPREPARED_HPP

#include "mysql_wrapper.hpp"

#include "cstdmf/smartpointer.hpp"
#include "cstdmf/debug.hpp"

#include <mysql/mysql.h>
#include <vector>
#include <ostream>
#include <stdexcept>

namespace MySqlUnPrep
{
	class BindColumn : public ReferenceCount
	{
	public:
		virtual void addValueToStream( std::ostream&, MYSQL * ) = 0;
		virtual void getValueFromString( const char * str, int len ) = 0;
	};

	typedef SmartPointer<BindColumn> BindColumnPtr;

	// a set of bound values for a MySQL statement
	class Bindings
	{
	public:
		typedef std::vector<BindColumnPtr>::size_type size_type;
	public:
		Bindings& attach( BindColumnPtr binding )
		{
			bindings_.push_back( binding );
			return *this;
		}

		std::vector<BindColumnPtr>::size_type size() const
		{
			return bindings_.size();
		}

		void clear()
		{
			bindings_.clear();
		}

		BindColumnPtr * get()
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
		std::vector<BindColumnPtr> bindings_;
	};

	// a previously prepared statement
	class Statement
	{
	public:
		Statement( MySql& con, const std::string& stmt );
		~Statement();

		uint paramCount() { return params_.size(); }
		uint resultCount() { return results_.size(); }
		int resultRows() { return resultSet_? mysql_num_rows( resultSet_ ) : 0; }
		void bindParams( const Bindings& bindings );
		void bindResult( const Bindings& bindings );

		bool fetch();

		// implementation detail for notprepared
		std::string getQuery( MYSQL * sql );
		void giveResult( MYSQL_RES * resultSet );

	private:
		Statement( const Statement& );
		void operator=( const Statement& );

		MYSQL_RES * resultSet_;
		std::vector<std::string> queryParts_;
		Bindings params_;
		Bindings results_;
	};
}

namespace StringConv
{
	inline void toValue( float& value, const char * str )
	{
		char* remainder;
		value = strtof( str, &remainder );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}

	inline void toValue( double& value, const char * str )
	{
		char* remainder;
		value = strtod( str, &remainder );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}
	
	inline void toValue( int32& value, const char * str )
	{
		char* remainder;
		value = strtol( str, &remainder, 10 );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}


	inline void toValue( int8& value, const char * str )
	{
		int32 i;
		toValue( i, str );
		value = int8(i);
		if (value != i)
			throw std::runtime_error( "out of range" );
	}

	inline void toValue( int16& value, const char * str )
	{
		int32 i;
		toValue( i, str );
		value = int16(i);
		if (value != i)
			throw std::runtime_error( "out of range" );
	}

	inline void toValue( uint32& value, const char * str )
	{
		char* remainder;
		value = strtoul( str, &remainder, 10 );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}

	inline void toValue( uint8& value, const char * str )
	{
		uint32 ui;
		toValue( ui, str );
		value = uint8(ui);
		if (value != ui)
			throw std::runtime_error( "out of range" );
	}

	inline void toValue( uint16& value, const char * str )
	{
		uint32 ui;
		toValue( ui, str );
		value = uint16(ui);
		if (value != ui)
			throw std::runtime_error( "out of range" );
	}

	inline void toValue( int64& value, const char * str )
	{
		char* remainder;
		value = strtoll( str, &remainder, 10 );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}

	inline void toValue( uint64& value, const char * str )
	{
		char* remainder;
		value = strtoull( str, &remainder, 10 );
		if (*remainder)
			throw std::runtime_error( "not a number" );
	}

	inline void toValue( MySqlTimestampNull& value, const char * str )
	{
		MYSQL_TIME& timestamp = value.getBuf();

		int numAssigned = sscanf( str, "%d-%d-%d %d:%d:%d", &timestamp.year,
				&timestamp.month, &timestamp.day, &timestamp.hour,
				&timestamp.minute, &timestamp.second );
		if (numAssigned == 6)
		{
			timestamp.second_part = 0;
			value.valuefy();
		}
		else
		{
			throw std::runtime_error( "not a timestamp" );
		}
	}
	
	template <class TYPE>
	inline std::string toStr( const TYPE& value )
	{
		std::stringstream ss;
		ss << value;
		return ss.str();
	}

	// Special version for int8 and uint8 because they are characters but
	// we want them formatted as a number.
	inline std::string toStr( int8 value )
	{
		return toStr<int>( value );
	}
	inline std::string toStr( uint8 value )
	{
		return toStr<int>( value );
	}
}

namespace MySqlUnPrep
{
	template < class TYPE, class STRM_TYPE >
	class ValueWithNullBinding : public BindColumn
	{
		MySqlValueWithNull<TYPE>& x_;

	public:
		ValueWithNullBinding( MySqlValueWithNull<TYPE>& x ) : x_(x) {}
		void addValueToStream( std::ostream& os, MYSQL * sql )
		{
			const TYPE * val = x_.get();
			if (val)
				os << STRM_TYPE(*val);
			else
				os << "NULL";
		}
		void getValueFromString( const char * str, int len )
		{
			if (str)
			{
				TYPE value;
				StringConv::toValue( value, str );
				x_.set( value );
			}
			else
			{
				x_.nullify();
			}
		}
	};

	template < class TYPE, class STRM_TYPE >
	class ValueBinding : public BindColumn
	{
		TYPE& x_;

	public:
		ValueBinding( TYPE& x ) : x_(x) {}
		void addValueToStream( std::ostream& os, MYSQL * sql )
		{
			os << STRM_TYPE(x_);
		}
		void getValueFromString( const char * str, int len )
		{
			if (!str)
				throw std::runtime_error("NULL not supported on this field");
			StringConv::toValue( x_, str );
		}
	};
	
	class MySqlTimestampNullBinding : public BindColumn
	{
		MySqlTimestampNull& x_;

	public:
		MySqlTimestampNullBinding( MySqlTimestampNull& x ) : x_(x) {}
		void addValueToStream( std::ostream& os, MYSQL * sql )
		{
			const MYSQL_TIME* pMySqlTime = x_.get();
			if (pMySqlTime)
			{
				os << pMySqlTime->year << '-' << pMySqlTime->month << '-' 
						<< pMySqlTime->day << ' ' << pMySqlTime->hour << ':' 
						<< pMySqlTime->minute << pMySqlTime->second;
			}
			else
			{
				os << "NULL";
			}
		}
		void getValueFromString( const char * str, int len )
		{
			if (str)
			{
				StringConv::toValue( x_, str );
			}
			else
			{
				x_.nullify();
			}
		}
	};
	
}

// NOTE: although we write binding<<value, we always
// take a reference to the value being put on... so don't deallocate
// the value, or allow a temporary to be passed in

inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& lhs,
										  MySqlUnPrep::Bindings& rhs )
{
	lhs += rhs;
	return lhs;
}

template < class TYPE >
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b,
										  MySqlValueWithNull< TYPE >& x )
{
	b.attach( new MySqlUnPrep::ValueWithNullBinding< TYPE, TYPE >(x) );
	return b;
}

template < class TYPE >
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b, TYPE& x )
{
	b.attach( new MySqlUnPrep::ValueBinding< TYPE, TYPE >(x) );
	return b;
}

// __kyl__ (2/8/2005) Need special versions for int8 and uint8 because they are chars
// and gets put into a std::stringstream a bit differently to other integer types.
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b, int8& x )
{
	b.attach( new MySqlUnPrep::ValueBinding< int8, int >(x) );
	return b;
}
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b, uint8& x )
{
	b.attach( new MySqlUnPrep::ValueBinding< uint8, int >(x) );
	return b;
}
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b,
										  MySqlValueWithNull< int8 >& x )
{
	b.attach( new MySqlUnPrep::ValueWithNullBinding< int8, int >(x) );
	return b;
}
inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b,
										  MySqlValueWithNull< uint8 >& x )
{
	b.attach( new MySqlUnPrep::ValueWithNullBinding< uint8, int >(x) );
	return b;
}

MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings&, MySqlBuffer& );

inline MySqlUnPrep::Bindings& operator<<( MySqlUnPrep::Bindings& b,
		MySqlTimestampNull& x )
{
	b.attach( new MySqlUnPrep::MySqlTimestampNullBinding(x) );
	return b;
}

#endif // #ifndef MYSQL_NOTPREPARED_HPP
