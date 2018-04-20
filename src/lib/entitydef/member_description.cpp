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
 * 	@file description.cpp
 *
 *	This file provides a common base class for descriptions, mainly for stats.
 *
 *	@ingroup entity
 */
#include "pch.hpp"
#include "cstdmf/watcher.hpp"
#include "entitydef/member_description.hpp"

MemberDescription::MemberDescription()
#if ENABLE_WATCHERS
	:
	sentToOwnClient_( 0 ),	
	sentToOtherClients_( 0 ),
	addedToHistoryQueue_( 0 ),
	sentToGhosts_( 0 ),
	sentToBase_( 0 ),
	received_( 0 ),
	bytesSentToOwnClient_( 0 ),
	bytesSentToOtherClients_( 0 ),
	bytesAddedToHistoryQueue_( 0 ),
	bytesSentToGhosts_( 0 ),
	bytesSentToBase_( 0 ),
	bytesReceived_( 0 )
#endif
{
}

#if ENABLE_WATCHERS
WatcherPtr MemberDescription::pWatcher()
{
	static WatcherPtr watchMe = NULL;

	if (!watchMe)
	{
		watchMe = new DirectoryWatcher();
		MemberDescription * pNull = NULL;

		watchMe->addChild( "messagesSentToOwnClient", 
						   makeWatcher( pNull->sentToOwnClient_ ));
		watchMe->addChild( "messagesSentToOtherClients", 
						   makeWatcher( pNull->sentToOtherClients_ ));
		watchMe->addChild( "messagesAddedToHistoryQueue", 
						   makeWatcher( pNull->addedToHistoryQueue_ ));
		watchMe->addChild( "messagesSentToGhosts", 
						   makeWatcher( pNull->sentToGhosts_ ));
		watchMe->addChild( "messagesSentToBase", 
						   makeWatcher( pNull->sentToBase_ ));
		watchMe->addChild( "bytesSentToOwnClient", 
						   makeWatcher( pNull->bytesSentToOwnClient_ ));
		watchMe->addChild( "messagesReceived", 
						   makeWatcher( pNull->received_ ));
		watchMe->addChild( "bytesSentToOtherClients", 
						   makeWatcher( pNull->bytesSentToOtherClients_ ));
		watchMe->addChild( "bytesAddedToHistoryQueue", 
						   makeWatcher( pNull->bytesAddedToHistoryQueue_ ));
		watchMe->addChild( "bytesSentToGhosts", 
						   makeWatcher( pNull->bytesSentToGhosts_ ));
		watchMe->addChild( "bytesSentToBase", 
						   makeWatcher( pNull->bytesSentToBase_ ));
		watchMe->addChild( "bytesReceived", 
						   makeWatcher( pNull->bytesReceived_ ));
	}
	return watchMe;
}

#endif
