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
 * 	@file data_description.hpp
 *
 *	This file provides the implementation of the DataDescription class.
 *
 *	@ingroup entity
 */

#ifndef DATA_DESCRIPTION_HPP
#define DATA_DESCRIPTION_HPP

#include <Python.h>
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "resmgr/datasection.hpp"
#include <set>
#include "entitydef/member_description.hpp"

class BinaryOStream;
class BinaryIStream;
class MD5;


/**
 *	This enumeration is used for flags to indicate properties of data associated
 *	with an entity type.
 *
 *	@ingroup entity
 */
enum EntityDataFlags
{
	DATA_GHOSTED		= 0x01,		///< Synchronised to ghost entities.
	DATA_OTHER_CLIENT	= 0x02,		///< Sent to other clients.
	DATA_OWN_CLIENT		= 0x04,		///< Sent to own client.
	DATA_BASE			= 0x08,		///< Sent to the base.
	DATA_CLIENT_ONLY	= 0x10,		///< Static client-side data only.
	DATA_PERSISTENT		= 0x20,		///< Saved to the database.
	DATA_EDITOR_ONLY	= 0x40,		///< Only read and written by editor.
	DATA_ID				= 0X80		///< Is an indexed column in the database.
};

#define DATA_DISTRIBUTION_FLAGS (DATA_GHOSTED | DATA_OTHER_CLIENT | \
								DATA_OWN_CLIENT | DATA_BASE | 		\
								DATA_CLIENT_ONLY | DATA_EDITOR_ONLY)

#define DEFAULT_DATABASE_LENGTH 65535

class MetaDataType;

#ifdef EDITOR_ENABLED
class GeneralProperty;
class EditorChunkEntity;
class ChunkItem;
#endif



class DataType;
typedef SmartPointer<DataType> DataTypePtr;


/**
 *	This class is the base class for objects that are used to create data types.
 */
class MetaDataType
{
public:
	MetaDataType() {}			// call addMetaType in your constructor
	virtual ~MetaDataType()	{}	// call delMetaType in your destructor

	static MetaDataType * find( const std::string & name );

	static void fini();

	/**
	 *	This virtual method should return your basic meta type name
	 */
	virtual const char * name() const = 0;

	/**
	 *	This virtual method is used in the creation of DataTypes. Once a
	 *	metatype is found for the current data section, it is asked for the type
	 *	associated with that data section.
	 *
	 *	Derived MetaDataTypes should override this method to return their data
	 *	type.
	 */
	virtual DataTypePtr getType( DataSectionPtr pSection ) = 0;

	static void addAlias( const std::string& orig, const std::string& alias );

protected:
	static void addMetaType( MetaDataType * pMetaType );
	static void delMetaType( MetaDataType * pMetaType );

private:
	typedef std::map< std::string, MetaDataType * > MetaDataTypes;
	static MetaDataTypes * s_metaDataTypes_;
};


class PropertyOwnerBase;


/**
 *	Objects derived from this class are used to describe a type of data that can
 *	be used in a data description. Note: When implementing the abstract methods
 *	of this class, in general data from Python can be trusted as it has been
 *	through the 'isSameType' check, but data from sections and streams cannot
 *	and must always be checked for errors.
 *
 *	@ingroup entity
 */
class DataType : public ReferenceCount
{
public:
	/**
	 *	Constructor.
	 */
	DataType( MetaDataType * pMetaDataType, bool isConst = true ) :
		pMetaDataType_( pMetaDataType ),
		isConst_( isConst )
	{
	}

	/**
	 *	Destructor.
	 */
	virtual ~DataType();

	/**
	 *	This method causes any stored script objects dervied from user script
	 *	to be reloaded.
	 */
	virtual void reloadScript()
	{
	}

	/**
	 *	This method causes any stored script objects dervied from user script
	 *	to be reloaded.
	 */
	virtual void clearScript()
	{
	}

	/**
	 *	This method sets the default value associated with this type. This
	 * 	value will be subsequently returned by the pDefaultValue() meth
	 */
	virtual void setDefaultValue( DataSectionPtr pSection ) = 0;

	/**
	 *	This method returns a new reference to the default value associated
	 *	with this data type. That is, the caller is responsible for
	 *	dereferencing it.
	 */
	virtual PyObjectPtr pDefaultValue() const = 0;

	/**
	 *	This method returns the default section for this type as defined
	 *	in alias.xml or the entity definition files.
	 */
	virtual DataSectionPtr pDefaultSection() const;

	/**
	 *	This method returns whether the input object is of this type.
	 */
	virtual bool isSameType( PyObject * pValue ) = 0;

	/**
	 *	This method adds the value of the appropriate type onto the input
	 *	bundle. The value can be assumed to have been created by this
	 *	class or to have (previously) passed the isSameType check.
	 * 	If isPersistentOnly is true, then the object should streamed only the
	 *  persistent parts of itself.
	 *
	 *	@param pValue	The value to add.
	 *	@param stream	The stream to add the value to.
	 *	@param isPersistentOnly	Indicates if only persistent data is being added.
	 */
	virtual void addToStream( PyObject * pValue, BinaryOStream & stream,
		bool isPersistentOnly )	const = 0;

	/**
	 *	This method returns a new object created from the input bundle.
	 *	The caller is responsible for decrementing the reference of the
	 *	object.
	 *
	 *	@param stream		The stream to read from.
	 *  @param isPersistentOnly	If true, then the stream only contains
	 * 						persistent properties of the object.
	 *
	 *	@return			A <b>new reference</b> to an object created from the
	 *					stream.
	 */
	virtual PyObjectPtr createFromStream( BinaryIStream & stream,
		bool isPersistentOnly ) const = 0;


	/**
	 *	This method adds the value of the appropriate type into the input
	 *	data section. The value can be assumed to have been created by this
	 *	class or to have (previously) passed the isSameType check.
	 */
	virtual void addToSection( PyObject * pValue, DataSectionPtr pSection )
		const = 0;

	/**
	 *	This method returns a new object created from the given
	 *	DataSection.
	 *
	 *	@param pSection	The datasection to use
	 *
	 *	@return			A <b>new reference</b> to an object created from the
	 *					section.
	 */
	virtual PyObjectPtr createFromSection( DataSectionPtr pSection ) const = 0;

	// DEPRECATED
	virtual bool fromStreamToSection( BinaryIStream & stream,
			DataSectionPtr pSection, bool isPersistentOnly ) const;
	// DEPRECATED
	virtual bool fromSectionToStream( DataSectionPtr pSection,
			BinaryOStream & stream, bool isPersistentOnly ) const;


	virtual PyObjectPtr attach( PyObject * pObject,
		PropertyOwnerBase * pOwner, int ownerRef );
	virtual void detach( PyObject * pObject );

	virtual PropertyOwnerBase * asOwner( PyObject * pObject );


	/**
	 *	This method adds this object to the input MD5 object.
	 */
	virtual void addToMD5( MD5 & md5 ) const = 0;


	static DataTypePtr buildDataType( DataSectionPtr pSection );
	static DataTypePtr buildDataType( const std::string& typeName );

#ifdef EDITOR_ENABLED
	virtual GeneralProperty * createEditorProperty( const std::string& name,
		ChunkItem* chunkItem, int editorEntityPropertyId )
		{ return NULL; }

	static DataSectionPtr findAliasWidget( const std::string& name )
	{
		AliasWidgets::iterator i = s_aliasWidgets_.find( name );
		if ( i != s_aliasWidgets_.end() )
			return (*i).second;
		else
			return NULL;
	}

	static void fini()
	{
		s_aliasWidgets_.clear();
	}
#endif

	MetaDataType * pMetaDataType()	{ return pMetaDataType_; }
	const MetaDataType * pMetaDataType() const	{ return pMetaDataType_; }

	bool isConst() const			{ return isConst_; }


	// derived class should call this first then do own checks
	virtual bool operator<( const DataType & other ) const
		{ return pMetaDataType_ < other.pMetaDataType_ ; }

	virtual std::string typeName() const
		{ return pMetaDataType_->name(); }

	static void clearStaticsForReload();

	static void reloadAllScript()
	{
		DataType::callOnEach( &DataType::reloadScript );
	}

	static void clearAllScript()
	{
		DataType::callOnEach( &DataType::clearScript );
	}
	static void callOnEach( void (DataType::*f)() );

protected:
	MetaDataType * pMetaDataType_;
	bool isConst_;

private:
	struct SingletonPtr
	{
		explicit SingletonPtr( DataType * pInst ) : pInst_( pInst ) { }

		DataType * pInst_;

		bool operator<( const SingletonPtr & me ) const
			{ return (*pInst_) < (*me.pInst_); }
	};
	typedef std::set<SingletonPtr> SingletonMap;
	static SingletonMap * s_singletonMap_;

	static DataTypePtr findOrAddType( DataTypePtr pDT );


	static bool initAliases();

	typedef std::map< std::string, DataTypePtr >	Aliases;
	static Aliases s_aliases_;
#ifdef EDITOR_ENABLED
	typedef std::map< std::string, DataSectionPtr >	AliasWidgets;
	static AliasWidgets s_aliasWidgets_;
#endif // EDITOR_ENABLED
};



/**
 *	This class is used to describe a type of data associated with an entity
 *	class.
 *
 *	@ingroup entity
 */
class DataDescription : public MemberDescription
{
public:
	DataDescription();
	DataDescription( const DataDescription& description );
	~DataDescription();

	enum PARSE_OPTIONS
	{
		PARSE_DEFAULT,			// Parses all known sections.
		PARSE_IGNORE_FLAGS = 1	// Ignores the 'Flags' section.
	};

	bool parse( DataSectionPtr pSection, const std::string & parentName,
		PARSE_OPTIONS options = PARSE_DEFAULT );

//	DataDescription & operator=( const DataDescription & description );

	bool isCorrectType( PyObject * pNewValue );

	void addToStream( PyObject * pNewValue, BinaryOStream & stream,
		bool isPersistentOnly ) const;
	PyObjectPtr createFromStream( BinaryIStream & stream,
		bool isPersistentOnly ) const;

	void addToSection( PyObject * pNewValue, DataSectionPtr pSection );
	PyObjectPtr createFromSection( DataSectionPtr pSection ) const;

	void fromStreamToSection( BinaryIStream & stream, DataSectionPtr pSection,
			bool isPersistentOnly ) const;
	void fromSectionToStream( DataSectionPtr pSection,
			BinaryOStream & stream, bool isPersistentOnly ) const;

	void addToMD5( MD5 & md5 ) const;

	/// @name Accessors
	//@{
	const std::string& name() const;
	PyObjectPtr pInitialValue() const;

	INLINE bool isGhostedData() const;
	INLINE bool isOtherClientData() const;
	INLINE bool isOwnClientData() const;
	INLINE bool isCellData() const;
	INLINE bool isBaseData() const;
	INLINE bool isClientOnlyData() const;
	INLINE bool isClientServerData() const;
	INLINE bool isPersistent() const;
	INLINE bool isIdentifier() const;

	DataSectionPtr pDefaultSection() const;

	bool isEditorOnly() const;

	bool isOfType( EntityDataFlags flags );
	const char* getDataFlagsAsStr() const;

	int index() const;
	void index( int index );

	int localIndex() const					{ return localIndex_; }
	void localIndex( int i )				{ localIndex_ = i; }

	int eventStampIndex() const;
	void eventStampIndex( int index );

	int clientServerFullIndex() const		{ return clientServerFullIndex_; }
	void clientServerFullIndex( int i )		{ clientServerFullIndex_ = i; }

	int detailLevel() const;
	void detailLevel( int level );

	int databaseLength() const;

#ifdef EDITOR_ENABLED
	bool editable() const;
	void editable( bool v );

	void widget( DataSectionPtr pSection );
	DataSectionPtr widget();
#endif

	DataType *		dataType() { return &*pDataType_; }
	const DataType* dataType() const { return &*pDataType_; }
	//@}

#ifdef ENABLE_WATCHERS
	static WatcherPtr pWatcher();
#endif

private:
	std::string	name_;
	DataTypePtr	pDataType_;
	int			dataFlags_;
	PyObjectPtr	pInitialValue_;
	DataSectionPtr	pDefaultSection_;

	int			index_;
	int			localIndex_;		// Index into local prop value vector.
	int			eventStampIndex_;	// Index into time-stamp vector.
	int			clientServerFullIndex_;

	int			detailLevel_;

	int			databaseLength_;

#ifdef EDITOR_ENABLED
	bool		editable_;
	DataSectionPtr pWidgetSection_;
#endif

	// NOTE: If adding data, check the assignment operator.
};



/**
 *	This base class is an object that can own properties.
 */
class PropertyOwnerBase
{
public:
	virtual ~PropertyOwnerBase() { }

	typedef std::basic_string<int> ChangePath;

	// called going to the root of the tree
	virtual void propertyChanged( PyObjectPtr val, const DataType & type,
		ChangePath path ) = 0;

	// called going to the leaves of the tree
	virtual int propertyDivisions() = 0;
	virtual PropertyOwnerBase * propertyVassal( int ref ) = 0;
	virtual PyObjectPtr propertyRenovate( int ref, BinaryIStream & data,
		PyObjectPtr & pValue, DataType *& pType ) = 0;

// base class methods:
	uint8 addToStream( PyObject * pValue, const DataType & type,
		const ChangePath & path, BinaryOStream & stream, int messageID );
	PropertyOwnerBase * getPathFromStream( int messageID, BinaryIStream & data,
		ChangePath & path );
};

/**
 *	This is the normal property owner for classes that are cool with a
 *	virtual function table.
 */
class PropertyOwner : public PyObjectPlus, public PropertyOwnerBase
{
protected:
	PropertyOwner( PyTypePlus * pType ) : PyObjectPlus( pType ) { }
};

/**
 *	This is a handy linking class for objects that dislike virtual functions.
 */
template <class C> class PropertyOwnerLink : public PropertyOwnerBase
{
public:
	PropertyOwnerLink( C & self ) : self_( self ) { }

	virtual void propertyChanged( PyObjectPtr val, const DataType & type,
			ChangePath path )
		{ self_.propertyChanged( val, type, path ); }

	virtual int propertyDivisions()
		{ return self_.propertyDivisions(); }
	virtual PropertyOwnerBase * propertyVassal( int ref )
		{ return self_.propertyVassal( ref ) ; }
	virtual PyObjectPtr propertyRenovate( int ref, BinaryIStream & data,
			PyObjectPtr & pValue, DataType *& pType )
		{ return self_.propertyRenovate( ref, data, pValue, pType ); }

private:
	C & self_;
};



#ifdef CODE_INLINE
#include "data_description.ipp"
#endif

#endif // DATA_DESCRIPTION_HPP

// data_description.hpp
