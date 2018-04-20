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
 * 	@file
 *
 *	This file provides the implementation of the EntityDescription class.
 *
 * 	@ingroup entity
 */

#ifndef ENTITY_DESCRIPTION_HPP
#define ENTITY_DESCRIPTION_HPP

#include "Python.h"	// Included in data_description.hpp and
					// method_description.hpp but should come before system
					// includes

#include <vector>
#include <float.h>

#include "data_description.hpp"
#include "method_description.hpp"
#include "network/basictypes.hpp"
#include "resmgr/datasection.hpp"
#include "base_user_data_object_description.hpp"


const float VOLATILE_ALWAYS = FLT_MAX;


/**
 *	This class is used to describe what information of an entity changes
 *	frequently and should be sent frequently.
 */
class VolatileInfo
{
public:
	VolatileInfo();
	bool parse( DataSectionPtr pSection );

	bool shouldSendPosition() const	{ return positionPriority > 0.f; }
	int dirType( float priority ) const;

	bool isLessVolatileThan( const VolatileInfo & info ) const;
	bool isValid() const;
	bool hasVolatile( float priority ) const;

	static bool priorityFromPyObject( PyObject * pObject, float & priority );
	static PyObject * pyObjectFromPriority( float priority );

	float	positionPriority;
	float	yawPriority;
	float	pitchPriority;
	float	rollPriority;

private:
	float asPriority( DataSectionPtr pSection ) const;
};

bool operator==( const VolatileInfo & info1, const VolatileInfo & info2 );
bool operator!=( const VolatileInfo & info1, const VolatileInfo & info2 );


namespace Script
{
	int setData( PyObject * pObject, VolatileInfo & rInfo,
			const char * varName = "" );
	PyObject * getData( const VolatileInfo & info );
}


class MD5;

/**
 *	This class is used by DataLoDLevels. If the priority goes below the low
 *	value, the consumer should more to a more detailed level. If the priority
 *	goes above the high value, we should move to a less detailed level.
 */
class DataLoDLevel
{
public:
	DataLoDLevel();

	float low() const					{ return low_; }
	float high() const					{ return high_; }

	float start() const					{ return start_; }
	float hyst() const					{ return hyst_; }

	void set( const std::string & label, float start, float hyst )
	{
		label_ = label;
		start_ = start;
		hyst_ = hyst;
	}

	const std::string & label() const	{ return label_; }

	void finalise( DataLoDLevel * pPrev, bool isLast );

	int index() const			{ return index_; }
	void index( int i )			{ index_ = i; }

	enum
	{
		OUTER_LEVEL = -2,
		NO_LEVEL = -1
	};

private:
	float low_;
	float high_;
	float start_;
	float hyst_;
	std::string label_;

	// Only used when starting up. It is used to translate detailLevel if the
	// detail levels were reordered because of a derived interface.
	int index_;
};


/**
 *	This class is used to store where the "Level of Detail" transitions occur.
 */
class DataLoDLevels
{
public:
	DataLoDLevels();
	bool addLevels( DataSectionPtr pSection );

	int size() const;
	const DataLoDLevel & getLevel( int i ) const	{ return level_[i]; }

	DataLoDLevel *  find( const std::string & label );
	bool findLevel( int & level, DataSectionPtr pSection ) const;

	bool needsMoreDetail( int level, float priority ) const;
	bool needsLessDetail( int level, float priority ) const;

private:
	// TODO: Reconsider what MAX_DATA_LOD_LEVELS needs to be.
	DataLoDLevel level_[ MAX_DATA_LOD_LEVELS + 1 ];

	int size_;
};


class AddToStreamVisitor;


/**
 *	This interface is used by EntityDescription::visit. Derive from this
 *	interface if you want to visit a subset of an EntityDescription's
 *	DataDescriptions.
 */
class IDataDescriptionVisitor
{
public:
	virtual ~IDataDescriptionVisitor() {};

	/**
	 *	This function is called to visit a DataDescription.
	 *
	 *	@param	propDesc	Info about the property.
	 *	@return	Return true when successful.
	 */
	virtual bool visit( const DataDescription& propDesc ) = 0;
};

/**
 *	This class is used to describe a type of entity. It describes all properties
 *	and methods of an entity type, as well as other information related to
 *	object instantiation, level-of-detail etc. It is normally created on startup
 *	when the entities.xml file is parsed.
 *
 * 	@ingroup entity
 */

class EntityDescription: public BaseUserDataObjectDescription

{
public:
	typedef std::vector< MethodDescription >	MethodList;

	/**
	 *	This class is used to store the event number when a property last
	 *	changed for each property in an entity that is 'otherClient'.
	 */
	class PropertyEventStamps
	{
	public:
		void init( const EntityDescription & entityDescription );
		void init( const EntityDescription & entityDescription,
			   EventNumber lastEventNumber );

		void set( const DataDescription & dataDescription,
				EventNumber eventNumber );

		EventNumber get( const DataDescription & dataDescription ) const;

		void addToStream( BinaryOStream & stream ) const;
		void removeFromStream( BinaryIStream & stream );

	private:
		typedef std::vector< EventNumber > Stamps;
		Stamps eventStamps_;
	};

	/**
	 *	This class is used to store descriptions of the methods of an entity.
	 */
	class Methods
	{
	public:
		bool init( DataSectionPtr pMethods,
			MethodDescription::Component component,
			const char * interfaceName );
		void checkExposedForSubSlots();
		void checkExposedForPythonArgs( const char * interfaceName );
		void supersede();

		unsigned int size() const;
		unsigned int exposedSize() const { return exposedMethods_.size(); }

		MethodDescription * internalMethod( unsigned int index ) const;
		MethodDescription * exposedMethod( uint8 topIndex, BinaryIStream & data ) const;
		MethodDescription * find( const std::string & name ) const;

		MethodList & internalMethods()	{ return internalMethods_; }
		const MethodList & internalMethods() const	{ return internalMethods_; }
#if ENABLE_WATCHERS
		static WatcherPtr pWatcher();
#endif
	private:
		typedef std::map< std::string, uint32 > Map;
		typedef MethodList	List;

		Map		map_;
		List	internalMethods_;

		std::vector< unsigned int >	exposedMethods_;
	};

public:
	EntityDescription();
	~EntityDescription();

	bool	parse( const std::string & name,
				DataSectionPtr pSection = NULL, bool isFinal = true );
	void	supersede( MethodDescription::Component component );

	enum DataDomain
	{
		BASE_DATA   = 0x1,
		CLIENT_DATA = 0x2,
		CELL_DATA   = 0x4,
		EXACT_MATCH = 0x8,
		ONLY_OTHER_CLIENT_DATA = 0x10,
		ONLY_PERSISTENT_DATA = 0x20
	};

	bool	addSectionToStream( DataSectionPtr pSection,
				BinaryOStream & stream,
				int dataDomains ) const;

	bool	addSectionToDictionary( DataSectionPtr pSection,
				PyObject * pDict,
				int dataDomains ) const;

	bool	addDictionaryToStream( PyObject * pDict,
				BinaryOStream & stream,
				int dataDomains ) const;

	bool	addAttributesToStream( PyObject * pDict,
				BinaryOStream & stream,
				int dataDomains,
				int32 * pDataSizes = NULL,
		   		int numDataSizes = 0 ) const;

	bool	readStreamToDict( BinaryIStream & stream,
				int dataDomains,
				PyObject * pDest ) const;

	bool	readStreamToSection( BinaryIStream & stream,
				int dataDomains,
				DataSectionPtr pSection ) const;

	bool	visit( int dataDomains, IDataDescriptionVisitor & visitor ) const;

	EntityTypeID			index() const;
	void					index( EntityTypeID index );

	EntityTypeID			clientIndex() const;
	void					clientIndex( EntityTypeID index );
	const std::string&		clientName() const;
	void					setParent( const EntityDescription & parent );

	bool hasCellScript() const		{ return hasCellScript_; }
	bool hasBaseScript() const		{ return hasBaseScript_; }
	bool hasClientScript() const	{ return hasClientScript_; }
	// note: the client script is found under 'clientName' not 'name'
	bool isClientOnlyType() const
	{ return !hasCellScript_ && !hasBaseScript_; }

	bool isClientType() const		{ return name_ == clientName_; }

	const VolatileInfo &	volatileInfo() const;

	unsigned int			clientServerPropertyCount() const;
	DataDescription*		clientServerProperty( unsigned int n ) const;

	unsigned int			clientMethodCount() const;
	MethodDescription*		clientMethod( uint8 n, BinaryIStream & data ) const;

	unsigned int			exposedBaseMethodCount() const
								{ return this->base().exposedSize(); }
	unsigned int			exposedCellMethodCount() const
								{ return this->cell().exposedSize(); }

	INLINE unsigned int		numEventStampedProperties() const;

	MethodDescription*		findClientMethod( const std::string& name ) const;

#ifdef MF_SERVER
	const DataLoDLevels &	lodLevels() const { return lodLevels_; }
#endif

	const Methods &			cell() const	{ return cell_; }
	const Methods &			base() const	{ return base_; }
	const Methods &			client() const	{ return client_; }

	void					addToMD5( MD5 & md5 ) const;
	void					addPersistentPropertiesToMD5( MD5 & md5 ) const;

	// ---- Error checking ----
	bool checkMethods( const EntityDescription::MethodList & methods,
		PyObject * pClass, bool warnOnMissing = true ) const;

#if ENABLE_WATCHERS
	static WatcherPtr pWatcher();
#endif
protected:
	bool				parseProperties( DataSectionPtr pProperties );
	bool				parseInterface( DataSectionPtr pSection,
								const char * interfaceName );
	bool				parseImplements( DataSectionPtr pInterfaces );

	const std::string	getDefsDir() const;
	const std::string	getClientDir() const;
	const std::string	getCellDir() const;
	const std::string	getBaseDir() const;

private:
//	EntityDescription( const EntityDescription & );
//	EntityDescription &		operator=( const EntityDescription & );

	bool				parseClientMethods( DataSectionPtr pMethods,
							const char * interfaceName );
	bool				parseCellMethods( DataSectionPtr pMethods,
							const char * interfaceName );
	bool				parseBaseMethods( DataSectionPtr pMethods,
							const char * interfaceName );

//	bool				parseMethods( DataSectionPtr parseMethods,
//											bool isForServer );
	bool	addToStream( const AddToStreamVisitor & visitor,
				BinaryOStream & stream,
				int dataTypes,
				int32 * pDataSizes = NULL,
		   		int numDataSizes = 0 ) const;

	static bool shouldConsiderData( int pass, const DataDescription * pDD,
		int dataDomains );
	static bool shouldSkipPass( int pass, int dataDomains );

	typedef std::vector< unsigned int >			PropertyIndices;

	EntityTypeID		index_;
	EntityTypeID		clientIndex_;
	std::string			clientName_;
	bool				hasCellScript_;
	bool				hasBaseScript_;
	bool				hasClientScript_;
	VolatileInfo 		volatileInfo_;

	/// Stores indices of properties sent between the client and the server in
	/// order of their client/server index.
	PropertyIndices		clientServerProperties_;

	// TODO:PM We should probably combine the property and method maps for
	// efficiency. Only one lookup instead of two.

	/// Stores all methods associated with the cell instances of this entity.
	Methods				cell_;

	/// Stores all methods associated with the base instances of this entity.
	Methods				base_;

	/// Stores all methods associated with the client instances of this entity.
	Methods				client_;

	/// Stores the number of properties that may be time-stamped with the last
	/// time that they changed.
	unsigned int		numEventStampedProperties_;
#ifdef MF_SERVER
	DataLoDLevels		lodLevels_;
#endif

#ifdef EDITOR_ENABLED
	std::string			editorModel_;
#endif

};

#ifdef CODE_INLINE
#include "entity_description.ipp"
#endif

#endif // ENTITY_DESCRIPTION_HPP
