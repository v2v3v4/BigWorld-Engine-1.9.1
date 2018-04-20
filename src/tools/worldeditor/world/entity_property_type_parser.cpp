/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "worldeditor/world/entity_property_type_parser.hpp"
#include "worldeditor/world/entity_property_parser.hpp"
#include "worldeditor/world/items/editor_chunk_entity.hpp"
#include "worldeditor/world/editor_entity_proxy.hpp"
#include "entitydef/entity_description_map.hpp"


/*static*/ std::vector<EntityPropertyTypeParser::FactoryPtr>
	EntityPropertyTypeParser::s_factories_;


// -----------------------------------------------------------------------------
// Section: Helper parser classes
// -----------------------------------------------------------------------------


/**
 *	Implementation of the INT entity property parser
 */
class EntityIntParser : public EntityPropertyTypeParser
{
public:
	bool checkVal( PyObject* val )
	{
		return PyInt_Check( val );
	}


	int addEnum( PyObject* val, int index )
	{
		if ( val )
			index = PyInt_AsLong( val );
		return index;
	}


	int addEnum( DataSectionPtr val, int index )
	{
		if ( val )
			index = val->asInt();
		return index;
	}


	GeneralProperty* plainProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType )
	{
		if ( dataType->typeName().substr(0, 5) == "UINT8" )
		{
			return new GenIntProperty( name,
				new EntityIntProxy( props, propIndex, EntityIntProxy::UINT8 ) );
		}
		else if ( dataType->typeName().substr(0, 4) == "INT8" )
		{
			return new GenIntProperty( name,
				new EntityIntProxy( props, propIndex, EntityIntProxy::SINT8 ) );
		}
		else
		{
			return new GenIntProperty( name,
				new EntityIntProxy( props, propIndex, EntityIntProxy::OTHER ) );
		}
	}


	GeneralProperty* enumProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType,
		DataSectionPtr choices )
	{
		return new ChoiceProperty( name,
			new EntityIntProxy( props, propIndex, EntityIntProxy::OTHER ),
			choices );
	}
private:
	class IntFactory : public EntityPropertyTypeParser::Factory
	{
	public:
		IntFactory()
		{
			EntityPropertyTypeParser::registerFactory( this );
		}

		EntityPropertyTypeParserPtr create( const std::string& name, DataTypePtr dataType )
		{
			if ( dataType->typeName().substr(0, 4) == "INT8" ||
				 dataType->typeName().substr(0, 5) == "UINT8" )
				return new EntityIntParser();
			else
				return NULL;
		}
	};
	typedef SmartPointer<IntFactory> IntFactoryPtr;
	static IntFactoryPtr s_factory;
};
EntityIntParser::IntFactoryPtr EntityIntParser::s_factory =
	new EntityIntParser::IntFactory;


/**
 *	Implementation of the FLOAT entity property parser
 */
class EntityFloatParser : public EntityPropertyTypeParser
{
public:
	bool checkVal( PyObject* val )
	{
		return PyFloat_Check( val );
	}


	int addEnum( PyObject* val, int index )
	{
		if ( val )
			enumMap_[ float(PyFloat_AsDouble( val )) ] = index;
		return index;
	}


	int addEnum( DataSectionPtr val, int index )
	{
		if ( val )
			enumMap_[ val->asFloat() ] = index;
		return index;
	}


	GeneralProperty* plainProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType )
	{
		return new GenFloatProperty( name,
			new EntityFloatProxy( props, propIndex ) );
	}


	GeneralProperty* enumProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType,
		DataSectionPtr choices )
	{
		return new ChoiceProperty( name,
			new EntityFloatEnumProxy( props, propIndex, enumMap_ ),
			choices );
	}


	GeneralProperty* radiusProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType,
		MatrixProxy* pMP,
		uint32 widgetColour,
		float widgetRadius )
	{
		return new GenRadiusProperty( name,
			new EntityFloatProxy( props, propIndex ), pMP,
			widgetColour, widgetRadius );
	}

private:
	std::map<float,int> enumMap_;

	class FloatFactory : public EntityPropertyTypeParser::Factory
	{
	public:
		FloatFactory()
		{
			EntityPropertyTypeParser::registerFactory( this );
		}

		EntityPropertyTypeParserPtr create( const std::string& name, DataTypePtr dataType )
		{
			if ( dataType->typeName().substr(0, 5) == "FLOAT" )
				return new EntityFloatParser();
			else
				return NULL;
		}
	};
	typedef SmartPointer<FloatFactory> FloatFactoryPtr;
	static FloatFactoryPtr s_factory;
};
EntityFloatParser::FloatFactoryPtr EntityFloatParser::s_factory =
	new EntityFloatParser::FloatFactory;


/**
 *	Implementation of the STRING entity property parser
 */
class EntityStringParser : public EntityPropertyTypeParser
{
public:
	bool checkVal( PyObject* val )
	{
		return PyString_Check( val );
	}


	int addEnum( PyObject* val, int index )
	{
		if ( val )
			enumMap_[ PyString_AsString( val ) ] = index;
		return index;
	}


	int addEnum( DataSectionPtr val, int index )
	{
		if ( val )
			enumMap_[ val->asString() ] = index;
		return index;
	}


	GeneralProperty* plainProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType )
	{
		return new TextProperty( name,
			new EntityStringProxy( props, propIndex ) );
	}


	GeneralProperty* enumProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType,
		DataSectionPtr choices )
	{
		return new ChoiceProperty( name,
			new EntityStringEnumProxy( props, propIndex, enumMap_ ),
			choices );
	}

private:
	std::map<std::string,int> enumMap_;
	class StringFactory : public EntityPropertyTypeParser::Factory
	{
	public:
		StringFactory()
		{
			EntityPropertyTypeParser::registerFactory( this );
		}

		EntityPropertyTypeParserPtr create( const std::string& name, DataTypePtr dataType )
		{
			if ( dataType->typeName().substr(0, 6) == "STRING" )
				return new EntityStringParser();
			else
				return NULL;
		}
	};
	typedef SmartPointer<StringFactory> StringFactoryPtr;
	static StringFactoryPtr s_factory;
};
EntityStringParser::StringFactoryPtr EntityStringParser::s_factory =
	new EntityStringParser::StringFactory;


/**
 *	Implementation of the ARRAY entity property parser
 */
class EntityArrayParser : public EntityPropertyTypeParser
{
public:
	bool checkVal( PyObject* val )
	{
		return PySequence_Check( val ) == 1;
	}


	int addEnum( PyObject* val, int index )
	{
		// Not supported in arrays
		return -1;
	}


	int addEnum( DataSectionPtr val, int index )
	{
		// Not supported in arrays
		return -1;
	}


	GeneralProperty* plainProperty(
		BasePropertiesHelper* props,
		int propIndex,
		const std::string& name,
		DataTypePtr dataType )
	{
		return new ArrayProperty( name,
			new EntityArrayProxy(
				props, dataType, propIndex ),
			props->pItem() );
	}


private:
	class ArrayFactory : public EntityPropertyTypeParser::Factory
	{
	public:
		ArrayFactory()
		{
			EntityPropertyTypeParser::registerFactory( this );
		}

		EntityPropertyTypeParserPtr create( const std::string& name, DataTypePtr dataType )
		{
			if ( dataType->typeName().substr(0, 5) == "ARRAY" )
				return new EntityArrayParser();
			else
				return NULL;
		}
	};
	typedef SmartPointer<ArrayFactory> ArrayFactoryPtr;
	static ArrayFactoryPtr s_factory;
};
EntityArrayParser::ArrayFactoryPtr EntityArrayParser::s_factory =
	new EntityArrayParser::ArrayFactory;


// -----------------------------------------------------------------------------
// Section: EntityPropertyTypeParser
// -----------------------------------------------------------------------------


/**
 *	Default implementation of the enum property, which prints an error
 *	and returns the result of calling the parser's plainProperty.
 */
GeneralProperty* EntityPropertyTypeParser::enumProperty(
	BasePropertiesHelper* props,
	int propIndex,
	const std::string& name,
	DataTypePtr dataType,
	DataSectionPtr choices )
{
	ERROR_MSG(
		"'%s': The ENUM widget is not supported in the '%s' data type\n",
		props->pItem()->edDescription().c_str(), dataType->typeName().c_str() );
	return plainProperty( props, propIndex, name, dataType );
}


/**
 *	Default implementation of the radius property, which prints an error
 *	and returns the result of calling the parser's plainProperty.
 */
GeneralProperty* EntityPropertyTypeParser::radiusProperty(
	BasePropertiesHelper* props,
	int propIndex,
	const std::string& name,
	DataTypePtr dataType,
	MatrixProxy* pMP,
	uint32 widgetColour,
	float widgetRadius )
{
	ERROR_MSG(
		"'%s': The RADIUS widget is not supported in the '%s' data type\n",
		props->pItem()->edDescription().c_str(), dataType->typeName().c_str() );
	return plainProperty( props, propIndex, name, dataType );
}


/**
 *	Static method that creates the approriate parser for the DataDescription
 *  passed in, and returns it.
 *
 *	@param pDD		Entity property data description
 *	@return			Appropriate parser for pDD
 */
/*static*/ EntityPropertyTypeParserPtr EntityPropertyTypeParser::create(
	const std::string& name, DataTypePtr dataType )
{
	EntityPropertyTypeParserPtr result;
	for ( std::vector<FactoryPtr>::iterator i = s_factories_.begin();
		i != s_factories_.end(); ++i )
	{
		result = (*i)->create( name, dataType );
		if ( result )
			break;
	}
	return result;
}


/**
 *	Static method that registers a parser factory.
 *
 *	@param factory		Factory to add to the list
 */
/*static*/ void EntityPropertyTypeParser::registerFactory( FactoryPtr factory )
{
	if ( factory )
		s_factories_.push_back( factory );
}
