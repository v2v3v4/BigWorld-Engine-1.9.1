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

#include "nub.hpp"

#ifndef CODE_INLINE
#include "nub.ipp"
#endif

#include "bundle.hpp"
#include "channel.hpp"
#include "interface_minder.hpp"
#include "cstdmf/memory_tracker.hpp"
#include "cstdmf/memory_stream.hpp"
#include "mercury.hpp"

#include "cstdmf/config.hpp"
#include "cstdmf/concurrency.hpp"
#include "cstdmf/profile.hpp"

#include <sstream>

#ifdef PLAYSTATION3
#include <netex/errno.h>
#define unix
#undef EAGAIN
#define EAGAIN SYS_NET_EAGAIN
#define ECONNREFUSED SYS_NET_ECONNREFUSED
#define EHOSTUNREACH SYS_NET_EHOSTUNREACH
#define ENOBUFS SYS_NET_ENOBUFS
#define select socketselect
#undef errno
#define errno sys_net_errno
#endif

DECLARE_DEBUG_COMPONENT2( "Network", 0 );

namespace Mercury
{

const char *Mercury::Nub::USE_BWMACHINED = "bwmachined";

// -----------------------------------------------------------------------------
// Section: Nub
// -----------------------------------------------------------------------------

/**
 *	The minimum time that an exception can be reported from when it was first
 *	reported.
 */
const uint Nub::ERROR_REPORT_MIN_PERIOD_MS = 2000; // 2 seconds

/**
 *	The nominal maximum time that a report count for a Mercury address and
 *	error is kept after the last raising of the error.
 */
const uint Nub::ERROR_REPORT_COUNT_MAX_LIFETIME_MS = 10000; // 10 seconds

/**
 *  How much receive buffer we want for sockets.
 */
const int Nub::RECV_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB

/**
 * 	This is the constructor. It initialises the socket, and
 * 	establishes the default internal Nub interfaces.
 *
 * 	@param listeningPort		Port to listen on in network byte order.
 * 	@param listeningInterface	Interface name/IP/netmask to listen on.
 */
Nub::Nub( uint16 listeningPort, const char * listeningInterface ) :
	socket_( /* useSyncHijack */ false ),
	interfaceName_(),
	interfaceID_( 0 ),
	interfaceTable_( 256 ),
	timerQueue_(),
	pCurrentTimer_( NULL ),
	artificialDropPerMillion_( 0 ),
	artificialLatencyMin_( 0 ),
	artificialLatencyMax_( 0 ),
	dropNextSend_( false ),
	nextReplyID_( (uint32(timestamp())%100000) + 10101 ),
	nextSequenceID_( 1 ),
	nextPacket_( NULL ),
	clearFragmentedBundlesTimerID_( TIMER_ID_NONE ),
	breakProcessing_( false ),
	drainSocketInput_( false ),
	fdLargest_( -1 ),
	fdWriteCount_( 0 ),
	pBundleFinishHandler_( NULL ),
	pPacketMonitor_( NULL ),
	channelMap_(),
	pChannelFinder_( NULL ),
	lastStatisticsGathered_( 0 ),
	lastTxQueueSize_( 0 ),
	lastRxQueueSize_( 0 ),
	maxTxQueueSize_( 0 ),
	maxRxQueueSize_( 0 ),
	numBytesSent_( 0 ),
	numBytesResent_( 0 ),
	numBytesReceived_( 0 ),
	numPacketsSent_( 0 ),
	numPacketsResent_( 0 ),
	numPiggybacks_( 0 ),
	numPacketsSentOffChannel_( 0 ),
	numPacketsReceived_( 0 ),
	numDuplicatePacketsReceived_( 0 ),
	numPacketsReceivedOffChannel_( 0 ),
	numBundlesSent_( 0 ),
	numBundlesReceived_( 0 ),
	numMessagesSent_( 0 ),
	numReliableMessagesSent_( 0 ),
	numMessagesReceived_( 0 ),
	numOverheadBytesReceived_( 0 ),
	numFailedPacketSend_( 0 ),
	numFailedBundleSend_( 0 ),
	numCorruptedPacketsReceived_( 0 ),
	numCorruptedBundlesReceived_( 0 ),
	lastNumBytesSent_( 0 ),
	lastNumBytesReceived_( 0 ),
	lastNumPacketsSent_( 0 ),
	lastNumPacketsReceived_( 0 ),
	lastNumBundlesSent_( 0 ),
	lastNumBundlesReceived_( 0 ),
	lastNumMessagesSent_( 0 ),
	lastNumMessagesReceived_( 0 ),
	spareTime_( 0 ),
	accSpareTime_( 0 ),
	oldSpareTime_( 0 ),
	totSpareTime_( 0 ),
	pOpportunisticPoller_( 0 ),
	onceOffPacketCleaningTimerID_( 0 ),
	onceOffMaxResends_( DEFAULT_ONCEOFF_MAX_RESENDS ),
	onceOffResendPeriod_( DEFAULT_ONCEOFF_RESEND_PERIOD ),
	isExternal_( false ),
	reportLimitTimerID_( 0 ),
	pMasterNub_( NULL ),
	tickChildNubsTimerID_( TIMER_ID_NONE ),
	shouldUseChecksums_( false ),
	isVerbose_( true ),
	interfaceStatsTimerID_( TIMER_ID_NONE )
{
	// make sure the compiler is using at least 4-byte
	//  padding on this struct (for efficiency reasons)
	// MF_ASSERT( sizeof(InterfaceElement) == 8 );

	// get this expensive call out of the way
	::stampsPerSecond();

	// init all lastVisitTime to now
	startupTime_ = timestamp();
	for (int i = 0; i < LVT_END; i++)
	{
		lastVisitTime_[i] = startupTime_;
	}

	// set up our list of file descriptors
	FD_ZERO( &fdReadSet_ );
	FD_ZERO( &fdWriteSet_ );
	fdLargest_ = -1;

	// This registers the file descriptor and so needs to be done after
	// initialising fdReadSet_ etc.
	this->recreateListeningSocket( listeningPort, listeningInterface );

	// and put ourselves in as the reply handler
	this->serveInterfaceElement( InterfaceElement::REPLY,
		InterfaceElement::REPLY.id(), this );

	// always have a packet handy
	nextPacket_ = new Packet();
	memset( loopStats_, 0, sizeof(loopStats_) );

	// report any pending exceptions every so often
	reportLimitTimerID_ = this->registerTimer( ERROR_REPORT_MIN_PERIOD_MS * 1000,
			this, NULL );

	// Clear stale incomplete fragmented bundles every so often.
	clearFragmentedBundlesTimerID_ = this->registerTimer(
		FragmentedBundle::MAX_AGE * 1000000, this, NULL );

	interfaceStatsTimerID_ = this->registerTimer( 1000000, this, NULL );
}


/**
 * 	This is the destructor. It closes the socket, discards all
 * 	packets, and sends out a MachineGuardMessage to deregister
 * 	this component with any watchers.
 */
Nub::~Nub()
{
	// We do not want to trigger any handler exceptions here so delete them all.
	if (!replyHandlerMap_.empty())
	{
		INFO_MSG( "Nub::~Nub: Num pending reply handlers = %zu\n",
				replyHandlerMap_.size() );

		ReplyHandlerMap::iterator iter = replyHandlerMap_.begin();

		while (iter != replyHandlerMap_.end())
		{
			delete iter->second;
			++iter;
		}

		replyHandlerMap_.clear();
	}

	// Delete any channels that the nub owns.
	ChannelMap::iterator iter = channelMap_.begin();
	while (iter != channelMap_.end())
	{
		ChannelMap::iterator oldIter = iter++;
		Channel * pChannel = oldIter->second;

		if (pChannel->isOwnedByNub())
		{
			// delete pChannel;
			pChannel->destroy();
		}
		else
		{
			WARNING_MSG( "Nub::~Nub: "
					"Channel to %s is still registered\n",
				pChannel->c_str() );
		}
	}

	this->deregisterWithMachined();

	// Deregister this nub from its master nub if it has one
	if (pMasterNub_)
	{
		pMasterNub_->deregisterChildNub( this );
	}

	// close the socket
	if (socket_.good())
	{
		socket_.close();
	}

	if (onceOffPacketCleaningTimerID_)
	{
		this->cancelTimer( onceOffPacketCleaningTimerID_ );
		onceOffPacketCleaningTimerID_ = 0;
	}

	this->reportPendingExceptions();
	if (reportLimitTimerID_)
	{
		this->cancelTimer( reportLimitTimerID_ );
		reportLimitTimerID_ = 0;
	}

	while (!timerQueue_.empty())
	{
		TimerQueueElement * tqe = timerQueue_.top();
		timerQueue_.pop();

		this->cancelTimer( tqe );
		this->finishProcessingTimerEvent( tqe );
	}
}


Nub::QueryInterfaceHandler::QueryInterfaceHandler( int requestType ) :
	hasResponded_( false ),
	request_( requestType )
{
}


bool Nub::QueryInterfaceHandler::onQueryInterfaceMessage(
	QueryInterfaceMessage &qim, uint32 addr )
{
	address_ = qim.address_;
	hasResponded_ = true;

	return false;
}


bool Nub::queryMachinedInterface( u_int32_t & addr )
{
	Endpoint ep;
	u_int32_t ifaddr;

	ep.socket( SOCK_DGRAM );

	// First we have to send a message over the local interface to
	// bwmachined to ask for which interface to treat as 'internal'.
	if (ep.getInterfaceAddress( "lo", ifaddr ) != 0)
	{
		WARNING_MSG( "Nub::queryMachinedInterface: "
			"Could not get 'lo' by name, defaulting to 127.0.0.1\n" );
		ifaddr = LOCALHOST;
	}

	QueryInterfaceMessage qim;
	QueryInterfaceHandler qih( QueryInterfaceMessage::INTERNAL );

	if (REASON_SUCCESS != qim.sendAndRecv( ep, ifaddr, &qih ))
	{
		ERROR_MSG( "Nub::queryMachinedInterface: "
			"Failed to send interface discovery message to bwmachined.\n" );
		return false;
	}

	if (qih.hasResponded_)
	{
		addr = qih.address_;
		return true;
	}

	return false;
}

/**
 *	This method throws away the existing socket and attempts to create a new one
 *	with the given parameters. The interfaces served by a nub are re-registered
 *	on the new socket if successful.
 *
 *	@param listeningPort		listeningPort as in Nub::Nub
 *	@param listeningInterface	listeningInterface as in Nub::Nub
 *	@return	true if successful, otherwise false.
 */
bool Nub::recreateListeningSocket( uint16 listeningPort,
	const char * listeningInterface )
{
	// first unregister any existing interfaces.
	if (socket_.good())
	{
		this->deregisterWithMachined();

		this->deregisterFileDescriptor( socket_ );
		socket_.close();
		socket_.detach();	// in case close failed
	}


// 	TRACE_MSG( "Mercury::Nub:recreateListeningSocket: %s\n",
// 		listeningInterface ? listeningInterface : "NULL" );

	// clear this unless it gets set otherwise
	advertisedAddress_.ip = 0;
	advertisedAddress_.port = 0;
	advertisedAddress_.salt = 0;

	// make the socket
	socket_.socket( SOCK_DGRAM );

	if (!socket_.good())
	{
		ERROR_MSG( "Mercury::Nub::Nub: couldn't create a socket\n" );
		return false;
	}

	this->registerFileDescriptor( socket_, NULL );

	// ask endpoint to parse the interface specification into a name
	char ifname[IFNAMSIZ];
	u_int32_t ifaddr = INADDR_ANY;
	bool listeningInterfaceEmpty =
		(listeningInterface == NULL || listeningInterface[0] == 0);

	// Query bwmachined over the local interface (dev: lo) for what it
	// believes the internal interface is.
	if (listeningInterface &&
		(strcmp(listeningInterface, USE_BWMACHINED) == 0))
	{
		INFO_MSG( "Nub::Nub: Querying BWMachined for interface\n" );
		if (!this->queryMachinedInterface( ifaddr ))
		{
			WARNING_MSG( "Nub::Nub: No address received from machined so "
					"binding to all interfaces.\n" );
		}
	}
	else if (socket_.findIndicatedInterface( listeningInterface, ifname ) == 0)
	{
		INFO_MSG( "Nub::Nub: creating on interface '%s' (= %s)\n",
			listeningInterface, ifname );
		if (socket_.getInterfaceAddress( ifname, ifaddr ) != 0)
		{
			WARNING_MSG( "Mercury::Nub::Nub: couldn't get addr of interface %s "
				"so using all interfaces\n", ifname );
		}
	}
	else if (!listeningInterfaceEmpty)
	{
		WARNING_MSG( "Mercury::Nub::Nub: couldn't parse interface spec '%s' "
			"so using all interfaces\n", listeningInterface );
	}

	// now we know where to bind, so do so
	if (socket_.bind( listeningPort, ifaddr ) != 0)
	{
		ERROR_MSG( "Mercury::Nub::Nub: couldn't bind the socket to %s (%s)\n",
			(char*)Address( ifaddr, listeningPort ), strerror( errno ) );
		socket_.close();
		socket_.detach();
		return false;
	}

	// but for advertising it ask the socket for where it thinks it's bound
	socket_.getlocaladdress( (u_int16_t*)&advertisedAddress_.port,
		(u_int32_t*)&advertisedAddress_.ip );

	if (advertisedAddress_.ip == 0)
	{
		// we're on INADDR_ANY, report the address of the
		//  interface used by the default route then
		if (socket_.findDefaultInterface( ifname ) != 0 ||
			socket_.getInterfaceAddress( ifname,
				(u_int32_t&)advertisedAddress_.ip ) != 0)
		{
			ERROR_MSG( "Mercury::Nub::Nub: "
				"couldn't determine ip addr of default interface\n" );

			socket_.close();
			socket_.detach();
			return false;
		}

		INFO_MSG( "Mercury::Nub::Nub: bound to all interfaces "
				"but advertising only %s ( %s )\n",
			ifname, (char*)advertisedAddress_ );
	}

	INFO_MSG( "Nub::recreateListeningSocket: Advertised address %s\n",
		(char*)advertisedAddress_ );

	socket_.setnonblocking( true );

#if defined( unix ) && !defined( PLAYSTATION3 )
	int recverrs = true;
	setsockopt( socket_, SOL_IP, IP_RECVERR, &recverrs, sizeof(int) );
#endif

#ifdef MF_SERVER
	if (!socket_.setBufferSize( SO_RCVBUF, RECV_BUFFER_SIZE ))
	{
		WARNING_MSG( "Nub::Nub: Operating with a receive buffer of "
			"only %d bytes (instead of %d)\n",
			socket_.getBufferSize( SO_RCVBUF ), RECV_BUFFER_SIZE );
	}
#endif

	if (!interfaceName_.empty())
	{
		this->registerWithMachined( interfaceName_, interfaceID_ );
	}

	return true;
}


/**
 *  This method registers the interface element as the handler for the given
 *  message ID on this Nub.
 */
void Nub::serveInterfaceElement( const InterfaceElement & ie, MessageID id,
	InputMessageHandler * pHandler )
{
	InterfaceElement & element = interfaceTable_[ id ];
	element	= ie;
	element.pHandler( pHandler );
}


/**
 *  This method registers a callback with machined to be called when a certain
 *	type of process is started.
 *
 *	@note This needs to be fixed up if rebind is called on this nub.
 */
Reason Nub::registerBirthListener( Bundle & bundle, int addrStart,
		const char * ifname )
{
	return this->registerListener( bundle, addrStart, ifname, true );
}


/**
 *  This method registers a callback with machined to be called when a certain
 *	type of process stops unexpectedly.
 *
 *	@note This needs to be fixed up if rebind is called on this nub.
 */
Reason Nub::registerDeathListener( Bundle & bundle, int addrStart,
		const char * ifname )
{
	return this->registerListener( bundle, addrStart, ifname, false );
}


/**
 *	This method registers a callback with machined to be called when a certain
 *	type of process is started.
 *
 *	@param ie		The interface element of the callback message. The message
 *				must be callable with one parameter of type Mercury::Address.
 *	@param ifname	The name of the interface to watch for.
 */
Reason Nub::registerBirthListener( const InterfaceElement & ie,
				const char * ifname )
{
	Mercury::Bundle bundle;

	bundle.startMessage( ie, false );
	int startOfAddress = bundle.size();
	bundle << Mercury::Address::NONE;

	return this->registerBirthListener( bundle, startOfAddress, ifname );
}


/**
 *  This method registers a callback with machined to be called when a certain
 *	type of process stops unexpectedly.
 *
 *	@param ie		The interface element of the callback message. The message
 *				must be callable with one parameter of type Mercury::Address.
 *	@param ifname	The name of the interface to watch for.
 */
Reason Nub::registerDeathListener( const InterfaceElement & ie,
				const char * ifname )
{
	Mercury::Bundle bundle;

	bundle.startMessage( ie, false );
	int startOfAddress = bundle.size();
	bundle << Mercury::Address::NONE;

	return this->registerDeathListener( bundle, startOfAddress, ifname );
}


/**
 *	This method is used to register a birth or death listener with machined.
 */
Reason Nub::registerListener( Bundle & bundle, int addrStart,
		const char * ifname, bool isBirth, bool anyUID )
{
	// finalise the bundle first
	bundle.finalise();
	const Packet * p = bundle.firstPacket_.get();

	MF_ASSERT( p->flags() == 0 );

	// prepare the message for machined
	ListenerMessage lm;
	lm.param_ = (isBirth ? lm.ADD_BIRTH_LISTENER : lm.ADD_DEATH_LISTENER) |
		lm.PARAM_IS_MSGTYPE;
	lm.category_ = lm.SERVER_COMPONENT;
	lm.uid_ = anyUID ? lm.ANY_UID : getUserId();
	lm.pid_ = mf_getpid();
	lm.port_ = this->address().port;
	lm.name_ = ifname;

	const int addrLen = 6;
	unsigned int postSize = p->totalSize() - addrStart - addrLen;

	lm.preAddr_ = std::string( p->data(), addrStart );
	lm.postAddr_ = std::string( p->data() + addrStart + addrLen, postSize );

	uint32 srcip = advertisedAddress_.ip, destip = LOCALHOST;
	return lm.sendAndRecv( srcip, destip, NULL );
}

namespace
{
class FindInterfaceHandler : public MachineGuardMessage::ReplyHandler
{
public:
	FindInterfaceHandler( Address &address ) :
		found_( false ), address_( address ) {}

	virtual bool onProcessStatsMessage( ProcessStatsMessage &psm, uint32 addr )
	{
		if (psm.pid_ != 0)
		{
			address_.ip = addr;
			address_.port = psm.port_;
			address_.salt = 0;
			found_ = true;
			DEBUG_MSG( "Found interface %s at %s:%d\n",
				psm.name_.c_str(), inet_ntoa( (in_addr&)addr ),
				ntohs( address_.port ) );
		}
		return true;
	}

	bool found_;
	Address &address_;
};
}

/**
 * 	This method finds the specified interface on the network.
 * 	WARNING: This function always blocks.
 *
 * 	@return	A Mercury::Reason.
 */
Reason Nub::findInterface( const char * name, int id,
		Address & address, int retries, bool verboseRetry )
{
	ProcessStatsMessage pm;
	pm.param_ = pm.PARAM_USE_CATEGORY |
		pm.PARAM_USE_UID |
		pm.PARAM_USE_NAME |
		(id < 0 ? 0 : pm.PARAM_USE_ID);
	pm.category_ = pm.SERVER_COMPONENT;
	pm.uid_ = getUserId();
	pm.name_ = name;
	pm.id_ = id;

	int attempt = 0;
	FindInterfaceHandler handler( address );

	while (pm.sendAndRecv( 0, BROADCAST, &handler ) == REASON_SUCCESS)
	{
		if (handler.found_)
			return REASON_SUCCESS;

		if (verboseRetry)
		{
			INFO_MSG( "Nub::findInterface: Failed to find %s on attempt %d.\n",
				name, attempt );
		}

		if (attempt++ >= retries)
			break;

		// Sleep a little because sendAndReceiveMGM() is too fast now! :)
#if defined( PLAYSTATION3 )
		sys_timer_sleep( 1 );
#elif !defined( _WIN32 )
		sleep( 1 );
#else
		Sleep( 1000 );
#endif
	}

	return REASON_TIMER_EXPIRED;
}

/**
 *	This private method finishes processing a timer event. It is used by
 *	processPendingEvents.
 */
void Nub::finishProcessingTimerEvent( TimerQueueElement * pElement )
{
	pCurrentTimer_ = NULL;

	// handleTimeout could well cancel it, so we have another if
	if (pElement->state == TimerQueueElement::STATE_CANCELLED)
	{
		// this is the only place that deletes
		// TimerQueueElements - i.e. after they get
		// off the top of the queue, and only in
		// STATE_CANCELLED (=> deleted from timerMap_)
		delete pElement;
	}
	else
	{
		// put it back on the queue
		pElement->deliveryTime += pElement->intervalTime;
		pElement->state = TimerQueueElement::STATE_PENDING;
		timerQueue_.push( pElement );

		loopStats_[TIMER_RESCHEDS]++;
	}

	loopStats_[TIMER_CALLS]++;	// not 100% accurate but in the right spirit
}


/**
 *	This method processes network events, and calls user callbacks.  Call this
 *	function with the resolution you want for resends and event processing. This
 *	function may call back into your InputMessageHandlers (and hopefully will!)
 *
 *	@throw NubException
 *	@return True if a packet was received.
 *	@see processContinuously
 */
bool Nub::processPendingEvents( bool expectingPacket )
{
	this->sendDelayedChannels();

	// call any expired timers (if there isn't a packet there)
	while ((!timerQueue_.empty()) &&
		(timerQueue_.top()->deliveryTime <= timestamp() ||
			timerQueue_.top()->state == TimerQueueElement::STATE_CANCELLED) &&
		!drainSocketInput_)	// line above only to keep queue small
	{
		TimerQueueElement * tqe = timerQueue_.top();

		timerQueue_.pop();
		if (tqe->state != TimerQueueElement::STATE_CANCELLED)
		{
			tqe->state = TimerQueueElement::STATE_EXECUTING;

			try
			{
				// The current timer is stored so that cancelTimers can check
				// whether the current timer should be cancelled.
				MF_ASSERT( pCurrentTimer_ == NULL );
				pCurrentTimer_ = tqe;

				tqe->handler->handleTimeout( tqe, tqe->arg );
			}
			catch (...)
			{
				// if it's not going to repeat, cancel it
				if (tqe->intervalTime == 0)
					this->cancelTimer( tqe );

				this->finishProcessingTimerEvent( tqe );

				throw;
			}

			// if it's not going to repeat, cancel it
			if (tqe->intervalTime == 0)
				this->cancelTimer( tqe );
		}

		this->finishProcessingTimerEvent( tqe );
	}

	// gather statistics if we haven't for a while
	if (timestamp() - lastStatisticsGathered_ >= stampsPerSecond())
	{
		socket_.getQueueSizes( lastTxQueueSize_, lastRxQueueSize_ );

		// Warn if the buffers are getting fuller
		if (lastTxQueueSize_ > maxTxQueueSize_ && lastTxQueueSize_ > 128*1024)
			WARNING_MSG( "Transmit queue peaked at new max (%d bytes)\n",
				lastTxQueueSize_ );
		if (lastRxQueueSize_ > maxRxQueueSize_ && lastRxQueueSize_ > 1024*1024)
			WARNING_MSG( "Receive queue peaked at new max (%d bytes)\n",
				lastRxQueueSize_ );

		maxTxQueueSize_ = std::max( lastTxQueueSize_, maxTxQueueSize_ );
		maxRxQueueSize_ = std::max( lastRxQueueSize_, maxRxQueueSize_ );

		oldSpareTime_ = totSpareTime_;
		totSpareTime_ = accSpareTime_ + spareTime_;

		lastStatisticsGathered_ = timestamp();
	}

	recvMercuryTimer_.start();
	recvSystemTimer_.start();

	loopStats_[RECV_TRYS]++;

	// try a recvfrom
	Address	srcAddr;
	int len = nextPacket_->recvFromEndpoint( socket_, srcAddr );

	recvSystemTimer_.stop( len > 0 );

	// Successful receive
	if (len > 0)
	{
		loopStats_[RECV_GETS]++;

		numPacketsReceived_++;

		numBytesReceived_ += len + UDP_OVERHEAD;
		// Payload subtracted later
		numOverheadBytesReceived_ += len + UDP_OVERHEAD;

		// process it if it succeeded
		PacketPtr curPacket = nextPacket_;
		nextPacket_ = new Packet();

		// We set the message end offset to the end of the packet here, and
		// processFilteredPacket() and processOrderedPacket() will move it
		// backwards and increase footerSize_ as they strip footers.
		curPacket->msgEndOffset( len );

		Reason ret = this->processPacket( srcAddr, curPacket.get() );

		recvMercuryTimer_.stop( true );

		if (ret != REASON_SUCCESS)
		{
			throw NubExceptionWithAddress( ret, srcAddr );
		}

		return true;
	}

	// Socket error
	else
	{
		// we don't try to measure this cost... (too many returns!)
		recvMercuryTimer_.stop( false );

		// is len weird?
		if (len == 0 )
		{
			loopStats_[RECV_ERRS]++;

			WARNING_MSG( "Nub::processPendingEvents: "
				"Throwing REASON_GENERAL_NETWORK (1)- %s\n",
				strerror( errno ) );

			throw NubException(REASON_GENERAL_NETWORK);
		}
			// I'm not quite sure what it means if len is 0
			// (0 => 'end of file', but with dgram sockets?)

#ifdef _WIN32
        DWORD wsaErr = WSAGetLastError();
#endif //def _WIN32

		// is the buffer empty?
		if (
#ifdef _WIN32
			wsaErr == WSAEWOULDBLOCK
#else
			errno == EAGAIN && !expectingPacket
#endif
			)
		{
			return false;
		}

		loopStats_[RECV_ERRS]++;

#ifdef unix
		// is it telling us there's an error?
		if (errno == EAGAIN ||
			errno == ECONNREFUSED ||
			errno == EHOSTUNREACH)
		{
#if defined( PLAYSTATION3 )
			throw NubException( REASON_NO_SUCH_PORT );
#else
			Mercury::Address offender;

			if (socket_.getClosedPort( offender ))
			{
				// If we got a NO_SUCH_PORT error and there is an internal
				// channel to this address, mark it as remote failed.  The logic
				// for dropping external channels that get NO_SUCH_PORT
				// exceptions is built into BaseApp::onClientNoSuchPort().
				if (errno == ECONNREFUSED)
				{
					Channel * pDeadChannel = this->findChannel( offender );

					if (pDeadChannel && pDeadChannel->isInternal())
					{
						INFO_MSG( "Nub::processPendingEvents: "
							"Marking channel to %s as dead (%s)\n",
							pDeadChannel->c_str(),
							reasonToString( REASON_NO_SUCH_PORT ) );

						pDeadChannel->hasRemoteFailed( true );
					}
				}

				throw NubExceptionWithAddress( REASON_NO_SUCH_PORT, offender );
			}
			else
			{
				WARNING_MSG( "Nub::processPendingEvents: "
					"getClosedPort() failed\n" );
			}
#endif
		}
#else
        if (wsaErr == WSAECONNRESET)
        {
            return true;
			// throw NubException( REASON_NO_SUCH_PORT );
        }
#endif // unix

		// ok, I give up, something's wrong
#ifdef _WIN32
		WARNING_MSG( "Nub::processPendingEvents: "
					"Throwing REASON_GENERAL_NETWORK - %d\n",
					wsaErr );
#else
		WARNING_MSG( "Nub::processPendingEvents: "
					"Throwing REASON_GENERAL_NETWORK - %s\n",
				strerror( errno ) );
#endif
		throw NubException(REASON_GENERAL_NETWORK);
	}
}


/**
 * 	This method processes events continuously until interrupted by
 * 	a call to breakProcessing.
 *
 *	@throw NubException
 *	@see breakProcessing
 */
void Nub::processContinuously()
{
	fd_set	readFDs;
	fd_set	writeFDs;
	struct timeval	nextTimeout;
	struct timeval	*selectArg;

	breakProcessing_ = false;
	FD_ZERO( &readFDs );
	FD_ZERO( &writeFDs );

	// Should we expect a packet on this nub's main socket in the next call to
	// processPendingEvents()?
	bool expectPacket = false;

	while (!breakProcessing_)
	{
		// __kyl__ (1/6/2005) Currently using polling to see if child threads
		// have finished their tasks. Potentially should get child threads
		// to signal us or write to a special pipe or socket so that we can
		// wait in select() for their completion.
		if (pOpportunisticPoller_)
			pOpportunisticPoller_->poll();

		bool 	gotPacket;

		// receive packets while they're there
		do
		{
			gotPacket = this->processPendingEvents( expectPacket );
			expectPacket = false;

			// if we were ignoring timers to drain the socket and there are
			// no more packets left, then stop draining and give the loop
			// one more go to check all the timers out (of which there are
			// likely many waiting if draining in a high load situation)
			if (drainSocketInput_ && !gotPacket)
			{
				drainSocketInput_ = false;
				gotPacket = true;	// pretend we got a packet
			}
		}
		while (gotPacket && !breakProcessing_);

		// if processing has been stopped then get out
		if (breakProcessing_)
		{
			break;
		}

		// ok, nothing's urgent then: settle down to a select
		//  on the socket and the topmost timer
		BeginThreadBlockingOperation();

		uint64		startSelect = timestamp();

		readFDs = fdReadSet_;
		writeFDs = fdWriteSet_;
		if (timerQueue_.empty())
		{
			selectArg = NULL;
		}
		else
		{
			double maxWait = 0.0;
			uint64 topTime = timerQueue_.top()->deliveryTime;

			if (topTime > startSelect)
			{
#ifdef _WIN32
				maxWait = (double)(int64)(topTime - startSelect);
#else
				maxWait = topTime - startSelect;
#endif
				maxWait /= stampsPerSecondD();
			}

			MF_ASSERT( 0.0 <= maxWait && maxWait <= 36000.0);

			nextTimeout.tv_sec = (int)maxWait;
			nextTimeout.tv_usec =
				(int)( (maxWait - (double)nextTimeout.tv_sec) * 1000000.0 );

			selectArg = &nextTimeout;
		}

		int countReady = select( fdLargest_+1, &readFDs,
				fdWriteCount_ ? &writeFDs : NULL, NULL, selectArg );

		uint64 endofSelect = timestamp();
		spareTime_ += endofSelect - startSelect;
		loopStats_[RECV_SELS]++;

		CeaseThreadBlockingOperation();

		if (countReady > 0)
		{
			// If the primary socket for this nub is ready to read, it takes
			// priority over the other sockets registered here.
			if (FD_ISSET( socket_, &readFDs ))
			{
				expectPacket = true;
			}

			// We only check slave sockets if the main socket was dry
			else
			{
				this->handleInputNotifications( countReady, readFDs, writeFDs );
			}
		}

		else if (countReady == -1)
		{
			if (!breakProcessing_)
			{
				WARNING_MSG( "Nub::processContinuously: "
					"error in select(): %s\n", strerror( errno ) );
			}
		}
	}
}


/**
 *	This method call processContinuously until breakProcessing is called on the
 *	nub. It takes care of catching any Nub exceptions thrown.
 */
void Nub::processUntilBreak()
{
	for (;;)
	{
		try
		{
			this->processContinuously();
			break;
		}
		catch (Mercury::NubException & ne)
		{
			this->reportException( ne );

			if (breakProcessing_)
			{
				break;
			}
		}
	}
	this->reportPendingExceptions( true /* reportBelowThreshold */ );
}


/**
 *  This method processes events on this Nub until all registered channels have
 *  no unacked packets.
 */
void Nub::processUntilChannelsEmpty( float timeout )
{
	bool done = false;
	uint64 startTime = timestamp();
	uint64 endTime = startTime + uint64( timeout * stampsPerSecondD() );

	while (!done && (timestamp() < endTime))
	{
		try
		{
			while (this->processPendingEvents())
			{
				; // Drain incoming events
			}
		}
		catch (Mercury::NubException & ne)
		{
			this->reportException( ne );
		}

		// Go through registered channels and check if any have unacked packets
		bool haveAnyUnackedPackets = false;
		ChannelMap::iterator iter = channelMap_.begin();
		while (iter != channelMap_.end() && !haveAnyUnackedPackets)
		{
			Channel & channel = *iter->second;

			if (channel.hasUnackedPackets())
			{
				haveAnyUnackedPackets = true;
			}

			++iter;
		}

		done = !haveAnyUnackedPackets;

		if (!condemnedChannels_.deleteFinishedChannels())
		{
			done = false;
		}

		// Wait 100ms
#if defined( PLAYSTATION3 )
		sys_timer_usleep( 100000 );
#elif !defined( _WIN32 )
		usleep( 100000 );
#else
		Sleep( 100 );
#endif
	}

	this->reportPendingExceptions( true /* reportBelowThreshold */ );

	if (!done)
	{
		WARNING_MSG( "Nub::processUntilChannelsEmpty: "
			"Timed out after %.1fs, unacked packets may have been lost\n",
			timeout );
	}
}


/**
 *  Trigger input notification handlers for ready file descriptors.
 */
void Nub::handleInputNotifications( int &countReady,
	fd_set &readFDs, fd_set &writeFDs )
{
#ifdef _WIN32
	// X360 fd_sets don't look like POSIX ones, we know exactly what they are
	// and can just iterate over the provided FD arrays

	for (unsigned i=0; i < readFDs.fd_count; i++)
	{
		int fd = readFDs.fd_array[ i ];
		--countReady;
		InputNotificationHandler * pHandler = fdHandlers_[ fd ];
		if (pHandler)
			pHandler->handleInputNotification( fd );
	}

	for (unsigned i=0; i < writeFDs.fd_count; i++)
	{
		int fd = writeFDs.fd_array[ i ];
		--countReady;
		InputNotificationHandler * pHandler = fdWriteHandlers_[ fd ];
		if (pHandler)
			pHandler->handleInputNotification( fd );
	}

#else
	// POSIX fd_sets are more opaque and we just have to count up blindly until
	// we hit valid FD's with FD_ISSET

	for (int fd = 0; fd <= fdLargest_ && countReady > 0; ++fd)
	{
		if (FD_ISSET( fd, &readFDs ))
		{
			--countReady;
			InputNotificationHandler * pHandler = fdHandlers_[ fd ];
			if (pHandler)
				pHandler->handleInputNotification( fd );
		}
		if (FD_ISSET( fd, &writeFDs ))
		{
			--countReady;
			InputNotificationHandler * pHandler = fdWriteHandlers_[ fd ];
			if (pHandler)
				pHandler->handleInputNotification( fd );
		}
	}
#endif
}


/**
 *  This method is the nub's own input notification callback.  This is used by
 *  slave nubs when registering with a master nub.  It simply calls process
 *  pending events on the nub so that it can process incoming packets and timers
 *  just like the master nub.
 */
int Nub::handleInputNotification( int fd )
{
	this->processPendingEvents( /* expectingPacket: */ true );
	return 0;
}


/**
 *	This method breaks out of 'processContinuously' at the next opportunity.
 *	Any messages in bundles that are being processed or timers that have
 *	expired will still get called. Note: if this is called from another
 *	thread it will NOT interrupt a select call if one is in progress, so
 *	processContinuously will not return. Try sending the process a (handled)
 *	signal if this is your intention.
 */
void Nub::breakProcessing( bool breakState )
{
	breakProcessing_ = breakState;
}


/**
 *	This method returns whether or not we have broken out of the
 *	processContinuously loop.
 *
 *	@see breakProcessing
 *	@see processContinuously
 */
bool Nub::processingBroken() const
{
	return breakProcessing_;
}


/**
 *	This method breaks out of the current bundle loop
 */
void Nub::breakBundleLoop()
{
	breakBundleLoop_ = true;
}

/**
 *	This method drains all pending network input on the Nub's socket.
 *	The next (or current) processContinuously call will process only
 *	packets until there are no more packets left to receive.
 */
void Nub::drainSocketInput()
{
	drainSocketInput_ = true;
}


/**
 *  This method closes the endpoint and stops and processing in
 *  processContinuously loop - needed to stop select(,,,,NULL)
 *
 *	@see breakProcessing
 */
void Nub::shutdown()
{
    this->breakProcessing();
    socket_.close();
}


/**
 * 	This method returns the address of the interface the nub is bound to, or
 * 	the address of the first non-local interface if it's bound to all
 * 	interfaces. A zero address is returned if the nub couldn't initialise.
 */
const Address & Nub::address() const
{
	return advertisedAddress_;
}


/**
 *	This method registers the channel with the nub.
 */
bool Nub::registerChannel( Channel & channel )
{
	MF_ASSERT( channel.addr() != Address::NONE );
	MF_ASSERT( &channel.nub() == this );

	ChannelMap::iterator iter = channelMap_.find( channel.addr() );
	Channel * pExisting = iter != channelMap_.end() ? iter->second : NULL;

	// __glenc__ Shouldn't ever register a channel twice.
	IF_NOT_MF_ASSERT_DEV( !pExisting )
	{
		return false;
	}

	channelMap_[ channel.addr() ] = &channel;

	return true;
}


/**
 *	This method deregisters the channel with the nub.
 */
bool Nub::deregisterChannel( Channel & channel )
{
	const Address & addr = channel.addr();
	MF_ASSERT( addr != Address::NONE );

	if (!channelMap_.erase( addr ))
	{
		CRITICAL_MSG( "Nub::deregisterChannel: Channel not found %s!\n",
			   (char *)addr );
		return false;
	}

	if (isExternal_)
	{
		TimerID timeoutID = this->registerCallback( 60 * 1000000, this,
			(void*)TIMEOUT_RECENTLY_DEAD_CHANNEL );

		recentlyDeadChannels_[ addr ] = timeoutID;
	}

	return true;
}


/**
 *  Set the ChannelFinder object to be used for resolving channel IDs.
 */
void Nub::registerChannelFinder( ChannelFinder *pFinder )
{
	MF_ASSERT( pChannelFinder_ == NULL );
	pChannelFinder_ = pFinder;
}


/**
 *  This method returns the channel to the provided address.  If createAnonymous
 *  is true, an anonymous channel will be created if one doesn't already exist.
 */
Channel * Nub::findChannel( const Address & addr, bool createAnonymous )
{
	ChannelMap::iterator iter = channelMap_.find( addr );
	Channel * pChannel = iter != channelMap_.end() ? iter->second : NULL;

	// Indexed channels aren't supposed to be registered with the nub.
	MF_ASSERT( !pChannel || pChannel->id() == CHANNEL_ID_NULL );

	// Make a new anonymous channel if it didn't already exist and this is an
	// internal nub.
	if (!pChannel && createAnonymous)
	{
		MF_ASSERT( !isExternal_ );

		INFO_MSG( "Nub::findChannel: "
			"Creating anonymous channel to %s\n",
			addr.c_str() );

		pChannel = new Channel( *this, addr, Channel::INTERNAL );
		pChannel->isAnonymous( true );
	}

	return pChannel;
}


/**
 *  This method condemns the anonymous channel to the specified address.
 */
void Nub::delAnonymousChannel( const Address & addr )
{
	ChannelMap::iterator iter = channelMap_.find( addr );

	if (iter != channelMap_.end())
	{
		if (iter->second->isAnonymous())
		{
			iter->second->condemn();
		}
		else
		{
			ERROR_MSG( "Nub::delAnonymousChannel: "
				"Channel to %s is not anonymous!\n",
				addr.c_str() );
		}
	}
	else
	{
		ERROR_MSG( "Nub::delAnonymousChannel: "
			"Couldn't find channel for address %s\n",
			addr.c_str() );
	}
}


/**
 * 	This method returns the next sequence ID to use.
 * 	It is private, and for use only within Mercury.
 */
inline SeqNum Nub::getNextSequenceID()
{
	SeqNum ret = nextSequenceID_;
	nextSequenceID_ = Channel::seqMask( nextSequenceID_ + 1 );
	return ret;
}


/**
 * 	This method sends a bundle to the given address.
 * 	Note: any pointers you have into the packet may become invalid
 * 	after this call (and whenever a channel calls this too).
 *
 * 	@param address	The address to send to.
 * 	@param bundle	The bundle to send
 *	@param pChannel	The Channel that is sending the bundle.
 *				(even if the bundle is not sent reliably, it is still passed
 *				through the filter associated with the channel).
 *
 * 	@return A Mercury::Reason.
 */
void Nub::send( const Address & address, Bundle & bundle, Channel * pChannel )
{
	MF_ASSERT( address != Address::NONE );
	MF_ASSERT( !pChannel || pChannel->addr() == address );

	// You aren't allowed to send a channel's bundle via the Nub (however you
	// are allowed to send a non-channel bundle via the channel).
	MF_ASSERT( !bundle.pChannel() || (bundle.pChannel() == pChannel) );

	sendMercuryTimer_.start();

	// ok, first of all finalise the bundle
	bundle.finalise();

	// now go through and add any reply handlers,
	for (Bundle::ReplyOrders::iterator order = bundle.replyOrders_.begin();
		order != bundle.replyOrders_.end();
		order++)
	{
		ReplyHandlerElement * pRHE = new ReplyHandlerElement;

		int replyID = this->getNextReplyID();
		pRHE->replyID_ = replyID;
		pRHE->timerID_ = TIMER_ID_NONE;
		pRHE->pHandler_ = order->handler;
		pRHE->arg_ = order->arg;
		pRHE->pChannel_ = pChannel;

		replyHandlerMap_[ replyID ] = pRHE;

		// fix up the replyID in the bundle
		*(order->pReplyID) = BW_HTONL( replyID );

		if (!pChannel)
		{
 			MF_ASSERT( order->microseconds > 0 );
			pRHE->timerID_ =
				this->registerCallback( order->microseconds, pRHE, this );
		}
	}

	// fill in all the footers that are left to us
	Packet * pFirstOverflowPacket = NULL;

	int	numPackets = bundle.sizeInPackets();
	SeqNum firstSeq = 0;
	SeqNum lastSeq = 0;
	Bundle::AckOrders::iterator ackIter = bundle.ackOrders_.begin();

	// Write footers for each packet.
	for (Packet * p = bundle.firstPacket_.get(); p; p = p->next())
	{
		MF_ASSERT( p->msgEndOffset() >= Packet::HEADER_SIZE );

		// Reserve space for the checksum footer if necessary
		if (shouldUseChecksums_)
		{
			MF_ASSERT( !p->hasFlags( Packet::FLAG_HAS_CHECKSUM ) );
			p->reserveFooter( sizeof( Packet::Checksum ) );
			p->enableFlags( Packet::FLAG_HAS_CHECKSUM );
		}

		// Mark the packet as being on a channel if required.
		MF_ASSERT( !p->hasFlags( Packet::FLAG_ON_CHANNEL ) );
		if (pChannel)
		{
			p->enableFlags( Packet::FLAG_ON_CHANNEL );
		}

		// At this point, p->back() is positioned just after the message
		// data, so we advance it to the end of where the footers end, then
		// write backwards towards the message data. We check that we finish
		// up back at the message data as a sanity check.
		const int msgEndOffset = p->msgEndOffset();
		p->grow( p->footerSize() );

		// Pack in a zero checksum.  We'll calculate it later.
		Packet::Checksum * pChecksum = NULL;

		if (p->hasFlags( Packet::FLAG_HAS_CHECKSUM ))
		{
			p->packFooter( Packet::Checksum( 0 ) );
			pChecksum = (Packet::Checksum*)p->back();
		}

		// Write piggyback info.  Note that we should only ever do this to
		// the last packet in a bundle as it should be the only packet with
		// piggybacks on it.  Piggybacks go first so that they can be
		// stripped and the rest of the packet can be dealt with as normal.
		if (p->hasFlags( Packet::FLAG_HAS_PIGGYBACKS ))
		{
			MF_ASSERT( p->next() == NULL );

			int16 * lastLen = NULL;

			// Remember where the end of the piggybacks is for setting up
			// the piggyFooters Field after all the piggies have been
			// streamed on
			int backPiggyOffset = p->msgEndOffset();

			for (Bundle::Piggybacks::const_iterator it =
					bundle.piggybacks_.begin();
				 it != bundle.piggybacks_.end(); ++it)
			{
				const Bundle::Piggyback &pb = **it;

				// Stream on the length first
				p->packFooter( pb.len_ );
				lastLen = (int16*)p->back();

				// Reserve the area for the packet data
				p->shrink( pb.len_ );
				char * pdata = p->back();

				// Stream on the packet header
				*(Packet::Flags*)pdata = BW_HTONS( pb.flags_ );
				pdata += sizeof( Packet::Flags );

				// Stream on the reliable messages
				for (ReliableVector::const_iterator rvit = pb.rvec_.begin();
					 rvit != pb.rvec_.end(); ++rvit)
				{
					memcpy( pdata, rvit->segBegin, rvit->segLength );
					pdata += rvit->segLength;
				}

				// Stream on sequence number footer
				*(SeqNum*)pdata = BW_HTONL( pb.seq_ );
				pdata += sizeof( SeqNum );

				// Stream on any sub-piggybacks that were lost
				if (pb.flags_ & Packet::FLAG_HAS_PIGGYBACKS)
				{
					const Field & subPiggies = pb.pPacket_->piggyFooters();
					memcpy( pdata, subPiggies.beg_, subPiggies.len_ );
					pdata += subPiggies.len_;
				}

				// Sanity check
				MF_ASSERT( pdata == (char*)lastLen );

				++numPiggybacks_;

				if (isVerbose_)
				{
					DEBUG_MSG( "Nub::send( %s ): "
						"Piggybacked #%u (%d bytes) onto outgoing bundle\n",
						address.c_str(), pb.seq_, pb.len_ );
				}
			}

			// One's complement the length of the last piggyback to indicate
			// that it's the last one.
			*lastLen = BW_HTONS( ~BW_NTOHS( *lastLen ) );

			// Finish setting up the piggyFooters field for this packet.
			// We'll need this if this packet is lost and we need to
			// piggyback it onto another packet later on.
			p->piggyFooters().beg_ = p->back();
			p->piggyFooters().len_ =
				uint16( backPiggyOffset - p->msgEndOffset() );
		}

		// Stream on channel ID and version if set
		if (p->hasFlags( Packet::FLAG_INDEXED_CHANNEL ))
		{
			MF_ASSERT( pChannel && pChannel->isIndexed() );

			p->channelID() = pChannel->id();
			p->packFooter( p->channelID() );

			p->channelVersion() = pChannel->version();
			p->packFooter( p->channelVersion() );
		}

		// Add acks
		if (p->hasFlags( Packet::FLAG_HAS_ACKS ))
		{
			p->packFooter( (uint8)p->nAcks() );

			int numAcks = 0;
			while (ackIter != bundle.ackOrders_.end() && ackIter->p == p)
			{
				p->packFooter( ackIter->forseq );
				++ackIter;
				++numAcks;
			}

			// There cannot be more than this since ACK count is 8 bits
			MF_ASSERT( numAcks <= Packet::MAX_ACKS );
			MF_ASSERT( numAcks == p->nAcks() );
		}

		// Add the sequence number
		if (p->hasFlags( Packet::FLAG_HAS_SEQUENCE_NUMBER ))
		{
			// If we're sending reliable traffic on a channel, use the
			// channel's sequence numbers.  Otherwise use the nub's.
			p->seq() = (pChannel && p->hasFlags( Packet::FLAG_IS_RELIABLE )) ?
				pChannel->useNextSequenceID() : this->getNextSequenceID();

			p->packFooter( p->seq() );

			if (p == bundle.firstPacket_)
			{
				firstSeq = p->seq();
				lastSeq = p->seq() + numPackets - 1;
			}
		}

		// Add the first request offset.
		if (p->hasFlags( Packet::FLAG_HAS_REQUESTS ))
		{
			p->packFooter( p->firstRequestOffset() );
		}

		// add the fragment info
		if (p->hasFlags( Packet::FLAG_IS_FRAGMENT ))
		{
			p->packFooter( lastSeq );
			p->packFooter( firstSeq );
		}

		// Make sure writing all the footers brought us back to the end of
		// the message data.
		MF_ASSERT( p->msgEndOffset() == msgEndOffset );

		// Calculate the checksum and write it in.  We don't have to worry
		// about padding issues here because we have written a 0 into the
		// checksum field, and any overrun will be read from that field.
		if (p->hasFlags( Packet::FLAG_HAS_CHECKSUM ))
		{
			Packet::Checksum sum = 0;
			for (Packet::Checksum * pData = (Packet::Checksum *)p->data();
				 pData < pChecksum; pData++)
			{
				sum ^= BW_NTOHL( *pData );
			}

			*pChecksum = BW_HTONL( sum );
		}

		// set up the reliable machinery
		if (p->hasFlags( Packet::FLAG_IS_RELIABLE ))
		{
			if (pChannel)
			{
				const ReliableOrder *roBeg, *roEnd;

				if (pChannel->isInternal())
				{
					roBeg = roEnd = NULL;
				}
				else
				{
					bundle.reliableOrders( p, roBeg, roEnd );
				}

				if (!pChannel->addResendTimer( p->seq(), p, roBeg, roEnd ))
				{
					if (pFirstOverflowPacket == NULL)
					{
						pFirstOverflowPacket = p;
					}
					// return REASON_WINDOW_OVERFLOW;
				}
				else
				{
					MF_ASSERT( pFirstOverflowPacket == NULL );
				}
			}
			else
			{
				this->addOnceOffResendTimer( address, p->seq(), p );
			}
		}
	}

	// Finally actually send the darned thing. Do not send overflow packets.
	for (Packet * p = bundle.firstPacket_.get();
			p != pFirstOverflowPacket;
			p = p->next())
	{
		this->sendPacket( address, p, pChannel, false );
	}

	sendMercuryTimer_.stop(true);
	numBundlesSent_++;
	numMessagesSent_ += bundle.numMessages();

	// NOTE: This statistic will be incorrect on internal nubs, since reliableOrders_
	// will always be empty, but numMessagesSent_ has the correct value, since
	// all messages are reliable.
	numReliableMessagesSent_ += bundle.reliableOrders_.size();
}


/**
 *	This method sends a packet. No result is returned as it cannot be trusted.
 *	The packet may never get to the other end.
 */
void Nub::sendPacket( const Address & address,
						Packet * p,
						Channel * pChannel, bool isResend )
{
	if (isResend)
	{
		numBytesResent_ += p->totalSize();
		++numPacketsResent_;
	}
	else
	{
		if (!p->hasFlags( Packet::FLAG_ON_CHANNEL ))
		{
			++numPacketsSentOffChannel_;
		}
	}

	// Check if we want artificial loss or latency
	if (!this->rescheduleSend( address, p, /* isResend: */ false ))
	{
		PacketFilterPtr pFilter = pChannel ? pChannel->pFilter() : NULL;

		if(pPacketMonitor_)
		{
			pPacketMonitor_->packetOut( address, *p );
		}

		// Otherwise send as appropriate
		if (pFilter)
		{
			pFilter->send( *this, address, p );
		}
		else
		{
			this->basicSendWithRetries( address, p );
		}
	}
}


/**
 *	Basic packet sending functionality that retries a few times
 *	if there are transitory errors.
 */
Reason Nub::basicSendWithRetries( const Address & addr, Packet * p )
{
	// try sending a few times
	int retries = 0;
	Reason reason;

	while (1)
	{
		retries++;

		sendSystemTimer_.start();
		reason = this->basicSendSingleTry( addr, p );
		sendSystemTimer_.stop(true);

		if (reason == REASON_SUCCESS)
			return reason;

		// If we've got an error in the queue simply send it again;
		// we'll pick up the error later.
		if (reason == REASON_NO_SUCH_PORT && retries <= 3)
		{
			continue;
		}

		// If the transmit queue is full wait 10ms for it to empty.
		if (reason == REASON_RESOURCE_UNAVAILABLE && retries <= 3)
		{
			fd_set	fds;
			struct timeval tv = { 0, 10000 };
			FD_ZERO(&fds);
			FD_SET(socket_,&fds);

			WARNING_MSG( "Nub::send: "
				"Transmit queue full, waiting for space... (%d)\n",
				retries );

			sendSystemTimer_.start();
			select(socket_+1,NULL,&fds,NULL,&tv);
			sendSystemTimer_.stop(true);

			continue;
		}

		// some other error, so don't bother retrying
		break;
	}

	// flush the error queue ... or something ('tho shouldn't we do this
	// before the next retry? if not why do we bother with it at all?
	// we'd really like to get this badPorts in fact I think)

	// Could you please add a comment saying why it was necessary?
	// From the commit message:
	// Fixed problem where the baseapp process was taking 100% CPU. This was
	// occurring when there was an EHOSTUNREACH error. The error queue was not
	// being read. select was then always return > 1.

	// The following is a bit dodgy. Used to get any pending messages
	// from the error queue.

	Address badAddress;

	while (socket_.getClosedPort( badAddress ))
	{
		ERROR_MSG( "Nub::send: Bad address is %s (discarding)\n",
				badAddress.c_str() );
	}

	return reason;
}


/**
 *	Basic packet sending function that just tries to send once.
 *
 *	@return 0 on success otherwise an error code.
 */
Reason Nub::basicSendSingleTry( const Address & addr, Packet * p )
{
	int len = socket_.sendto( p->data(), p->totalSize(), addr.port, addr.ip );

	if (len == p->totalSize())
	{
		numBytesSent_ += len + UDP_OVERHEAD;
		numPacketsSent_++;

		return REASON_SUCCESS;
	}
	else
	{
		int err;
		Reason reason;

		numFailedPacketSend_++;

		// We need to store the error number because it may be changed by later
		// calls (such as OutputDebugString) before we use it.
		#ifdef unix
			err = errno;

			switch (err)
			{
				case ECONNREFUSED:	reason = REASON_NO_SUCH_PORT; break;
				case EAGAIN:		reason = REASON_RESOURCE_UNAVAILABLE; break;
				case ENOBUFS:		reason = REASON_TRANSMIT_QUEUE_FULL; break;
				default:			reason = REASON_GENERAL_NETWORK; break;
			}
		#else
			err = WSAGetLastError();

			if (err == WSAEWOULDBLOCK || err == WSAEINTR)
			{
				reason = REASON_RESOURCE_UNAVAILABLE;
			}
			else
			{
				reason = REASON_GENERAL_NETWORK;
			}
		#endif

		if (len == -1)
		{
			if (reason != REASON_NO_SUCH_PORT)
			{
				this->reportError( addr,
					"Nub::basicSendSingleTry( %s ): could not send packet: %s",
					addr.c_str(), strerror( err ) );
			}
		}
		else
		{
			WARNING_MSG( "Nub::basicSendSingleTry( %s ): "
				"packet length %d does not match sent length %d (err = %s)\n",
				(char*)addr, p->totalSize(), len, strerror( err ) );
		}

		return reason;
	}
}

/**
 * This method reschedules a packet to be sent to the address provided
 * some short time in the future (or drop it) depending on the latency
 * settings on the nub.
 *
 * @return true if rescheduled
 */
bool Nub::rescheduleSend( const Address & addr, Packet * packet, bool isResend )
{

	// see if we drop it
	if (dropNextSend_ ||
		((artificialDropPerMillion_ != 0) &&
		rand() < int64( artificialDropPerMillion_ ) * RAND_MAX / 1000000))
	{
		if (packet->seq() != Channel::SEQ_NULL)
		{
			if (packet->channelID() != CHANNEL_ID_NULL)
			{
				DEBUG_MSG( "Nub::rescheduleSend: "
					"dropped packet #%u to %s/%d due to artificial loss\n",
					packet->seq(), addr.c_str(), packet->channelID() );
			}
			else
			{
				DEBUG_MSG( "Nub::rescheduleSend: "
					"dropped packet #%u to %s due to artificial loss\n",
					packet->seq(), addr.c_str() );
			}
		}
		else
		{
			// All internal messages should be reliable
			MF_ASSERT( isExternal_ ||
				packet->msgEndOffset() == Packet::HEADER_SIZE );

			DEBUG_MSG( "Nub::rescheduleSend: "
				"Dropped packet with %d ACKs to %s due to artificial loss\n",
				packet->nAcks(), addr.c_str() );
		}

		dropNextSend_ = false;

		return true;
	}

	// now see if we delay it
	if (artificialLatencyMax_ == 0)
		return false;

	int latency = (artificialLatencyMax_ > artificialLatencyMin_) ?
		artificialLatencyMin_ +
				rand() % (artificialLatencyMax_ - artificialLatencyMin_) :
		artificialLatencyMin_;

	if (latency < 2)
		return false;	// 2ms => send now

	// ok, we'll delay this packet then
	new RescheduledSender( *this, addr, packet, latency, isResend );

	return true;
}


/**
 *	Constructor for RescheduledSender.
 */
Nub::RescheduledSender::RescheduledSender( Nub & nub, const Address & addr,
	Packet * pPacket, int latencyMilli, bool isResend ) :
	nub_( nub ),
	addr_( addr ),
	pPacket_( pPacket ),
	isResend_( isResend )
{
	nub_.registerCallback( latencyMilli*1000, this );
}


int Nub::RescheduledSender::handleTimeout( TimerID, void * )
{
	Channel * pChannel = pPacket_->hasFlags( Packet::FLAG_ON_CHANNEL ) ?
		nub_.findChannel( addr_ ) : NULL;

	if (isResend_)
	{
		nub_.sendPacket( addr_, pPacket_.get(), pChannel, true );
	}
	else
	{
		PacketFilterPtr pFilter = pChannel ? pChannel->pFilter() : NULL;

		if(nub_.pPacketMonitor_)
		{
			nub_.pPacketMonitor_->packetOut( addr_, *pPacket_ );
		}

		if (pFilter)
		{
			pFilter->send( nub_, addr_, pPacket_.get() );
		}
		else
		{
			nub_.basicSendWithRetries( addr_, pPacket_.get() );
		}

	}

	delete this;

	return 0;
}


/**
 *	This method registers a channel for delayed sending. This is used by
 *	irregular channels so that they still have an opportunity to aggregate
 *	messages. Instead of sending a packet for each method call, these are
 *	aggregated.
 */
void Nub::delayedSend( Channel * pChannel )
{
	delayedChannels_.insert( pChannel );
}


/**
 *	This method sends the irregular channels that have been queued for delayed
 *	sending.
 */
void Nub::sendDelayedChannels()
{
	DelayedChannels::iterator iter = delayedChannels_.begin();

	while (iter != delayedChannels_.end())
	{
		Channel * pChannel = iter->get();

		if (!pChannel->isDead())
		{
			pChannel->send();
		}

		++iter;
	}

	delayedChannels_.clear();
}


/**
 *  This method cleans up all internal data structures and timers related to the
 *  specified address.
 */
void Nub::onAddressDead( const Address & addr )
{
	// Iterate through all the unacked once-off sends and remove those going to
	// the dead address.
	OnceOffPackets::iterator iter = onceOffPackets_.begin();
	int numRemoved = 0;

	while (iter != onceOffPackets_.end())
	{
		if (iter->addr_ == addr)
		{
			OnceOffPackets::iterator deadIter = iter++;
			this->delOnceOffResendTimer( deadIter );
			numRemoved++;
		}
		else
		{
			++iter;
		}
	}

	if (numRemoved)
	{
		WARNING_MSG( "Nub::onAddressDead( %s ): "
			"Discarded %d unacked once-off sends\n",
			addr.c_str(), numRemoved );
	}

	// Clean up the anonymous channel to the dead address, if there was one.
	Channel * pDeadChannel = this->findChannel( addr );

	if (pDeadChannel && pDeadChannel->isAnonymous())
	{
		pDeadChannel->hasRemoteFailed( true );
	}
}


/**
 *  Handler to notify registerWithMachined() of the receipt of an expected
 *  message.
 */
bool Nub::ProcessMessageHandler::onProcessMessage(
		ProcessMessage &pm, uint32 addr )
{
	hasResponded_ = true;
	return false;
}

class ProcessMessageHandler;


/**
 *	This method is used to register or deregister an interface with the machine
 *	guard (a.k.a. machined).
 */
Reason Nub::registerWithMachined( const std::string & name, int id,
	bool isRegister )
{
	ProcessMessage pm;
	ProcessMessageHandler pmh;
	Reason response;

	pm.param_ = (isRegister ? pm.REGISTER : pm.DEREGISTER) |
		pm.PARAM_IS_MSGTYPE;
	pm.category_ = ProcessMessage::SERVER_COMPONENT;
	pm.uid_ = getUserId();
	pm.pid_ = mf_getpid();
	pm.port_ = this->address().port;
	pm.name_ = name;
	pm.id_ = id;

	// send and wait for the reply
	uint32 src = advertisedAddress_.ip, dest = htonl( 0x7F000001U );
	response = pm.sendAndRecv( src, dest, &pmh );

	if (response == REASON_SUCCESS)
	{
		if (pmh.hasResponded_)
		{
			interfaceName_ = name;
			interfaceID_ = id;
		}
		else
		{
			response = REASON_TIMER_EXPIRED;
		}
	}

	return response;
}


/**
 *  This method deregisters this interface with machined, if it was already
 *  registered.
 */
Reason Nub::deregisterWithMachined()
{
	if (!interfaceName_.empty())
	{
		return this->registerWithMachined(
			interfaceName_, interfaceID_, false );
	}
	else
	{
		return REASON_SUCCESS;
	}
}


/**
 *	This is the entrypoint for new packets, which just gives it to the filter.
 */
Reason Nub::processPacket( const Address & addr, Packet * p )
{
	// Packets arriving on external nubs will probably be encrypted, so there's
	// no point examining their header flags right now.
	Channel * pChannel = this->findChannel( addr,
			!isExternal_ && p->shouldCreateAnonymous() );

	if (pChannel != NULL)
	{
		// We update received times for addressed channels here.  Indexed
		// channels are done in processFilteredPacket().
		pChannel->onPacketReceived( p->totalSize() );

		if (pChannel->pFilter() && !pChannel->hasRemoteFailed())
		{
			// let the filter decide what to do with it
			return pChannel->pFilter()->recv( *this, addr, p );
		}
	}

	// If we're monitoring recent channel deaths, check now if this packet
	// should be dropped.
	if (isExternal_ &&
		isVerbose_ &&
		(recentlyDeadChannels_.find( addr ) != recentlyDeadChannels_.end()))
	{
		DEBUG_MSG( "Nub::processPacket( %s ): "
			"Ignoring incoming packet on recently dead channel\n",
			addr.c_str() );

		return REASON_SUCCESS;
	}

	return this->processFilteredPacket( addr, p );
}


/**
 *  This macro is used by Nub::processFilteredPacket() and
 *  Nub::processOrderedPacket() whenever they need to return early due to a
 *  corrupted incoming packet.
 */
#define RETURN_FOR_CORRUPTED_PACKET()										\
	++numCorruptedPacketsReceived_;											\
	return REASON_CORRUPTED_PACKET;											\


/**
 *	This function has to be very robust, if we intend to use this transport over
 *	the big bad internet. We basically have to assume it'll be complete garbage.
 */
Reason Nub::processFilteredPacket( const Address & addr, Packet * p )
{
	if (p->totalSize() < int( sizeof( Packet::Flags ) ))
	{
		if (!isExternal_)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"received undersized packet\n",
				addr.c_str() );
		}

		RETURN_FOR_CORRUPTED_PACKET();
	}

	if (pPacketMonitor_)
	{
		pPacketMonitor_->packetIn( addr, *p );
	}

	// p->debugDump();

	// Make sure we understand all the flags
	if (p->flags() & ~Packet::KNOWN_FLAGS)
	{
		if (!isExternal_)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"received packet with bad flags %x\n",
				addr.c_str(), p->flags() );
		}

		RETURN_FOR_CORRUPTED_PACKET();
	}

	if (!p->hasFlags( Packet::FLAG_ON_CHANNEL ))
	{
		++numPacketsReceivedOffChannel_;
	}

	// Don't allow FLAG_CREATE_CHANNEL on external nubs
	if (isExternal_ && p->hasFlags( Packet::FLAG_CREATE_CHANNEL ))
	{
		WARNING_MSG( "Nub::processFilteredPacket( %s ): "
			"Got FLAG_CREATE_CHANNEL on external nub\n",
			addr.c_str() );

		RETURN_FOR_CORRUPTED_PACKET();
	}

	// make sure there's something in the packet
	if (p->totalSize() <= Packet::HEADER_SIZE)
	{
		WARNING_MSG( "Nub::processFilteredPacket( %s ): "
			"received undersize packet (%d bytes)\n",
			addr.c_str(), p->totalSize() );

		RETURN_FOR_CORRUPTED_PACKET();
	}

	// Start stripping footers
	if (p->hasFlags( Packet::FLAG_HAS_CHECKSUM ))
	{
		// Strip checksum and verify correctness
		if (!p->stripFooter( p->checksum() ))
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Packet too short (%d bytes) for checksum!\n",
				addr.c_str(), p->totalSize() );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		// Zero data in checksum field on packet to avoid padding issues
		*(Packet::Checksum*)p->back() = 0;

		// Calculate correct checksum for packet
		Packet::Checksum sum = 0;

		for (const Packet::Checksum * pData = (Packet::Checksum*)p->data();
			 pData < (Packet::Checksum*)p->back(); pData++)
		{
			sum ^= BW_NTOHL( *pData );
		}

		// Put the checksum back on the stream in case this packet is forwarded
		// on.
		*(Packet::Checksum*)p->back() = BW_HTONL( p->checksum() );

		if (sum != p->checksum())
		{
			ERROR_MSG( "Nub::processFilteredPacket( %s ): "
				"Packet (flags %hx, size %d) failed checksum "
				"(wanted %08x, got %08x)\n",
				addr.c_str(), p->flags(), p->totalSize(), sum, p->checksum() );

			RETURN_FOR_CORRUPTED_PACKET();
		}
	}

	// Strip off piggybacks and process them immediately (i.e. before the
	// messages on this packet) as the piggybacks must be older than this packet
	if (p->hasFlags( Packet::FLAG_HAS_PIGGYBACKS ))
	{
		bool done = false;

		while (!done)
		{
			int16 len;

			if (!p->stripFooter( len ))
			{
				WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"Not enough data for piggyback length (%d bytes left)\n",
					addr.c_str(), p->bodySize() );

				RETURN_FOR_CORRUPTED_PACKET();
			}

			// The last piggyback on a packet has a negative length.
			if (len < 0)
			{
				len = ~len;
				done = true;
			}

			// Check there's enough space on the packet for this
			if (p->bodySize() < len)
			{
				WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"Packet too small to contain piggyback message of "
					"length %d (only %d bytes remaining)\n",
					addr.c_str(), len, p->bodySize() );

				RETURN_FOR_CORRUPTED_PACKET();
			}

			// Create piggyback packet and handle it
			PacketPtr piggyPack = new Packet();
			p->shrink( len );
			memcpy( piggyPack->data(), p->back(), len );
			piggyPack->msgEndOffset( len );

			try
			{
				this->processFilteredPacket( addr, piggyPack.get() );
			}
			catch (NubException & ne)
			{
				ERROR_MSG( "Nub::processFilteredPacket( %s ): "
					"Got an exception whilst processing piggyback packet: %s\n",
					addr.c_str(), Mercury::reasonToString( ne.reason() ) );
			}
		}
	}

	ChannelPtr pChannel = NULL;	// don't bother getting channel twice
	bool shouldSendChannel = false;

	// Strip off indexed channel ID if present
	if (p->hasFlags( Packet::FLAG_INDEXED_CHANNEL ))
	{
		if (pChannelFinder_ == NULL)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Got indexed channel packet with no finder registered\n",
				(char*)addr );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		if (!p->stripFooter( p->channelID() ) ||
			!p->stripFooter( p->channelVersion() ))
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Not enough data for indexed channel footer (%d bytes left)\n",
				addr.c_str(), p->bodySize() );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		bool hasBeenHandled = false;
		pChannel = pChannelFinder_->find( p->channelID(), p, hasBeenHandled );

		// If the packet has already been handled, we're done!
		if (hasBeenHandled)
		{
			return REASON_SUCCESS;
		}

		// If we couldn't find the channel, check if it was recently condemned.
		if (!pChannel)
		{
			pChannel = condemnedChannels_.find( p->channelID() );

			if (pChannel)
			{
				NOTICE_MSG( "Nub::processFilteredPacket( %s ): "
						"Received packet for condemned channel.\n",
					pChannel->c_str() );
			}
		}

		if (pChannel)
		{
			// We update received times for indexed channels here.  Addressed
			// channels are done in processPacket().
			pChannel->onPacketReceived( p->totalSize() );
		}
		else
		{
			ERROR_MSG( "Nub::processFilteredPacket( %s ): "
				"Couldn't get indexed channel for id %d\n",
				addr.c_str(), p->channelID() );

			RETURN_FOR_CORRUPTED_PACKET();
		}
	}

	if (!pChannel && p->hasFlags( Packet::FLAG_ON_CHANNEL ))
	{
		pChannel = this->findChannel( addr );

		if (!pChannel)
		{
			MF_ASSERT_DEV( !p->shouldCreateAnonymous() );

			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Dropping packet due to absence of local channel\n",
				addr.c_str() );

			return REASON_GENERAL_NETWORK;
		}
	}

	if (pChannel)
	{
		if (pChannel->hasRemoteFailed())
		{
			// __glenc__ We could consider resetting the channel here if it has
			// FLAG_CREATE_CHANNEL.  This would help cope with fast restarts on
			// the same address.  Haven't particularly thought this through
			// though.  Could be issues with app code if you do this
			// underneath.  In fact I'm guessing you would need an onReset()
			// callback to inform the app code that this has happened.
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Dropping packet due to remote process failure\n",
				pChannel->c_str() );

			return REASON_GENERAL_NETWORK;
		}

		else if (pChannel->wantsFirstPacket())
		{
			if (p->hasFlags( Packet::FLAG_CREATE_CHANNEL ))
			{
				pChannel->gotFirstPacket();
			}
			else
			{
				WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"Dropping packet on "
					"channel wanting FLAG_CREATE_CHANNEL (flags: %x)\n",
					pChannel->c_str(), p->flags() );

				return REASON_GENERAL_NETWORK;
			}
		}
	}

	// Strip and handle ACKs
	if (p->hasFlags( Packet::FLAG_HAS_ACKS ))
	{
		if (!p->stripFooter( p->nAcks() ))
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Not enough data for ack count footer (%d bytes left)\n",
				addr.c_str(), p->bodySize() );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		if (p->nAcks() == 0)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Packet with FLAG_HAS_ACKS had 0 acks\n",
				addr.c_str() );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		// The total size of all the ACKs on this packet
		int ackSize = p->nAcks() * sizeof( SeqNum );

		// check that we have enough footers to account for all of the
		// acks the packet claims to have (thanks go to netease)
		if (p->bodySize() < ackSize)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Not enough footers for %d acks "
				"(have %d bytes but need %d)\n",
				addr.c_str(), p->nAcks(), p->bodySize(), ackSize );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		// For each ACK that we receive, we no longer need to store the
		// corresponding packet.
		if (pChannel)
		{
			for (uint i=0; i < p->nAcks(); i++)
			{
				SeqNum seq;
				p->stripFooter( seq );

				if (!pChannel->delResendTimer( seq ))
				{
					WARNING_MSG( "Nub::processFilteredPacket( %s ): "
						"delResendTimer() failed for #%u\n",
						addr.c_str(), seq );

					RETURN_FOR_CORRUPTED_PACKET();
				}
			}
		}
		else if (!p->hasFlags( Packet::FLAG_ON_CHANNEL ))
		{
			for (uint i=0; i < p->nAcks(); i++)
			{
				SeqNum seq;
				p->stripFooter( seq );
				this->delOnceOffResendTimer( addr, seq );
			}
		}
		else
		{
			p->shrink( ackSize );

			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Got %d acks without a channel\n",
				addr.c_str(), p->nAcks() );
		}
	}

	// Strip sequence number
	if (p->hasFlags( Packet::FLAG_HAS_SEQUENCE_NUMBER ))
	{
		if (!p->stripFooter( p->seq() ))
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): "
				"Not enough data for sequence number footer (%d bytes left)\n",
				addr.c_str(), p->bodySize() );

			RETURN_FOR_CORRUPTED_PACKET();
		}
	}

	// now do something if it's reliable
	if (p->hasFlags( Packet::FLAG_IS_RELIABLE ))
	{
		// first make sure it has a sequence number, so we can address it
		if (p->seq() == Channel::SEQ_NULL)
		{
			WARNING_MSG( "Nub::processFilteredPacket( %s ): Dropping packet "
				"due to illegal request for reliability "
				"without related sequence number\n", addr.c_str() );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		// should we be looking in a channel
		if (pChannel)
		{
			// Queue an ACK for this packet on the next outgoing bundle.
			std::pair< Packet*, bool > result =
				pChannel->queueAckForPacket( p, p->seq(), addr );

			if (pChannel->isIrregular())
			{
				shouldSendChannel = true;
			}

			// A NULL return packet indicates that the packet should not be
			// processed right now.
			if (result.first == NULL)
			{
				// The packet is not corrupted, and has either already been
				// received, or is too early and has been buffered. In either
				// case, we send the ACK immediately, as long as the channel is
				// established.
				if (result.second)
				{
					++numDuplicatePacketsReceived_;

					if (pChannel->isEstablished() && shouldSendChannel)
						pChannel->send();

					return REASON_SUCCESS;
				}

				// The packet has an invalid sequence number.
				else
				{
					RETURN_FOR_CORRUPTED_PACKET();
				}
			}
		}

		// If the packet is not on a channel (i.e. FLAG_ON_CHANNEL is not
		// set), it must be once-off reliable.
		else
		{
			// Don't allow incoming once-off-reliable traffic on external nubs,
			// since this is a potential DOS vulnerability.
			if (isExternal_)
			{
				WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"Dropping illegal once-off-reliable packet\n",
					addr.c_str() );

				RETURN_FOR_CORRUPTED_PACKET();
			}

			// send back the ack for this packet
			Bundle backBundle;
			backBundle.addAck( p->seq() );
			this->send( addr, backBundle );

			if (this->onceOffReliableReceived( addr, p->seq() ))
			{
				return REASON_SUCCESS;
			}
		}
	}
	else // if !RELIABLE
	{
		// If the packet is unreliable, confirm that the sequence number
		// is incrementing as a sanity check against malicious parties.
		if ((pChannel != NULL) && (pChannel->isExternal()))
		{
			if (!pChannel->validateUnreliableSeqNum( p->seq() ))
			{
				WARNING_MSG( "Nub::processFilteredPacket( %s ): "
					"Dropping packet due to invalid unreliable seqNum\n",
					addr.c_str() );
				RETURN_FOR_CORRUPTED_PACKET();
			}
		}

	}

	Reason oret = REASON_SUCCESS;
	PacketPtr pCurrPacket = p;
	PacketPtr pNextPacket = NULL;

	// push this packet chain (frequently one) through processOrderedPacket

	// NOTE: We check isCondemned on the channel and not isDead. If a channel has
	// isDestroyed set to true but isCondemned false, we still want to process
	// remaining messages. This can occur if there is a message that causes the
	// entity to teleport. Any remaining messages are still processed and will likely
	// be forwarded from the ghost entity to the recently teleported real entity.

	while (pCurrPacket &&
		((pChannel == NULL) || !pChannel->isCondemned()))
	{
		// processOrderedPacket expects packets not to be chained, since
		// chaining is used for grouping fragments into bundles.  The packet
		// chain we've set up doesn't have anything to do with bundles, so we
		// break the packet linkage before passing the packets into
		// processOrderedPacket.  This can mean that packets that aren't the one
		// just received drop their last reference, hence the need for
		// pCurrPacket and pNextPacket.
		pNextPacket = pCurrPacket->next();
		pCurrPacket->chain( NULL );

		// At this point, the only footers left on the packet should be the
		// request and fragment footers.
		Reason ret = this->processOrderedPacket(
			addr, pCurrPacket.get(), pChannel.get() );

		if (oret == REASON_SUCCESS)
		{
			oret = ret;
		}

		pCurrPacket = pNextPacket;
	}

	// If this bundle was delivered to a channel and there are still ACKs to
	// send, do it now.
	if (pChannel &&
		!pChannel->isDead() &&
		shouldSendChannel &&
		pChannel->isEstablished() &&
		(pChannel->bundle().firstPacket_->nAcks() > 0))
	{
		pChannel->send();
	}

	return oret;
}

/**
 * Process a packet after any ordering guaranteed by reliable channels
 * has been imposed (further ordering guaranteed by fragmented bundles
 * is still to be imposed)
 */
Reason Nub::processOrderedPacket( const Address & addr, Packet * p,
	Channel * pChannel )
{
	// Label to use in debug output
#	define SOURCE_STR (pChannel ? pChannel->c_str() : addr.c_str())

	// Strip first request offset.
	if (p->hasFlags( Packet::FLAG_HAS_REQUESTS ))
	{
		if (!p->stripFooter( p->firstRequestOffset() ))
		{
			WARNING_MSG( "Nub::processPacket( %s ): "
				"Not enough data for first request offset (%d bytes left)\n",
				SOURCE_STR, p->bodySize() );

			RETURN_FOR_CORRUPTED_PACKET();
		}
	}

	// Smartpointer required to keep the packet chain alive until the
	// outputBundle is constructed in the event a fragment chain completes.
	PacketPtr pChain = NULL;

	// Strip fragment footers
	if (p->hasFlags( Packet::FLAG_IS_FRAGMENT ))
	{
		if (p->bodySize() < int( sizeof( SeqNum ) * 2 ))
		{
			WARNING_MSG( "Nub::processPacket( %s ): "
				"Not enough footers for fragment spec "
				"(have %d bytes but need %zu)\n",
				SOURCE_STR, p->bodySize(), 2 * sizeof( SeqNum ) );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		// Take off the fragment sequence numbers.
		p->stripFooter( p->fragEnd() );
		p->stripFooter( p->fragBegin() );

		const int numFragmentsInBundle = p->fragEnd() - p->fragBegin() + 1;

		// TODO: Consider a maximum number of fragments per packet,
		// smaller than this (about 2^31 :)
		if (numFragmentsInBundle < 2)
		{
			WARNING_MSG( "Nub::processPacket( %s ): "
				"Dropping fragment due to illegal bundle fragment count (%d)\n",
				SOURCE_STR, numFragmentsInBundle );

			RETURN_FOR_CORRUPTED_PACKET();
		}

		FragmentedBundle::Key key( addr, p->fragBegin() );

		// Find the existing packet chain for this bundle, if any.
		FragmentedBundlePtr pFragments = NULL;
		FragmentedBundles::iterator fragIter = fragmentedBundles_.end();

		// Channels are easy, they just maintain their own fragment chain, which
		// makes lookup really cheap.  Note that only reliable packets use the
		// channel's sequence numbers.
		bool isOnChannel = pChannel && p->hasFlags( Packet::FLAG_IS_RELIABLE );

		if (isOnChannel)
		{
			pFragments = pChannel->pFragments();
		}

		// We need to look up off-channel fragments in fragmentedBundles_
		else
		{
			fragIter = fragmentedBundles_.find( key );
			pFragments = (fragIter != fragmentedBundles_.end()) ?
				fragIter->second : NULL;
		}

		// If the previous fragment is really old, then this must be some failed bundle
		// that has not been resent and has been given up on, so we get rid of it now.
		if (pFragments && pFragments->isOld() && !isOnChannel)
		{
			WARNING_MSG( "Nub::processPacket( %s ): "
				"Discarding abandoned stale overlapping fragmented bundle "
				"from seq %u to %u\n",
				SOURCE_STR, p->fragBegin(), pFragments->lastFragment_ );

			pFragments = NULL;

			fragmentedBundles_.erase( fragIter );
		}

		if (pFragments == NULL)
		{
			// If this is on a channel, it must be the first packet in the
			// bundle, since channel traffic is ordered.
			if (pChannel && p->seq() != p->fragBegin())
			{
				ERROR_MSG( "Nub::processOrderedPacket( %s ): "
					"Bundle (#%u,#%u) is missing packets before #%u\n",
					SOURCE_STR, p->fragBegin(), p->fragEnd(), p->seq() );

				RETURN_FOR_CORRUPTED_PACKET();
			}

			// This is the first fragment from this bundle we've seen, so make a
			// new element and bail out.
			pFragments = new FragmentedBundle(
				p->fragEnd(),
				numFragmentsInBundle - 1,
				timestamp(),
				p );

			if (isOnChannel)
			{
				pChannel->pFragments( pFragments );
			}
			else
			{
				fragmentedBundles_[ key ] = pFragments;
			}

			return REASON_SUCCESS;
		}

		// If the last fragment seqnums for existing packets in this bundle
		// and this one don't match up, we can't process it.
		if (pFragments->lastFragment_ != p->fragEnd())
		{
			// If the incoming fragment is unreliable, then we're still waiting
			// for the reliable bundle attached to 'pFragments' to complete.
			if (!p->hasFlags( Packet::FLAG_IS_RELIABLE ))
			{
				MF_ASSERT( pFragments->isReliable() || isExternal_ );

				WARNING_MSG( "Nub::processPacket( %s ): "
					"Discarding unreliable fragment #%u (#%u,#%u) while "
					"waiting for reliable chain (#%u,#%u) to complete\n",
					SOURCE_STR, p->seq(), p->fragBegin(), p->fragEnd(),
					pFragments->pChain_->seq(), pFragments->lastFragment_ );

				return REASON_SUCCESS;
			}

			// If we're on an external nub, then it could be someone mangling
			// the packets on purpose.
			if (isExternal_)
			{
				WARNING_MSG( "Nub::processPacket( %s ): "
					"Mangled fragment footers, "
					"lastFragment(%u) != p->fragEnd()(%u)\n",
					SOURCE_STR, pFragments->lastFragment_, p->fragEnd() );

				RETURN_FOR_CORRUPTED_PACKET();
			}

			// We should never get this for reliable traffic on internal nubs.
			else
			{
				CRITICAL_MSG( "Nub::processPacket( %s ): "
					"Mangled fragment footers, "
					"lastFragment(%u) != p->fragEnd()(%u)\n",
					(char*)addr, pFragments->lastFragment_, p->fragEnd() );
			}
		}

		pFragments->touched_ = timestamp();

		// find where this goes in the chain
		Packet * pre = NULL;
		Packet * walk;

		for (walk = pFragments->pChain_.get(); walk; walk = walk->next())
		{
			// If p is already in this chain, bail now
			if (walk->seq() == p->seq())
			{
				WARNING_MSG( "Nub::processPacket( %s ): "
					"Discarding duplicate fragment #%u\n",
					SOURCE_STR, p->seq() );

				return REASON_SUCCESS;
			}

			// Stop when 'walk' is the packet in the chain after p.
			if (Channel::seqLessThan( p->seq(), walk->seq() ))
			{
				break;
			}

			pre = walk;
		}

		// add it to the chain
		p->chain( walk );

		if (pre == NULL)
		{
			pFragments->pChain_ = p; // pFragments -> chain -> p -> walk
		}
		else
		{
			pre->chain( p ); // pFragments -> chain -> ... -> pre -> p -> walk
		}

		// If the bundle is still incomplete, stop processing now.
		if (--pFragments->remaining_ > 0)
		{
			return REASON_SUCCESS;
		}

		// The bundle is now complete, so set p to the start of the chain and
		// we'll process the whole bundle below.
		else
		{
			// We need to acquire a reference to the fragment chain here to keep
			// it alive until we construct the outputBundle below.
			pChain = pFragments->pChain_;
			p = pChain.get();

			if (isOnChannel)
			{
				pChannel->pFragments( NULL );
			}
			else
			{
				fragmentedBundles_.erase( fragIter );
			}
		}
	}

	// We have a complete packet chain.  We can drop the reference in pChain now
	// since the Bundle owns it.
	Bundle outputBundle( p );
	pChain = NULL;

	breakBundleLoop_ = false;
	Reason ret = REASON_SUCCESS;

	// NOTE: The channel may be destroyed while processing the messages so it is
	// important not to use it after the first call to handleMessage.
	MessageFilterPtr pMessageFilter =
		pChannel ? pChannel->pMessageFilter() : NULL;

	// now we simply iterate over the messages in that bundle
	Bundle::iterator iter	= outputBundle.begin();
	Bundle::iterator end	= outputBundle.end();
	while (iter != end && !breakBundleLoop_)
	{
		// find out what this message looks like
		InterfaceElementWithStats & ie = interfaceTable_[ iter.msgID() ];
		if (ie.pHandler() == NULL)
		{
			// If there aren't any interfaces served on this nub
			// then don't print the warning (slightly dodgy I know)
			ERROR_MSG( "Nub::processOrderedPacket( %s ): "
				"Discarding bundle after hitting unhandled message id %d\n",
				SOURCE_STR, (int)iter.msgID() );

			// Note: Early returns are OK because the bundle will
			// release the packets it owns for us!
			ret = REASON_NONEXISTENT_ENTRY;
			break;
		}

		// get the details out of it
		UnpackedMessageHeader & header = iter.unpack( ie );
		header.pNub = this;
		header.pChannel = pChannel;
		if (header.flags & Packet::FLAG_IS_FRAGMENT)
		{
			ERROR_MSG( "Nub::processOrderedPacket( %s ): "
				"Discarding bundle due to corrupted header for message id %d\n",
				SOURCE_STR, (int)iter.msgID() );

			// dumpBundleMessages( outputBundle );
			numCorruptedPacketsReceived_++;
			ret = REASON_CORRUPTED_PACKET;
			break;
		}

		// get the data out of it
		const char * msgData = iter.data();
		if (msgData == NULL)
		{
			ERROR_MSG( "Nub::processOrderedPacket( %s ): "
				"Discarding rest of bundle since chain too short for data of "
				"message id %d length %d\n",
				SOURCE_STR, (int)iter.msgID(), header.length );

			// dumpBundleMessages( outputBundle );
			numCorruptedPacketsReceived_++;
			ret = REASON_CORRUPTED_PACKET;
			break;
		}

		// make a stream to belay it
		MemoryIStream mis = MemoryIStream( msgData, header.length );

		numMessagesReceived_++;
		ie.messageReceived( header.length );
		numOverheadBytesReceived_ -= header.length;

		recvMercuryTimer_.stop();

		if (!pMessageFilter)
		{
			// and call the handler
			ie.pHandler()->handleMessage( addr, header, mis );
		}
		else
		{
			// or pass to our channel's message filter if it has one
			pMessageFilter->filterMessage( addr, header, mis, ie.pHandler() );
		}

		recvMercuryTimer_.start();

		// next! (note: can only call this after unpack)
		iter++;

		if (mis.remainingLength() != 0)
		{
			if (header.identifier == REPLY_MESSAGE_IDENTIFIER)
			{
				WARNING_MSG( "Nub::processOrderedPacket( %s ): "
					"Handler for reply left %d bytes\n",
					SOURCE_STR, mis.remainingLength() );

			}
			else
			{
				WARNING_MSG( "Nub::processOrderedPacket( %s ): "
					"Handler for message %s (id %d) left %d bytes\n",
					SOURCE_STR, ie.name(),
					header.identifier, mis.remainingLength() );
			}
		}
	}

	if (iter != end && !breakBundleLoop_)
	{
		numCorruptedBundlesReceived_++;
	}
	else
	{
		numBundlesReceived_++;
	}

	if (pBundleFinishHandler_)
	{
		pBundleFinishHandler_->onBundleFinished();
	}

	return ret;
#	undef SOURCE_STR
}


/**
 *  This method reads data from the stream into a packet and then processes it.
 */
Reason Nub::processPacketFromStream( const Address & addr, BinaryIStream & data )
{
	// Set up a fresh packet from the message and feed it to the nub
	PacketPtr pPacket = new Packet();
	int len = data.remainingLength();

	memcpy( pPacket->data(), data.retrieve( len ), len );
	pPacket->msgEndOffset( len );

	return this->processPacket( addr, pPacket.get() );
}


/**
 *  This method returns true if this fragmented bundle is too old and should be
 *  discarded.
 */
bool Nub::FragmentedBundle::isOld() const
{
	return timestamp() - touched_ > stampsPerSecond() * MAX_AGE;
}


/**
 *  This method returns true if this fragmented bundle is reliable.
 */
bool Nub::FragmentedBundle::isReliable() const
{
	return pChain_->hasFlags( Packet::FLAG_IS_RELIABLE );
}


/**
 *	This method dumps the messages in a (received) bundle.
 */
void Nub::dumpBundleMessages( Bundle & outputBundle )
{
	Bundle::iterator iter	= outputBundle.begin();
	Bundle::iterator end	= outputBundle.end();
	int count = 0;

	while (iter != end && count < 1000)	// can get stuck here
	{
		InterfaceElement & ie = interfaceTable_[ iter.msgID() ];

		if (ie.pHandler() != NULL)
		{
			UnpackedMessageHeader & header =
				iter.unpack( ie );

			WARNING_MSG( "\tMessage %d: ID %d, len %d\n",
					count, header.identifier, header.length );
		}

		iter++;
		count++;
	}
}


/**
 * 	This method handles internal Mercury messages.
 * 	Currently the only such message is a reply message.
 */
void Nub::handleMessage( const Address & source,
	UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	// first let's do some sanity checks
	if (header.identifier != REPLY_MESSAGE_IDENTIFIER)
	{
		ERROR_MSG( "Mercury::Nub::handleMessage( %s ): "
			"received the wrong kind of message!\n",
			source.c_str() );

		return;
	}

	if (header.length < (int)(sizeof(int)))
	{
		ERROR_MSG( "Mercury::Nub::handleMessage( %s ): "
			"received a corrupted reply message (length %d)!\n",
			source.c_str(), header.length );

		return;
	}

	int		inReplyTo;
	data >> inReplyTo;
	header.length -= sizeof(int);
	// note 'header' is not const for this reason!

	// see if we can look up this ID
	ReplyHandlerMap::iterator rheIterator = replyHandlerMap_.find( inReplyTo );
	if (rheIterator == replyHandlerMap_.end())
	{
		WARNING_MSG( "Mercury::Nub::handleMessage( %s ): "
			"Couldn't find handler for reply id 0x%08x (maybe it timed out?)\n",
			source.c_str(), inReplyTo );

		data.finish();
		return;
	}

	ReplyHandlerElement	* pRHE = rheIterator->second;

	// Check the message came from the right place.  We only enforce this check
	// on external nubs because replies can come from different addresses on
	// internal nubs if a channel has been offloaded.
	if (isExternal_ &&
			((pRHE->pChannel_ == NULL) || (source != pRHE->pChannel_->addr())))
	{
		WARNING_MSG( "Mercury::Nub::handleMessage: "
			"Got reply to request %d from unexpected source %s\n",
			inReplyTo, source.c_str() );

		return;
	}

	// cancel the timer if it's got one
	if (pRHE->timerID_ != TIMER_ID_NONE)
	{
		this->cancelTimer( pRHE->timerID_ );
		pRHE->timerID_ = TIMER_ID_NONE;
	}

	// get it out of the pending map
	replyHandlerMap_.erase( rheIterator );

	// finally we call the reply handler
	pRHE->pHandler_->handleMessage( source, header, data, pRHE->arg_ );

	// and then clean up
	delete pRHE;
}


/**
 *	This is a helper method that creates timers.
 *	@see registerTimer
 *	@see registerCallback
 */
TimerID Nub::newTimer( int microseconds,
	TimerExpiryHandler * handler,
	void * arg,
	bool recurrent )
{
	MF_ASSERT( handler );

	if (microseconds <= 0) return Mercury::TIMER_ID_NONE;
	int64 interval = int64(
		( ((double)microseconds)/1000000.0 ) * stampsPerSecondD());

	// make up the timer queue element
	TimerQueueElement *tqe = new TimerQueueElement;
	tqe->deliveryTime = timestamp() + interval;
	tqe->intervalTime = recurrent ? interval : 0;
	tqe->state = TimerQueueElement::STATE_PENDING;
	tqe->arg = arg;
	tqe->handler = handler;

	// put it in the priority queue
	timerQueue_.push( tqe );

	// and element pointer as its id (solves lots of problems!)
	return TimerID( tqe );
}


/**
 * 	This method stops a timer. Ideally called from within the
 * 	TimerExpiryHandler.
 *
 * 	@param id	The id of the timer to cancel.
 *
 * 	@return true if timer found and cancelled
 */
void Nub::cancelTimer( TimerID id )
{
	// extract a TimerQueueElement pointer from the id (easy!)
	TimerQueueElement * tqe = (TimerQueueElement*)id;

	// ok, we can't get rid of it completely yet, or else
	// it'd be too expensive ('tho we do trade it off with
	// larger priority queue sizes), so set it to be cancelled.
	tqe->state = TimerQueueElement::STATE_CANCELLED;
		// even if it was STATE_EXECUTING
}

/**
 *	This method stops all timers associated with the input handler.
 *
 *	@param	pHandler The handler whose timers are to be stopped.
 *
 * 	@return The number of timers stopped.
 */
int Nub::cancelTimers( TimerExpiryHandler * pHandler )
{
	int numRemoved = 0;

	typedef TimerQueueElement * const * Iter;

	Iter begin = &timerQueue_.top();
	Iter end = begin + timerQueue_.size();
	for (Iter iter = begin; iter != end; iter++)
	{
		if ((*iter)->handler == pHandler)
		{
			(*iter)->state = TimerQueueElement::STATE_CANCELLED;
			numRemoved++;
		}
	}

	if (pCurrentTimer_ && (pCurrentTimer_->handler == pHandler))
	{
		if (numRemoved == 0)
		{
			NOTICE_MSG( "Nub::cancelTimers: It is more efficient to use "
					"Nub::cancelTimer to cancel the current timer\n" );
		}

		pCurrentTimer_->state = TimerQueueElement::STATE_CANCELLED;
		numRemoved++;
	}

	return numRemoved;
}


/**
 * Given a pointer to a reply handler, this method will remove any reference to
 * it in the reply handler elements so will not receive a message or time out
 * when it is not supposed to.
 */
void Nub::cancelReplyMessageHandler( ReplyMessageHandler * pHandler,
	   Reason reason )
{
	ReplyHandlerMap::iterator iter = replyHandlerMap_.begin();

	while (iter != replyHandlerMap_.end())
	{
		ReplyHandlerElement * pElement = iter->second;

		// Note: handleFailure can modify the map so iter needs to be moved
		// early.
		++iter;

		if (pElement->pHandler_ == pHandler)
		{
			pElement->handleFailure( *this, reason );
		}
	}

}


/**
 *	This method returns the time that the given timer id will be delivered,
 *	in timestamps.
 */
uint64 Nub::timerDeliveryTime( TimerID id ) const
{
	TimerQueueElement * tqe = (TimerQueueElement*)id;
	return (tqe->state == TimerQueueElement::STATE_EXECUTING) ?
			(tqe->deliveryTime + tqe->intervalTime) : tqe->deliveryTime;
}

/**
 *	This method returns the time between deliveries of the given timer id,
 *	in timestamps. The value returned may be modified.
 */
uint64 & Nub::timerIntervalTime( TimerID id )
{
	TimerQueueElement * tqe = (TimerQueueElement*)id;
	return tqe->intervalTime;
}


/**
 * 	This method registers a file descriptor with the nub. The handler
 * 	is called every time input is detected on that file
 * 	descriptor. Unlike timers, these handlers are only
 * 	called if you use processContinuously
 *
 *	@param fd			The file descriptor to register
 *	@param handler		The handler to receive notification messages.
 *
 * 	@return true if descriptor could be registered
 */
bool Nub::registerFileDescriptor( int fd,
	InputNotificationHandler * handler )
{
#ifndef _WIN32
	if ((fd < 0) || (FD_SETSIZE <= fd))
	{
		ERROR_MSG( "Nub::registerFileDescriptor: "
			"Tried to register invalid fd %d. FD_SETSIZE (%d)\n",
			fd, FD_SETSIZE );

		return false;
	}
#endif

	// Bail early if it's already in the read set
	if (FD_ISSET( fd, &fdReadSet_ ))
		return false;

	FD_SET( fd, &fdReadSet_ );
	fdHandlers_[fd] = handler;

	if (fd > fdLargest_) fdLargest_ = fd;

	return true;
}


/**
 * 	This method registers a file descriptor with the nub. The handler is called
 *	every time a write event is detected on that file descriptor. Unlike timers,
 *	these handlers are only called if you use processContinuously.
 *
 *	@param fd			The file descriptor to register
 *	@param handler		The handler to receive notification messages.
 *
 * 	@return true if descriptor could be registered
 */
bool Nub::registerWriteFileDescriptor( int fd,
	InputNotificationHandler * handler )
{
#ifndef _WIN32
	if ((fd < 0) || (FD_SETSIZE <= fd))
	{
		ERROR_MSG( "Nub::registerWriteFileDescriptor: "
			"Tried to register invalid fd %d. FD_SETSIZE (%d)\n",
			fd, FD_SETSIZE );

		return false;
	}
#endif

	if(FD_ISSET( fd, &fdWriteSet_ )) return false;

	FD_SET( fd, &fdWriteSet_ );
	fdWriteHandlers_[fd] = handler;

	if (fd > fdLargest_) fdLargest_ = fd;

	++fdWriteCount_;
	return true;
}



/**
 * 	This method stops watching out for input on this file descriptor.
 *
 *	@param fd	The fd to stop watching.
 *
 * 	@return true if descriptor could be deregistered.
 */
bool Nub::deregisterFileDescriptor( int fd )
{
#ifndef _WIN32
	if ((fd < 0) || (FD_SETSIZE <= fd))
	{
		return false;
	}
#endif

	if(!FD_ISSET( fd, &fdReadSet_ )) return false;

	FD_CLR( fd, &fdReadSet_ );
	fdHandlers_[fd] = NULL;

	if (fd == fdLargest_)
	{
		this->findLargestFileDescriptor();
	}

	return true;
}


/**
 * 	This method stops watching out for write events on this file descriptor.
 *
 *	@param fd	The fd to stop watching.
 *
 * 	@return true if descriptor could be deregistered.
 */
bool Nub::deregisterWriteFileDescriptor( int fd )
{
#ifndef _WIN32
	if ((fd < 0) || (FD_SETSIZE <= fd))
	{
		return false;
	}
#endif

	if(!FD_ISSET( fd, &fdWriteSet_ )) return false;

	FD_CLR( fd, &fdWriteSet_ );
	fdWriteHandlers_[fd] = NULL;

	if (fd == fdLargest_)
	{
		this->findLargestFileDescriptor();
	}

	--fdWriteCount_;
	return true;
}

/**
 * 	This method sets a handler that will be called after each successful bundle
 *	has finished being processed.
 */
void Nub::pBundleFinishHandler( BundleFinishHandler * pHandler )
{
	pBundleFinishHandler_ = pHandler;
}


/**
 *  Finds the highest file descriptor in the read and write sets and writes it
 *  to fdLargest_.
 */
void Nub::findLargestFileDescriptor()
{
#ifdef _WIN32
	fdLargest_ = 0;

	for (unsigned i=0; i < fdReadSet_.fd_count; ++i)
		if ((int)fdReadSet_.fd_array[i] > fdLargest_)
			fdLargest_ = fdReadSet_.fd_array[i];

	for (unsigned i=0; i < fdWriteSet_.fd_count; ++i)
		if ((int)fdWriteSet_.fd_array[i] > fdLargest_)
			fdLargest_ = fdWriteSet_.fd_array[i];

#else
	while (fdLargest_ > 0 &&
		!FD_ISSET( fdLargest_, &fdReadSet_ ) &&
		!FD_ISSET( fdLargest_, &fdWriteSet_ ))
	{
		fdLargest_--;
	}
#endif
}


/**
 *  This method deregisters a child nub from this nub.
 */
bool Nub::deregisterChildNub( Nub * pChildNub )
{
	if (pChildNub->pMasterNub_ != this)
	{
		WARNING_MSG( "Nub::deregisterChildNub: "
				"Input nub is not a child of this nub.\n" );

		return false;
	}

	this->deregisterFileDescriptor( pChildNub->socket_ );

	ChildNubs::iterator iter = std::find(
		childNubs_.begin(), childNubs_.end(), pChildNub );

	MF_ASSERT( iter != childNubs_.end() );
	childNubs_.erase( iter );

	// Cancel child nub timer if no more active child nubs.
	if (childNubs_.empty())
	{
		this->cancelTimer( tickChildNubsTimerID_ );
		tickChildNubsTimerID_ = TIMER_ID_NONE;
	}

	pChildNub->pMasterNub_ = NULL;
	return true;
}


/**
 *	This method is called when a channel has been condemned. It always the Nub
 *	to indicate failure for requests that are currently on that channel.
 */
void Nub::onChannelGone( Channel * pChannel )
{
	this->cancelRequestsFor( pChannel );
}


/**
 *	This method cancels the requests for the input channel.
 */
void Nub::cancelRequestsFor( Channel * pChannel )
{
	ReplyHandlerMap::iterator iter = replyHandlerMap_.begin();

	while (iter != replyHandlerMap_.end())
	{
		ReplyHandlerElement * pElement = iter->second;

		// Note: handleFailure can modify the map so iter needs to be moved
		// early.
		++iter;

		if (pElement->pChannel_ == pChannel)
		{
			pElement->handleFailure( *this, REASON_CHANNEL_LOST );
		}
	}
}


/**
 *  Register a nub as a slave to this nub, meaning that any call to
 *  processContinuously() or processPendingEvents() will also process incoming
 *  traffic and timers on the child nub.
 */
bool Nub::registerChildNub( Nub * pChildNub,
	InputNotificationHandler * pHandler )
{
	// If the child nub is already registered with another master nub,
	// deregister it now.
	if (pChildNub->pMasterNub_)
	{
		pChildNub->pMasterNub_->deregisterChildNub( pChildNub );
	}

	// If no handler passed, use the nub itself as the handler.
	if (!pHandler)
	{
		pHandler = pChildNub;
	}

	bool ret = this->registerFileDescriptor( pChildNub->socket_, pHandler );

	if (ret)
	{
		if (childNubs_.empty())
		{
			tickChildNubsTimerID_ =
				this->registerTimer( CHILD_NUB_TICK_PERIOD, this );
		}

		childNubs_.push_back( pChildNub );
		pChildNub->pMasterNub_ = this;
	}

	return ret;
}


/**
 * 	This method returns the amount of time (in timestamps) that the nub has
 * 	been idle. Currently the only activity it does in its spare time is
 * 	'select'. This can be used as a high-response measure of unused
 * 	processor time.
 */
uint64 Nub::getSpareTime() const
{
	return spareTime_;
}

/**
 * 	This method resets the spare time counter.
 */
void Nub::clearSpareTime()
{
	accSpareTime_ += spareTime_;
	spareTime_ = 0;
}

/**
 * 	This method returns the amount of spare time in the last statistics period.
 * 	This should only be used for monitoring statistics, as it may
 * 	be up to a whole period (currently 1s) out of date, and does
 * 	not assure the same accuracy that the methods above do.
 */
double Nub::proportionalSpareTime() const
{
	double ret = (double)(int64)(totSpareTime_ - oldSpareTime_);
	return ret/stampsPerSecondD();
}


double Nub::peakCalculator( LastVisitTime idx, const unsigned int & now,
		unsigned int & last ) const
{
	uint64 currTime = timestamp();
	double delta = double(currTime - lastVisitTime_[idx]) / stampsPerSecondD();
	lastVisitTime_[idx] = currTime;
	double items = now - last;
	last = now;
	return items / delta;
}


/**
 *	This method switches the socket used by this Nub with the socket used by the
 *	input Nub. It also switches the advertised address.
 *
 *	This is used by ServerConnection when it attempts to connect to the BaseApp
 *	from multiple sockets with only one winning. The winning Nub then swaps with
 *	the main Nub.
 *
 *	Care must be taken when using this method that no outstanding packets are
 *	waiting the sockets.
 */
void Nub::switchSockets( Nub * pOtherNub )
{
	int tempFD = (int)socket_;
	Address tempAddr = advertisedAddress_;

	this->deregisterFileDescriptor( socket_ );
	this->registerFileDescriptor( pOtherNub->socket_, NULL );
	pOtherNub->deregisterFileDescriptor( pOtherNub->socket_ );
	pOtherNub->registerFileDescriptor( socket_, NULL );

	socket_.setFileDescriptor( pOtherNub->socket_ );
	advertisedAddress_ = pOtherNub->advertisedAddress_;

	pOtherNub->socket_.setFileDescriptor( tempFD );
	pOtherNub->advertisedAddress_ = tempAddr;
}

namespace
{
	/**
	 *	This simple class is used to handle setting the hijack stream. It is
	 *	helpful for handling the case where processPendingEvents throws an
	 *	exception.
	 */
	class Hijacker
	{
	public:
		Hijacker( BinaryIStream & stream )
		{
			Endpoint::setHijackStream( &stream );
		}

		~Hijacker()
		{
			Endpoint::setHijackStream( NULL );
		}
	};
} // anonymous namespace


/**
 *	This method is used to process "artificial" data received from a front-end
 *	process.
 */
bool Nub::handleHijackData( int fd, BinaryIStream & stream )
{
	// Set the data the Endpoint::recvfrom etc will use.
	Hijacker hijacker( stream );

	if (fd == socket_)
	{
		this->processPendingEvents( /* expectingPacket: */ true );
		return true;
	}
	else
	{
		InputNotificationHandler *	pHandler = fdHandlers_[fd];

		if (pHandler)
		{
			pHandler->handleInputNotification( fd );
			return true;
		}
		else
		{
			ERROR_MSG( "Nub::handleHijackData: No handler for %d\n", fd );
		}
	}

	return false;
}


#if ENABLE_WATCHERS
/**
 * 	This method returns the generic watcher for Nubs.
 */
WatcherPtr Nub::pWatcher()
{
	static DirectoryWatcherPtr watchMe = NULL;

	if (watchMe == NULL)
	{
		watchMe = new DirectoryWatcher();

		Nub		*pNull = NULL;

		// First the really useful ones

		watchMe->addChild( "address", &Address::watcher(),
			(void*)&pNull->advertisedAddress_ );

		// The interface indexed by ID.
		{
			SequenceWatcher< InterfaceTable > * pWatcher =
				new SequenceWatcher< InterfaceTable >( pNull->interfaceTable_ );
			pWatcher->addChild( "*", InterfaceElementWithStats::pWatcher() );

			watchMe->addChild( "interfaceByID", pWatcher );
		}

		// The interface indexed by name.
		{
			SequenceWatcher< InterfaceTable > * pWatcher =
				new SequenceWatcher< InterfaceTable >( pNull->interfaceTable_ );
			pWatcher->setLabelSubPath( "name" );
			pWatcher->addChild( "*", InterfaceElementWithStats::pWatcher() );

			watchMe->addChild( "interfaceByName", pWatcher );
		}

#ifdef unix
		watchMe->addChild( "socket/transmitQueue",
			makeWatcher( pNull->lastTxQueueSize_ ) );

		watchMe->addChild( "socket/receiveQueue",
			makeWatcher( pNull->lastRxQueueSize_ ) );

		watchMe->addChild( "socket/maxTransmitQueue",
			makeWatcher( pNull->maxTxQueueSize_ ) );

		watchMe->addChild( "socket/maxReceiveQueue",
			makeWatcher( pNull->maxRxQueueSize_ ) );
#endif

		watchMe->addChild( "timing/spareTime",
			makeWatcher( *pNull, &Nub::proportionalSpareTime ) );
		watchMe->addChild( "timing/totalSpareTime",
				makeWatcher( pNull->totSpareTime_ ) );

		// Now the not so useful ones
		watchMe->addChild( "socket/socket",
			new MemberWatcher<int,Endpoint>(
				pNull->socket_,
				&Endpoint::operator int,
				static_cast< void (Endpoint::*)( int ) >( NULL ) ) );
		// timerMap_
		// replyHandlerMap_

		watchMe->addChild( "artificialLoss/dropPerMillion",
			makeWatcher( pNull->artificialDropPerMillion_,
				Watcher::WT_READ_WRITE ) );

		watchMe->addChild( "artificialLoss/minLatency",
			makeWatcher( pNull->artificialLatencyMin_,
				Watcher::WT_READ_WRITE ) );

		watchMe->addChild( "artificialLoss/maxLatency",
			makeWatcher( pNull->artificialLatencyMax_,
				Watcher::WT_READ_WRITE ) );

		watchMe->addChild( "misc/nextReplyID",
			makeWatcher( pNull->nextReplyID_ ) );

		watchMe->addChild( "misc/nextSequenceID",
			makeWatcher( pNull->nextSequenceID_ ) );

		watchMe->addChild( "misc/breakProcessing",
			makeWatcher( pNull->breakProcessing_, Watcher::WT_READ_WRITE ) );

		watchMe->addChild( "misc/largestFD",
			makeWatcher( pNull->fdLargest_ ) );

		watchMe->addChild( "timing/mercurySend",
				makeWatcher( pNull->sendMercuryTimer_ ) );
		watchMe->addChild( "timing/systemSend",
				makeWatcher( pNull->sendSystemTimer_ ) );

		watchMe->addChild( "timing/mercuryRecv",
				makeWatcher( pNull->recvMercuryTimer_ ) );
		watchMe->addChild( "timing/systemRecv",
				makeWatcher( pNull->recvSystemTimer_ ) );

		watchMe->addChild( "totals/failedPacketSends",
			makeWatcher( pNull->numFailedPacketSend_ ) );
		watchMe->addChild( "totals/failedBundleSends",
			makeWatcher( pNull->numFailedBundleSend_ ) );
		watchMe->addChild( "totals/corruptedPacketsReceived",
			makeWatcher( pNull->numCorruptedPacketsReceived_ ) );
		watchMe->addChild( "totals/corruptedBundlesReceived",
			makeWatcher( pNull->numCorruptedBundlesReceived_ ) );

		watchMe->addChild( "totals/bytesSent",
			makeWatcher( pNull->numBytesSent_ ) );
		watchMe->addChild( "averages/bytesSentPerSecond",
			makeWatcher( *pNull, &Nub::bytesSentPerSecondAvg ) );
		watchMe->addChild( "lastVisit/bytesSentPerSecond",
			makeWatcher( *pNull, &Nub::bytesSentPerSecondPeak ) );
		watchMe->addChild( "totals/bytesResent",
			makeWatcher( pNull->numBytesResent_ ) );

		watchMe->addChild( "totals/bytesReceived",
			makeWatcher( pNull->numBytesReceived_ ) );
		watchMe->addChild( "averages/bytesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::bytesReceivedPerSecondAvg ) );
		watchMe->addChild( "lastVisit/bytesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::bytesReceivedPerSecondPeak ) );

		watchMe->addChild( "totals/packetsSentOffChannel",
			makeWatcher( pNull->numPacketsSentOffChannel_ ) );
		watchMe->addChild( "totals/packetsSent",
			makeWatcher( pNull->numPacketsSent_ ) );
		watchMe->addChild( "averages/packetsSentPerSecond",
			makeWatcher( *pNull, &Nub::packetsSentPerSecondAvg ) );
		watchMe->addChild( "lastVisit/packetsSentPerSecond",
			makeWatcher( *pNull, &Nub::packetsSentPerSecondPeak ) );
		watchMe->addChild( "totals/packetsResent",
			makeWatcher( pNull->numPacketsResent_ ) );

		watchMe->addChild( "totals/numPiggybacks",
			makeWatcher( pNull->numPiggybacks_ ) );

		watchMe->addChild( "totals/packetsReceivedOffChannel",
			makeWatcher( pNull->numPacketsReceivedOffChannel_ ) );
		watchMe->addChild( "totals/packetsReceived",
			makeWatcher( pNull->numPacketsReceived_ ) );
		watchMe->addChild( "totals/duplicatePacketsReceived",
			makeWatcher( pNull->numDuplicatePacketsReceived_ ) );
		watchMe->addChild( "averages/packetsReceivedPerSecond",
			makeWatcher( *pNull, &Nub::packetsReceivedPerSecondAvg ) );
		watchMe->addChild( "lastVisit/packetsReceivedPerSecond",
			makeWatcher( *pNull, &Nub::packetsReceivedPerSecondPeak ) );

		watchMe->addChild( "totals/bundlesSent",
			makeWatcher( pNull->numBundlesSent_ ) );
		watchMe->addChild( "averages/bundlesSentPerSecond",
			makeWatcher( *pNull, &Nub::bundlesSentPerSecondAvg ) );
		watchMe->addChild( "lastVisit/bundlesSentPerSecond",
			makeWatcher( *pNull, &Nub::bundlesSentPerSecondPeak ) );

		watchMe->addChild( "totals/bundlesReceived",
			makeWatcher( pNull->numBundlesReceived_ ) );
		watchMe->addChild( "averages/bundlesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::bundlesReceivedPerSecondAvg ) );
		watchMe->addChild( "lastVisit/bundlesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::bundlesReceivedPerSecondPeak ) );

		watchMe->addChild( "totals/messagesSent",
			makeWatcher( pNull->numMessagesSent_ ) );
		watchMe->addChild( "totals/messagesSentReliableExt",
			makeWatcher( pNull->numReliableMessagesSent_ ) );
		watchMe->addChild( "averages/messagesSentPerSecond",
			makeWatcher( *pNull, &Nub::messagesSentPerSecondAvg ) );
		watchMe->addChild( "lastVisit/messagesSentPerSecond",
			makeWatcher( *pNull, &Nub::messagesSentPerSecondPeak ) );

		watchMe->addChild( "totals/messagesReceived",
			makeWatcher( pNull->numMessagesReceived_ ) );
		watchMe->addChild( "averages/messagesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::messagesReceivedPerSecondAvg ) );
		watchMe->addChild( "lastVisit/messagesReceivedPerSecond",
			makeWatcher( *pNull, &Nub::messagesReceivedPerSecondPeak ) );
	}

	return watchMe;
}
#endif /* #if ENABLE_WATCHER */


/**
 *	This method makes sure that the cleanup timer is initialised.
 */
void Nub::initOnceOffPacketCleaning()
{
	if (onceOffPacketCleaningTimerID_ == 0)
	{
		// set clean out period to be 10% more than maximum time to expect an
		// ack for a once-off reliable packet
		long onceOffPacketCleaningPeriod = (int)( 1.1 *
			onceOffMaxResends_ * onceOffResendPeriod_ );
		onceOffPacketCleaningTimerID_ =
				this->registerTimer( onceOffPacketCleaningPeriod, this );
	}
}


/**
 *	This method handles timer events.
 */
int Nub::handleTimeout( TimerID id, void * arg )
{
	if (id == onceOffPacketCleaningTimerID_)
	{
		this->onceOffReliableCleanup();
		return 0;
	}

	else if (id == reportLimitTimerID_)
	{
		this->reportPendingExceptions();
		return 0;
	}

	else if (id == tickChildNubsTimerID_)
	{
		// __glenc__ TODO: This is probably quite dodgy if exceptions are
		// thrown.
		ChildNubs::iterator iter = childNubs_.begin();

		while (iter != childNubs_.end())
		{
			// Need to be careful here because calling processPendingEvents()
			// may deregister the child and invalidate the iterator.
			Nub * pChild = *iter++;

			pChild->processPendingEvents( /* expectingPacket: */ false );
		}

		return 0;
	}

	else if (id == clearFragmentedBundlesTimerID_)
	{
		FragmentedBundles::iterator iter = fragmentedBundles_.begin();
		uint64 now = ::timestamp();

		while (iter != fragmentedBundles_.end())
		{
			FragmentedBundles::iterator oldIter = iter++;
			const FragmentedBundle::Key & key = oldIter->first;
			FragmentedBundlePtr pFragments = oldIter->second;
			uint64 age = now - pFragments->touched_;

			if (age > FragmentedBundle::MAX_AGE)
			{
				WARNING_MSG( "Nub::handleTimeout: "
					"Discarded stale fragmented bundle from %s "
					"(%.1fs old, %d packets)\n",
					key.addr_.c_str(), age / stampsPerSecondD(),
					pFragments->pChain_->chainLength() );

				fragmentedBundles_.erase( oldIter );
			}
		}
	}
	else if (id == interfaceStatsTimerID_)
	{
		for (uint i = 0; i < interfaceTable_.size(); ++i)
		{
			interfaceTable_[i].tick();
		}
	}

	else if (arg == (void*)TIMEOUT_RECENTLY_DEAD_CHANNEL)
	{
		// Find the dead channel in the map
		for (RecentlyDeadChannels::iterator iter = recentlyDeadChannels_.begin();
			 iter != recentlyDeadChannels_.end(); ++iter)
		{
			if (iter->second == id)
			{
				recentlyDeadChannels_.erase( iter );
				break;
			}
		}
	}

	return 1;
}


/**
 *	This method cleans up old information about received once-off reliable
 *	packets. This is done by keeping two collections - one for the recent
 *	information and another for the less recent information. This method clears
 *	the less recent information and demotes the recent information to be less
 *	recent.
 */
void Nub::onceOffReliableCleanup()
{
	prevOnceOffReceipts_.clear();
	currOnceOffReceipts_.swap( prevOnceOffReceipts_ );
}



/**
 *	This method adds a resend timer for a once-off reliable packet.
 */
void Nub::addOnceOffResendTimer( const Address & addr, int seq, Packet * p )
{
//	TRACE_MSG( "addOnceOffResendTimer(%s,%d)\n", (char*)addr, seq );
	OnceOffPacket temp( addr, seq, p );

	// a bit tricky here because STL set will make a copy of temp, and only
	// give out const references to it through the iterator
	// It is the set's copy that we want to call registerTimer on with this nub
	OnceOffPackets::iterator actualIter = onceOffPackets_.insert( temp ).first;
	OnceOffPacket * pOnceOffPacket =
		const_cast< OnceOffPacket * > (&(*actualIter) );

	pOnceOffPacket->registerTimer( this );

	// Note: while it would be better to avoid registering a timer for
	// every packet, this does have the advantage of spreading the resend
	// load nicely, as long as this facility is only used sparingly
	// (in which case the resend load is not that great anyway.. :-/ )
}

/**
 *	This method resends a packet that was sent once-off reliably.
 */
void Nub::expireOnceOffResendTimer( OnceOffPacket & oop )
{
	if (++oop.retries_ <= onceOffMaxResends_)
	{
		// If the packet has been dropped or delayed, skip it.
		if (this->rescheduleSend( oop.addr_, oop.pPacket_.get(),
				/* isResend: */ true ))
		{
			return;
		}

		this->sendPacket( oop.addr_, oop.pPacket_.get(), NULL, true );
	}
	else
	{
		OnceOffPackets::iterator iter = onceOffPackets_.find( oop );

		if (iter != onceOffPackets_.end())
		{
			DEBUG_MSG( "Nub::expOnceOffResendTimer( %s ): "
				"Discarding #%d after %d retries\n",
				oop.addr_.c_str(), oop.footerSequence_, onceOffMaxResends_ );

			this->delOnceOffResendTimer( iter );
		}
		else
		{
			CRITICAL_MSG( "Nub::expOnceOffResendTimer( %s ): "
				"Called for #%d that we haven't got!\n",
				oop.addr_.c_str(), oop.footerSequence_ );
		}
	}
}


/**
 *	This method removes the resend timer associated with a packet sent once-off
 *	reliably. It should be called when receipt of the packet has been
 *	acknowlegded.
 */
void Nub::delOnceOffResendTimer( const Address & addr, int seq )
{
	OnceOffPacket oop( addr, seq );
	OnceOffPackets::iterator found = onceOffPackets_.find( oop );

	if (found != onceOffPackets_.end())
	{
		this->delOnceOffResendTimer( found );
	}
	else
	{
		DEBUG_MSG( "Nub::delOnceOffResendTimer( %s ): "
			"Called for #%d that we no longer have (usually ok)\n",
			addr.c_str(), seq );
	}
}


void Nub::delOnceOffResendTimer( OnceOffPackets::iterator & iter )
{
	this->cancelTimer( iter->timerID_ );
	onceOffPackets_.erase( iter );
}


/**
 *	This method records the receipt of a packet sent once-off reliably. This
 *	needs to be done because we may receive this packet again but we should not
 *	process it again.
 *
 *  Returns true if the packet had already been received.
 */
bool Nub::onceOffReliableReceived( const Address & addr, int seq )
{
	this->initOnceOffPacketCleaning();

	OnceOffReceipt oor( addr, seq );

	if (currOnceOffReceipts_.find( oor ) != currOnceOffReceipts_.end() ||
		prevOnceOffReceipts_.find( oor ) != prevOnceOffReceipts_.end())
	{
		// ++numOnceOffReliableDupesReceived_;
		TRACE_MSG( "Nub::onceOffReliableReceived( %s ): #%d already received\n",
			addr.c_str(), seq );

		return true;
	}

	// ++numOnceOffReliableReceived_;
	currOnceOffReceipts_.insert( oor );

	return false;
}


// ----------------------------------------------------------------
// Section: Nub Support
// ----------------------------------------------------------------

/**
 * 	This operator writes the contents of the given MiniTimer
 * 	to a standard stream.
 */
std::ostream& operator<<( std::ostream &s, const Nub::MiniTimer &v )
{
	s << NiceTime( v.total ) << ", min ";
	s << NiceTime( v.min ) << ", max ";
	s << NiceTime( v.max ) << ", avg ";
	s << (v.count ? NiceTime( v.total/v.count ) : NiceTime(0));
	return s;
}

/**
 * 	This operator does nothing.
 */
std::istream& operator>>( std::istream &s, Nub::MiniTimer & )
{
	return s;
}

/**
 * 	This method handles timeouts for user requests. It generates
 * 	timer exceptions, which are thrown, and thus passed on to
 * 	the user.
 */
int Nub::ReplyHandlerElement::handleTimeout( TimerID /*id*/, void * nubArg )
{
	this->handleFailure( *static_cast< Nub * >( nubArg ),
			REASON_TIMER_EXPIRED );
	return 0;
}


/**
 *	This method handles failure of the request. This may be caused by failure of
 *	the channel or the request timing out.
 */
void Nub::ReplyHandlerElement::handleFailure( Nub & nub, Reason reason )
{
	// first get us out of the nub's replyHandlerQueue
	nub.replyHandlerMap_.erase( replyID_ );

	// cancel the timer if it has one
	if (timerID_ != TIMER_ID_NONE)
	{
		nub.cancelTimer( timerID_ );
		timerID_ = TIMER_ID_NONE;
	}

	// now call the exception function of the user's handler
	NubException e( reason );
	pHandler_->handleException( e, arg_ );

	// and finally delete ourselves
	delete this;
}


// -----------------------------------------------------------------------------
// Section: Report rate limiting of per-address exceptions
// -----------------------------------------------------------------------------

std::string Nub::addressErrorToString( const Address & address,
		const std::string & errorString )
{
	std::ostringstream out;
	out << ((char*) address) << ": " << errorString;
	return out.str();
}


std::string Nub::addressErrorToString(
		const Mercury::Address & address,
		const std::string & errorString,
		const ErrorReportAndCount & reportAndCount,
		const uint64 & now )
{
	int64 deltaStamps = now - reportAndCount.lastReportStamps;
	double deltaMillis = 1000 * deltaStamps / stampsPerSecondD();

	char * buf = NULL;
	int bufLen = 64;
	int strLen = bufLen;
	do
	{
		bufLen = strLen + 1;
		delete [] buf;
		buf = new char[ bufLen ];
#ifdef _WIN32
		strLen = _snprintf( buf, bufLen, "%d reports of '%s' "
				"in the last %.00fms",
			reportAndCount.count,
			addressErrorToString( address, errorString ).c_str(),
			deltaMillis );
		if (strLen == -1) strLen = (bufLen - 1) * 2;
#else
		strLen = snprintf( buf, bufLen, "%d reports of '%s' "
				"in the last %.00fms",
			reportAndCount.count,
			addressErrorToString( address, errorString ).c_str(),
			deltaMillis );
#endif
	} while (strLen >= bufLen);

	std::string out( buf );
	delete [] buf;
	return out;
}


/**
 *	Report a general error with printf style format string. If repeatedly the
 *	resulting formatted string is reported within the minimum output window,
 *	they are accumulated and output after the minimum output window has passed.
 */
void Nub::reportError(
		const Mercury::Address & address, const char* format, ... )
{
	char * buf = NULL;
	int bufLen = 32;
	int strLen = bufLen;
	do
	{
		delete [] buf;
		bufLen = strLen + 1;
		buf = new char[ bufLen ];

		va_list va;
		va_start( va, format );
#ifdef _WIN32
		strLen = _vsnprintf( buf, bufLen, format, va );
		if (strLen == -1) strLen = (bufLen - 1) * 2;
#else
		strLen = vsnprintf( buf, bufLen, format, va );
#endif
		va_end( va );
		buf[bufLen -1] = '\0';

	} while (strLen >= bufLen );

	std::string error( buf );

	delete [] buf;

	this->addReport( address, error );
}


/**
 *	Output the exception if it has not occurred before, otherwise only
 *	output the exception if the minimum period has elapsed since the
 *	last outputting of this exception.
 *
 *	@param ne 		the NubException thrown
 *	@param prefix 	any prefix to add to the error message, or NULL if no prefix
 *
 */
void Nub::reportException( Mercury::NubException & ne, const char* prefix )
{
	Mercury::Address offender( 0, 0 );
	ne.getAddress( offender );
	if (prefix)
	{
		this->reportError( offender,
			"%s: Exception was thrown: %s",
			prefix, reasonToString( ne.reason() ) );
	}
	else
	{
		this->reportError( offender, "Exception was thrown: %s",
				reasonToString( ne.reason() ) );
	}
}


/**
 *	Adds a new error message for an address to the reporter count map.
 *	Emits an error message if there has been no previous equivalent error
 *	string provider for this address.
 */
void Nub::addReport( const Address & address, const std::string & errorString )
{
	AddressAndErrorString addressError( address, errorString );
	ErrorsAndCounts::iterator searchIter =
		this->errorsAndCounts_.find( addressError );

	uint64 now = timestamp();
	// see if we have ever reported this error
	if (searchIter != this->errorsAndCounts_.end())
	{
		// this error has been reported recently..
		ErrorReportAndCount & reportAndCount = searchIter->second;
		reportAndCount.count++;

		int64 millisSinceLastReport = 1000 *
			(now - reportAndCount.lastReportStamps) /
			stampsPerSecond();

		reportAndCount.lastRaisedStamps = now;

		if (millisSinceLastReport >= ERROR_REPORT_MIN_PERIOD_MS)
		{
			ERROR_MSG( "%s\n",
				addressErrorToString( address, errorString,
					reportAndCount, now ).c_str() );
			reportAndCount.count = 0;
			reportAndCount.lastReportStamps = now;
		}

	}
	else
	{
		ERROR_MSG( "%s\n",
			addressErrorToString( address, errorString ).c_str() );

		ErrorReportAndCount reportAndCount = {
			now, 	// lastReportStamps,
			now, 	// lastRaisedStamps,
			0,		// count
		};

		this->errorsAndCounts_[ addressError ] = reportAndCount;
	}
}


/**
 *	Output all exception's reports that have not yet been output.
 */
void Nub::reportPendingExceptions( bool reportBelowThreshold )
{
	uint64 now = timestamp();

	// this is set to any iterator slated for removal
	ErrorsAndCounts::iterator staleIter = this->errorsAndCounts_.end();

	for (	ErrorsAndCounts::iterator exceptionCountIter =
				this->errorsAndCounts_.begin();
			exceptionCountIter != this->errorsAndCounts_.end();
			++exceptionCountIter )
	{
		// remove any stale mappings from the last loop's run
		if (staleIter != this->errorsAndCounts_.end())
		{
			this->errorsAndCounts_.erase( staleIter );
			staleIter = this->errorsAndCounts_.end();
		}

		// check this iteration's last report and see if we need to output
		// anything
		const AddressAndErrorString & addressError =
			exceptionCountIter->first;
		ErrorReportAndCount & reportAndCount = exceptionCountIter->second;

		int64 millisSinceLastReport = 1000 *
			(now - reportAndCount.lastReportStamps) / stampsPerSecond();
		if (reportBelowThreshold ||
				millisSinceLastReport >= ERROR_REPORT_MIN_PERIOD_MS)
		{
			if (reportAndCount.count)
			{
				ERROR_MSG( "%s\n",
					addressErrorToString(
						addressError.first, addressError.second,
						reportAndCount, now ).c_str()
				);
				reportAndCount.count = 0;
				reportAndCount.lastReportStamps = now;

			}
		}

		// see if we can remove this mapping if it has not been raised in a
		// while
		uint64 sinceLastRaisedMillis = 1000 * (now - reportAndCount.lastRaisedStamps) /
			stampsPerSecond();
		if (sinceLastRaisedMillis > ERROR_REPORT_COUNT_MAX_LIFETIME_MS)
		{
			// it's hung around for too long without being raised again,
			// so remove it in the next iteration
			staleIter = exceptionCountIter;
		}
	}

	// remove the last mapping if it is marked stale
	if (staleIter != this->errorsAndCounts_.end())
	{
		this->errorsAndCounts_.erase( staleIter );
	}
}

} // namespace Mercury

// nub.cpp
