/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cstdmf/config.hpp"
#if ENABLE_WATCHERS

#include "network/endpoint.hpp"

#include "network/portmap.hpp"
#include "network/machine_guard.hpp"
#include "network/watcher_nub.hpp"
#include "network/watcher_packet_handler.hpp"
#include "network/mercury.hpp"
#include "network/misc.hpp"

#include "cstdmf/config.hpp"
#include "cstdmf/memory_counter.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 )

memoryCounterDefine( watcher, Base );


/**
 * 	This is the constructor.
 */
WatcherNub::WatcherNub() :
	id_(-1),
	registered_(false),
	wrh_(NULL),
	insideReceiveRequest_(false),
	requestPacket_(new char[WN_PACKET_SIZE]),
	isInitialised_( false ),
	socket_( /* useSyncHijack */ false )
{
	memoryCounterAdd( watcher );
	memoryClaim( requestPacket_ );
}


/**
 *	This method initialises the watcher nub.
 */
bool WatcherNub::init( const char * listeningInterface, uint16 listeningPort )
{
	INFO_MSG( "WatcherNub::init: listeningInterface = '%s', listeningPort = "
			"%hd\n", listeningInterface ? listeningInterface : "", listeningPort );
	if (isInitialised_)
	{
		// WARNING_MSG( "WatcherNub::init: Already initialised.\n" );
		return true;
	}

	isInitialised_ = true;

	// open the socket
	socket_.socket(SOCK_DGRAM);
	if(!socket_.good())
	{
		ERROR_MSG( "WatcherNub::init: socket() failed\n" );
		return false;
	}

	if (socket_.setnonblocking(true))	// make it nonblocking
	{
		ERROR_MSG( "WatcherNub::init: fcntl(O_NONBLOCK) failed\n" );
		return false;
	}

	u_int32_t ifaddr = INADDR_ANY;
#ifndef _WIN32
	// If the interface resolves to an address, use that instead of
	// searching for a matching interface.
	if (inet_aton( listeningInterface, (struct in_addr *)&ifaddr ) == 0)
#endif
	{
		// ask endpoint to parse the interface specification into a name
		char ifname[IFNAMSIZ];
		bool listeningInterfaceEmpty =
			(listeningInterface == NULL || listeningInterface[0] == 0);
		if (socket_.findIndicatedInterface( listeningInterface, ifname ) == 0)
		{
			INFO_MSG( "WatcherNub::init: creating on interface '%s' (= %s)\n",
				listeningInterface, ifname );
			if (socket_.getInterfaceAddress( ifname, ifaddr ) != 0)
			{
				WARNING_MSG( "WatcherNub::init: couldn't get addr of interface %s "
					"so using all interfaces\n", ifname );
			}
		}
		else if (!listeningInterfaceEmpty)
		{
			WARNING_MSG( "WatcherNub::init: couldn't parse interface spec %s "
				"so using all interfaces\n", listeningInterface );
		}
	}

	if (socket_.bind( listeningPort, ifaddr ))
	{
		ERROR_MSG( "WatcherNub::init: bind() failed\n" );
		socket_.close();
		return false;
	}

	return true;
}


/**
 * 	This is the destructor.
 */
WatcherNub::~WatcherNub()
{
	memoryCounterSub( watcher );

	if (registered_)
	{
		this->deregisterWatcher();
	}

	if (socket_.good())
	{
		socket_.close();
	}

	if (requestPacket_ != NULL)
	{
		memoryClaim( requestPacket_ );
		delete [] requestPacket_;
		requestPacket_ = NULL;
	}
}


/**
 * 	This method broadcasts a watcher register message for this watcher.
 *
 * 	@param id			Numeric id for this watcher.
 * 	@param abrv			Short name for this watcher.
 * 	@param longName		Long name for this watcher.
 * 	@param listeningInterface	The name of the network interface to listen on.
 * 	@param listeningPort		The port to listen on.
 */
int WatcherNub::registerWatcher( int id, const char *abrv, const char *longName,
	   const char * listeningInterface, uint16 listeningPort )
{
	if (!this->init( listeningInterface, listeningPort ))
	{
		ERROR_MSG( "WatcherNub::registerWatcher: init failed.\n" );
		return -1;
	}

	// make sure we're not already registered...
	if (registered_)
		this->deregisterWatcher();

	// set up a few things
	id_ = id;

	strncpy( abrv_, abrv, sizeof(abrv_) );
	abrv_[sizeof(abrv_)-1]=0;

	strncpy( name_, longName, sizeof(name_) );
	name_[sizeof(name_)-1]=0;

	// and go for it
	int ret = this->watcherControlMessage(WATCHER_MSG_REGISTER,true);

	if (ret == 0)
	{
		registered_ = true;
		this->notifyMachineGuard();
	}
	return ret;
}


/**
 * 	This method broadcasts a watcher deregister message for this watcher.
 */
int WatcherNub::deregisterWatcher()
{
	if (!registered_)
		return 0;
	else
	{
		int ret = this->watcherControlMessage( WATCHER_MSG_DEREGISTER, true );
		if (ret == 0)
		{
			registered_ = false;
			this->notifyMachineGuard();
		}

		return ret;
	}
}


/**
 * 	This method sends a message to the machined process.
 */
void WatcherNub::notifyMachineGuard()
{
	u_int16_t port = 0;
	socket_.getlocaladdress( &port, NULL );

	ProcessMessage pm;
	pm.param_ = pm.PARAM_IS_MSGTYPE |
		(registered_ ? pm.REGISTER : pm.DEREGISTER);
	pm.category_ = pm.WATCHER_NUB;
	pm.uid_ = getUserId();
	pm.pid_ = mf_getpid();
	pm.port_ = port;
	pm.id_ = id_;
	pm.name_ = abrv_;

	uint32 destip = htonl( 0x7F000001U );
	int reason;
	if ((reason = pm.sendAndRecv( 0, destip )) != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "Couldn't register watcher nub with machined: %s\n",
			Mercury::reasonToString( (Mercury::Reason)reason ) );
	}
}


/**
 * 	This method broadcasts a watcher flush components message for
 * 	this watcher.
 */
int WatcherNub::resetServer()
{
	if(registered_)
		return -1;
	else
		return this->watcherControlMessage( WATCHER_MSG_FLUSHCOMPONENTS, true );
}


/**
 * 	This method broadcasts a message for this watcher.
 *
 * 	@param message		The message to broadcast.
 * 	@param withid		If true, the id and names are also sent.
 */
int WatcherNub::watcherControlMessage(int message, bool withid)
{
    int ret = 0;

	// turn on the broadcast flag
	if (socket_.setbroadcast(true))
	{
		perror( "WatcherNub::watcherControlMessage: "
                "setsockopt(SO_BROADCAST) failed\n" );
        ret = -1;
    }
    else
    {
        // build the packet
        WatcherRegistrationMsg  wrm;
        wrm.version = 0;
        wrm.uid = getUserId();
        wrm.message = message;

        if(withid)
        {
            wrm.id = id_;
            strcpy(wrm.abrv,abrv_);
            strcpy(wrm.name,name_);
        }
        else
        {
            wrm.id = -1;
            wrm.abrv[0] = 0;
            wrm.name[0] = 0;
        }

        // send the message
        if (socket_.sendto( &wrm,sizeof(wrm),
					htons( PORT_WATCHER ) ) != sizeof(wrm))
        {
            perror( "WatcherNub::watcherControlMessage: sendto failed\n" );
            ret = -1;
        }

        // turn off the broadcast flag
        if (socket_.setbroadcast( false ))
        {
            perror( "WatcherNub::watcherControlMessage: "
                    "setsockopt(-SO_BROADCAST) failed\n" );
            ret = -1;
        }

    }

    return ret;
}


/**
 * 	This method sents the handler to receive events for this watcher.
 *
 * 	@param wrh	The WatcherRequestHandler object to receive events.
 */
void WatcherNub::setRequestHandler( WatcherRequestHandler *wrh )
{
	if (insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::setRequestHandler: "
			"Can't call me while inside receiveRequest!\n" );
		return;
	}

	wrh_ = wrh;
}


/**
 * 	This method returns the UDP socket used by this watcher.
 */
int WatcherNub::getSocketDescriptor()
{
	return socket_;
}


/**
 * 	This method should be called to handle requests on the socket.
 */
bool WatcherNub::receiveRequest()
{
	if (!isInitialised_)
	{
		// TODO: Allow calls to this when not initialised before the client
		// currently does this. Should really fix the client so that this is
		// only called once initialised.
		return false;
	}

	sockaddr_in		senderAddr;
	int				len;

	if (wrh_ == NULL)
	{
		ERROR_MSG( "WatcherNub::receiveRequest: Can't call me before\n"
			"\tcalling setRequestHandler(WatcherRequestHandler*)\n" );
		return false;
	}

	if (insideReceiveRequest_)
	{
		ERROR_MSG( "WatcherNub::receiveRequest: BAD THING NOTICED:\n"
			"\tAttempted re-entrant call to receiveRequest\n" );
		return false;
	}

	insideReceiveRequest_ = true;

	// try to recv
	len = socket_.recvfrom( requestPacket_, WN_PACKET_SIZE, senderAddr );

	if (len == -1)
	{
		// EAGAIN = no packets waiting, ECONNREFUSED = rejected outgoing packet

#ifdef _WIN32
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK && err != WSAECONNREFUSED && err != WSAECONNRESET)
#else
		int err = errno;
		if (err != EAGAIN && err != ECONNREFUSED)
#endif
		{
			ERROR_MSG( "WatcherNub::receiveRequest: recvfrom failed\n" );
		}

		insideReceiveRequest_ = false;
		return false;
	}

	// make sure we haven't picked up a weird packet (from when broadcast
	// was on, say ... hey, it could happen!)
	WatcherDataMsg * wdm = (WatcherDataMsg*)requestPacket_;

	if (len < (int)sizeof(WatcherDataMsg))
	{
		ERROR_MSG( "WatcherNub::receiveRequest: Packet is too short\n" );
		insideReceiveRequest_ = false;
		return false;
	}

	if (! (wdm->message == WATCHER_MSG_GET ||
		   wdm->message == WATCHER_MSG_GET_WITH_DESC ||
		   wdm->message == WATCHER_MSG_SET ||
		   wdm->message == WATCHER_MSG_GET2 ||
		   wdm->message == WATCHER_MSG_SET2
		) )
	{
		wrh_->processExtensionMessage( wdm->message,
								requestPacket_ + sizeof( wdm->message ),
								len - sizeof( wdm->message ),
								Mercury::Address( senderAddr.sin_addr.s_addr,
									senderAddr.sin_port ) );
		insideReceiveRequest_ = false;
		return true;
	}


	// Our reply handler for the current request.
	WatcherPacketHandler *packetHandler = NULL;

	// and call back to the program
	switch (wdm->message)
	{
		case WATCHER_MSG_GET:
		case WATCHER_MSG_GET_WITH_DESC:
		{
			// Create the packet reply handler for the current incoming request
			packetHandler = new WatcherPacketHandler( socket_, senderAddr,
					wdm->count, WatcherPacketHandler::WP_VERSION_1, false );

			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				wrh_->processWatcherGetRequest( *packetHandler, astr,
					   	(wdm->message == WATCHER_MSG_GET_WITH_DESC) );
				astr += strlen(astr)+1;
			}
		}
		break;

		case WATCHER_MSG_GET2:
		{
			// Create the packet reply handler for the current incoming request
			packetHandler = new WatcherPacketHandler( socket_, senderAddr,
					wdm->count, WatcherPacketHandler::WP_VERSION_2, false );

			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				unsigned int & seqNum = (unsigned int &)*astr;
				astr += sizeof(unsigned int);
				wrh_->processWatcherGet2Request( *packetHandler, astr, seqNum );
				astr += strlen(astr)+1;
			}
		}
		break;


		case WATCHER_MSG_SET:
		{
			// Create the packet reply handler for the current incoming request
			packetHandler = new WatcherPacketHandler( socket_, senderAddr,
					wdm->count, WatcherPacketHandler::WP_VERSION_1, true );

			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				char	*bstr = astr + strlen(astr)+1;
				wrh_->processWatcherSetRequest( *packetHandler, astr,bstr );
				astr = bstr + strlen(bstr)+1;
			}
		}
		break;


		case WATCHER_MSG_SET2:
		{
			// Create the packet reply handler for the current incoming request
			packetHandler = new WatcherPacketHandler( socket_, senderAddr,
					wdm->count, WatcherPacketHandler::WP_VERSION_2, true );

			char	*astr = wdm->string;
			for(int i=0;i<wdm->count;i++)
			{
				wrh_->processWatcherSet2Request( *packetHandler, astr );
			}
		}
		break;

		default:
		{
			Mercury::Address srcAddr( senderAddr.sin_addr.s_addr,
									senderAddr.sin_port );
			WARNING_MSG( "WatcherNub::receiveRequest: "
						"Unknown message %d from %s\n",
					wdm->message, srcAddr.c_str() );
		}
		break;
	}

	// Start running the packet handler now, it will delete itself when
	// complete.
	if (packetHandler)
	{
		packetHandler->run();
		packetHandler = NULL;
	}

	// and we're done!
	insideReceiveRequest_ = false;

	return true;
}



// -----------------------------------------------------------------------------
// Section: WatcherRequestHandler
// -----------------------------------------------------------------------------

/**
 *	This virtual method handles any messages that the watcher nub does not know
 *	how to handle.
 */
void WatcherRequestHandler::processExtensionMessage( int messageID,
				char * data, int dataLen, const Mercury::Address & addr )
{
	ERROR_MSG( "WatcherRequestHandler::processExtensionMessage: "
							"Unknown message %d from %s. Message len = %d\n",
						messageID, (char *)addr, dataLen );
}


// -----------------------------------------------------------------------------
// Section: StandardWatcherRequestHandler
// -----------------------------------------------------------------------------

StandardWatcherRequestHandler::StandardWatcherRequestHandler(
		WatcherNub & nub ) : nub_( nub )
{
}


/**
 * 	This method handles watcher get requests.
 *
 * 	@param path		The path of the watcher request.
 *	@param withDesc	Indicates whether the description should also be returned.
 *  @param packetHandler	The WatcherPacketHandler to use to notify of
 *                          watcher results upon completion of the get
 *                          operation.
 */
void StandardWatcherRequestHandler::processWatcherGetRequest(
	WatcherPacketHandler & packetHandler, const char * path, bool withDesc )
{
#if ENABLE_WATCHERS
	std::string newPath( path );
	WatcherPathRequestV1 *pRequest =
			(WatcherPathRequestV1 *)packetHandler.newRequest( newPath );
	pRequest->useDescription( withDesc );
#endif
}


void StandardWatcherRequestHandler::processWatcherGet2Request(
	WatcherPacketHandler & packetHandler, const char * path, uint32 seqNum )
{
#if ENABLE_WATCHERS
	std::string newPath( path );
	WatcherPathRequestV2 *pRequest = (WatcherPathRequestV2 *)
								packetHandler.newRequest( newPath );
	pRequest->setSequenceNumber( seqNum );
#endif
}

/**
 * 	This method handles watcher set requests.
 *
 * 	@param path			The path of the watcher object.
 * 	@param valueString	The value to which it should be set.
 *  @param packetHandler	The WatcherPacketHandler to use to notify of
 *                          watcher results upon completion of the set.
 */
void StandardWatcherRequestHandler::processWatcherSetRequest(
		WatcherPacketHandler & packetHandler, const char * path,
		const char * valueString )
{
#if ENABLE_WATCHERS
	std::string newPath( path );
	WatcherPathRequestV1 *pRequest =
			(WatcherPathRequestV1 *)packetHandler.newRequest( newPath );

	pRequest->setValueData( valueString );
#endif
}


void StandardWatcherRequestHandler::processWatcherSet2Request(
		WatcherPacketHandler & packetHandler, char* & packet )
{
#if ENABLE_WATCHERS
/* TODO: cleanup the passing in of packet as a reference here */
	uint32 & seqNum = (uint32 &)*packet;
	const char *path = packet + sizeof(uint32);
	char *curr = (char *)path + strlen(path)+1;

	// Determine the size of the contained data
	// then add the size of the prefixed information;
	// ie: type (1 byte), size data (1 or 4 bytes)
	//
	// Structure of stream pointed to by curr: <type> <data size> <data>.
	// If first byte after <type> is 0xff, then <data size> is packed in the
	// next 3 bytes.  Otherwise, this byte is the data size.
	// Also see pycommon/watcher_data_type.py.
	uint32 size = 0;
	uint8 sizeHint = (uint8)*(curr+1); // Skips "type" byte.
	if (sizeHint == 0xff)
	{
		// Skips "type" and "sizeHint" bytes.
		size = BW_UNPACK3( (curr+2) );
		size += 5;  // Size of prefixed information.
	}
	else
	{
		size = sizeHint + 2;
	}

	// Construct the path request handler
	std::string newPath( path );

	WatcherPathRequestV2 *pRequest = (WatcherPathRequestV2 *)
								packetHandler.newRequest( newPath );
	pRequest->setSequenceNumber( seqNum );

	// Notify the path request of the outgoing data stream
	pRequest->setPacketData( size, curr );

	// Update where packet is pointing to for the next loop
	packet = curr + size;
#endif
}

#endif /* ENABLE_WATCHERS */

// watcher_nub.cpp
