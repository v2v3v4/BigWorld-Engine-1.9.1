/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "unit_test_lib/multi_proc_test_case.hpp"

#include "cstdmf/timestamp.hpp"
#include "cstdmf/smartpointer.hpp"

#include "network/interfaces.hpp"
#include "network/nub.hpp"
#include "network/packet_filter.hpp"
#include "network/unit_test/network_app.hpp"

#include "test_fragment_interfaces.hpp"

#include <map>
#include <vector>
#include <set>

#include <string.h>


// -----------------------------------------------------------------------------
// Section: Test constants
// -----------------------------------------------------------------------------

/**
 *	How many ticks/iterations clients will run for, (i.e. how many msg1
 *	messages from the client to the server to send)
 */
static const int NUM_ITERATIONS = 100;

/**
 *	The size of the message payloads between the client and server.
 */
static const uint PAYLOAD_SIZE = 8 * 1024;


/**
 *	The tick period in microseconds.
 */
static const long TICK_PERIOD = 100000L;


/**
 *  The loss ratio for sends on channels.
 */
static const float RELIABLE_LOSS_RATIO = 0.1f;



// -----------------------------------------------------------------------------
// Section: FragmentServerApp
// -----------------------------------------------------------------------------


/**
 *	The fragment server application.
 */
class FragmentServerApp : public NetworkApp
{

public:
	FragmentServerApp( unsigned payloadSizeBytes,
		unsigned long maxRunTimeMicros );

	~FragmentServerApp();

	virtual int run();

	void connect( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void disconnect( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void channelMsg( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	void onceOffMsg( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data );

	int handleTimeout( Mercury::TimerID id, void * /*arg*/ );

	unsigned channelMsgCount() const { return channelMsgCount_; }
	unsigned onceOffMsgCount() const { return onceOffMsgCount_; }

	/**
	 *	Singleton instance accessor.
	 */
	static FragmentServerApp & instance()
	{
		MF_ASSERT( s_pInstance != NULL );
		return *s_pInstance;
	}

private:
	class ConnectedClient :
		public Mercury::ChannelOwner,
		public SafeReferenceCount
	{
	public:
		ConnectedClient( Mercury::Nub & nub, const Mercury::Address & addr,
			Mercury::Channel::Traits traits ) :
			Mercury::ChannelOwner( nub, addr, traits ),
			channelSeqAt_( 0 ),
			onceOffSeqAt_( 0 )
		{
			// We don't send anything to clients
			this->channel().isIrregular( true );
		}

		unsigned channelSeqAt_;
		unsigned onceOffSeqAt_;
	};

	typedef SmartPointer< ConnectedClient > ConnectedClientPtr;
	typedef std::map< Mercury::Address, ConnectedClientPtr > ConnectedClients;
	ConnectedClients clients_;

	void handleMessage( ConnectedClientPtr pClient,
		const char * msgName,
		BinaryIStream & data,
		unsigned * pClientSeqAt,
		unsigned * pServerCount );

	unsigned			channelMsgCount_;
	unsigned			onceOffMsgCount_;
	unsigned 			payloadSize_;
	unsigned long 		maxRunTimeMicros_;

	Mercury::TimerID 	watchTimerID_;

	static FragmentServerApp * s_pInstance;
};


/** Singleton instance pointer. */
FragmentServerApp * FragmentServerApp::s_pInstance = NULL;

/**
 *	Constructor.
 *
 *	@param payloadSizeBytes		the length of the payload to send to clients
 *	@param maxRunTimeMicros		the maximum time to run the test for before
 *								aborting and asserting test failure
 */
FragmentServerApp::FragmentServerApp( unsigned payloadSizeBytes,
	unsigned long maxRunTimeMicros ):
		NetworkApp(),
		channelMsgCount_( 0 ),
		onceOffMsgCount_( 0 ),
		payloadSize_( payloadSizeBytes ),
		maxRunTimeMicros_( maxRunTimeMicros )
{
	// Dodgy singleton code
	MF_ASSERT( s_pInstance == NULL );
	s_pInstance = this;

	FragmentServerInterface::registerWithNub( this->nub() );
}


/**
 *	Destructor.
 */
FragmentServerApp::~FragmentServerApp()
{
	MF_ASSERT( s_pInstance == this );
	s_pInstance = NULL;
}


/**
 *	App run function.
 */
int FragmentServerApp::run()
{
	INFO_MSG( "FragmentServerApp(%d)::run: started\n", getpid() );

	this->startTimer( TICK_PERIOD );

	watchTimerID_ = this->nub().registerTimer( maxRunTimeMicros_, this, NULL );

	bool done = false;
	while (!done)
	{
		try
		{
			this->nub().processContinuously();
			done = true;
		}
		catch (Mercury::NubExceptionWithAddress & ea)
		{
			Mercury::Address addr;
			ea.getAddress( addr );

			// was this one for one of channel addresses?
			ConnectedClients::iterator iter = clients_.find( addr );
			if (iter != clients_.end())
			{
				ERROR_MSG( "FragmentServerApp(%d): Dropping channel to %s "
						"due to exception: %s\n",
					getpid(), addr.c_str(),
					Mercury::reasonToString( ea.reason() ) );

				clients_.erase( iter );
			}
			else
			{
				// unknown channel
				ERROR_MSG( "FragmentServerApp(%d)::run: "
						"got nub exception for address %s: %s\n",
					getpid(), addr.c_str(),
					Mercury::reasonToString( ea.reason() ) );
			}


		}
		catch (Mercury::NubException & e)
		{
			ERROR_MSG( "FragmentServerApp(%d)::run: got nub exception: %s\n",
				getpid(), Mercury::reasonToString( e.reason() ) );
		}
	}

	TRACE_MSG( "FragmentServerApp(%d)::run: "
		"Processing until channels empty\n", getpid() );
	this->nub().processUntilChannelsEmpty();

	INFO_MSG( "FragmentServerApp(%d)::run: finished\n",	getpid() );
	return 0;
}


/**
 *	Timeout handler. If the watch timer (configured with maxRunTimeMicros
 *	parameter at construction) goes off, then abort test and assert test
 *	failure. Otherwise, it is a tick timer and so send all connected clients a
 *	msg2 message.
 */
int FragmentServerApp::handleTimeout( Mercury::TimerID timerID, void * /*arg*/ )
{
	if (timerID == watchTimerID_)
	{
		// our watchdog timer has expired
		NETWORK_APP_ASSERT_WITH_MESSAGE_RET( !clients_.empty(),
			"Timer expired but no clients remaining", 1 );
		ConnectedClientPtr pClient = clients_.begin()->second;

		ERROR_MSG( "FragmentServerApp(%d)::handleTimeout: "
			"Max run time (%.1fs) is up (%d sent/%d recvd)\n",
			getpid(), maxRunTimeMicros_ / 1000000.f,
			pClient->channel().numPacketsSent(),
			pClient->channelSeqAt_ );

		this->nub().breakProcessing();
		return 1;
	}

	return 0;
}


// -----------------------------------------------------------------------------
// Section: FragmentServerApp Message Handlers
// -----------------------------------------------------------------------------

/**
 *	Clients send this message to establish a channel.
 */
void FragmentServerApp::connect( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	TRACE_MSG( "FragmentServerApp(%d)::connect from %s\n",
		getpid(), srcAddr.c_str() );

	if (clients_.find( srcAddr ) != clients_.end())
	{
		// we may already have one - the client spams connect until it gets
		// connectAck
		TRACE_MSG( "FragmentServerApp(%d)::connect(%s): already have channel\n",
			getpid(), srcAddr.c_str() );

		return;
	}

	ConnectedClientPtr pClient =
		new ConnectedClient( this->nub(), srcAddr, Mercury::Channel::EXTERNAL );

	clients_[ srcAddr ] = pClient;
}


/**
 *	Clients disconnect after they have sent a certain number of msg1 messages
 *	to the server. The channel is expected to be destroyed.
 */
void FragmentServerApp::disconnect( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	TRACE_MSG( "FragmentServerApp(%d)::disconnect( %s )\n",
		getpid(), srcAddr.c_str() );

	ConnectedClients::iterator iter = clients_.find( srcAddr );

	if (iter != clients_.end())
	{
		clients_.erase( iter );
	}
	else
	{
		ERROR_MSG( "FragmentServerApp(%d)::disconnect( %s ): "
				"unknown address\n",
			getpid(), srcAddr.c_str() );

		return;
	}

	if (clients_.empty())
	{
		TRACE_MSG( "FragmentServerApp(%d)::disconnect: no more clients\n",
			getpid() );

		this->nub().breakProcessing();
	}
}


/**
 *  Handler for channelMsg.
 */
void FragmentServerApp::channelMsg( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	ConnectedClientPtr pClient = clients_[ srcAddr ];
	NETWORK_APP_ASSERT_WITH_MESSAGE( pClient != NULL,
		"Got message from unknown address" );

	this->handleMessage( pClient, "channelMsg", data,
		&pClient->channelSeqAt_, &channelMsgCount_ );
}


/**
 *  Handler for onceOffMsg.
 */
void FragmentServerApp::onceOffMsg( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	ConnectedClientPtr pClient = clients_[ srcAddr ];
	NETWORK_APP_ASSERT_WITH_MESSAGE( pClient != NULL,
		"Got message from unknown address" );

	this->handleMessage( pClient, "onceOffMsg", data,
		&pClient->onceOffSeqAt_, &onceOffMsgCount_ );
}


/**
 *	Clients send NUM_ITERATIONS messages to the server.  Half of them are sent
 *	on-channel, and the other half off.  All messages are multi-packet, the idea
 *	being that the fragment reassembly code works for both kinds of traffic and
 *	can handle both types simultaneously.
 */
void FragmentServerApp::handleMessage( ConnectedClientPtr pClient,
	const char * msgName,
	BinaryIStream & data,
	unsigned * pClientSeqAt,
	unsigned * pServerCount )
{
	// Each message starts with a sequence number.
	unsigned seq = 0;
	data >> seq;

	TRACE_MSG( "FragmentServerApp(%d)::%s (%s): seq=%u\n",
		getpid(), msgName, pClient->channel().c_str(), seq );

	// Verify message length
	NETWORK_APP_ASSERT_WITH_MESSAGE(
		(unsigned)data.remainingLength() == payloadSize_,
		"Incorrect message size" );

	NETWORK_APP_ASSERT_WITH_MESSAGE( *pClientSeqAt == seq,
		"Got message out of sequence" );

	++(*pClientSeqAt);

	// The message payload should be increasing ints, starting from 1.
	int prev = 0, curr = 0;
	while (data.remainingLength() > 0)
	{
		data >> curr;
		NETWORK_APP_ASSERT_WITH_MESSAGE( curr == prev + 1,
			"Payload incorrect" );
		prev = curr;
	}

	++(*pServerCount);
}


// -----------------------------------------------------------------------------
// Section: FragmentClientApp
// -----------------------------------------------------------------------------

class FragmentClientApp : public NetworkApp
{
public:
	FragmentClientApp( const Mercury::Address & dstAddr,
			unsigned payloadSizeBytes,
			unsigned numIterations );

	virtual ~FragmentClientApp();

	virtual int run();

	void startTest();

	int handleTimeout( Mercury::TimerID id, void * arg );

	static FragmentClientApp & instance()
	{
		MF_ASSERT( s_pInstance != NULL );
		return *s_pInstance;
	}

private:
	void connect();
	void disconnect();
	void sendMessage( const Mercury::InterfaceElement & ie );
	bool isGood() const { return status_ == Mercury::REASON_SUCCESS; }
	const char * errorMsg() const { return Mercury::reasonToString( status_ ); }

	Mercury::Channel *	pChannel_;
	unsigned			payloadSize_;
	unsigned			channelSeqAt_;
	unsigned			onceOffSeqAt_;
	unsigned 			numIterations_;
	Mercury::Reason		status_;

	static FragmentClientApp * s_pInstance;
};

/** Singleton instance. */
FragmentClientApp * FragmentClientApp::s_pInstance = NULL;


/**
 *	Constructor.
 *
 *	@param dstAddr				the server address
 *	@param payloadSizeBytes		the size of the payload to send to the server
 *								(msg1)
 *	@param numIterations		how many msg1 messages to send to the server
 */
FragmentClientApp::FragmentClientApp( const Mercury::Address & dstAddr,
		unsigned payloadSize,
		unsigned numIterations ):
	NetworkApp(),
	pChannel_( new Mercury::Channel(
			this->nub(), dstAddr, Mercury::Channel::EXTERNAL ) ),
	payloadSize_( payloadSize ),
	channelSeqAt_( 0 ),
	onceOffSeqAt_( 0 ),
	numIterations_( numIterations ),
	status_( Mercury::REASON_SUCCESS )
{
	INFO_MSG( "FragmentClientApp(%p)::ClientApp: server is at %s\n",
		this, dstAddr.c_str() );

	// Dodgy singleton code
	MF_ASSERT( s_pInstance == NULL );
	s_pInstance = this;
}


/**
 *	Destructor.
 */
FragmentClientApp::~FragmentClientApp()
{
	INFO_MSG( "FragmentClientApp(%d)::~ClientApp\n", getpid() );
	MF_ASSERT( s_pInstance == this );
	s_pInstance = NULL;

	if (pChannel_ != NULL)
	{
		pChannel_->destroy();
	}
}


/**
 *	App run function.
 */
int FragmentClientApp::run()
{
	INFO_MSG( "FragmentClientApp(%d)::run: Starting\n", getpid() );

	this->startTimer( TICK_PERIOD );

	// Connect to the server immediately
	this->connect();

	if (this->isGood())
	{
		try
		{
			this->nub().processContinuously();

			// Condemn our channel and wait till it runs dry
			INFO_MSG( "ClientApp(%d): Processing until channels empty\n",
				getpid() );

			pChannel_->condemn();
			pChannel_ = NULL;
			this->nub().processUntilChannelsEmpty();
		}
		catch (Mercury::NubException & ne)
		{
			ERROR_MSG( "ClientApp(%d): Caught exception %s\n",
					   getpid(), Mercury::reasonToString( ne.reason() ) );

			status_ = ne.reason();
		}
	}

	return status_ == Mercury::REASON_SUCCESS ? 0 : 1;
}


/**
 *	Attempt to connect to the server.
 */
void FragmentClientApp::connect()
{
	// Send connect unreliably as we don't have a channel yet
	Mercury::Bundle bundle;
	bundle.startMessage(
		FragmentServerInterface::connect, Mercury::RELIABLE_NO );

	this->nub().send( pChannel_->addr(), bundle );

	if (this->isGood())
	{
		TRACE_MSG( "FragmentClientApp(%d)::connect: Sent connect\n", getpid() );
	}
	else
	{
		ERROR_MSG( "FragmentClientApp(%d)::connect: "
			"Couldn't connect to server (%s)\n",
			getpid(), this->errorMsg() );
	}
}


/**
 *  This method disconnects from the server.
 */
void FragmentClientApp::disconnect()
{
	pChannel_->bundle().startMessage( FragmentServerInterface::disconnect );
	pChannel_->send();

	if (this->isGood())
	{
		TRACE_MSG( "FragmentClientApp(%d): Disconnected\n", getpid() );
	}
	else
	{
		ERROR_MSG( "FragmentClientApp(%d)::disconnect: "
			"Couldn't disconnect from server (%s)\n",
			getpid(), this->errorMsg() );
	}
}


/**
 *	Timeout handler. This sends one of each message to the server each tick.
 */
int FragmentClientApp::handleTimeout( Mercury::TimerID id, void * arg )
{
	// Send a message on the channel
	if (this->isGood())
	{
		Mercury::Bundle & bundle = pChannel_->bundle();

		bundle.startMessage( FragmentServerInterface::channelMsg );

		// Stream on sequence number
		bundle << channelSeqAt_;
		++channelSeqAt_;

		// Stream on payload
		for (unsigned i=1; i <= payloadSize_ / sizeof( unsigned ); i++)
		{
			bundle << i;
		}

		// We need to temporarily toggle loss like this because we can't lose
		// any of the unreliable messages.
		this->nub().setLossRatio( RELIABLE_LOSS_RATIO );

		pChannel_->send();

		if (!this->isGood())
		{
			ERROR_MSG( "FragmentClientApp(%d): "
				"Couldn't send channel msg to server (%s)\n",
				getpid(), this->errorMsg() );
		}

		this->nub().setLossRatio( 0.f );
	}

	// Send a once-off message
	if (this->isGood())
	{
		Mercury::Bundle bundle;

		bundle.startMessage( FragmentServerInterface::onceOffMsg,
			Mercury::RELIABLE_NO );

		// Stream on sequence number
		bundle << onceOffSeqAt_;
		++onceOffSeqAt_;

		// Stream on payload
		for (unsigned i=1; i <= payloadSize_ / sizeof( unsigned ); i++)
		{
			bundle << i;
		}

		this->nub().send( pChannel_->addr(), bundle );

		if (!this->isGood())
		{
			ERROR_MSG( "FragmentClientApp(%d): "
				"Couldn't send once off msg to server (%s)",
				getpid(), this->errorMsg() );
		}
	}

	if (onceOffSeqAt_ == numIterations_ || !this->isGood())
	{
		this->disconnect();
		this->stopTimer();
		this->nub().breakProcessing();
	}

	return 0;
}


/**
 *	Template class for handling variable length messages.
 */
template <class OBJECT_TYPE>
class VarLenMessageHandler : public Mercury::InputMessageHandler
{
	public:
		/**
		 *	This type is the function pointer type that handles the incoming
		 *	message.
		 */
		typedef void (OBJECT_TYPE::*Handler)( const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & stream );

		/**
		 *	Constructor.
		 */
		VarLenMessageHandler( Handler handler ) : handler_( handler ) {}

	private:
		// Override
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & header,
				BinaryIStream & data )
		{
			OBJECT_TYPE & object = OBJECT_TYPE::instance();
			(object.*handler_)( srcAddr, header, data );
		}

		Handler handler_;
};

TEST( Fragment_timingMethod )
{
	ASSERT_WITH_MESSAGE( g_timingMethod == GET_TIME_OF_DAY_TIMING_METHOD,
			"Incorrect timing method. Set environment variable "
				"BW_TIMING_METHOD to 'gettimeofday'\n" );

}

TEST( Fragment_children )
{
	const unsigned numChildren = 5;
	const unsigned payloadSizeBytes = PAYLOAD_SIZE;
	const float maxRunTimeSeconds = NUM_ITERATIONS * (TICK_PERIOD / 1000000.) * 3;

	TRACE_MSG( "TestFragment::testChildren: "
			"numChildren = %d, payload = %d \n",
		numChildren, payloadSizeBytes );
	FragmentServerApp serverApp( payloadSizeBytes,
		(unsigned long)(maxRunTimeSeconds * 1000000L) );
	MultiProcTestCase mp( serverApp );

	for (unsigned i = 0; i < numChildren; ++i)
	{
		mp.runChild( new FragmentClientApp( serverApp.nub().address(),
			payloadSizeBytes, NUM_ITERATIONS ) );
	}

	MULTI_PROC_TEST_CASE_WAIT_FOR_CHILDREN( mp );

	INFO_MSG( "TestFragment::testChildren: "
		"Got %u channel msgs, %u once off msgs, expecting %u of each\n",
		serverApp.channelMsgCount(), serverApp.onceOffMsgCount(),
		numChildren * NUM_ITERATIONS );

	ASSERT_WITH_MESSAGE(
		(serverApp.channelMsgCount() >= numChildren * NUM_ITERATIONS) &&
		(serverApp.onceOffMsgCount() >= numChildren * NUM_ITERATIONS),
		"Failed to receive all messages that were expected" );

	ASSERT_WITH_MESSAGE(
		(serverApp.channelMsgCount() == numChildren * NUM_ITERATIONS) &&
		(serverApp.onceOffMsgCount() == numChildren * NUM_ITERATIONS),
		"Received more messages than were expected" );

}

#define DEFINE_SERVER_HERE
#include "test_fragment_interfaces.hpp"


// test_fragment.cpp
