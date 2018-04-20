/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "reviver_subject.hpp"

#include "reviver_common.hpp"

#include "server/bwconfig.hpp"
#include "network/bundle.hpp"

DECLARE_DEBUG_COMPONENT2( "Server", 0 );

// -----------------------------------------------------------------------------
// Section: Static data
// -----------------------------------------------------------------------------

ReviverSubject ReviverSubject::instance_;


// -----------------------------------------------------------------------------
// Section: Constructor/Destructor
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
ReviverSubject::ReviverSubject() :
	pNub_( NULL ),
	reviverAddr_( 0, 0 ),
	lastPingTime_( 0 ),
	priority_( 0xff ),
	msTimeout_( 0 )
{
}


/**
 *	This method initialises this object.
 *
 *	@param pNub The nub that reply will be sent with.
 *	@param componentName The name of the component type being monitored.
 */
void ReviverSubject::init( Mercury::Nub * pNub, const char * componentName )
{
	pNub_ = pNub;
	char buf[128];
#ifdef _WIN32 
	_snprintf( buf, sizeof(buf), "reviver/%s/subjectTimeout", componentName );
#else  // WIN32 (this is linux)
	snprintf( buf, sizeof(buf), "reviver/%s/subjectTimeout", componentName );
#endif // WIN32
	msTimeout_ = int(BWConfig::get( buf,
				BWConfig::get( "reviver/subjectTimeout", 0.2f ) ) * 1000);
	INFO_MSG( "ReviverSubject::init: msTimeout_ = %d\n", msTimeout_ );
}


/**
 *	This method finialises this object.
 */
void ReviverSubject::fini()
{
	pNub_ = NULL;
}


// -----------------------------------------------------------------------------
// Section: Misc
// -----------------------------------------------------------------------------

/**
 *	This method handles messages from revivers.
 */
void ReviverSubject::handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
{
	if (pNub_ == NULL)
	{
		ERROR_MSG( "ReviverSubject::handleMessage: "
						"ReviverSubject not initialised\n" );
		return;
	}

	uint64 currentPingTime = timestamp();

	ReviverPriority priority;
	data >> priority;

	bool accept = (reviverAddr_ == srcAddr);

	if (!accept)
	{
		if (priority < priority_)
		{
			if (priority_ == 0xff)
			{
				INFO_MSG( "ReviverSubject::handleMessage: "
							"Reviver is %s (Priority %d)\n",
						(char *)srcAddr, priority );
			}
			else
			{
				INFO_MSG( "ReviverSubject::handleMessage: "
							"%s has a better priority (%d)\n",
						(char *)srcAddr, priority );
			}
			accept = true;
		}
		else
		{
			uint64 delta = (currentPingTime - lastPingTime_) * uint64(1000);
			delta /= stampsPerSecond();	// careful - don't overflow the uint64
			int msBetweenPings = int(delta);

			if (msBetweenPings > msTimeout_)
			{
				std::string oldAddr = (char *)reviverAddr_;
				INFO_MSG( "ReviverSubject::handleMessage: "
								"%s timed out (%d ms). Now using %s\n",
							oldAddr.c_str(), msBetweenPings, (char *)srcAddr );
				accept = true;
			}
		}
	}


	Mercury::Bundle bundle;
	bundle.startReply( header.replyID );
	if (accept)
	{
		reviverAddr_ = srcAddr;
		lastPingTime_ = currentPingTime;
		priority_ = priority;
		bundle << REVIVER_PING_YES;
	}
	else
	{
		bundle << REVIVER_PING_NO;
	}
	pNub_->send( srcAddr, bundle );
}

// reviver_subject.cpp
