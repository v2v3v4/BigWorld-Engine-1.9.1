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


/**
 * 	This method changes the default inactivity timeout. If no packets are
 *	received for this amount of time, the connection will timeout and
 *	disconnect. This period is also used to calculate average packet loss. This
 *	method should be called before logging in.
 *
 * 	@param seconds Inactivity timeout in seconds
 */
INLINE
void ServerConnection::setInactivityTimeout( float seconds )
{
	inactivityTimeout_ = seconds;
}


/**
 *	This method returns the total number of packets received.
 */
INLINE uint32 ServerConnection::packetsIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return packetsIn_;
}


/**
 *	This method returns the total number of packets sent.
 */
INLINE uint32 ServerConnection::packetsOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return packetsOut_;
}


/**
 *	This method returns the total number of bits received.
 */
INLINE uint32 ServerConnection::bitsIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return bitsIn_;
}


/**
 *	This method returns the total number of bits sent.
 */
INLINE uint32 ServerConnection::bitsOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return bitsOut_;
}


/**
 *	This method returns the total number of messages received.
 */
INLINE uint32 ServerConnection::messagesIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return messagesIn_;
}


/**
 *	This method returns the total number of messages sent.
 */
INLINE uint32 ServerConnection::messagesOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return messagesOut_;
}


/**
 *	This method is used to set the pointer to current time so that this object
 *	has access to the application's time. It is used for server statistics and
 *	for syncronising between client and server time.
 *
 *	TODO: This needs to be reviewed.
 */
INLINE void ServerConnection::pTime( const double * pTime )
{
	pTime_ = pTime;
}


/**
 *	This method returns the current time that the application thinks it is.
 */
INLINE double ServerConnection::appTime() const
{
	return (pTime_ != NULL) ? *pTime_ : 0.f;
}


// servconn.ipp
