/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef NUB_HPP
#define NUB_HPP

#include "condemned_channels.hpp"
#include "endpoint.hpp"
#include "interfaces.hpp"
#include "interface_element.hpp"
#include "irregular_channels.hpp"
#include "keepalive_channels.hpp"
#include "machine_guard.hpp"
#include "misc.hpp"
#include "packet.hpp"

#include "cstdmf/timestamp.hpp"

#include <list>
#include <map>
#include <queue>
#include <set>

namespace Mercury
{

class Bundle;
class Channel;
class ChannelFinder;
class InterfaceElement;
class InterfaceIterator;
class PacketFilter;
class PacketMonitor;

typedef SmartPointer< Channel > ChannelPtr;

/**
 *	This is the base class for all exception types that are thrown by the Nub.
 *
 *	@ingroup mercury
 */
class NubException
{
public:
	NubException( Reason reason );
	virtual ~NubException() {};
	Reason reason() const;
	virtual bool getAddress( Address & addr ) const;

private:
	Reason		reason_;
};


/**
 *	This class implements an exception type that can be thrown by the Nub. It
 *	includes an address as a member.
 *
 *	@ingroup mercury
 */
class NubExceptionWithAddress : public NubException
{
public:
	NubExceptionWithAddress( Reason reason, const Address & addr );
	virtual bool getAddress( Address & addr ) const;

private:
	Address address_;
};




/**
 *  Accounting structure for keeping track of the number of exceptions reported
 *  in a given period.
 */
struct ErrorReportAndCount
{
	uint64 lastReportStamps;	//< When this error was last reported
	uint64 lastRaisedStamps;	//< When this error was last raised
	uint count;					//< How many of this exception have been
								//	reported since
};


/**
 *	Key type for ErrorsAndCounts.
 */

typedef std::pair< Address, std::string > AddressAndErrorString;

/**
 *	Accounting structure that keeps track of counts of Mercury exceptions
 *	in a given period per pair of address and error message.
 *
 */
typedef std::map< AddressAndErrorString, ErrorReportAndCount >
	ErrorsAndCounts;



/**
 *	This class is the core of Mercury. It handles sending and receiving
 *	packets, deliving timer messages, and general socket notifications.
 *
 * 	@ingroup mercury
 */
class Nub :
	public InputMessageHandler,
	public TimerExpiryHandler,
	public InputNotificationHandler
{
public:
	/**
	 *  The desired receive buffer size on a socket
	 */
	static const int RECV_BUFFER_SIZE;

	Nub( uint16 listeningPort = 0,
		 const char * listeningInterface = 0 );
	virtual ~Nub();

	bool recreateListeningSocket( uint16 listeningPort,
		const char * listeningInterface );

	void serveInterfaceElement( const InterfaceElement & ie, MessageID id,
		InputMessageHandler * pHandler );

	Reason registerWithMachined( const std::string & name, int id,
		bool isRegister = true );

	Reason deregisterWithMachined();

	Reason registerBirthListener( Bundle & bundle, int addrStart,
								const char * ifname );
	Reason registerDeathListener( Bundle & bundle, int addrStart,
								const char * ifname );

	Reason registerBirthListener( const InterfaceElement & ie,
								const char * ifname );
	Reason registerDeathListener( const InterfaceElement & ie,
								const char * ifname );

	Reason findInterface( const char * name, int id, Address & address,
		int retries = 0, bool verboseRetry = true );

	bool processPendingEvents( bool expectingPacket = false );
	void processContinuously();
	void processUntilBreak();
	void processUntilChannelsEmpty( float timeout = 10.f );

	Reason receivedCorruptedPacket();

	void handleInputNotifications( int &countReady,
		fd_set &readFDs, fd_set &writeFDs );

	virtual int handleInputNotification( int fd );

	void breakProcessing( bool breakState = true );
	bool processingBroken() const;
	void breakBundleLoop();
	void drainSocketInput();

	void shutdown();

	const Address & address() const;
	int socket() const;

	void send( const Address & address, Bundle & bundle,
		Channel * pChannel = NULL );
	void sendPacket( const Address & addr, Packet * p,
			Channel * pChannel, bool isResend );

	Reason basicSendWithRetries( const Address & addr, Packet * p );
	Reason basicSendSingleTry( const Address & addr, Packet * p );

	void delayedSend( Channel * pChannel );

	INLINE TimerID registerTimer( int microseconds,
					TimerExpiryHandler * handler, void* arg = NULL );
	TimerID registerCallback( int microseconds, TimerExpiryHandler * handler,
			void * arg = NULL );
	void cancelTimer( TimerID id );
	int cancelTimers( TimerExpiryHandler * pHandler );
	uint64   timerDeliveryTime( TimerID id ) const;
	uint64 & timerIntervalTime( TimerID id );

	bool registerFileDescriptor( int fd, InputNotificationHandler * handler );
	bool deregisterFileDescriptor( int fd );
	bool registerWriteFileDescriptor( int fd, InputNotificationHandler * handler );
	bool deregisterWriteFileDescriptor( int fd );
	void pBundleFinishHandler( BundleFinishHandler * handler );
	void findLargestFileDescriptor();

	bool registerChildNub( Nub * pChildNub,
		InputNotificationHandler * pHandler = NULL );

	bool deregisterChildNub( Nub * pChildNub );

	void onChannelGone( Channel * pChannel );
	void cancelRequestsFor( Channel * pChannel );

	INLINE void prepareToShutDown();

	CondemnedChannels & condemnedChannels() { return condemnedChannels_; }
	IrregularChannels & irregularChannels()	{ return irregularChannels_; }
	KeepAliveChannels & keepAliveChannels()	{ return keepAliveChannels_; }

	void setIrregularChannelsResendPeriod( float seconds )
	{
		irregularChannels_.setPeriod( seconds, *this );
	}

	uint64 getSpareTime() const;
	void clearSpareTime();
	double proportionalSpareTime() const;

#if ENABLE_WATCHERS
	static WatcherPtr pWatcher();
#endif

	void setLatency( float latencyMin, float latencyMax );
	void setLossRatio( float lossRatio );

	bool hasArtificialLossOrLatency() const
	{
		return (artificialLatencyMin_ != 0) || (artificialLatencyMax_ != 0) ||
			(artificialDropPerMillion_ != 0);
	}

	void setPacketMonitor( PacketMonitor* pPacketMonitor );

	bool registerChannel( Channel & channel );
	bool deregisterChannel( Channel & channel );

	void registerChannelFinder( ChannelFinder *pFinder );

	void dropNextSend( bool v = true ) { dropNextSend_ = v; }

	void onAddressDead( const Address & addr );

	bool rebind( const Address & addr );

	void switchSockets( Nub * pOtherNub );

	bool handleHijackData( int fd, BinaryIStream & stream );

	INLINE bool isVerbose() const;
	INLINE void isVerbose( bool value );

	const char * c_str() const { return socket_.c_str(); }

	const char * msgName( MessageID msgID ) const
	{
		return interfaceTable_[ msgID ].name();
	}

	static const char *USE_BWMACHINED;
private:
	Endpoint		socket_;

	class QueryInterfaceHandler : public MachineGuardMessage::ReplyHandler
	{
	public:
		QueryInterfaceHandler( int requestType );
		bool onQueryInterfaceMessage( QueryInterfaceMessage &qim, uint32 addr );

		bool hasResponded_;
		u_int32_t address_;
		char request_;
	};

	// Used when creating a nub to query bwmachined for the
	// internalInterface
	bool queryMachinedInterface( u_int32_t & addr );

	// ProcessMessage Handler for registerWithMachined to determine whether it
	// received a valid response.
	class ProcessMessageHandler : public MachineGuardMessage::ReplyHandler
	{
	public:
		ProcessMessageHandler() { hasResponded_ = false; }
		bool onProcessMessage( ProcessMessage &pm, uint32 addr );

		bool hasResponded_;
	};

	Reason registerListener( Bundle & bundle,
		int addrStart, const char * ifname, bool isBirth, bool anyUID = false );

	/// The name of the Mercury interface served by this Nub, or an empty string
	/// if not registered with machined.
	std::string		interfaceName_;

	/// The ID this interface is registered with machined as (e.g. cellapp01).
	int				interfaceID_;

	typedef std::vector< InterfaceElementWithStats > InterfaceTable;
	InterfaceTable interfaceTable_;

	class TimerQueueElement
	{
	public:
		uint64		deliveryTime;
		uint64		intervalTime;
		int			state;
		void		*arg;
		TimerExpiryHandler	*handler;

		enum
		{
			STATE_PENDING = 0,
			STATE_EXECUTING = 1,
			STATE_CANCELLED = 2
		};
	};

	class TimerQueueComparator
	{
	public:
		bool operator()
			( const TimerQueueElement * a, const TimerQueueElement * b )
		{
			return a->deliveryTime > b->deliveryTime;
		}
	};

	typedef std::priority_queue< TimerQueueElement*,
		std::vector<TimerQueueElement*>, TimerQueueComparator > TimerQueue;
	TimerQueue	timerQueue_;
	TimerQueueElement * pCurrentTimer_;

	bool rescheduleSend( const Address & addr, Packet * packet, bool isResend );

	// Of every million packets sent, this many packets will be dropped for
	// debugging.
	int		artificialDropPerMillion_;

	// In milliseconds
	int		artificialLatencyMin_;
	int		artificialLatencyMax_;

	/// State flag used in debugging to indicate that the next outgoing packet
	/// should be dropped
	bool	dropNextSend_;

	class RescheduledSender : public TimerExpiryHandler
	{
	public:
		RescheduledSender( Nub & nub, const Address & addr, Packet * pPacket,
			int latencyMilli, bool isResend );

		virtual int handleTimeout( TimerID id, void * arg );

	private:
		Nub & nub_;
		Address addr_;
		PacketPtr pPacket_;
		bool isResend_;
	};

	Reason processPacket( const Address & addr, Packet * p );
	Reason processFilteredPacket( const Address & addr, Packet * p );
	Reason processOrderedPacket( const Address & addr, Packet * p,
		Channel * pChannel );

public:
	Reason processPacketFromStream( const Address & addr, BinaryIStream & data );

private:
	void dumpBundleMessages( Bundle & bundle );

	friend class PacketFilter;	// to expose processFilteredPacket

	ReplyID nextReplyID_;
	ReplyID getNextReplyID()
	{
		if (nextReplyID_ > REPLY_ID_MAX)
			nextReplyID_ = 1;

		return nextReplyID_++;
	}

	SeqNum nextSequenceID_;
	SeqNum getNextSequenceID();	// defined in the .cpp

	class ReplyHandlerElement : public TimerExpiryHandler
	{
	public:
		int			replyID_;
		TimerID		timerID_;
		ReplyMessageHandler * pHandler_;
		void *		arg_;
		Channel *	pChannel_;

		virtual int handleTimeout( TimerID id, void * nubArg );
		void handleFailure( Nub & nub, Reason reason );
	};
	friend class ReplyHandlerElement;

	typedef std::map<int, ReplyHandlerElement *> ReplyHandlerMap;
	ReplyHandlerMap replyHandlerMap_;

public:
	void cancelReplyMessageHandler( ReplyMessageHandler * pHandler,
		   Reason reason = REASON_CHANNEL_LOST );

	/**
	 * Called by the nub when it gets a reply message.
	 * Users should have no need to call this function.
	 */
	virtual void handleMessage( const Address & source,
		UnpackedMessageHeader & header,
		BinaryIStream & data );
private:

	TimerID newTimer( int microseconds,
		TimerExpiryHandler * handler,
		void * arg,
		bool recurrent );

	void finishProcessingTimerEvent( TimerQueueElement * pElement );

	PacketPtr nextPacket_;
	Address	advertisedAddress_;

public:
	/**
	 *  This class represents partially reassembled multi-packet bundles.
	 */
	class FragmentedBundle : public SafeReferenceCount
	{
	public:
		FragmentedBundle( SeqNum lastFragment, int remaining,
				uint64 touched, Packet * firstPacket ) :
			lastFragment_( lastFragment ),
			remaining_( remaining ),
			touched_( touched ),
			pChain_( firstPacket )
		{}

		/// The age (in seconds) at which a fragmented bundle is abandoned.
		static const uint64 MAX_AGE = 10;

		bool isOld() const;
		bool isReliable() const;

		SeqNum		lastFragment_;
		int			remaining_;
		uint64		touched_;
		PacketPtr	pChain_;

		/**
		 *  Keys used in FragmentedBundleMaps (below).
		 */
		class Key
		{
		public:
			Key( const Address & addr, SeqNum firstFragment ) :
				addr_( addr ),
				firstFragment_( firstFragment )
			{}

			Address		addr_;
			SeqNum		firstFragment_;
		};
	};

	typedef SmartPointer< FragmentedBundle > FragmentedBundlePtr;

private:
	/// This map contains partially reassembled packets received by this nub
	/// off-channel.  Channels maintain their own FragmentedBundle.
	typedef std::map< FragmentedBundle::Key, FragmentedBundlePtr >
		FragmentedBundles;

	FragmentedBundles fragmentedBundles_;
	// TODO: eventually allocate FragmentedBundleInfo's from
	// a rotating list; when it gets full drop old fragments.

	/// Timer for discarding incomplete stale bundles.
	TimerID clearFragmentedBundlesTimerID_;

	bool breakProcessing_;
	bool breakBundleLoop_;
	bool drainSocketInput_;

#ifdef _WIN32
	std::map<int,InputNotificationHandler*> fdHandlers_;
	std::map<int,InputNotificationHandler*> fdWriteHandlers_;
#else
	// yeah, yeah - 4KB+512B of baggage:
	InputNotificationHandler *	fdHandlers_[FD_SETSIZE];
	// May be better with a map?
	InputNotificationHandler *	fdWriteHandlers_[FD_SETSIZE];
#endif

	fd_set						fdReadSet_;
	fd_set						fdWriteSet_;
	int							fdLargest_;
	int							fdWriteCount_;

	BundleFinishHandler *		pBundleFinishHandler_;

	PacketMonitor*				pPacketMonitor_;

	typedef std::map< Address, Channel * >	ChannelMap;
	ChannelMap					channelMap_;

	ChannelFinder*				pChannelFinder_;

	uint64	lastStatisticsGathered_;
	int		lastTxQueueSize_;
	int		lastRxQueueSize_;
	int		maxTxQueueSize_;
	int		maxRxQueueSize_;

public:
	Channel * findChannel( const Address & addr, bool createAnonymous = false );

	Channel & findOrCreateChannel( const Address & addr )
	{
		return *this->findChannel( addr, /* createAnonymous: */ true );
	}

	void delAnonymousChannel( const Address & addr );

	unsigned int numPacketsReceived() const;
	unsigned int numMessagesReceived() const;
	unsigned int numBytesReceived() const;
	unsigned int numOverheadBytesReceived() const;
	unsigned int numBytesReceivedForMessage(uint8 message) const;
	unsigned int numMessagesReceivedForMessage(uint8 message) const;

	double bytesSentPerSecondAvg() const;
	double bytesSentPerSecondPeak() const;
	double bytesReceivedPerSecondAvg() const;
	double bytesReceivedPerSecondPeak() const;
	double packetsSentPerSecondAvg() const;
	double packetsSentPerSecondPeak() const;
	double packetsReceivedPerSecondAvg() const;
	double packetsReceivedPerSecondPeak() const;
	double bundlesSentPerSecondAvg() const;
	double bundlesSentPerSecondPeak() const;
	double bundlesReceivedPerSecondAvg() const;
	double bundlesReceivedPerSecondPeak() const;
	double messagesSentPerSecondAvg() const;
	double messagesSentPerSecondPeak() const;
	double messagesReceivedPerSecondAvg() const;
	double messagesReceivedPerSecondPeak() const;

	void incNumDuplicatePacketsReceived()
	{
		++numDuplicatePacketsReceived_;
	}

	/**
	 * 	@internal
	 * 	This class provides a timer for profiling Nub operations.
	 * 	Normally the start and stop methods are called each time
	 * 	the operation takes place. It is also possible to call start
	 * 	and stop multiple times for one operation. When the operation
	 * 	is finally complete, the stop method should be called with a
	 * 	'true' argument to indicate that it is finished.
	 */
	struct MiniTimer
	{
		MiniTimer();
		void start();
		void stop( bool increment = false );
		void reset();

		double getMinDurationSecs() const;
		double getMaxDurationSecs() const;
		double getAvgDurationSecs() const;

		uint64	total;	///< The total time spent performing this operation.
		uint64	last;	///< The current time spent performing this operation.
		uint64	sofar;	///< The time this operation last commenced.
		uint64	min; 	///< The minimum time taken to perform the operation.
		uint64	max; 	///< The maximum time taken to perform the operation.
		uint	count;	///< The number of times this operation has occurred.
	};

	/**
	*	This class extends Mercury::Nub::MiniTimer by resetting the timers and
	*	counters every so often (configurable). This allows the measurement of
	*	more transient fluctuations that gets "absorbed" by a much longer running
	*	MiniTimer. This class uses a lazy reset mechanism that only checks when
	*	start is called. So you cannot characterise the data as representing
	*	"the last N seconds" or "at most the last N seconds". It is more like
	*	"at most the last N seconds, plus the time since start was last called".
	*/
	class TransientMiniTimer : public MiniTimer
	{
		uint64	resetPeriodStamp_;
		uint64	resetTimeStamp_;

	public:
		TransientMiniTimer(int resetPeriodSecs);

		INLINE void start();
		INLINE void stop();
		INLINE void reset();

		double getElapsedResetSecs() const;
		double getCountPerSec()	const;

	public:
		/**
		 *	This class makes using a TransientMiniTimer easier by calling
		 *	start and stop in the constructor and destructor respectively.
		 *
		 *	NOTE: This class it not really useful for MiniTimer at the moment
		 *	because the stop() method with default arguments is really more
		 *	like pause.
		 */
		template < class TIMERTYPE >
		class Op
		{
			TIMERTYPE& timer_;
		public:
			Op(TIMERTYPE& timer)
				: timer_(timer)
			{	timer_.start();	}

			~Op()
			{	timer_.stop();	}
		};
	};

	int * loopStats()	{ return loopStats_; }
	enum LoopStat
	{
		RECV_TRYS = 0,
		RECV_GETS,
		RECV_ERRS,
		RECV_SELS,

		TIMER_CALLS = 8,
		TIMER_RESCHEDS
	};

private:
	unsigned int numBytesSent_;
	unsigned int numBytesResent_;
	unsigned int numBytesReceived_;
	unsigned int numPacketsSent_;
	unsigned int numPacketsResent_;
	unsigned int numPiggybacks_;
	unsigned int numPacketsSentOffChannel_;
	unsigned int numPacketsReceived_;
	unsigned int numDuplicatePacketsReceived_;
	unsigned int numPacketsReceivedOffChannel_;
	unsigned int numBundlesSent_;
	unsigned int numBundlesReceived_;
	unsigned int numMessagesSent_;
	unsigned int numReliableMessagesSent_;
	unsigned int numMessagesReceived_;
	unsigned int numOverheadBytesReceived_;
	unsigned int numFailedPacketSend_;
	unsigned int numFailedBundleSend_;
	unsigned int numCorruptedPacketsReceived_;
	unsigned int numCorruptedBundlesReceived_;

	mutable unsigned int lastNumBytesSent_;
	mutable unsigned int lastNumBytesReceived_;
	mutable unsigned int lastNumPacketsSent_;
	mutable unsigned int lastNumPacketsReceived_;
	mutable unsigned int lastNumBundlesSent_;
	mutable unsigned int lastNumBundlesReceived_;
	mutable unsigned int lastNumMessagesSent_;
	mutable unsigned int lastNumMessagesReceived_;

	MiniTimer	sendMercuryTimer_;
	MiniTimer	sendSystemTimer_;
	MiniTimer	recvMercuryTimer_;
	MiniTimer	recvSystemTimer_;

	uint64		spareTime_,		accSpareTime_;
	uint64		oldSpareTime_,	totSpareTime_;

	int		loopStats_[16];

	enum LastVisitTime
	{
		LVT_BytesSent,
		LVT_BytesReceived,
		LVT_PacketsSent,
		LVT_PacketsReceived,
		LVT_BundlesSent,
		LVT_BundlesReceived,
		LVT_MessagesSent,
		LVT_MessagesReceived,
		LVT_END
	};
	mutable uint64 lastVisitTime_[LVT_END];
	uint64 startupTime_;

	double delta() const { return (double)( timestamp() - startupTime_ ) / stampsPerSecondD(); };
	double peakCalculator( LastVisitTime, const unsigned int &, unsigned int & ) const;

public:
	/**
	 *	This callback interface is used by Nub to notify clients who wants
	 *	to poll something "every now and then".
	 */
	struct IOpportunisticPoller
	{
		virtual ~IOpportunisticPoller() {};

		// This method is called whenever the main loop
		// (Nub::processContinuously) is woken up from its "wait for
		// something to happen" system call. This would include incoming
		// messages and timeouts. You can ensure a minimum frequency of
		// polling by setting a timer which will guarantee that the main
		// loop is woken up regularly.
		virtual void poll() = 0;
	};

	void setOpportunisticPoller(IOpportunisticPoller* pPoller)	{	pOpportunisticPoller_ = pPoller;	}
	IOpportunisticPoller* getOpportunisticPoller() const		{	return pOpportunisticPoller_;	}

private:
	IOpportunisticPoller* pOpportunisticPoller_;

	// Once-off reliable message sending

public:
	/**
	 *	This struct is used to store details of once-off packets that we have
	 *	received.
	 */
	class OnceOffReceipt
	{
	public:
		OnceOffReceipt( const Address & addr, int footerSequence ) :
			addr_( addr ),
			footerSequence_( footerSequence )
		{
		}

		Address addr_;
		int footerSequence_;
	};

	// This property is so that data can be associated with a nub. Message
	// handlers get access to the nub that received the message and can get
	// access to this data. Bots use this so that they know which
	// ServerConnection should handle the message.
	void * pExtensionData() const			{ return pExtensionData_; }
	void pExtensionData( void * pData )		{ pExtensionData_ = pData; }

private:
	typedef std::set< OnceOffReceipt > OnceOffReceipts;
	OnceOffReceipts currOnceOffReceipts_;
	OnceOffReceipts prevOnceOffReceipts_;

	class OnceOffPacket : public TimerExpiryHandler, public OnceOffReceipt
	{
	public:
		OnceOffPacket( const Address & addr, int footerSequence,
						Packet * pPacket = NULL ) :
			OnceOffReceipt( addr, footerSequence ),
			pPacket_( pPacket ),
			timerID_( 0 ),
			retries_( 0 )
		{
		}

		void registerTimer( Nub * pNub )
		{
			timerID_ = pNub->registerTimer(
				pNub->onceOffResendPeriod() /* microseconds */,
				this /* handler */,
				pNub /* arg */ );
		}

		virtual int handleTimeout( TimerID id, void * arg )
		{
			static_cast< Nub * >( arg )->expireOnceOffResendTimer( *this );
			return 0;
		}

		PacketPtr pPacket_;
		TimerID timerID_;
		int retries_;
	};
	typedef std::set< OnceOffPacket > OnceOffPackets;
	OnceOffPackets onceOffPackets_;

private:

	virtual int handleTimeout( TimerID id, void * arg );
	void initOnceOffPacketCleaning();
	void onceOffReliableCleanup();

	void addOnceOffResendTimer( const Address & addr, int seq, Packet * p );
	void expireOnceOffResendTimer( OnceOffPacket & packet );
	void delOnceOffResendTimer( const Address & addr, int seq );
	void delOnceOffResendTimer( OnceOffPackets::iterator & iter );

	bool onceOffReliableReceived( const Address & addr, int seq );
public:
	int onceOffResendPeriod() const
	{	return onceOffResendPeriod_; }

	void onceOffResendPeriod( int microseconds )
	{	onceOffResendPeriod_ = microseconds; }

	int onceOffMaxResends() const
	{	return onceOffMaxResends_; }

	void onceOffMaxResends( int retries )
	{	onceOffMaxResends_ = retries; }

	void isExternal( bool state )
	{
		isExternal_ = state;
	}

	void shouldUseChecksums( bool b )
	{	shouldUseChecksums_ = b; }

	bool shouldUseChecksums() const
	{	return shouldUseChecksums_; }

private:
	TimerID onceOffPacketCleaningTimerID_;
	int onceOffPacketCleaningPeriod_;
	int onceOffMaxResends_;
	int onceOffResendPeriod_;
	void * pExtensionData_;

	/// Indicates whether this nub is listening on an external interface.  At
	/// the moment this is just used to decide whether or not incoming
	/// once-off-reliable traffic should be allowed.
	bool isExternal_;

public:
	void reportException( Mercury::NubException & ne,
			const char * prefix = NULL);
	void reportError( const Mercury::Address & address,
			const char* format, ... );
	void reportPendingExceptions( bool reportBelowThreshold = false );

private:
	/**
	 *	The minimum time that an exception can be reported from when it was
	 *	first reported.
	 */
	static const uint ERROR_REPORT_MIN_PERIOD_MS;

	/**
	 *	The nominal maximum time that a report count for a Mercury address and
	 *	error pair is kept.
	 */
	static const uint ERROR_REPORT_COUNT_MAX_LIFETIME_MS;

	void addReport( const Address & address, const std::string & error );

	static std::string addressErrorToString(
			const Address & address,
			const std::string & errorString );

	static std::string addressErrorToString(
			const Address & address,
			const std::string & errorString,
			const ErrorReportAndCount & reportAndCount,
			const uint64 & now );

	TimerID reportLimitTimerID_;
	ErrorsAndCounts errorsAndCounts_;

	Nub *pMasterNub_;

	/// The collection of nubs that have registered with this Nub using
	/// registerChildNub().  Calling processContinuously() on this nub should be
	/// enough to cause all processing to happen on all child nubs too (and on
	/// their children, etc etc).  This is deliberately a list and not a vector
	/// because we are relying on the non-invalidation of iterators when
	/// deleting other elements of the sequence.
	typedef std::list< Nub* > ChildNubs;
	ChildNubs childNubs_;

	/// Timer ID for ticking child nubs.
	TimerID tickChildNubsTimerID_;

	/// The default tick period for child nubs.
	static const int CHILD_NUB_TICK_PERIOD = 50000;

	/// External nubs remember recently deregistered channels for a little while
	/// and drop incoming packets from those addresses.  This is to avoid
	/// processing packets from recently disconnected clients, especially ones
	/// using channel filters.  Attempting to process these packets generates
	/// spurious corruption warnings because they are processed raw once the
	/// channel is gone.
	///
	/// The mapping is addresses of recently deregistered channels, mapped to
	/// the timer ID for when they will time out.
	typedef std::map< Address, TimerID > RecentlyDeadChannels;
	RecentlyDeadChannels recentlyDeadChannels_;

	void sendDelayedChannels();

	typedef std::set< ChannelPtr > DelayedChannels;
	DelayedChannels delayedChannels_;

	enum TimeoutType
	{
		TIMEOUT_DEFAULT = 0,
		TIMEOUT_RECENTLY_DEAD_CHANNEL
	};

	IrregularChannels irregularChannels_;
	CondemnedChannels condemnedChannels_;
	KeepAliveChannels keepAliveChannels_;

	bool shouldUseChecksums_;
	bool isVerbose_;

	TimerID interfaceStatsTimerID_;
};

std::ostream& operator <<( std::ostream &s, const Nub::MiniTimer &v );
std::istream& operator >>( std::istream &s, Nub::MiniTimer & );

inline bool
operator<( const Nub::OnceOffReceipt & r1, const Nub::OnceOffReceipt & r2 )
{
	return (r1.footerSequence_ < r2.footerSequence_) ||
		((r1.footerSequence_ == r2.footerSequence_) && (r1.addr_ < r2.addr_));
}

inline bool
operator==( const Nub::OnceOffReceipt & r1, const Nub::OnceOffReceipt & r2 )
{
	return (r1.footerSequence_ == r2.footerSequence_) && (r1.addr_ == r2.addr_);
}

bool operator==( const Nub::FragmentedBundle::Key & a,
	const Nub::FragmentedBundle::Key & b );
bool operator<( const Nub::FragmentedBundle::Key & a,
	const Nub::FragmentedBundle::Key & b );

} // namespace Mercury

#ifdef CODE_INLINE
#include "nub.ipp"
#endif


#endif // NUB_HPP
