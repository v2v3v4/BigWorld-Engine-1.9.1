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
#define INLINE
#endif

// -----------------------------------------------------------------------------
// Section: Cell
// -----------------------------------------------------------------------------

/**
 *	This method returns the size of cell hysteresis (in metres). This is the
 *	extra distance an entity has to be over the cell boundary before it is
 *	offloaded.
 */
INLINE
float Cell::cellHysteresisSize() const
{
	// return (this->state() == IN_USE) ? cellHysteresisSize_ : 0.f;
	return cellHysteresisSize_;
}


/**
 *	This method returns the number of real entities on this cell.
 */
INLINE
int Cell::numRealEntities() const
{
	return realEntities_.size();
}


/**
 *	This method returns a map of the real entities on this cell. The map is
 *	keyed by their IDs.
 */
INLINE
Cell::Entities & Cell::realEntities()
{
	return realEntities_;
}

// cell.ipp
