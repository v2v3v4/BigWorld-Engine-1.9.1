/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ADD_TO_MANAGER_HELPER
#define ADD_TO_MANAGER_HELPER

#include "network/interfaces.hpp"
#include "network/nub.hpp"


/**
 *  This class is used by CellApps and BaseApps to add themselves to their
 *  managers.  It handles the replies from the manager process and handles
 *  sending and resending the add message until it succeeds.
 *
 *  This object deletes itself.
 */
class AddToManagerHelper :
	public Mercury::ReplyMessageHandler,
	public Mercury::TimerExpiryHandler
{
public:
	AddToManagerHelper( Mercury::Nub & nub );


	/**
	 *  Derived classes must implement this method to send the add message to
	 *  the manager.
	 */
	virtual void send() = 0;


	/**
	 *  This method is called when a non-empty reply is received from the
	 *  manager.  The data on the stream should be the *AppInitData struct for
	 *  this app pair.
	 */
	virtual bool finishInit( BinaryIStream & data ) = 0;

private:
	void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );


	void handleException( const Mercury::NubException & exception, void * arg );
	int handleTimeout( Mercury::TimerID timerID, void * arg );

	/// The nub that's being used for comms.
	Mercury::Nub & nub_;
};

#endif // ADD_TO_MANAGER_HELPER
