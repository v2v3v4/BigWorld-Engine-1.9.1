/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef CODE_INLINE
#define INLINE    inline
#else
/// INLINE macro.
#define INLINE
#endif


// -----------------------------------------------------------------------------
// Section: VolatileInfo
// -----------------------------------------------------------------------------

/**
 *	This method returns what volatile direction info should be sent.
 *	@return 0 = Yaw, pitch and roll
 *			1 = Yaw, pitch
 *			2 = Yaw
 *			3 = No direction
 *
 *	@note This assumes that the VolatileInfo is valid. That is, the direction
 *		priorities are in descending order.
 */
INLINE int VolatileInfo::dirType( float priority ) const
{
	return (priority > yawPriority) +
		(priority > pitchPriority) +
		(priority > rollPriority);
}


/**
 *	This method returns whether or not this object has volatile data to send.
 */
INLINE bool VolatileInfo::hasVolatile( float priority ) const
{
	return (priority < positionPriority) || (priority < yawPriority);
}


/**
 *	This function implements the == operator for VolatileInfo.
 */
INLINE bool operator==( const VolatileInfo & info1, const VolatileInfo & info2 )
{
	return info1.positionPriority == info2.positionPriority &&
			info1.yawPriority == info2.yawPriority &&
			info1.pitchPriority == info2.pitchPriority &&
			info1.rollPriority == info2.rollPriority;
}


/**
 *	This function implements the != operator for VolatileInfo.
 */
INLINE bool operator!=( const VolatileInfo & info1, const VolatileInfo & info2 )
{
	return !(info1 == info2);
}


// -----------------------------------------------------------------------------
// Section: PropertyEventStamps
// -----------------------------------------------------------------------------

/**
 *	This method is used to initialise PropertyEventStamps. This basically means
 *	that the number of stamps that this object can store is set to the number of
 *	properties in the associated entity that are stamped.
 */
INLINE void EntityDescription::PropertyEventStamps::init(
		const EntityDescription & entityDescription )
{
	// Resize the stamps to the required size and set all values to 1.
	eventStamps_.resize( entityDescription.numEventStampedProperties(), 1 );
}


/**
 *	This method is also used to initialise PropertyEventStamps but sets all
 *	values to the input value.
 */
INLINE void EntityDescription::PropertyEventStamps::init(
		const EntityDescription & entityDescription, EventNumber number )
{
	// TODO: This is probably only temporary. PropertyEventStamps shouldn't be
	// initialised with one stamps. The event stamps should probably be stored
	// in the database for each property.

	this->init( entityDescription );

	Stamps::iterator iter = eventStamps_.begin();

	while (iter != eventStamps_.end())
	{
		(*iter) = number;

		iter++;
	}
}


/**
 *	This method is used to set an event number corresponding to a data
 *	description.
 */
INLINE void EntityDescription::PropertyEventStamps::set(
		const DataDescription & dataDescription, EventNumber eventNumber )
{
	// Each DataDescription has an index for which element it stores its stamp
	// in.
	const int index = dataDescription.eventStampIndex();
	IF_NOT_MF_ASSERT_DEV( 0 <= index && index < (int)eventStamps_.size() )
	{
		MF_EXIT( "invalid event stamp index" );
	}
	

	eventStamps_[ index ] = eventNumber;
}


/**
 *	This method is used to get an event number corresponding to a data
 *	description.
 */
INLINE EventNumber EntityDescription::PropertyEventStamps::get(
		const DataDescription & dataDescription ) const
{
	const int index = dataDescription.eventStampIndex();
	IF_NOT_MF_ASSERT_DEV( 0 <= index && index < (int)eventStamps_.size() )
	{
		MF_EXIT( "invalid event stamp index" );
	}

	return eventStamps_[ index ];
}


// -----------------------------------------------------------------------------
// Section: Accessors
// -----------------------------------------------------------------------------

/**
 *	This method returns the index of this entity description.
 */
INLINE EntityTypeID EntityDescription::index() const
{
   	return index_;
}


/**
 *	This method sets the index of this entity description.
 */
INLINE void EntityDescription::index( EntityTypeID index )
{
	index_ = index;
}

/**
 *	This method returns the client index of this entity description. If no
 *	client type is associated with this type, -1 is returned.
 */
INLINE EntityTypeID EntityDescription::clientIndex() const
{
   	return clientIndex_;
}


/**
 *	This method sets the client index of this entity description.
 */
INLINE void EntityDescription::clientIndex( EntityTypeID index )
{
	clientIndex_ = index;
}




/**
 *	This method returns the client name of this entity description.
 *	The client name is the entity type that should be sent to the
 *	client. For example, if NPC is derived from Avatar, and NPC
 *	contains additional properties that the client does not need
 *	to know about, NPC objects can be sent to the client as Avatars.
 *	This means that the client does not need a specific script to
 *	handle NPCs.
 */
INLINE const std::string& EntityDescription::clientName() const
{
	return clientName_;
}


/**
 *	This method returns the volatile info of the entity class.
 */
INLINE const VolatileInfo & EntityDescription::volatileInfo() const
{
	return volatileInfo_;
}




/**
 *	This method returns the number of properties that may be event-stamped. For
 *	properties that are sent to other clients, the event number of their last
 *	change is kept by the entity on the cell. This is used to decide what is out
 *	of date for a cache.
 */
INLINE unsigned int EntityDescription::numEventStampedProperties() const
{
	return numEventStampedProperties_;
}



// -----------------------------------------------------------------------------
// Section: DataLoDLevels
// -----------------------------------------------------------------------------

/**
 *	This method returns the number of LoD levels.
 *
 *	@note A "Level of Detail" refers to the range and not the step. That is,
 *		if an entity type specifies two detail level steps at 20 metres and
 *		100 metres, say, it has 3 levels. [0, 20), [20, 100), [100, ...).
 */
INLINE int DataLoDLevels::size() const
{
	return size_;
}


/**
 *	This methods returns whether or not the input priority threshold needs more
 *	detail than the input level.
 */
INLINE bool DataLoDLevels::needsMoreDetail( int level, float priority ) const
{
	return priority < level_[ level ].low();
}


/**
 *	This methods returns whether or not the input priority threshold needs less
 *	detail than the input level.
 */
INLINE bool DataLoDLevels::needsLessDetail( int level, float priority ) const
{
	return priority > level_[ level ].high();
}

// entity_description.ipp
