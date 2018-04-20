/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 * 	@file data_description.cpp
 *
 *	This file provides the implementation of the DataDescription class.
 *
 *	@ingroup entity
 */

#include "pch.hpp"

#include "data_description.hpp"
#include "constants.hpp"

#include "cstdmf/base64.h"
#include "cstdmf/debug.hpp"
#include "cstdmf/md5.hpp"
#include "cstdmf/memory_stream.hpp"

#include "network/basictypes.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"

#ifndef CODE_INLINE
#include "data_description.ipp"
#endif

DECLARE_DEBUG_COMPONENT2( "DataDescription", 0 )


// -----------------------------------------------------------------------------
// Section: DataType static methods
// -----------------------------------------------------------------------------

DataType::SingletonMap * DataType::s_singletonMap_;
DataType::Aliases DataType::s_aliases_;
#ifdef EDITOR_ENABLED
DataType::AliasWidgets DataType::s_aliasWidgets_;
#endif // EDITOR_ENABLED
static bool s_aliasesDone = false;

/**
 *	Default implementation for pDefaultSection().
 */
DataSectionPtr DataType::pDefaultSection() const
{
	DataSectionPtr pDefaultSection = new XMLSection( "Default" );
	this->addToSection( this->pDefaultValue().getObject(), pDefaultSection );
	return pDefaultSection;
}

/**
 *	This factory method returns the DataType derived object associated with the
 *	input data section.
 *
 *	@param pSection	The data section describing the data type.
 *
 *	@return		A pointer to the data type. NULL if an invalid id is entered.
 *
 *	@ingroup entity
 */
DataTypePtr DataType::buildDataType( DataSectionPtr pSection )
{
	if (!pSection)
	{
		WARNING_MSG( "DataType::buildDataType: No <Type> section\n" );
		return NULL;
	}

	if (!s_aliasesDone)
	{
	   	s_aliasesDone = true;
		DataType::initAliases();
	}

	std::string typeName = pSection->asString();

	// See if it is an alias
	Aliases::iterator found = s_aliases_.find( typeName );
	if (found != s_aliases_.end())
	{
		if ( pSection->findChild( "Default" ) )
		{
			WARNING_MSG( "DataType::buildDataType: New default value for "
					"aliased data type '%s' is ignored. The default value of an"
					" aliased data type can only be overridden by the "
					"default value of an entity property.\n",
					typeName.c_str() );
		}
		return found->second;
	}

	// OK look for the MetaDataType then
	MetaDataType * pMetaType = MetaDataType::find( typeName );
	if (pMetaType == NULL)
	{
		ERROR_MSG( "DataType::buildDataType: "
			"Could not find MetaDataType '%s'\n", typeName.c_str() );
		return NULL;
	}

	// Build a DataType from the contents of the <Type> section
	DataTypePtr pDT = pMetaType->getType( pSection );

	if (!pDT)
	{
		ERROR_MSG( "DataType::buildDataType: "
			"Could not build %s from spec given\n", typeName.c_str() );
		return NULL;
	}

	pDT->setDefaultValue( pSection->findChild( "Default" ) );

	// And return either it or an existing one if this is a dupe
	return DataType::findOrAddType( pDT.getObject() );
}


/**
 *	Static method to find an equivalent data type in our set and delete
 *	the given one, or if there is no such data type then add this one.
 */
DataTypePtr DataType::findOrAddType( DataTypePtr pDT )
{
	if (s_singletonMap_ == NULL) s_singletonMap_ = new SingletonMap();

	SingletonMap::iterator found = s_singletonMap_->find(
										SingletonPtr( pDT.getObject() ) );

	if (found != s_singletonMap_->end())
		return found->pInst_;

	s_singletonMap_->insert( SingletonPtr( pDT.getObject() ) );
	return pDT;
}


/**
 *	This static method initialises the type aliases from alias.xml.
 *
 *	Note that these are full instances of DataType here, not just alternative
 *	labels for MetaDataTypes.
 */
bool DataType::initAliases()
{
	// Add internal aliases
	MetaDataType::addAlias( "FLOAT32", "FLOAT" );

	DataSectionPtr pAliases =
		BWResource::openSection( EntityDef::Constants::aliasesFile() );

	if (pAliases)
	{
		DataSection::iterator iter;
		for (iter = pAliases->begin(); iter != pAliases->end(); ++iter)
		{
			DataTypePtr pAliasedType = DataType::buildDataType( *iter );

			if (pAliasedType)
			{
				s_aliases_.insert( std::make_pair(
					(*iter)->sectionName().c_str(), pAliasedType ) );

#ifdef EDITOR_ENABLED
				s_aliasWidgets_.insert( std::make_pair(
					(*iter)->sectionName().c_str(),
					(*iter)->findChild( "Widget" ) ) );
#endif // EDITOR_ENABLED
			}
			else
			{
				ERROR_MSG( "DataType::initAliases: Failed to add %s\n",
					(*iter)->sectionName().c_str() );
			}
		}
	}
	else
	{
		WARNING_MSG( "Couldn't open aliases file '%s'\n",
			EntityDef::Constants::aliasesFile() );
	}

	return true;
}


/**
 *	This static method clears our internal statics in preparation for a full
 *	reload of all entitydef stuff.
 */
void DataType::clearStaticsForReload()
{
	// only need to clear the singleton map for UserDataTypes
	DataType::SingletonMap * oldMap = s_singletonMap_;
	s_singletonMap_ = NULL;	// set to NULL first for safety
	delete oldMap;

	s_aliases_.clear();
	s_aliasesDone = false;

	IF_NOT_MF_ASSERT_DEV( s_singletonMap_ == NULL )
	{
		MF_EXIT( "something is really wrong (NULL is no longer NULL)" );
	}
}

void DataType::callOnEach( void (DataType::*fn)() )
{
	if (s_singletonMap_)
	{
		DataType::SingletonMap::iterator iter = s_singletonMap_->begin();
		const DataType::SingletonMap::iterator endIter = s_singletonMap_->end();

		while (iter != endIter)
		{
			(iter->pInst_->*fn)();

			++iter;
		}
	}
}


// -----------------------------------------------------------------------------
// Section: DataType base class methods
// -----------------------------------------------------------------------------

/**
 *	Destructor
 */
DataType::~DataType()
{
	//SingletonPtr us( this );
	//SingletonMap::iterator found = s_singletonMap_->find( us );
	//if (found->pInst_ == this)
	//	s_singletonMap_->erase( found );

	// TODO: Make this more like code above than code below.
	// Unfortunately, code above doesn't work, because by the time we get here
	// in Windows, our virtual fn table has already been changed back to the
	// base class DataType one, and we can no longer call operator< on ourself.

	if (s_singletonMap_ != NULL)
	{
		for (SingletonMap::iterator it = s_singletonMap_->begin();
			it != s_singletonMap_->end();
			++it)
		{
			if (it->pInst_ == this)
			{
				s_singletonMap_->erase( it );
				break;
			}
		}
	}
}


/**
 *	This method reads this data type from a stream and adds it to a data
 *	section. The default implementation uses createFromStream then
 *	addToSection. DEPRECATED
 *
 *	@param stream		The stream to read from.
 *	@param pSection		The section to add to.
 *	@param isPersistentOnly Indicates whether only persistent data should be
 *		considered.
 *	@return true for success
 */
bool DataType::fromStreamToSection( BinaryIStream & stream,
	DataSectionPtr pSection, bool isPersistentOnly ) const
{
	PyObjectPtr pValue = this->createFromStream( stream, isPersistentOnly );
	if (!pValue)
		return false;

	this->addToSection( &*pValue, pSection );

	return true;
}


/**
 *	This method reads this data type from a data section and adds it to
 *	a stream. The default implementation uses createFromSection then
 *	addToStream. DEPRECATED
 *
 *	@param pSection		The section to read from.
 *	@param stream		The stream to write to.
 *	@param isPersistentOnly Indicates whether only persistent data should be
 *		considered.
 *	@return true for success
 */
bool DataType::fromSectionToStream( DataSectionPtr pSection,
							BinaryOStream & stream, bool isPersistentOnly ) const
{
	if (!pSection)
		return false;

	PyObjectPtr pValue = this->createFromSection( pSection );
	if (!pValue)
		return false;

	this->addToStream( &*pValue, stream, isPersistentOnly );

	return true;
}


/**
 *	This method first checks the type of the given object. If it fails, then
 *	it returns NULL. If it succeeds then it tells the object who its owner is,
 *	if it needs to know (i.e. if it is mutable and needs to tell its owner when
 *	it is modified). This base class implementation assumes that the object
 *	is const. Finally it returns a pointer to the same object passed in. Some
 *	implementations will need to copy the object if it already has another
 *	owner. In this case the newly-created object is returned instead.
 */
PyObjectPtr DataType::attach( PyObject * pObject,
	PropertyOwnerBase * pOwner, int ownerRef )
{
	if (this->isSameType( pObject ))
		return pObject;

	return NULL;
}


/**
 *	This method detaches the given object from its present owner.
 *	This base class implementation does nothing.
 */
void DataType::detach( PyObject * pObject )
{
}


/**
 *	This method returns the given object, which was created by us, in the
 *	form of a PropertyOwnerBase, i.e. an object which can own other objects.
 *	If the object cannot own other objects, then NULL should be returned.
 *
 *	This base class implementation always returns NULL.
 */
PropertyOwnerBase * DataType::asOwner( PyObject * pObject )
{
	return NULL;
}



// -----------------------------------------------------------------------------
// Section: MetaDataType
// -----------------------------------------------------------------------------

MetaDataType::MetaDataTypes * MetaDataType::s_metaDataTypes_ = NULL;

/*static*/ void MetaDataType::fini()
{
	delete s_metaDataTypes_;
	s_metaDataTypes_ = NULL;
}

/**
 * 	This static method adds an alias to a native data type name e.g. FLOAT to
 * 	FLOAT32. This is different to alias.xml which aliases a name to a whole
 * 	data type definition e.g. Gun to FIXED_DICT of 3 properties: STRING name,
 * 	UINT32 ammo, FLOAT accuracy.
 */
void MetaDataType::addAlias( const std::string& orig, const std::string& alias )
{
	MetaDataType * pMetaDataType = MetaDataType::find( orig );
	IF_NOT_MF_ASSERT_DEV( pMetaDataType )
	{
		return;
	}
	(*s_metaDataTypes_)[ alias ] = pMetaDataType;
}

/**
 *	This static method registers a meta data type.
 */
void MetaDataType::addMetaType( MetaDataType * pMetaType )
{
	if (s_metaDataTypes_ == NULL)
		s_metaDataTypes_ = new MetaDataTypes();

	const char * name = pMetaType->name();

	// Some error checking
	if (s_metaDataTypes_->find( name ) != s_metaDataTypes_->end())
	{
		CRITICAL_MSG( "MetaDataType::addType: "
			"%s has already been registered.\n", name );
		return;
	}

	(*s_metaDataTypes_)[ name ] = pMetaType;
}


/**
 *	This static method deregisters a meta data type.
 */
void MetaDataType::delMetaType( MetaDataType * pMetaType )
{
	//s_metaDataTypes_->erase( pMetaType->name() );
	// too tricky to do this on shutdown...
}


/**
 *	This static method finds the given meta data type by name.
 */
MetaDataType * MetaDataType::find( const std::string & name )
{
	MetaDataTypes::iterator found = s_metaDataTypes_->find( name );
	if (found != s_metaDataTypes_->end()) return found->second;
	return NULL;
}


// -----------------------------------------------------------------------------
// Section: DataDescription
// -----------------------------------------------------------------------------

#include "entity_description.hpp"

/**
 *	This is the default constructor.
 */
DataDescription::DataDescription() :
	MemberDescription(),
	pDataType_( NULL ),
	dataFlags_( 0 ),
	pInitialValue_( NULL ),
	pDefaultSection_( NULL ),
	index_( -1 ),
	localIndex_( -1 ),
	eventStampIndex_( -1 ),
	clientServerFullIndex_( -1 ),
	detailLevel_( DataLoDLevel::NO_LEVEL ),
	databaseLength_( DEFAULT_DATABASE_LENGTH )
#ifdef EDITOR_ENABLED
	, editable_( false )
#endif
{
}


// Anonymous namespace to make static to the file.
namespace
{
struct EntityDataFlagMapping
{
	const char *	name_;
	int				flags_;
	const char *	newName_;
};

EntityDataFlagMapping s_entityDataFlagMappings[] =
{
	{ "CELL_PRIVATE",		0,								NULL },
	{ "CELL_PUBLIC",		DATA_GHOSTED,					NULL },
	{ "OTHER_CLIENTS",		DATA_GHOSTED|DATA_OTHER_CLIENT,	NULL },
	{ "OWN_CLIENT",			DATA_OWN_CLIENT,				NULL },
	{ "BASE",				DATA_BASE,						NULL },
	{ "BASE_AND_CLIENT",	DATA_OWN_CLIENT|DATA_BASE,		NULL },
	{ "CELL_PUBLIC_AND_OWN",DATA_GHOSTED|DATA_OWN_CLIENT,	NULL },
	{ "ALL_CLIENTS",		DATA_GHOSTED|DATA_OTHER_CLIENT|DATA_OWN_CLIENT, NULL },
	{ "EDITOR_ONLY",		DATA_EDITOR_ONLY,				NULL },

	{ "PRIVATE",			0,								"CELL_PRIVATE" },
	{ "CELL",				DATA_GHOSTED,					"CELL_PUBLIC" },
	{ "GHOSTED",			DATA_GHOSTED,					"CELL_PUBLIC" },
	{ "OTHER_CLIENT",		DATA_GHOSTED|DATA_OTHER_CLIENT,	"OTHER_CLIENTS" },
	{ "GHOSTED_AND_OWN",	DATA_GHOSTED|DATA_OWN_CLIENT,	"CELL_PUBLIC_AND_OWN" },
	{ "CELL_AND_OWN",		DATA_GHOSTED|DATA_OWN_CLIENT,	"CELL_PUBLIC_AND_OWN" },
	{ "ALL_CLIENT",			DATA_GHOSTED|DATA_OTHER_CLIENT|DATA_OWN_CLIENT, "ALL_CLIENTS" },
};


/*
 *	This function converts a string to the appropriate flags. It is used when
 *	parsing the properties in the def files.
 *
 *	@param name The string to convert.
 *	@param flags This value is set to the appropriate flag values.
 *	@param parentName The name of the entity type to be used if a warning needs
 *		to be displayed.
 *	@param propName The name of the property that these flags are associated
 *		with.
 *	@return True on success, otherwise false.
 */
bool setEntityDataFlags( const std::string & name, int & flags,
		const std::string & parentName, const std::string & propName )
{
	const int size = sizeof( s_entityDataFlagMappings )/
		sizeof( s_entityDataFlagMappings[0] );

	for (int i = 0; i < size; ++i)
	{
		const EntityDataFlagMapping & mapping = s_entityDataFlagMappings[i];

		if (name == mapping.name_)
		{
			flags = mapping.flags_;

			if (mapping.newName_)
			{
				WARNING_MSG( "DataDescription::parse: "
						"Using old Flags option - %s instead of %s for %s.%s\n",
					mapping.name_, mapping.newName_,
					parentName.c_str(), propName.c_str() );
			}

			return true;
		}
	}

	return false;
};


/*
 *	This helper function returns the string that is assoicated with the input
 *	DataDescription flags.
 */
const char * getEntityDataFlagStr( int flags )
{
	const int size = sizeof( s_entityDataFlagMappings )/
		sizeof( s_entityDataFlagMappings[0] );

	for (int i = 0; i < size; ++i)
	{
		const EntityDataFlagMapping & mapping = s_entityDataFlagMappings[i];

		if (flags == mapping.flags_)
			return mapping.name_;
	}

	return NULL;
}

} // anonymous namespace


/**
 *	This method returns the data flags as a string (hopefully looking like
 * 	the one specified in the defs file).
 */
const char* DataDescription::getDataFlagsAsStr() const
{
	return getEntityDataFlagStr( dataFlags_ & DATA_DISTRIBUTION_FLAGS );
}


/**
 *	This method parses a data description.
 *
 *	@param pSection		The data section to parse.
 *	@param parentName	The name of the parent or an empty string if not parent
 *						exists.
 *	@param options		Additional parsing options. At the moment, the option
 *						PARSE_IGNORE_FLAGS is used by User Data Objects to
 *						ignore the 'Flags' section instead of failing to parse.
 *	@return				true if successful
 */
bool DataDescription::parse( DataSectionPtr pSection,
		const std::string & parentName,
		PARSE_OPTIONS options /*= PARSE_DEFAULT*/  )
{
	DataSectionPtr pSubSection;

	name_ = pSection->sectionName();

	DataSectionPtr typeSection = pSection->openSection( "Type" );

	pDataType_ = DataType::buildDataType( typeSection );

	if (!pDataType_)
	{
		ERROR_MSG( "DataDescription::parse: "
					"Unable to find data type '%s' for %s.%s\n",
				pSection->readString( "Type" ).c_str(),
				parentName.c_str(),
				name_.c_str() );

		return false;
	}

#ifdef EDITOR_ENABLED
	// try to get the default widget, if it's an alias and has one that is
	widget( DataType::findAliasWidget( typeSection->asString() ) );
#endif // EDITOR_ENABLED

	if ( options & PARSE_IGNORE_FLAGS )
	{
		dataFlags_ = 0;
	}
	else
	{
		// dataFlags_ = pSection->readEnum( "Flags", "EntityDataFlags" );
		if (!setEntityDataFlags( pSection->readString( "Flags" ), dataFlags_,
					parentName, name_ ))
		{
			ERROR_MSG( "DataDescription::parse: Invalid Flags section '%s' for %s\n",
					pSection->readString( "Flags" ).c_str(), name_.c_str() );
			return false;
		}
	}

	if (pSection->readBool( "Persistent", false ))
	{
		dataFlags_ |= DATA_PERSISTENT;
	}

	if (pSection->readBool( "Identifier", false ))
	{
		dataFlags_ |= DATA_ID;
	}

	// If the data lives on the base, it should not be on the cell.
	MF_ASSERT_DEV( !this->isBaseData() ||
			(!this->isGhostedData() && !this->isOtherClientData()) );

	if (this->isClientOnlyData())
	{
		WARNING_MSG( "DataDescription::parse(type %s): "
			"ClientOnlyData not yet supported.\n",
			pSection->asString().c_str() );
	}

	pSubSection = pSection->findChild( "Default" );

	// If they include a <Default> tag, use it to create the
	// default value. Otherwise, just use the default for
	// that datatype.

#ifdef EDITOR_ENABLED
	editable_ = pSection->readBool( "Editable", false );
#endif

	if (pSubSection)
	{
		if (pDataType_->isConst())
		{
			pInitialValue_ = pDataType_->createFromSection( pSubSection );
		}
		else
		{
			pDefaultSection_ = pSubSection;
		}
	}
#ifdef EDITOR_ENABLED
	// The editor always pre loads the default value, so it won't try to make
	// it in the loading thread, which causes issues
	else if (editable())
	{
		pInitialValue_ = pDataType_->pDefaultValue();
//		Py_XDECREF( pInitialValue_.getObject() );
	}

//	MF_ASSERT( pInitialValue_ );
#endif

	databaseLength_ = pSection->readInt( "DatabaseLength", databaseLength_ );
	// TODO: If CLASS data type, then DatabaseLength should be default for
	// the individual members of the class if it is not explicitly specified
	// for the member.

	return true;
}


/**
 *	The copy constructor is needed for this class so that we can handle the
 *	reference counting of pInitialValue_.
 */
DataDescription::DataDescription(const DataDescription & description) :
	pInitialValue_( NULL )
{
	(*this) = description;
}


/**
 *	The destructor for DataDescription handles the reference counting.
 */
DataDescription::~DataDescription()
{
}


/**
 *	This method returns whether or not the input value is the correct type to
 *	set as new value.
 */
bool DataDescription::isCorrectType( PyObject * pNewValue )
{
	return pDataType_ ? pDataType_->isSameType( pNewValue ) : false;
}


/**
 *	This method adds this object to the input MD5 object.
 */
void DataDescription::addToMD5( MD5 & md5 ) const
{
	md5.append( name_.c_str(), name_.size() );
	int md5DataFlags = dataFlags_ & DATA_DISTRIBUTION_FLAGS;
	md5.append( &md5DataFlags, sizeof(md5DataFlags) );
	pDataType_->addToMD5( md5 );
}


/**
 * 	This method returns the initial value of this data item, as a Python
 * 	object. It returns a smart pointer to avoid refcounting problems.
 */
PyObjectPtr DataDescription::pInitialValue() const
{
	if (pInitialValue_)
	{
		return pInitialValue_;
	}
	else if (pDefaultSection_)
	{
		PyObjectPtr pResult = pDataType_->createFromSection( pDefaultSection_ );
		if (pResult)
			return pResult;
	}

	return pDataType_->pDefaultValue();
}

/**
 * 	This method returns the default value section of this property.
 */
DataSectionPtr DataDescription::pDefaultSection() const
{
	if (pDataType_->isConst())
	{
		// We didn't store the default section. Re-construct it from the
		// initial value.
		if (pInitialValue_)
		{
			DataSectionPtr pDefaultSection = new XMLSection( "Default" );
			pDataType_->addToSection( pInitialValue_.getObject(),
					pDefaultSection );
			return pDefaultSection;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return pDefaultSection_;
	}
}

#ifdef EDITOR_ENABLED
/**
 *  This method sets the value "Widget" data section that describes
 *  specifics about how to show the property.
 */
void DataDescription::widget( DataSectionPtr pSection )
{
	pWidgetSection_ = pSection;
}

/**
 *  This method gets the value "Widget" data section that describes
 *  specifics about how to show the property.
 */
DataSectionPtr DataDescription::widget()
{
	return pWidgetSection_;
}
#endif // EDITOR_ENABLED


#if ENABLE_WATCHERS

WatcherPtr DataDescription::pWatcher()
{
	static WatcherPtr watchMe = NULL;
	
	if (!watchMe)
	{
		watchMe = new DirectoryWatcher();
		DataDescription * pNull = NULL;

		watchMe->addChild( "type", new SmartPointerDereferenceWatcher( 
				new MemberWatcher<std::string,DataType>(
				*static_cast< DataType *> ( NULL ),
				&DataType::typeName,
				static_cast< void (DataType::*)( std::string ) >( NULL ) )), 
				&pNull->pDataType_);

		watchMe->addChild( "name", 
						   makeWatcher( pNull->name_ ));
		watchMe->addChild( "localIndex", 
						   makeWatcher( pNull->localIndex_ ));
		watchMe->addChild( "clientServerFullIndex", 
						   makeWatcher( pNull->clientServerFullIndex_ ));
		watchMe->addChild( "index",
						   new MemberWatcher<int,DataDescription>
						   ( *static_cast< DataDescription *> ( NULL ),
							 &DataDescription::index,
							 static_cast< void (DataDescription::*)
							 ( int ) >( NULL ) ));
		watchMe->addChild( "stats", MemberDescription::pWatcher());
	}
	return watchMe;
}

#endif

// -----------------------------------------------------------------------------
// Section: PropertyOwnerBase
// -----------------------------------------------------------------------------

/**
 *	This class is used to manage writing to a stream of bits.
 */
class BitWriter
{
public:
	BitWriter() : byteCount_( 0 ), bitsLeft_( 8 )
   	{
		memset( bytes_, 0, sizeof(bytes_) );
	}

	void add( int dataBits, int dataWord )
	{
		uint32 dataHigh = dataWord << (32-dataBits);

		int bitAt = 0;
		while (bitAt < dataBits)
		{
			bytes_[byteCount_] |= (dataHigh>>(32-bitsLeft_));
			dataHigh <<= bitsLeft_;

			bitAt += bitsLeft_;
			if (bitAt <= dataBits)
				bitsLeft_ = 8, byteCount_++;
			else
				bitsLeft_ = bitAt-dataBits;
		}
	}

	int		usedBytes() { return byteCount_ + (bitsLeft_ != 8); }

	int		byteCount_;
	int		bitsLeft_;
	uint8	bytes_[224];
};


/**
 *	This method adds both the change path and the value to the stream.
 *	The messageID parameter indicates the message ID used to send part of
 *	the ChangePath. Specify -1 for it if you're not using it for that.
 */
uint8 PropertyOwnerBase::addToStream( PyObject * pValue, const DataType & type,
	const ChangePath & path, BinaryOStream & stream, int messageID )
{
	uint8 ret = (uint8)~0U;

	if (messageID == -1)
	{
		stream << uint8( path.size() );
		for (uint i = 0; i < path.size(); ++i)
			stream << path[i];
	}
	else
	{
		// we needn't add anything if this is a top-level property update
		// of a low-numebred property
		if (messageID < 61 && path.size() == 1)
		{
			ret = uint8(messageID);
		}
		// otherwise we need to do more encoding
		else
		{
			// ok, for now we are using message id 61 as an escape
			ret = 61;

			BitWriter bits;

			// put on the index of each property in reverse order,
			// with an extra bit between each to say 'keep going'
			// if we expected long chains it'd be better just to put on the
			// size at the beginning... but we don't so we wont :)
			PropertyOwnerBase * pOwner = this;
			for (int i = path.size()-1; i >= 0; --i)
			{
				int curIdx = path[i];

				if (i != (int)path.size()-1)
					bits.add( 1, 1 ); // add a 1 bit to say: keep going
				else
					curIdx = messageID;

				int maxIdx = pOwner->propertyDivisions();
				if (maxIdx > 1)
				{
					maxIdx--;
					register int nbits;
					// get the bit width of the maximum index, which will
					// be used to store the current index
#ifdef _WIN32
					_asm bsr eax, maxIdx	// Bit Scan Reverse(maxIdx)->eax
					_asm mov nbits, eax		// eax-> nbits
#else
					__asm__ (
						"bsr 	%1, %%eax" 	// Bit Scan Reverse(maxIdx)->eax
						:"=a"	(nbits) 	// output eax: nbits
						:"r"	(maxIdx)	// input %1: maxIdx
					);
#endif
					bits.add( nbits+1, curIdx );
				}
				else if (maxIdx >= 0)
				{
					// nothing to do
				}
				else
				{
					// maxIdx < 0 - i.e. this property owner does not have
					// is not a property owner in its own right

					// Add the current index to the stream according to the
					// following to maximise bandwidth in a lightweight
					// fashion:
					if (curIdx < 64)
					{
						// If it can be represented in 6 bits or less,
						// signal this by prefixing with a single clear bit,
						// 0b0
						bits.add( 1, 0 );
						bits.add( 6, curIdx );
					}
					else if (curIdx < 8192)
					{
						// If it can be represented in 13 bits or less,
						// signal this by prefixing with 0b10
						bits.add( 2, 2 );
						bits.add( 13, curIdx );
					}
					else
					{
						// If we need the entire int32, signal this by
						// prefixing with 0b11
						bits.add( 2, 3 );
						bits.add( 32, curIdx );
					}
				}

				pOwner = pOwner->propertyVassal( path[i] );
			}

			// if we can still have an owner here, need to put on
			// something to say this is the end of the list
			if (pOwner != NULL)
				bits.add( 1, 0 );	// add a 0 bit to say: stop here

			// and put it on the stream (to the nearest byte)
			int used = bits.usedBytes();
			memcpy( stream.reserve( used ), bits.bytes_, used );
		}
	}

	type.addToStream( pValue, stream, false );

	return ret;
}


/**
 *	This class is used to read from a stream of bits.
 */
class BitReader
{
public:
	BitReader( BinaryIStream & data ) : data_( data ), bitsLeft_( 0 ) { }

	int get( int nbits )
	{
		int	ret = 0;

		int gbits = 0;
		while (gbits < nbits)	// not as efficient as the writer...
		{
			if (bitsLeft_ == 0)
			{
				byte_ = *(uint8*)data_.retrieve( 1 );
				bitsLeft_ = 8;
			}

			int bitsTake = std::min( nbits-gbits, bitsLeft_ );
			ret = (ret << bitsTake) | (byte_ >> (8-bitsTake));
			byte_ <<= bitsTake;
			bitsLeft_ -= bitsTake;
			gbits += bitsTake;
		}

		return ret;
	}

	BinaryIStream & data_;
	int	bitsLeft_;
	uint8 byte_;
};


/**
 *	This method sets a value in the tree rooted at this property owner,
 *	as described by the given stream. It records the pointer to the object,
 *	the type of the object, and the change path in the input references.
 *	Specify -1 as messageID if it was not used to encode part of the
 *	ChangePath.	It returns the old value of the property, or NULL if an
 *	error occurred.
 */
PropertyOwnerBase * PropertyOwnerBase::getPathFromStream( int messageID,
	BinaryIStream & data, ChangePath & path )
{
	PropertyOwnerBase * pOwner = this;

	// extract the path somehow
	if (messageID == -1)
	{
		uint8 cpLen;
		data >> cpLen;
		path.assign( cpLen, 0 );
		for (uint i = 0; i < cpLen; ++i)
			data >> path[i];

		// find the owner of the property
		for (uint i = path.size()-1; i >= 1 && pOwner != NULL; --i)
			pOwner = pOwner->propertyVassal( path[i] );
	}
	else
	{
		if (messageID < 61)
		{
			path.assign( 1, messageID );
		}
		else
		{
			BitReader bits( data );

			int lpath[256];
			int * ppath = lpath;

			while (1)
			{
				int curIdx;
				int maxIdx = pOwner->propertyDivisions();
				if (maxIdx > 1)
				{
					// get the expected bit width of the current index

					maxIdx--;
					register int nbits;
#ifdef _WIN32
					_asm bsr eax, maxIdx	// Bit Scan Reverse(maxIdx)->eax
					_asm mov nbits, eax		// eax-> nbits
#else
					__asm__ (
						"bsr 	%1, %%eax" 	// Bit Scan Reverse(maxIdx)->eax
						:"=a"	(nbits) 	// output eax: nbits
						:"r"	(maxIdx)	// input %1: maxIdx
					);
#endif
					curIdx = bits.get( nbits+1 );
				}
				else if (maxIdx >= 0)	// 0 isn't much sense for exclusive max
				{
					curIdx = 0;
				}
				else
				{
					// Get the prefix indicating the packed index width
					if (bits.get( 1 ) == 0)
					{
						// If data of the form 0b0...
						// index is 6 bits wide
						curIdx = bits.get( 6 );
					}
					else if (bits.get( 1 ) == 0)
					{
						// 0b10...
						// index is 13 bits wide
						curIdx = bits.get( 13 );
					}
					else
					{
						// 0b11...
						// index required the entire int32
						curIdx = bits.get( 32 );
					}
				}

				// ok we've got the index
				*ppath++ = curIdx;

				// now take a peek at the next owner
				PropertyOwnerBase * pNext = pOwner->propertyVassal( curIdx );

				if (pNext == NULL) break;	// can't go any further

				// otherwise read a bit to say whether or not we go on
				if (bits.get( 1 ) == 0) break;

				// move on to the next owner
				pOwner = pNext;
			}

			// ok, we've got the path, yay! now reverse it and return it
			path.assign( ppath-lpath, 0 );
			for (uint i = 0; i < path.size(); ++i)
				path[i] = *--ppath;
		}
	}

	return pOwner;
}



// The following is a hack to make sure that data_types.cpp gets linked.
extern int DATA_TYPES_TOKEN;
int * pDataTypesToken = &DATA_TYPES_TOKEN;

// data_description.cpp
