/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "add_to_manager_helper.hpp"

#include "network/nub.hpp"

/**
 *  Constructor.
 */
AddToManagerHelper::AddToManagerHelper( Mercury::Nub & nub ) :
	nub_( nub )
{}


/**
 *  This method handles the reply from the manager process.  Zero-length replies
 *  mean that the manager is not ready to add child apps at the moment and we
 *  should wait and try again later.
 */
void AddToManagerHelper::handleMessage(
	const Mercury::Address & source,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * arg )
{
	if (data.remainingLength())
	{
		if (!this->finishInit( data ))
		{
			ERROR_MSG( "AddToManagerHelper::handleMessage: "
				"finishInit() failed, aborting\n" );

			nub_.breakProcessing();
		}

		delete this;
	}
	else
	{
		nub_.registerCallback( 1000000, this );
	}
}


/**
 *  This method handles a reply timeout, which means that this app couldn't add
 *  itself to the manager and should bail out.
 */
void AddToManagerHelper::handleException(
	const Mercury::NubException & exception,
	void * arg )
{
	ERROR_MSG( "AddToManagerHelper::handleException: "
		"Failed to add ourselves to the manager (%s)\n",
		Mercury::reasonToString( exception.reason() ) );

	nub_.breakProcessing();
	delete this;
}


/**
 *  This method handles a callback timeout, which means it's time to send
 *  another add message to the manager.
 */
int AddToManagerHelper::handleTimeout( Mercury::TimerID timerID, void * arg )
{
	this->send();
	return 0;
}

// add_to_manager_helper.cpp
