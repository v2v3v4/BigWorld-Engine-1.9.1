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
#define INLINE inline
#else
#define INLINE
#endif



// -----------------------------------------------------------------------------
// AlertManager:: Methods for flagging alerts and updating them.
// -----------------------------------------------------------------------------

/**
 *	Sets the current status of the alert.
 *
 *	@return	The true/false value of the alert.
 */
INLINE bool AlertManager::alertStatus( AlertID alert)
{
	return (alert < MAXIMUM_ALERT_ID) ? alertStatus_[alert] : false;
}

/**
 *	Sets the appropriate alert to the new status.
 *
 *	@param	alert	The ID of the alert to be modified.
 *	@param	status	The new status for the alert.
 *
 */
INLINE void AlertManager::alertStatus( AlertID alert, bool status )
{
	if ( alert < MAXIMUM_ALERT_ID )
		alertStatus_[ alert ] = status;
}

/**
 *	Signal the alert to be on for the current frame (only)
 */
INLINE void AlertManager::signalAlert( AlertID alert)
{
	if ( alert < MAXIMUM_ALERT_ID )
	{
		alertStatus_[ alert ] = true;
		signaledStatus_[ alert ] = true;
	}
}

// alert_manager.ipp
