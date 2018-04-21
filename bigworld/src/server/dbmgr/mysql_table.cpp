/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/



#include "mysql_table.hpp"

//------------------------------------------------------------------------------
// Section: class IMySqlColumnMapping::Type
//------------------------------------------------------------------------------
/**
 *	This method converts the information in this ColumnType object into a
 * 	string suitable for a CREATE TABLE or ALTER TABLE statement. 
 */
std::string IMySqlColumnMapping::Type::getAsString( MySql& connection,
		IndexType idxType ) const
{
	// Builds up the type string using this object
	struct ResultStream
	{
		std::stringstream 	ss;
		bool				isUnsignedOrBinary;

		ResultStream( bool isUnsignedOrBinaryIn ) : 
			isUnsignedOrBinary( isUnsignedOrBinaryIn )
		{}

		void addType( const char * typeName )
		{
			ss << typeName;
		}
		void addNumericalType( const char * typeName )
		{
			ss << typeName;
			if (isUnsignedOrBinary)
			{
				ss << " UNSIGNED";
			}
		}
		void addVarLenType( const char * typeName, uint length )
		{
			ss << typeName << '(' << length << ')';
		}
		void addVarLenStrType( uint length, const char * prefix = "" )
		{
			const char * charTypeStr = isUnsignedOrBinary ? "BINARY" : "CHAR";
			ss << prefix << charTypeStr << '(' << length << ')';
		}
		void addMultiLenType( const char * lenType )
		{
			const char * blobTypeStr = isUnsignedOrBinary ? "BLOB" : "TEXT";
			ss << lenType << blobTypeStr;
		}
		void addAutoIncrement()
		{
			ss << " AUTO_INCREMENT";
		}
		void addDefaultValue( const std::string& defaultValue, 
				MySql* pConnection )
		{
			if (!defaultValue.empty())
			{
				ss << " DEFAULT ";
				if (pConnection)
				{
					ss << '\'' 
						<<	MySqlEscapedString( *pConnection, defaultValue )
						<< '\''; 
				}
				else
				{
					ss << defaultValue;
				}
			}
		}
		void addPrimaryKey()
		{
			ss << " PRIMARY KEY";
		}
		void addNotNull()
		{
			ss << " NOT NULL";
		}
		void addOnUpdate( const std::string & cmd )
		{
			ss << " ON UPDATE " << cmd;
		}
	} result( this->isUnsignedOrBinary );

	switch (fieldType)
	{
		case MYSQL_TYPE_TINY:
			result.addNumericalType( "TINYINT" );
			break;
		case MYSQL_TYPE_SHORT:
			result.addNumericalType( "SMALLINT" );
			break;
		case MYSQL_TYPE_INT24:
			result.addNumericalType( "MEDIUMINT" );
			break;
		case MYSQL_TYPE_LONG:
			result.addNumericalType( "INT" );
			break;
		case MYSQL_TYPE_LONGLONG:
			result.addNumericalType( "BIGINT" );
			break;
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
			result.addNumericalType( "DECIMAL" );
			break;
		case MYSQL_TYPE_FLOAT:
			result.addType( "FLOAT" );
			break;
		case MYSQL_TYPE_DOUBLE:
			result.addType( "DOUBLE" );
			break;
		case MYSQL_TYPE_TIMESTAMP:
			result.addType( "TIMESTAMP" );
			break;
		case MYSQL_TYPE_TIME:
			result.addType( "TIME" );
			break;
		case MYSQL_TYPE_NEWDATE:
			result.addType( "DATE" );
			break;
		case MYSQL_TYPE_YEAR:
			result.addVarLenType( "YEAR", this->length );
			break;
		case MYSQL_TYPE_VARCHAR:
			result.addVarLenType( "VARCHAR", this->length );
			break;
		case MYSQL_TYPE_TINY_BLOB:
			result.addMultiLenType( "TINY" );
			break;
		case MYSQL_TYPE_MEDIUM_BLOB:
			result.addMultiLenType( "MEDIUM" );
			break;
		case MYSQL_TYPE_LONG_BLOB:
			result.addMultiLenType( "LONG" );
			break;
		case MYSQL_TYPE_BLOB:
			result.addMultiLenType( "" );
			break;
		case MYSQL_TYPE_BIT:
			result.addVarLenType( "BIT", this->length );
			break;
		case MYSQL_TYPE_VAR_STRING:
			result.addVarLenStrType( this->length, "VAR" );
			break;
		case MYSQL_TYPE_STRING:
			result.addVarLenStrType( this->length );
			break;
//		case MYSQL_NULL:
//		case MYSQL_TYPE_SET:
//		case MYSQL_TYPE_GEOMETRY:
//		case MYSQL_TYPE_ENUM:
		default:
			MF_ASSERT( false );
			break;
	}

	if (this->isAutoIncrement)
	{
		result.addAutoIncrement();
	}
	else if (this->isDefaultValueSupported())
	{
		result.addDefaultValue( this->defaultValue, 
				this->isStringType() ? &connection : NULL );
	}

	if (!this->onUpdateCmd.empty())
	{
		result.addOnUpdate( this->onUpdateCmd );
	}

	if (idxType == IndexTypePrimary)
	{
		// The primary key has to be part of the column specification because
		// we want to create it when we create the table. If we do not, then
		// MySQL will create one automatically, and it may not be what we want.
		result.addPrimaryKey();
	}

	result.addNotNull();

	return result.ss.str();
}

/**
 * 	Returns true if the type supports the default value specification.
 */
std::string IMySqlColumnMapping::Type::getDefaultValueAsString( 
		MySql& connection ) const
{
	if (this->isStringType())
	{
		std::stringstream ss;
		ss << '\'' <<  MySqlEscapedString( connection, this->defaultValue )
				<< '\'';
		return ss.str();
	}
	else
	{
		return this->defaultValue;
	}
}

/**
 * 	Returns true if the type supports the default value specification.
 */
bool IMySqlColumnMapping::Type::isDefaultValueSupported() const
{
	switch (this->fieldType)
	{
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
			return false;
		default:
			return true;
	}
}

/**
 *	Whether the type is a string-like type.
 */
bool IMySqlColumnMapping::Type::isStringType() const
{
	switch (this->fieldType)
	{
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VARCHAR:
			return true;
		default:
			return false;
	}
}

bool IMySqlColumnMapping::Type::isSimpleNumericalType() const
{
	switch (this->fieldType)
	{
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_LONGLONG:
			return true;
		default:
			return false;
	}
}

/**
 * 	Equality operator. Need to take into account that not all members are 
 * 	relevant. 
 */
bool IMySqlColumnMapping::Type::operator==( const Type& other ) const
{
	struct HelperFunctions
	{
		static bool equalOptionalStuff( const Type& self, const Type& other )
		{
			switch (self.fieldType)	// Assume field types are the same.
			{
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_NEWDECIMAL:
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_NEWDATE:
					return true;
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_INT24:
				case MYSQL_TYPE_LONG:
				case MYSQL_TYPE_LONGLONG:
					return (self.isUnsignedOrBinary == other.isUnsignedOrBinary)
							&& (self.isAutoIncrement == other.isAutoIncrement);
				case MYSQL_TYPE_TINY_BLOB:
				case MYSQL_TYPE_MEDIUM_BLOB:
				case MYSQL_TYPE_LONG_BLOB:
				case MYSQL_TYPE_BLOB:
					return self.isUnsignedOrBinary == other.isUnsignedOrBinary;
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
					return (self.length == other.length) && 
						self.isUnsignedOrBinary == other.isUnsignedOrBinary;
				case MYSQL_TYPE_YEAR:
				case MYSQL_TYPE_VARCHAR:
					return self.length == other.length;
				default:
					MF_ASSERT( false );
					return true;
			}
		}

		static bool equalDefaultValue( const Type& self, const Type& other )
		{
			if (self.isAutoIncrement || !self.isDefaultValueSupported())
				return true;

			if (self.defaultValue == other.defaultValue)
				return true;

			if (self.isSimpleNumericalType())
			{
				return (self.defaultValue.empty() && (other.defaultValue == "0")) ||
					((self.defaultValue == "0") && other.defaultValue.empty());
			}

			if (self.fieldType == MYSQL_TYPE_STRING)
			{
				std::string nullString( self.length, 
						self.isBinary() ? '\0' : ' ' );
				return (self.defaultValue.empty() && 
								(other.defaultValue == nullString)) ||
						((self.defaultValue == nullString) && 
								other.defaultValue.empty());
			}

			if (self.fieldType == MYSQL_TYPE_TIMESTAMP)
			{
				return true;
			}

			return false;
		}
	};

	return (this->fieldType == other.fieldType) &&
			HelperFunctions::equalOptionalStuff( *this, other ) &&
			(this->isAutoIncrement == other.isAutoIncrement) &&
			HelperFunctions::equalDefaultValue( *this, other );
}

/**
 *	Returns true if the field is either an unsigned field or a binary blob 
 * 	field.
 */
bool IMySqlColumnMapping::Type::deriveIsUnsignedOrBinary( 
		const MYSQL_FIELD& field )
{
	switch (field.type)
	{
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_LONGLONG:
			return (field.flags & UNSIGNED_FLAG);
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
			return (field.charsetnr == 63);	// Binary
		default:
			// Doesn't apply to other types so we can return whatever we like.
			return false;
	}
}
