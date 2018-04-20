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

#include "network_app.hpp"
#include "test_channel_interfaces.hpp"

#include "network/nub.hpp"

namespace
{
uint g_tickRate = 1000; // 1ms

// The number of messages the client should send to the server.  Also the
// maximum number of messages the server will send to the client.
unsigned NUM_ITERATIONS = 100;
}

// -----------------------------------------------------------------------------
// Section: Peer
// -----------------------------------------------------------------------------

/**
 *  Someone the app is talking to.  Servers can have more than one of these,
 *  clients should only have one.
 */
class Peer : public Mercury::ChannelOwner, public SafeReferenceCount
{
public:
	Peer( Mercury::Nub & nub,
			const Mercury::Address & addr,
			Mercury::Channel::Traits traits ) :
		Mercury::ChannelOwner( nub, addr, traits ),
		timerID_( 0 ),
		inSeq_( 0 ),
		outSeq_( 0 )
	{}

	~Peer()
	{
		if (timerID_ != Mercury::TIMER_ID_NONE)
		{
			this->channel().nub().cancelTimer( timerID_ );
			timerID_ = Mercury::TIMER_ID_NONE;
		}
	}

	void startTimer( Mercury::Nub & nub, uint tickRate,
			Mercury::TimerExpiryHandler * pHandler )
	{
		timerID_ = nub.registerTimer( tickRate, pHandler, this );
	}

	void sendNextMessage()
	{
		ClientInterface::msg1Args & args =
			ClientInterface::msg1Args::start( this->bundle() );

		args.seq = outSeq_++;
		args.data = 0;

		if (outSeq_ == NUM_ITERATIONS)
		{
			this->channel().nub().cancelTimer( timerID_ );
			timerID_ = Mercury::TIMER_ID_NONE;
			this->channel().isIrregular( true );
		}

		this->send();
	}

	void receiveMessage( uint32 seq, uint32 data );
	void disconnect( uint32 seq );

private:
	Mercury::TimerID timerID_;
	uint32 inSeq_;
	uint32 outSeq_;
};

typedef SmartPointer< Peer > PeerPtr;

// -----------------------------------------------------------------------------
// Section: ChannelServerApp
// -----------------------------------------------------------------------------

class ChannelServerApp : public NetworkApp
{
public:
	ChannelServerApp() : NetworkApp()
	{
		// Dodgy singleton code
		MF_ASSERT( s_pInstance == NULL );
		s_pInstance = this;

		ServerInterface::registerWithNub( nub_ );
	}

	~ChannelServerApp()
	{
		MF_ASSERT( s_pInstance == this );
		s_pInstance = NULL;
	}

	void disconnect( const Mercury::Address & srcAddr,
			const ServerInterface::disconnectArgs & args );

	void msg1( const Mercury::Address & srcAddr,
			const ServerInterface::msg1Args & args );

	static ChannelServerApp & instance()
	{
		MF_ASSERT( s_pInstance != NULL );
		return *s_pInstance;
	}

	typedef std::map< Mercury::Address, PeerPtr > Peers;

protected:
	int handleTimeout( Mercury::TimerID id, void * arg )
	{
		PeerPtr pPeer = (Peer*)arg;
		pPeer->sendNextMessage();

		return 0;
	}

private:
	PeerPtr startChannel( const Mercury::Address & addr,
		Mercury::Channel::Traits traits );

	Peers peers_;

	static ChannelServerApp * s_pInstance;
};

ChannelServerApp * ChannelServerApp::s_pInstance = NULL;


/**
 *	Class for struct-style Mercury message handler objects.
 */
template <class ARGS> class ServerStructMessageHandler :
	public Mercury::InputMessageHandler
{
public:
	typedef void (ChannelServerApp::*Handler)(
			const Mercury::Address & srcAddr,
			const ARGS & args );

	ServerStructMessageHandler( Handler handler ) :
		handler_( handler )
	{}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
	{
		ARGS * pArgs = (ARGS*)data.retrieve( sizeof(ARGS) );
		(ChannelServerApp::instance().*handler_)( srcAddr, *pArgs );
	}

	Handler handler_;
};


PeerPtr ChannelServerApp::startChannel( const Mercury::Address & addr,
	Mercury::Channel::Traits traits )
{
	INFO_MSG( "Creating channel to %s\n", addr.c_str() );

	PeerPtr pPeer = new Peer( nub_, addr, traits );
	peers_[ addr ] = pPeer;

	pPeer->startTimer( nub_, g_tickRate, this );

	return pPeer;
}


// -----------------------------------------------------------------------------
// Section: ChannelServerApp Message Handlers
// -----------------------------------------------------------------------------

void ChannelServerApp::msg1( const Mercury::Address & srcAddr,
		const ServerInterface::msg1Args & args )
{
	PeerPtr pPeer = peers_[ srcAddr ];

	// If this is the first message from this client, connect him now.
	if (pPeer == NULL)
	{
		pPeer = this->startChannel( srcAddr, args.traits );
	}

	pPeer->receiveMessage( args.seq, args.data );
}


void ChannelServerApp::disconnect( const Mercury::Address & srcAddr,
		const ServerInterface::disconnectArgs & args )
{
	Peers::iterator peerIter = peers_.find( srcAddr );

	if (peerIter != peers_.end())
	{
		peerIter->second->disconnect( args.seq );
		peers_.erase( peerIter );

		if (peers_.empty())
		{
			this->nub().breakProcessing();
		}
	}
	else
	{
		ERROR_MSG( "ChannelServerApp::disconnectArgs: "
				"Got message from unknown peer at %s\n",
			srcAddr.c_str() );
	}
}


// -----------------------------------------------------------------------------
// Section: ChannelClientApp
// -----------------------------------------------------------------------------

class ChannelClientApp : public NetworkApp
{
public:
	ChannelClientApp( const Mercury::Address & dstAddr,
			Mercury::Nub * pMasterNub = NULL ) : NetworkApp(),
		outSeq_( 0 ),
		numToSend_( NUM_ITERATIONS ),
		pChannel_(
			new Mercury::Channel( nub_, dstAddr, Mercury::Channel::INTERNAL ) )
	{
		// Dodgy singleton code
		MF_ASSERT( s_pInstance == NULL );
		s_pInstance = this;

		// Register as a slave to the master nub
		pMasterNub->registerChildNub( &nub_ );

		ClientInterface::registerWithNub( nub_ );
	}

	~ChannelClientApp()
	{
		pChannel_->destroy();
		MF_ASSERT( s_pInstance == this );
		s_pInstance = NULL;
	}

	void startTest();

	int handleTimeout( Mercury::TimerID id, void * arg );

	void msg1( const Mercury::Address & srcAddr,
			const ClientInterface::msg1Args & args );

	static ChannelClientApp & instance()
	{
		MF_ASSERT( s_pInstance != NULL );
		return *s_pInstance;
	}

private:
	uint32 outSeq_;
	uint32 numToSend_;
	Mercury::Channel * pChannel_;

	static ChannelClientApp * s_pInstance;
};

ChannelClientApp * ChannelClientApp::s_pInstance = NULL;


/**
 *	Class for struct-style Mercury message handler objects.
 */
template <class ARGS> class ClientStructMessageHandler :
	public Mercury::InputMessageHandler
{
public:
	typedef void (ChannelClientApp::*Handler)(
			const Mercury::Address & srcAddr,
			const ARGS & args );

	ClientStructMessageHandler( Handler handler ) :
		handler_( handler )
	{}

private:
	virtual void handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data )
	{
		ARGS * pArgs = (ARGS*)data.retrieve( sizeof(ARGS) );
		(ChannelClientApp::instance().*handler_)( srcAddr, *pArgs );
	}

	Handler handler_;
};


void ChannelClientApp::startTest()
{
	this->startTimer( g_tickRate );
}

int ChannelClientApp::handleTimeout( Mercury::TimerID id, void * arg )
{
	ServerInterface::msg1Args & args =
		ServerInterface::msg1Args::start( pChannel_->bundle() );

	args.traits = pChannel_->traits();
	args.seq = outSeq_++;
	args.data = 0;

	if (outSeq_ == numToSend_)
	{
		ServerInterface::disconnectArgs & args =
			ServerInterface::disconnectArgs::start( pChannel_->bundle() );

		args.seq = outSeq_;
		this->stopTimer();
		pChannel_->isIrregular( true );
	}

	pChannel_->send();

	return 0;
}

void ChannelClientApp::msg1( const Mercury::Address & srcAddr,
			const ClientInterface::msg1Args & args )
{
}


// -----------------------------------------------------------------------------
// Section: Peer
// -----------------------------------------------------------------------------

void Peer::receiveMessage( uint32 seq, uint32 data )
{
	MF_ASSERT( inSeq_ == seq );
	inSeq_ = seq + 1;
}

void Peer::disconnect( uint32 seq )
{
	MF_ASSERT( inSeq_ == seq );
}

/**
 *	This method is a simple channel test.
 */
TEST( Channel_testSimpleChannel )
{
	ChannelServerApp serverApp;
	ChannelClientApp clientApp( serverApp.nub().address(), &serverApp.nub() );

	clientApp.startTest();
	serverApp.run();
}

/**
 *	This method is a channel test with loss.
 */
// This tests fails under windows: http://bugzilla/show_bug.cgi?id=14203
#ifdef MF_SERVER
TEST( Channel_testLoss )
{
	float LOSS_RATIO = 0.1f;

	ChannelServerApp serverApp;
	ChannelClientApp clientApp( serverApp.nub().address(), &serverApp.nub() );

	serverApp.nub().setLossRatio( LOSS_RATIO );
	clientApp.nub().setLossRatio( LOSS_RATIO );

	clientApp.startTest();
	serverApp.run();
}
#endif

#define DEFINE_SERVER_HERE
#include "test_channel_interfaces.hpp"


// test_channel.cpp
