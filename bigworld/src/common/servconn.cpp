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

#include "cstdmf/debug.hpp"
#include "cstdmf/concurrency.hpp"

DECLARE_DEBUG_COMPONENT2( "Connect", 0 )

#include "servconn.hpp"

#include "math/vector3.hpp"
#include "network/portmap.hpp"
#include "network/encryption_filter.hpp"
#include "network/public_key_cipher.hpp"
//#include "cstdmf/dprintf.hpp"

#include "common/login_interface.hpp"
#include "common/baseapp_ext_interface.hpp"
#include "common/client_interface.hpp"
#include "cstdmf/memory_stream.hpp"

#ifndef CODE_INLINE
#include "servconn.ipp"
#endif

namespace
{
/// The number of microseconds to wait for a reply to the login request.
const int LOGIN_TIMEOUT = 8000000;					// 8 seconds

/// The number of seconds of inactivity before a connection is closed.
const float DEFAULT_INACTIVITY_TIMEOUT = 60.f;			// 60 seconds

/// How many times should the LoginApp login message be sent before giving up.
const int MAX_LOGINAPP_LOGIN_ATTEMPTS = 10;

// How often we send a LoginApp login message.
const int LOGINAPP_LOGIN_ATTEMPT_PERIOD = 1000000;	// 1 second

/// How many times should the BaseApp login message be sent before giving up.
const int MAX_BASEAPP_LOGIN_ATTEMPTS = 10;

// How often we send a BaseApp login message. A new port is used for each.
const int BASEAPP_LOGIN_ATTEMPT_PERIOD = 1000000; // 1 second

}

#ifdef WIN32
#pragma warning (disable:4355)	// this used in base member initialiser list
#endif


// -----------------------------------------------------------------------------
// Section: RetryingRequest
// -----------------------------------------------------------------------------


/**
 *  Constructor.
 */
RetryingRequest::RetryingRequest( LoginHandler & parent,
		const Mercury::Address & addr,
		const Mercury::InterfaceElement & ie,
		int retryPeriod,
		int timeoutPeriod,
		int maxAttempts,
		bool useParentNub ) :
	pParent_( &parent ),
	pNub_( NULL ),
	addr_( addr ),
	ie_( ie ),
	timerID_( Mercury::TIMER_ID_NONE ),
	done_( false ),
	retryPeriod_( retryPeriod ),
	timeoutPeriod_( timeoutPeriod ),
	numAttempts_( 0 ),
	numOutstandingAttempts_( 0 ),
	maxAttempts_( maxAttempts )
{
	if (useParentNub)
	{
		this->setNub( &parent.servConn().nub() );
	}

	parent.addChildRequest( this );
}


/**
 *  Destructor.
 */
RetryingRequest::~RetryingRequest()
{
	MF_ASSERT_DEV( timerID_ == Mercury::TIMER_ID_NONE );
}


/**
 *  This method handles a reply to one of our requests.
 */
void RetryingRequest::handleMessage( const Mercury::Address & srcAddr,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data,
	void * arg )
{
	if (!done_)
	{
		this->onSuccess( data );
		this->cancel();
	}

	--numOutstandingAttempts_;
	this->decRef();
}


/**
 *  This method handles an exception, usually a timeout.  Whatever happens, it
 *  indicates that one of our attempts is over.
 */
void RetryingRequest::handleException( const Mercury::NubException & exc,
	void * arg )
{
	// If something has gone terribly wrong, call the failure callback.
	if (!done_ && (exc.reason() != Mercury::REASON_TIMER_EXPIRED))
	{
		ERROR_MSG( "RetryingRequest::handleException( %s ): "
			"Request to %s failed (%s)\n",
			ie_.name(), addr_.c_str(), Mercury::reasonToString( exc.reason() ) );

		this->onFailure( exc.reason() );
		this->cancel();
	}

	--numOutstandingAttempts_;

	// If the last attempt has failed, we're done.
	if (!done_ && numOutstandingAttempts_ == 0)
	{
		// If this request had a maxAttempts of 1, we assume that retrying is
		// taking place by spawning multiple instances of this request (a la
		// BaseAppLoginRequest), so we don't emit this error message.
		if (maxAttempts_ > 1)
		{
			ERROR_MSG( "RetryingRequest::handleException( %s ): "
				"Final attempt of %d has failed (%s), aborting\n",
				ie_.name(), maxAttempts_,
				Mercury::reasonToString( exc.reason() ) );
		}

		this->onFailure( exc.reason() );
		this->cancel();
	}

	this->decRef();
}


/**
 *  This method handles an internally scheduled timeout, which causes another
 *  attempt to be sent.
 */
int RetryingRequest::handleTimeout( Mercury::TimerID id, void * arg )
{
	this->send();
	return 0;
}


/**
 *  This method sets the nub to be used by this object.  You cannot call send()
 *  until you have called this.
 */
void RetryingRequest::setNub( Mercury::Nub * pNub )
{
	MF_ASSERT_DEV( pNub_ == NULL );
	pNub_ = pNub;
	timerID_ = pNub_->registerTimer( retryPeriod_, this );
}


/**
 *  This method sends the request once.  This method should be called as the
 *  last statement in the constructor of a derived class.
 */
void RetryingRequest::send()
{
	if (done_) return;

	if (numAttempts_ < maxAttempts_)
	{
		++numAttempts_;

		Mercury::Bundle bundle;
		bundle.startRequest( ie_, this, NULL, timeoutPeriod_,
			Mercury::RELIABLE_NO );

		this->addRequestArgs( bundle );

		// Calling send may decrement this if an exception occurs.
		RetryingRequestPtr pThis = this;

		this->incRef();
		++numOutstandingAttempts_;

		pNub_->send( addr_, bundle );
	}
}


/**
 *  This method removes this request from the parent's childRequests_ list,
 *  which means it will be destroyed as soon as all of its outstanding requests
 *  have either been replied to (and ignored) or timed out.
 */
void RetryingRequest::cancel()
{
	if (timerID_ != Mercury::TIMER_ID_NONE)
	{
		pNub_->cancelTimer( timerID_ );
		timerID_ = Mercury::TIMER_ID_NONE;
	}
	pParent_->removeChildRequest( this );
	done_ = true;
}


// -----------------------------------------------------------------------------
// Section: LoginRequest
// -----------------------------------------------------------------------------

/**
 *  Constructor.
 */
LoginRequest::LoginRequest( LoginHandler & parent ) :
	RetryingRequest( parent, parent.loginAddr(), LoginInterface::login )
{
	this->send();
}


void LoginRequest::addRequestArgs( Mercury::Bundle & bundle )
{
	LogOnParamsPtr pParams = pParent_->pParams();

	bundle << LOGIN_VERSION;

#ifdef USE_OPENSSL
	if (!pParams->addToStream( bundle, LogOnParams::HAS_ALL,
			&pParent_->servConn().publicKey() ))
#else
	if (!pParams->addToStream( bundle, LogOnParams::HAS_ALL,
			NULL))
#endif
	{
		ERROR_MSG( "LoginRequest::addRequestArgs: "
			"Failed to assemble login bundle\n" );

		pParent_->onFailure( Mercury::REASON_CORRUPTED_PACKET );
	}
}


void LoginRequest::onSuccess( BinaryIStream & data )
{
	pParent_->onLoginReply( data );
}


void LoginRequest::onFailure( Mercury::Reason reason )
{
	pParent_->onFailure( reason );
}


// -----------------------------------------------------------------------------
// Section: BaseAppLoginRequest
// -----------------------------------------------------------------------------


/**
 *  Constructor.
 */
BaseAppLoginRequest::BaseAppLoginRequest( LoginHandler & parent ) :
	RetryingRequest( parent, parent.baseAppAddr(),
		BaseAppExtInterface::baseAppLogin,
		DEFAULT_RETRY_PERIOD,
		DEFAULT_TIMEOUT_PERIOD,
		/* maxAttempts: */ 1,
		/* useParentNub: */ false ),
	attempt_( pParent_->numBaseAppLoginAttempts() )
{
	ServerConnection & servConn = pParent_->servConn();

	// Each instance has its own Nub.  We need to do this to cope with strange
	// multi-level NATing issues in China.
	Mercury::Nub* pNub = new Mercury::Nub();

	// We set the nub to be internal to receive once-off reliable messages.
	pNub->isExternal( false );
	this->setNub( pNub );

	// MF_ASSERT( pNub->socket() != -1 );

	pChannel_ = new Mercury::Channel(
		*pNub_, pParent_->baseAppAddr(),
		Mercury::Channel::EXTERNAL,
		/* minInactivityResendDelay: */ 1.0,
		servConn.pFilter().getObject() );

	// Set the servconn as the bundle primer
	pChannel_->bundlePrimer( servConn );

	// The channel is irregular until we get cellPlayerCreate (i.e. entities
	// enabled).
	pChannel_->isIrregular( true );

	// This temporary nub must serve all the interfaces that the main nub
	// serves, because if the initial downstream packets are lost and the
	// reply to the baseAppLogin request actually comes back as a piggyback
	// on a packet with other ClientInterface messages, this nub will need
	// to know how to process them.
	servConn.registerInterfaces( *pNub_ );

	// Make use of the main nub's socket for the first one sent. It simplifies
	// things if we switchSockets here and switchSockets back if we win.
	if (attempt_ == 0)
	{
		pNub_->switchSockets( &servConn.nub() );
	}

	// Register as a slave to the main nub.
	servConn.nub().registerChildNub( pNub_ );

	this->send();
}


/**
 *  Destructor.
 */
BaseAppLoginRequest::~BaseAppLoginRequest()
{
	// The winner's pChannel_ will be NULL since it has been transferred to the
	// servconn.
	// delete pChannel_;
	if (pChannel_)
	{
		pChannel_->destroy();
		pChannel_ = NULL;
	}

	// Transfer the temporary Nub to the LoginHandler to clean up later.  We
	// can't just delete the Nub now because it is halfway through processing
	// when this destructor is called.
	pParent_->addCondemnedNub( pNub_ );
	pNub_ = NULL;
}


/**
 *  This method creates another instance of this class, instead of the default
 *  behaviour which is to just resend another request from this object.
 */
int BaseAppLoginRequest::handleTimeout( Mercury::TimerID id, void * arg )
{
	// Each request should only spawn one other request
	pNub_->cancelTimer( timerID_ );
	timerID_ = Mercury::TIMER_ID_NONE;

	if (!done_)
	{
		pParent_->sendBaseAppLogin();
	}
	return 0;
}


/**
 *  Streams on required args for baseapp login.
 */
void BaseAppLoginRequest::addRequestArgs( Mercury::Bundle & bundle )
{
	// Send the loginKey and number of attempts (for debugging reasons)
	bundle << pParent_->replyRecord().sessionKey << attempt_;
}


/**
 *	This method handles the reply to the baseAppLogin message. Getting this
 *	called means that this is the winning BaseAppLoginHandler.
 */
void BaseAppLoginRequest::onSuccess( BinaryIStream & data )
{
	SessionKey sessionKey = 0;
	data >> sessionKey;
	pParent_->onBaseAppReply( this, sessionKey );

	// Forget about our Channel because it has been transferred to the
	// ServerConnection.
	pChannel_ = NULL;
}


/**
 * This method is an override of the equivalant method in retryingrequest.
 * Because these things keep their own nubs it is important to cancel the
 * request now so that when the parent LoginHandler is finally dereferenced
 * it not be via one of these timing out since that will happen in a member
 * function of one of the child nubs that will be freed.
 */
void BaseAppLoginRequest::cancel()
{
	this->RetryingRequest::cancel();

	pNub_->cancelReplyMessageHandler( this );
}


// -----------------------------------------------------------------------------
// Section: LoginHandler
// -----------------------------------------------------------------------------

/**
 *	Constructor
 */
LoginHandler::LoginHandler(
		ServerConnection* pServerConnection, LogOnStatus loginNotSent ) :
	loginAppAddr_( Mercury::Address::NONE ),
	baseAppAddr_( Mercury::Address::NONE ),
	pParams_( NULL ),
	pServerConnection_( pServerConnection ),
	done_( loginNotSent != LogOnStatus::NOT_SET ),
	status_( loginNotSent ),
	numBaseAppLoginAttempts_( 0 )
{
}


LoginHandler::~LoginHandler()
{
	// Destroy any condemned Nubs that have been left to us
	for (CondemnedNubs::iterator iter = condemnedNubs_.begin();
		 iter != condemnedNubs_.end(); ++iter)
	{
		delete *iter;
	}
}


void LoginHandler::start( const Mercury::Address & loginAppAddr,
	LogOnParamsPtr pParams )
{
	loginAppAddr_ = loginAppAddr;
	pParams_ = pParams;
	this->sendLoginAppLogin();
}


void LoginHandler::finish()
{
	// Clear out all child requests
	while (!childRequests_.empty())
	{
		(*childRequests_.begin())->cancel();
	}
	pServerConnection_->nub().breakProcessing();
	done_ = true;
}


/**
 *  This method sends the login request to the server.
 */
void LoginHandler::sendLoginAppLogin()
{
	new LoginRequest( *this );
}


/**
 *	This method handles the login reply message from the LoginApp.
 */
void LoginHandler::onLoginReply( BinaryIStream & data )
{
	data >> status_;

	if (status_ == LogOnStatus::LOGGED_ON)
	{
		// The reply record is symmetrically encrypted.
#ifdef USE_OPENSSL
		if (pServerConnection_->pFilter())
		{
			MemoryOStream clearText;
			pServerConnection_->pFilter()->decryptStream( data, clearText );
			clearText >> replyRecord_;
		}
		else
#endif
		{
			data >> replyRecord_;
		}

		// Correct sized reply
		if (!data.error())
		{
			baseAppAddr_ = replyRecord_.serverAddr;
			this->sendBaseAppLogin();
			errorMsg_ = "";
		}
		else
		{
			ERROR_MSG( "LoginHandler::handleMessage: "
				"Got reply of unexpected size (%d)\n",
				data.remainingLength() );

			status_ = LogOnStatus::CONNECTION_FAILED;
			errorMsg_ = "Mercury::REASON_CORRUPTED_PACKET";

			this->finish();
		}
	}
	else
	{
		data >> errorMsg_;

		if (errorMsg_.empty())	// this really shouldn't happen
		{
			if (status_ == LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR)
			{
				errorMsg_ = "Unspecified error.";
			}
			else
			{
				errorMsg_ = "Unelaborated error.";
			}
		}

		this->finish();
	}
}


/**
 *	This method sends a login request to the BaseApp. There can be multiple
 *	login requests outstanding at any given time (sent from different sockets).
 *	Only one will win.
 */
void LoginHandler::sendBaseAppLogin()
{
	if (numBaseAppLoginAttempts_ < MAX_BASEAPP_LOGIN_ATTEMPTS)
	{
		new BaseAppLoginRequest( *this );
		++numBaseAppLoginAttempts_;
	}
	else
	{
		status_ = LogOnStatus::CONNECTION_FAILED;
		errorMsg_ =
			"Unable to connect to BaseApp: "
			"A NAT or firewall error may have occurred?";

		this->finish();
	}
}


/**
 *	This method is called when a reply to baseAppLogin is received from the
 *	BaseApp (or an exception occurred - likely a timeout).
 *
 *	The first successful reply wins and we do not care about the rest.
 */
void LoginHandler::onBaseAppReply( BaseAppLoginRequestPtr pHandler,
	SessionKey sessionKey )
{
	Mercury::Nub * pMainNub = &pServerConnection_->nub();

	// Make this successful socket the main nub's socket.
	pHandler->nub().switchSockets( pMainNub );

	// Transfer the successful channel to the main nub and the servconn.
	pHandler->channel().switchNub( pMainNub );
	pServerConnection_->channel( pHandler->channel() );

	// This is the session key that authenticate message should send.
	replyRecord_.sessionKey = sessionKey;
	pServerConnection_->sessionKey( sessionKey );

	this->finish();
}


/**
 *	This method handles a network level failure.
 */
void LoginHandler::onFailure( Mercury::Reason reason )
{
	status_ = LogOnStatus::CONNECTION_FAILED;
	errorMsg_ = "Mercury::";
	errorMsg_ += Mercury::reasonToString( reason );

	this->finish();
}


/**
 *  This method add a RetryingRequest to this LoginHandler.
 */
void LoginHandler::addChildRequest( RetryingRequestPtr pRequest )
{
	childRequests_.insert( pRequest );
}


/**
 *  This method removes a RetryingRequest from this LoginHandler.
 */
void LoginHandler::removeChildRequest( RetryingRequestPtr pRequest )
{
	childRequests_.erase( pRequest );
}


/**
 *  This method adds a condemned Nub to this LoginHandler.  This is used by
 *  BaseAppLoginRequests to clean up their Nubs as they are unable to do it
 *  themselves.
 */
void LoginHandler::addCondemnedNub( Mercury::Nub * pNub )
{
	condemnedNubs_.push_back( pNub );
}


// -----------------------------------------------------------------------------
// Section: EntityMessageHandler declaration
// -----------------------------------------------------------------------------

/**
 *	This class is used to handle handleDataFromServer message from the server.
 */
class EntityMessageHandler : public Mercury::InputMessageHandler
{
	public:
		EntityMessageHandler();

	protected:
		/// This method handles messages from Mercury.
		virtual void handleMessage( const Mercury::Address & /* srcAddr */,
			Mercury::UnpackedMessageHeader & msgHeader,
			BinaryIStream & data );
};

EntityMessageHandler g_entityMessageHandler;


// -----------------------------------------------------------------------------
// Section: DownloadSegment / DataDownload
// -----------------------------------------------------------------------------

DataDownload::~DataDownload()
{
	for (iterator it = this->begin(); it != this->end(); ++it)
		delete *it;

	if (pDesc_ != NULL)
		delete pDesc_;
}


/**
 *  Insert the segment into this record in a sorted fashion.
 */
void DataDownload::insert( DownloadSegment *pSegment, bool isLast )
{
	uint8 inseq = pSegment->seq_;

	// Make a note of holes if we're making them
	if (!this->empty() && offset( inseq, this->back()->seq_ ) > 1)
	{
		for (int hole = (this->back()->seq_ + 1) % 0xff;
			 hole != inseq;
			 hole = (hole + 1) % 0xff)
		{
			holes_.insert( hole );
		}
	}

	// An iterator pointing to the newly inserted segment
	iterator insPos;

	// Chuck the new segment at the end if that's obviously the place for it
	if (this->empty() || offset( inseq, this->back()->seq_ ) > 0)
	{
		this->push_back( pSegment );
		insPos = this->end(); --insPos;
	}

	// Otherwise, find an insertion point working backwards from the end
	else
	{
		iterator it = this->end(); --it;
		while (it != this->begin() && offset( inseq, (*it)->seq_ ) > 0)
			--it;

		insPos = std::list< DownloadSegment* >::insert( it, pSegment );
	}

	// Check if we've filled a hole
	if (!holes_.empty())
	{
		std::set< int >::iterator it = holes_.find( inseq );

		if (it != holes_.end())
			holes_.erase( it );
	}

	// If we received the expected packet, update the expected field
	if (inseq == expected_)
	{
		for (++insPos; insPos != this->end(); ++insPos)
		{
			iterator next = insPos; ++next;

			// If the iterator is pointing to the last element in the chain or
			// the next segment does not follow this one, then we are expecting
			// the packet after this one
			if (*insPos == this->back() ||
				offset( (*next)->seq_, (*insPos)->seq_ ) != 1)
			{
				expected_ = ((*insPos)->seq_ + 1) % 0xff;
				break;
			}
		}
	}

	if (isLast)
		hasLast_ = true;
}


/**
 *  Returns true if this piece of DataDownload is complete and ready to be returned
 *  back into onStreamComplete().
 */
bool DataDownload::complete()
{
	return holes_.empty() && hasLast_ && pDesc_ != NULL;
}


/**
 *  Write the contents of this DataDownload into a BinaryOStream.  This DataDownload
 *  must be complete() before calling this.
 */
void DataDownload::write( BinaryOStream &os )
{
	MF_ASSERT_DEV( this->complete() );
	for (iterator it = this->begin(); it != this->end(); ++it)
	{
		DownloadSegment &segment = **it;
		os.addBlob( segment.data(), segment.size() );
	}
}


/**
 *  Set the description for this download from the provided stream.
 */
void DataDownload::setDesc( BinaryIStream &is )
{
	pDesc_ = new std::string();
	is >> *pDesc_;
}


/**
 *  Basically returns seq1 - seq2, adjusted for the ring buffery-ness of the
 *  8-bit sequence numbers used.
 */
int DataDownload::offset( int seq1, int seq2 )
{
	seq1 = (seq1 + 0xff - expected_) % 0xff;
	seq2 = (seq2 + 0xff - expected_) % 0xff;
	return seq1 - seq2;
}


static bool s_requestQueueEnabled;


/**
 *	This static members stores the number of updates per second to expect from
 *	the server.
 */
float ServerConnection::s_updateFrequency_ = 10.f;

/**
 *	Constructor.
 */
ServerConnection::ServerConnection() :
	sessionKey_( 0 ),
	pHandler_( NULL ),
	pTime_( NULL ),
	lastTime_( 0.0 ),
	minSendInterval_( 1.01/20.0 ),
	nub_( ),
	pChannel_( NULL ),
	inactivityTimeout_( DEFAULT_INACTIVITY_TIMEOUT ),
	// see also initialiseConnectionState
	FIRST_AVATAR_UPDATE_MESSAGE(
		ClientInterface::avatarUpdateNoAliasFullPosYawPitchRoll.id() ),
	LAST_AVATAR_UPDATE_MESSAGE(
		ClientInterface::avatarUpdateAliasNoPosNoDir.id() ),
#ifdef USE_OPENSSL
	pFilter_( new Mercury::EncryptionFilter() ),
	publicKey_( /* hasPrivate: */ false )
#else
	pFilter_( NULL )
#endif
{

	tryToReconfigurePorts_ = false;
	this->initialiseConnectionState();

	memset( &digest_, 0, sizeof( digest_ ) );

	nub_.isExternal( true );

	// set once-off reliability resend period
	nub_.onceOffResendPeriod( CLIENT_ONCEOFF_RESEND_PERIOD );
	nub_.onceOffMaxResends( CLIENT_ONCEOFF_MAX_RESENDS );
}


/**
 *	Destructor
 */
ServerConnection::~ServerConnection()
{
	// disconnect if we didn't already do so
	this->disconnect();

	// Destroy our channel.  This must be done immediately (i.e. we can't
	// just condemn it) because the ~Channel() must execute before ~Nub().
	if (pChannel_)
	{
		// delete pChannel_;
		pChannel_->destroy();
		pChannel_ = NULL;
	}
}


/**
 *	This private method initialises or reinitialises our state related to a
 *	connection. It should be called before a new connection is made.
 */
void ServerConnection::initialiseConnectionState()
{
	id_ = EntityID( -1 );
	spaceID_ = SpaceID( -1 );
	bandwidthFromServer_ = 0;

	lastSendTime_ = 0.0;

	everReceivedPacket_ = false;
	entitiesEnabled_ = false;

	serverTimeHandler_ = ServerTimeHandler();

	sendingSequenceNumber_ = 0;

	memset( idAlias_, 0, sizeof( idAlias_ ) );

	// Setting the nub to be internal here so that once-off reliable messages are
	// delivered. Once we get the channel to the BaseApp, we set the nub to be
	// external to block once-off reliable messages.
	// External nubs also drop packets from recently dead channels, which will
	// suppress ugly errors on disconnect.
	nub_.isExternal( false );

	controlledEntities_.clear();
}

/**
 *	This private helper method registers the mercury interfaces with the
 *	provided nub, so that it will serve them.  This should be done for every Nub
 *	that might receive messages from the server, which includes the temporary
 *	Nubs used in the BaseApp login process.
 */
void ServerConnection::registerInterfaces( Mercury::Nub & nub )
{
	ClientInterface::registerWithNub( nub );

	for (Mercury::MessageID id = 128; id != 255; id++)
	{
		nub.serveInterfaceElement( ClientInterface::entityMessage,
			id, &g_entityMessageHandler );
	}

	// Message handlers have access to the nub that the message arrived on. Set
	// the ServerConnection here so that the message handler knows which one to
	// deliver the message to. (This is used by bots).
	nub.pExtensionData( this );
}


THREADLOCAL( bool ) g_isMainThread( false );


/**
 *	This method logs in to the named server with the input username and
 *	password.
 *
 *	@param pHandler		The handler to receive messages from this connection.
 *	@param serverName	The name of the server to connect to.
 *	@param username		The username to log on with.
 *	@param password		The password to log on with.
 *	@param publicKeyPath	The path to locate the public key to use for
 *						encrypting login communication.
 *	@param port			The port that the server should talk to us on.
 */
LogOnStatus ServerConnection::logOn( ServerMessageHandler * pHandler,
	const char * serverName,
	const char * username,
	const char * password,
	const char * publicKeyPath,
	uint16 port )
{
	LoginHandlerPtr pLoginHandler = this->logOnBegin(
		serverName, username, password, publicKeyPath, port );

	// Note: we could well get other messages here and not just the
	// reply (if the proxy sends before we get the reply), but we
	// don't add ourselves to the master/slave system before then
	// so we won't be found ... and in the single servconn case the
	// channel (data) won't be created until then so the the intermediate
	// messages will be resent.

	while (!pLoginHandler->done())
	{
		try
		{
			nub_.processContinuously();
		}
		catch (Mercury::NubException& ex)
		{
			WARNING_MSG(
				"servconn::logOn: Got Mercury Exception %d\n", ex.reason() );
		}
	}

	LogOnStatus status = this->logOnComplete( pLoginHandler, pHandler );

	if (status == LogOnStatus::LOGGED_ON)
	{
		this->enableEntities();
	}

	return status;
}


#if defined( PLAYSTATION3 ) || defined( _XBOX360 )
static char s_pubKey[] = "-----BEGIN PUBLIC KEY-----\n"\
"MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA7/MNyWDdFpXhpFTO9LHz\n"\
"CUQPYv2YP5rqJjUoxAFa3uKiPKbRvVFjUQ9lGHyjCmtixBbBqCTvDWu6Zh9Imu3x\n"\
"KgCJh6NPSkddH3l+C+51FNtu3dGntbSLWuwi6Au1ErNpySpdx+Le7YEcFviY/ClZ\n"\
"ayvVdA0tcb5NVJ4Axu13NvsuOUMqHxzCZRXCe6nyp6phFP2dQQZj8QZp0VsMFvhh\n"\
"MsZ4srdFLG0sd8qliYzSqIyEQkwO8TQleHzfYYZ90wPTCOvMnMe5+zCH0iPJMisP\n"\
"YB60u6lK9cvDEeuhPH95TPpzLNUFgmQIu9FU8PkcKA53bj0LWZR7v86Oco6vFg6V\n"\
"sQIDAQAB\n"\
"-----END PUBLIC KEY-----\n";
#endif

/**
 *	This method begins an asynchronous login
 */
LoginHandlerPtr ServerConnection::logOnBegin(
	const char * serverName,
	const char * username,
	const char * password,
	const char * publicKeyPath,
	uint16 port )
{
	std::string key = pFilter_ ? pFilter_->key() : "";
	LogOnParamsPtr pParams = new LogOnParams( username, password, key );

	pParams->digest( this->digest() );

	g_isMainThread = true;

	// make sure we are not already logged on
	if (this->online())
	{
		return new LoginHandler( this, LogOnStatus::ALREADY_ONLINE_LOCALLY );
	}

	this->initialiseConnectionState();

	TRACE_MSG( "ServerConnection::logOnBegin: "
		"server:%s username:%s\n", serverName, pParams->username().c_str() );

	username_ = pParams->username();

	// Register the interfaces if they have not already been registered.
	this->registerInterfaces( nub_ );

	// Find out where we want to log in to
	uint16 loginPort = port ? port : PORT_LOGIN;

	const char * serverPort = strchr( serverName, ':' );
	std::string serverNameStr;
	if (serverPort != NULL)
	{
		loginPort = atoi( serverPort+1 );
		serverNameStr.assign( serverName, serverPort - serverName );
		serverName = serverNameStr.c_str();
	}

	Mercury::Address loginAddr( 0, htons( loginPort ) );
	if (Endpoint::convertAddress( serverName, (u_int32_t&)loginAddr.ip ) != 0 ||
		loginAddr.ip == 0)
	{
		return new LoginHandler( this, LogOnStatus::DNS_LOOKUP_FAILED );
	}

	// Use a standard key path if none is provided
	if (!publicKeyPath || *publicKeyPath == '\0')
	{
		publicKeyPath = "loginapp.pubkey";
	}

#ifdef USE_OPENSSL
#if defined( PLAYSTATION3 ) || defined( _XBOX360 )
	if (!publicKey_.setKey( s_pubKey ))
	{
		return new LoginHandler( this, LogOnStatus::PUBLIC_KEY_LOOKUP_FAILED );
	}
#else
	// Read the public key we're using to encrypt the login credentials
	if (!publicKey_.setKeyFromResource( publicKeyPath ))
	{
		return new LoginHandler( this, LogOnStatus::PUBLIC_KEY_LOOKUP_FAILED );
	}
#endif
#endif

	// Create a LoginHandler and start the handshake
	LoginHandlerPtr pLoginHandler = new LoginHandler( this );
	pLoginHandler->start( loginAddr, pParams );
	return pLoginHandler;
}

/**
 *	This method completes an asynchronous login.
 *
 *	Note: Don't call this from within processing the bundle that contained
 *	the reply if you have multiple ServConns, as it could stuff up processing
 *	for another of the ServConns.
 */
LogOnStatus ServerConnection::logOnComplete(
	LoginHandlerPtr pLoginHandler,
	ServerMessageHandler * pHandler )
{
	LogOnStatus status = LogOnStatus::UNKNOWN_ERROR;

	MF_ASSERT_DEV( pLoginHandler != NULL );

	if (this->online())
	{
		status = LogOnStatus::ALREADY_ONLINE_LOCALLY;
	}

	status = pLoginHandler->status();

	if ((status == LogOnStatus::LOGGED_ON) &&
			!this->online())
	{
		WARNING_MSG( "ServerConnection::logOnComplete: "
				"Already logged off\n" );

		status = LogOnStatus::CANCELLED;
		errorMsg_ = "Already logged off";
	}

	if (status == LogOnStatus::LOGGED_ON)
	{
		DEBUG_MSG( "ServerConnection::logOn: status==LOGGED_ON\n" );

		const LoginReplyRecord & result = pLoginHandler->replyRecord();

		DEBUG_MSG( "ServerConnection::logOn: from: %s\n",
				nub_.address().c_str() );
		DEBUG_MSG( "ServerConnection::logOn: to:   %s\n",
				result.serverAddr.c_str() );

		// We establish our channel to the BaseApp in
		// BaseAppLoginHandler::handleMessage - this is just a sanity check.
		if (result.serverAddr != this->addr())
		{
			char winningAddr[ 256 ];
			strncpy( winningAddr, this->addr().c_str(), sizeof( winningAddr ) );

			WARNING_MSG( "ServerConnection::logOnComplete: "
				"BaseApp address on login reply (%s) differs from winning "
				"BaseApp reply (%s)\n",
				result.serverAddr.c_str(), winningAddr );
		}
	}
	else if (status == LogOnStatus::CONNECTION_FAILED)
	{
		ERROR_MSG( "ServerConnection::logOnComplete: Logon failed (%s)\n",
				pLoginHandler->errorMsg().c_str() );
		status = LogOnStatus::CONNECTION_FAILED;
		errorMsg_ = pLoginHandler->errorMsg();
	}
	else if (status == LogOnStatus::DNS_LOOKUP_FAILED)
	{
 		errorMsg_ = "DNS lookup failed";
		ERROR_MSG( "ServerConnection::logOnComplete: Logon failed: %s\n",
				errorMsg_.c_str() );
	}
	else
	{
		errorMsg_ = pLoginHandler->errorMsg();
		INFO_MSG( "ServerConnection::logOnComplete: Logon failed: %s\n",
				errorMsg_.c_str() );
	}

	// Release the reply handler
	pLoginHandler = NULL;

	// Get out if we didn't log on
	if (status != LogOnStatus::LOGGED_ON)
	{
		return status;
	}

	// Yay we logged on!

	id_ = 0;

	s_requestQueueEnabled = true;

	// This nub may have been set to internal because of server discovery replies
	// needing once-off reliable replies. We reset it to be external here so that
	// we get support for ignoring packets on recently dead channels.
	nub_.isExternal( true );

	// Send an initial packet to the proxy to open up a hole in any
	// firewalls on our side of the connection.  We have to re-prime the
	// outgoing bundle since sessionKey_ would not have been set when it was
	// primed in our ctor.
	this->primeBundle( this->bundle() );
	this->send();

	// DEBUG_MSG( "ServerConnection::logOn: sent initial message to server\n" );

	// Install the user's server message handler until we disconnect
	// (serendipitous leftover argument from when it used to be called
	// here to create the player entity - glad I didn't clean that up :)
	pHandler_ = pHandler;

	// Disconnect if we do not receive anything for this number of seconds.
	this->channel().startInactivityDetection( inactivityTimeout_ );

	return status;
}


/**
 *	This method enables the receipt of entity and related messages from the
 *	server, i.e. the bulk of game traffic. The server does not start sending
 *	to us until we are ready for it. This should be called shortly after login.
 */
void ServerConnection::enableEntities()
{
	// Ok cell, we are ready for entity updates now.
	BaseAppExtInterface::enableEntitiesArgs & args =
		BaseAppExtInterface::enableEntitiesArgs::start(
			this->bundle(), Mercury::RELIABLE_DRIVER );

	args.dummy = 0;

	DEBUG_MSG( "ServerConnection::enableEntities: Enabling entities\n" );

	this->send();

	entitiesEnabled_ = true;
}


/**
 * 	This method returns whether or not we are online with the server.
 *	If a login is in progress, it will still return false.
 */
bool ServerConnection::online() const
{
	return pChannel_ != NULL;
}


/**
 *	This method removes the channel. This should be called when an exception
 *	occurs during processing input, or when sending data fails. The layer above
 *	can detect this by checking the online method once per frame. This is safer
 *	than using a callback, since a disconnect may happen at inconvenient times,
 *	e.g. when sending.
 */
void ServerConnection::disconnect( bool informServer )
{
	if (!this->online()) return;

	if (informServer)
	{
		BaseAppExtInterface::disconnectClientArgs::start( this->bundle(),
			Mercury::RELIABLE_NO ).reason = 0; // reason not yet used.

		this->channel().send();
	}

	// Destroy our channel
	// delete pChannel_;
	if (pChannel_ != NULL)
	{
		pChannel_->destroy();
		pChannel_ = NULL;
	}

	// clear in-progress proxy data downloads
	for (uint i = 0; i < dataDownloads_.size(); ++i)
		delete dataDownloads_[i];
	dataDownloads_.clear();

	// forget the handler and the session key
	pHandler_ = NULL;
	sessionKey_ = 0;
}


/**
 *  This method returns the ServerConnection's channel.
 */
Mercury::Channel & ServerConnection::channel()
{
	// Don't call this before this->online() is true.
	MF_ASSERT_DEV( pChannel_ );
	return *pChannel_;
}


const Mercury::Address & ServerConnection::addr() const
{
	MF_ASSERT_DEV( pChannel_ );
	return pChannel_->addr();
}


/**
 * 	This method adds a message to the server to inform it of the
 * 	new position and direction (and the rest) of an entity under our control.
 *	The server must have explicitly given us control of that entity first.
 *
 *	@param id			ID of entity.
 *	@param spaceID		ID of the space the entity is in.
 *	@param vehicleID	ID of the innermost vehicle the entity is on.
 * 	@param pos			Local position of the entity.
 * 	@param yaw			Local direction of the entity.
 * 	@param pitch		Local direction of the entity.
 * 	@param roll			Local direction of the entity.
 * 	@param onGround		Whether or not the entity is on terrain (if present).
 * 	@param globalPos	Approximate global position of the entity
 */
void ServerConnection::addMove( EntityID id, SpaceID spaceID,
	EntityID vehicleID, const Vector3 & pos, float yaw, float pitch, float roll,
	bool onGround, const Vector3 & globalPos )
{
	if (this->offline())
		return;

	if (spaceID != spaceID_)
	{
		ERROR_MSG( "ServerConnection::addMove: "
					"Attempted to move %u from space %u to space %u\n",
				id, spaceID_, spaceID );
		return;
	}

	if (!this->isControlledLocally( id ))
	{
		ERROR_MSG( "ServerConnection::addMove: "
				"Tried to add a move for entity id %u that we do not control\n",
			id );
		// be assured that even if we did not return here the server
		// would not accept the position update regardless!
		return;
	}

	bool changedVehicle = false;
	EntityID currVehicleID = this->getVehicleID( id );

	if (vehicleID != currVehicleID)
	{
		this->setVehicle( id, vehicleID );
		changedVehicle = true;
	}

	Coord coordPos( BW_HTONF( pos.x ), BW_HTONF( pos.y ), BW_HTONF( pos.z ) );
	YawPitchRoll dir( yaw , pitch, roll );

	Mercury::Bundle & bundle = this->bundle();

	if (id == id_)
	{
		// TODO: When on a vehicle, the reference number is not used and so does not
		// need to be sent (and remembered).

		uint8 refNum = sendingSequenceNumber_;
		sentPositions_[ sendingSequenceNumber_ ] = globalPos;
		++sendingSequenceNumber_;

		if (!changedVehicle)
		{
			BaseAppExtInterface::avatarUpdateImplicitArgs & upArgs =
				BaseAppExtInterface::avatarUpdateImplicitArgs::start(
						bundle, Mercury::RELIABLE_NO );

			upArgs.pos = coordPos;
			upArgs.dir = dir;
			upArgs.refNum = refNum;
		}
		else
		{
			BaseAppExtInterface::avatarUpdateExplicitArgs & upArgs =
				BaseAppExtInterface::avatarUpdateExplicitArgs::start( bundle,
						Mercury::RELIABLE_NO );

			upArgs.spaceID = BW_HTONL( spaceID );
			upArgs.vehicleID = BW_HTONL( vehicleID );
			upArgs.onGround = onGround;

			upArgs.pos = coordPos;
			upArgs.dir = dir;
			upArgs.refNum = refNum;
		}
	}
	else
	{
		if (!changedVehicle)
		{
			BaseAppExtInterface::avatarUpdateWardImplicitArgs & upArgs =
				BaseAppExtInterface::avatarUpdateWardImplicitArgs::start(
						bundle, Mercury::RELIABLE_NO );

			upArgs.ward = BW_HTONL( id );

			upArgs.pos = coordPos;
			upArgs.dir = dir;
		}
		else
		{
			BaseAppExtInterface::avatarUpdateWardExplicitArgs & upArgs =
				BaseAppExtInterface::avatarUpdateWardExplicitArgs::start(
						bundle, Mercury::RELIABLE_NO );

			upArgs.ward = BW_HTONL( id );
			upArgs.spaceID = BW_HTONL( spaceID );
			upArgs.vehicleID = BW_HTONL( vehicleID );
			upArgs.onGround = onGround;

			upArgs.pos = coordPos;
			upArgs.dir = dir;
		}
	}

	// Currently even when we control an entity we keep getting updates
	// for it but just ignore them. This is so we can get the various
	// prefixes. We could set the vehicle info ourself but changing
	// the space ID is not so straightforward. However the main
	// advantage of this approach is not having to change the server to
	// know about the entities that we control. Unfortunately it is
	// quite inefficient - both for sending unnecessary explicit
	// updates (sends for about twice as long) and for getting tons
	// of unwanted position updates, mad worse by the high likelihood
	// of controlled entities being near to the client. Oh well,
	// it'll do for now.
}


/**
 * 	This method is called to start a new message to the proxy.
 * 	Note that proxy messages cannot be sent on a bundle after
 * 	entity messages.
 *
 * 	@param messageId	The message to send.
 *
 * 	@return	A stream to write the message on.
 */
BinaryOStream & ServerConnection::startProxyMessage( int messageId )
{
	if (this->offline())
	{
		CRITICAL_MSG( "ServerConnection::startProxyMessage: "
				"Called when not connected to server!\n" );
	}

	static Mercury::InterfaceElement anie = BaseAppExtInterface::entityMessage;
	// 0x80 to indicate it is an entity message, 0x40 to indicate that it is for
	// the base.
	anie.id( ((uchar)messageId) | 0xc0 );
	this->bundle().startMessage( anie, /*isReliable:*/true );

	return this->bundle();
}


/**
 * 	This message sends an entity message for the player's avatar.
 *
 * 	@param messageId	The message to send.
 *
 * 	@return A stream to write the message on.
 */
BinaryOStream & ServerConnection::startAvatarMessage( int messageId )
{
	return this->startEntityMessage( messageId, 0 );
}

/**
 * 	This message sends an entity message to a given entity.
 *
 * 	@param messageId	The message to send.
 * 	@param entityId		The id of the entity to receive the message.
 *
 * 	@return A stream to write the message on.
 */
BinaryOStream & ServerConnection::startEntityMessage( int messageId,
		EntityID entityId )
{
	if (this->offline())
	{
		CRITICAL_MSG( "ServerConnection::startEntityMessage: "
				"Called when not connected to server!\n" );
	}

	static Mercury::InterfaceElement anie = BaseAppExtInterface::entityMessage;
	anie.id( ((uchar)messageId) | 0x80 );
	this->bundle().startMessage( anie, /*isReliable:*/true );
	this->bundle() << entityId;

	return this->bundle();
}


/**
 *	This method processes all pending network messages. They are passed to the
 *	input handler that was specified in logOnComplete.
 *
 *	@return	Returns true if any packets were processed.
 */
bool ServerConnection::processInput()
{
	// process any pending packets
	// (they may not be for us in a multi servconn environment)
	bool gotAnyPackets = false;
	try
	{
		bool gotAPacket = false;
		do
		{
			gotAPacket = nub_.processPendingEvents();

			gotAnyPackets |= gotAPacket;
			everReceivedPacket_ |= gotAPacket;
		}
		while (gotAPacket);
	}
	catch (Mercury::NubException & ne)
	{
		switch (ne.reason())
		{
			case Mercury::REASON_CORRUPTED_PACKET:
			{
				ERROR_MSG( "ServerConnection::processInput: "
					"Dropped corrupted incoming packet\n" );
				break;
			}

			// WINDOW_OVERFLOW is omitted here since we check for it in send()

			case Mercury::REASON_INACTIVITY:
			{
				if (this->online())
				{
					ERROR_MSG( "ServerConnection::processInput: "
						"Disconnecting due to nub exception (%s)\n",
						Mercury::reasonToString( ne.reason() ) );

					this->disconnect();
				}

				break;
			}

			default:

				WARNING_MSG( "ServerConnection::processInput: "
					"Got a nub exception (%s)\n",
					Mercury::reasonToString( ne.reason() ) );
		}
	}

	// Don't bother collecting statistics if we're not online.
	if (!this->online())
	{
		return gotAnyPackets;
	}

	// see how long that processing took
	if (gotAnyPackets)
	{
		static uint64 lastTimeStamp = timestamp();
		uint64 currTimeStamp = timestamp();
		uint64 delta = (currTimeStamp - lastTimeStamp)
						* uint64( 1000 ) / stampsPerSecond();
		int deltaInMS = int( delta );

		if (deltaInMS > 400)
		{
			WARNING_MSG( "ServerConnection::processInput: "
				"There were %d ms between packets\n", deltaInMS );
		}

		lastTimeStamp = currTimeStamp;
	}

	return gotAnyPackets;
}


/**
 *	This method handles an entity script message from the server.
 *
 *	@param messageID		Message Id.
 *	@param data		Stream containing message data.
 *	@param length	Number of bytes in the message.
 */
void ServerConnection::handleEntityMessage( int messageID, BinaryIStream & data,
	int length )
{
	// Get the entity id off the stream
	EntityID objectID;
	data >> objectID;
	length -= sizeof( objectID );

//	DEBUG_MSG( "ServerConnection::handleMessage: %d\n", messageID );
	if (pHandler_)
	{
		const int PROPERTY_FLAG = 0x40;

		if (messageID & PROPERTY_FLAG)
		{
			pHandler_->onEntityProperty( objectID,
				messageID & ~PROPERTY_FLAG, data );
		}
		else
		{
			pHandler_->onEntityMethod( objectID,
				messageID, data );
		}
	}
}


// -----------------------------------------------------------------------------
// Section: avatarUpdate and related message handlers
// -----------------------------------------------------------------------------

/**
 *	This method handles the relativePositionReference message. It is used to
 *	indicate the position that should be used as the base for future relative
 *	positions.
 */
void ServerConnection::relativePositionReference(
	const ClientInterface::relativePositionReferenceArgs & args )
{
	referencePosition_ =
		::calculateReferencePosition( sentPositions_[ args.sequenceNumber ] );
}


/**
 *	This method handles the relativePosition message. It is used to indicate the
 *	position that should be used as the base for future relative positions.
 */
void ServerConnection::relativePosition(
		const ClientInterface::relativePositionArgs & args )
{
	referencePosition_ = args.position;
}


/**
 *	This method indicates that the vehicle an entity is on has changed.
 */
void ServerConnection::setVehicle(
	const ClientInterface::setVehicleArgs & args )
{
	this->setVehicle( args.passengerID, args.vehicleID );
}


/**
 *	This method changes the vehicle an entity is on has changed.
 */
void ServerConnection::setVehicle( EntityID passengerID, EntityID vehicleID )
{
	if (vehicleID)
	{
		passengerToVehicle_[ passengerID ] = vehicleID;
	}
	else
	{
		passengerToVehicle_.erase( passengerID );
	}
}


#define AVATAR_UPDATE_GET_POS_ORIGIN										\
		const Vector3 & originPos =											\
			(vehicleID == 0) ? referencePosition_ : Vector3::zero();		\

#define AVATAR_UPDATE_GET_POS_FullPos										\
		AVATAR_UPDATE_GET_POS_ORIGIN										\
		args.position.unpackXYZ( pos.x, pos.y, pos.z );						\
		args.position.getXYZError( posError.x, posError.y, posError.z );	\
		pos += originPos;													\

#define AVATAR_UPDATE_GET_POS_OnChunk										\
		AVATAR_UPDATE_GET_POS_ORIGIN										\
		pos.y = -13000.f;													\
		args.position.unpackXZ( pos.x, pos.z );								\
		args.position.getXZError( posError.x, posError.z );					\
		/* TODO: This is not correct. Need to implement this later. */		\
		pos.x += originPos.x;												\
		pos.z += originPos.z;												\

#define AVATAR_UPDATE_GET_POS_OnGround										\
		AVATAR_UPDATE_GET_POS_ORIGIN										\
		pos.y = -13000.f;													\
		args.position.unpackXZ( pos.x, pos.z );								\
		args.position.getXZError( posError.x, posError.z );					\
		pos.x += originPos.x;												\
		pos.z += originPos.z;												\

#define AVATAR_UPDATE_GET_POS_NoPos											\
		pos.set( -13000.f, -13000.f, -13000.f );							\

#define AVATAR_UPDATE_GET_DIR_YawPitchRoll									\
		float yaw = 0.f, pitch = 0.f, roll = 0.f;							\
		args.dir.get( yaw, pitch, roll );									\

#define AVATAR_UPDATE_GET_DIR_YawPitch										\
		float yaw = 0.f, pitch = 0.f, roll = 0.f;							\
		args.dir.get( yaw, pitch );											\

#define AVATAR_UPDATE_GET_DIR_Yaw											\
		float yaw = int8ToAngle( args.dir );								\
		float pitch = 0.f;													\
		float roll = 0.f;													\

#define AVATAR_UPDATE_GET_DIR_NoDir											\
		float yaw = 0.f, pitch = 0.f, roll = 0.f;							\

#define AVATAR_UPDATE_GET_ID_NoAlias	args.id;
#define AVATAR_UPDATE_GET_ID_Alias		idAlias_[ args.idAlias ];

#define IMPLEMENT_AVUPMSG( ID, POS, DIR )									\
void ServerConnection::avatarUpdate##ID##POS##DIR(							\
		const ClientInterface::avatarUpdate##ID##POS##DIR##Args & args )	\
{																			\
	if (pHandler_ != NULL)													\
	{																		\
		Vector3 pos;														\
		Vector3 posError( 0.f, 0.f, 0.f );									\
																			\
		EntityID id = AVATAR_UPDATE_GET_ID_##ID								\
		EntityID vehicleID = this->getVehicleID( id );						\
																			\
		AVATAR_UPDATE_GET_POS_##POS											\
																			\
		AVATAR_UPDATE_GET_DIR_##DIR											\
																			\
		/* Ignore updates from controlled entities */						\
		if (this->isControlledLocally( id ))								\
			return;															\
																			\
		pHandler_->onEntityMoveWithError( id, spaceID_, vehicleID,			\
			pos, posError, yaw, pitch, roll, true );						\
	}																		\
}


IMPLEMENT_AVUPMSG( NoAlias, FullPos, YawPitchRoll )
IMPLEMENT_AVUPMSG( NoAlias, FullPos, YawPitch )
IMPLEMENT_AVUPMSG( NoAlias, FullPos, Yaw )
IMPLEMENT_AVUPMSG( NoAlias, FullPos, NoDir )
IMPLEMENT_AVUPMSG( NoAlias, OnChunk, YawPitchRoll )
IMPLEMENT_AVUPMSG( NoAlias, OnChunk, YawPitch )
IMPLEMENT_AVUPMSG( NoAlias, OnChunk, Yaw )
IMPLEMENT_AVUPMSG( NoAlias, OnChunk, NoDir )
IMPLEMENT_AVUPMSG( NoAlias, OnGround, YawPitchRoll )
IMPLEMENT_AVUPMSG( NoAlias, OnGround, YawPitch )
IMPLEMENT_AVUPMSG( NoAlias, OnGround, Yaw )
IMPLEMENT_AVUPMSG( NoAlias, OnGround, NoDir )
IMPLEMENT_AVUPMSG( NoAlias, NoPos, YawPitchRoll )
IMPLEMENT_AVUPMSG( NoAlias, NoPos, YawPitch )
IMPLEMENT_AVUPMSG( NoAlias, NoPos, Yaw )
IMPLEMENT_AVUPMSG( NoAlias, NoPos, NoDir )
IMPLEMENT_AVUPMSG( Alias, FullPos, YawPitchRoll )
IMPLEMENT_AVUPMSG( Alias, FullPos, YawPitch )
IMPLEMENT_AVUPMSG( Alias, FullPos, Yaw )
IMPLEMENT_AVUPMSG( Alias, FullPos, NoDir )
IMPLEMENT_AVUPMSG( Alias, OnChunk, YawPitchRoll )
IMPLEMENT_AVUPMSG( Alias, OnChunk, YawPitch )
IMPLEMENT_AVUPMSG( Alias, OnChunk, Yaw )
IMPLEMENT_AVUPMSG( Alias, OnChunk, NoDir )
IMPLEMENT_AVUPMSG( Alias, OnGround, YawPitchRoll )
IMPLEMENT_AVUPMSG( Alias, OnGround, YawPitch )
IMPLEMENT_AVUPMSG( Alias, OnGround, Yaw )
IMPLEMENT_AVUPMSG( Alias, OnGround, NoDir )
IMPLEMENT_AVUPMSG( Alias, NoPos, YawPitchRoll )
IMPLEMENT_AVUPMSG( Alias, NoPos, YawPitch )
IMPLEMENT_AVUPMSG( Alias, NoPos, Yaw )
IMPLEMENT_AVUPMSG( Alias, NoPos, NoDir )


/**
 *	This method handles a detailed position and direction update.
 */
void ServerConnection::detailedPosition(
	const ClientInterface::detailedPositionArgs & args )
{
	EntityID entityID = args.id;
	EntityID vehicleID = this->getVehicleID( entityID );

	this->detailedPositionReceived(
		entityID, spaceID_, 0, args.position );

	if ((pHandler_ != NULL) &&
			!this->isControlledLocally( entityID ))
	{
		pHandler_->onEntityMoveWithError(
			entityID,
			spaceID_,
			vehicleID,
			args.position,
			Vector3::zero(),
			args.direction.yaw,
			args.direction.pitch,
			args.direction.roll,
			false );
	}
}

/**
 *	This method handles a forced position and direction update.
 *	This is when an update is being forced back for an (ordinarily)
 *	client controlled entity, including for the player. Usually this is
 *	due to a physics correction from the server, but it could be for any
 *	reason decided by the server (e.g. server-initiated teleport).
 */
void ServerConnection::forcedPosition(
	const ClientInterface::forcedPositionArgs & args )
{
	if (!this->isControlledLocally( args.id ))
	{
		// if we got one out of order then always ignore it - we would not
		// expect to get forcedPosition before handling controlEntity on
		WARNING_MSG( "ServerConnection::forcedPosition: "
			"Received forced position for entity %u that we do not control\n",
			args.id );
		return;
	}

	if (args.id == id_)
	{
		if ((spaceID_ != 0) &&
				(spaceID_ != args.spaceID) &&
				(pHandler_ != NULL))
		{
			pHandler_->spaceGone( spaceID_ );
		}

		spaceID_ = args.spaceID;

		BaseAppExtInterface::ackPhysicsCorrectionArgs & ackArgs =
					BaseAppExtInterface::ackPhysicsCorrectionArgs::start(
							this->bundle() );

		ackArgs.dummy = 0;
	}
	else
	{
		BaseAppExtInterface::ackWardPhysicsCorrectionArgs & ackArgs =
					BaseAppExtInterface::ackWardPhysicsCorrectionArgs::start(
							this->bundle() );

		ackArgs.ward = BW_HTONL( args.id );
		ackArgs.dummy = 0;
	}


	// finally tell the handler about it
	if (pHandler_ != NULL)
	{
		pHandler_->onEntityMoveWithError(
			args.id,
			args.spaceID,
			args.vehicleID,
			args.position,
			Vector3::zero(),
			args.direction.yaw,
			args.direction.pitch,
			args.direction.roll,
			false );
	}
}


/**
 *	The server is telling us whether or not we are controlling this entity
 */
void ServerConnection::controlEntity(
	const ClientInterface::controlEntityArgs & args )
{
	if (args.on)
	{
		controlledEntities_.insert( args.id );
	}
	else
	{
		controlledEntities_.erase( args.id );
	}

	// tell the message handler about it
	if (pHandler_ != NULL)
	{
		pHandler_->onEntityControl( args.id, args.on );
	}
}


/**
 *	This method is called when a detailed position for an entity has been
 *	received.
 */
void ServerConnection::detailedPositionReceived( EntityID id,
	SpaceID spaceID, EntityID vehicleID, const Vector3 & position )
{
	if ((id == id_) && (vehicleID == 0))
	{
		referencePosition_ = ::calculateReferencePosition( position );
	}
}



// -----------------------------------------------------------------------------
// Section: Statistics methods
// -----------------------------------------------------------------------------

static void (*s_bandwidthFromServerMutator)( int bandwidth ) = NULL;
void setBandwidthFromServerMutator( void (*mutatorFn)( int bandwidth ) )
{
	s_bandwidthFromServerMutator = mutatorFn;
}

/**
 *	This method gets the bandwidth that this server connection should receive
 *	from the server.
 *
 *	@return		The current downstream bandwidth in bits per second.
 */
int ServerConnection::bandwidthFromServer() const
{
	return bandwidthFromServer_;
}


/**
 *	This method sets the bandwidth that this server connection should receive
 *	from the server.
 *
 *	@param bandwidth	The bandwidth in bits per second.
 */
void ServerConnection::bandwidthFromServer( int bandwidth )
{
	if (s_bandwidthFromServerMutator == NULL)
	{
		ERROR_MSG( "ServerConnection::bandwidthFromServer: Cannot comply "
			"since no mutator set with 'setBandwidthFromServerMutator'\n" );
		return;
	}

	const int MIN_BANDWIDTH = 0;
	const int MAX_BANDWIDTH = PACKET_MAX_SIZE * NETWORK_BITS_PER_BYTE * 10 / 2;

	bandwidth = Math::clamp( MIN_BANDWIDTH, bandwidth, MAX_BANDWIDTH );

	(*s_bandwidthFromServerMutator)( bandwidth );

	// don't set it now - wait to hear back from the server
	//bandwidthFromServer_ = bandwidth;
}



/**
 *	This method returns the number of bits received per second.
 */
double ServerConnection::bpsIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return bitsIn_.changePerSecond();
}


/**
 *	This method returns the number of bits sent per second.
 */
double ServerConnection::bpsOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return bitsOut_.changePerSecond();
}


/**
 *	This method returns the number of packets received per second.
 */
double ServerConnection::packetsPerSecondIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return packetsIn_.changePerSecond();
}


/**
 *	This method returns the number of packets sent per second.
 */
double ServerConnection::packetsPerSecondOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return packetsOut_.changePerSecond();
}


/**
 *	This method returns the number of messages sent per second.
 */
double ServerConnection::messagesPerSecondIn() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return messagesIn_.changePerSecond();
}


/**
 *	This method returns the number of messages received per second.
 */
double ServerConnection::messagesPerSecondOut() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return messagesOut_.changePerSecond();
}


/**
 *	This method returns the percentage of movement bytes received.
 */
double ServerConnection::movementBytesPercent() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return movementBytes_.changePerSecond() /
		totalBytes_.changePerSecond() * 100.0;
}

/**
 *	This method returns the percentage of non-movement bytes received
 */
double ServerConnection::nonMovementBytesPercent() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return nonMovementBytes_.changePerSecond() /
		totalBytes_.changePerSecond() * 100.0;
}


/**
 *	This method returns the percentage of overhead bytes received
 */
double ServerConnection::overheadBytesPercent() const
{
	const_cast<ServerConnection *>(this)->updateStats();

	return overheadBytes_.changePerSecond() /
		totalBytes_.changePerSecond() * 100.0;
}


/**
 *	This method returns the total number of bytes received that are associated
 *	with movement messages.
 */
int ServerConnection::movementBytesTotal() const
{
	return movementBytes_;
}


/**
 *	This method returns the total number of bytes received that are associated
 *	with non-movement messages.
 */
int ServerConnection::nonMovementBytesTotal() const
{
	return nonMovementBytes_;
}


/**
 *	This method returns the total number of bytes received that are associated
 *	with packet overhead.
 */
int ServerConnection::overheadBytesTotal() const
{
	return overheadBytes_;
}


/**
 *	This method returns the total number of messages received that are associated
 *	with associated with movement.
 */
int ServerConnection::movementMessageCount() const
{
	int count = 0;
	for (int i = FIRST_AVATAR_UPDATE_MESSAGE;
				i <= LAST_AVATAR_UPDATE_MESSAGE; i++)
	{
		nub_.numMessagesReceivedForMessage( i );
	}

	return count;
}



/**
 *	This method updates the timing statistics of the server connection.
 */
void ServerConnection::updateStats()
{
	const double UPDATE_PERIOD = 2.0;

	double timeDelta = this->appTime() - lastTime_;

	if ( timeDelta > UPDATE_PERIOD )
	{
		lastTime_ = this->appTime();

		packetsIn_ = nub_.numPacketsReceived();
		messagesIn_ = nub_.numMessagesReceived();
		bitsIn_ = nub_.numBytesReceived() * NETWORK_BITS_PER_BYTE;

		movementBytes_ = nub_.numBytesReceivedForMessage(
						ClientInterface::relativePositionReference.id() );

		for (int i = FIRST_AVATAR_UPDATE_MESSAGE;
				i <= LAST_AVATAR_UPDATE_MESSAGE; i++)
		{
			movementBytes_ += nub_.numBytesReceivedForMessage( i );
		}

		totalBytes_ = nub_.numBytesReceived();
		overheadBytes_ = nub_.numOverheadBytesReceived();
		nonMovementBytes_ = totalBytes_ - movementBytes_ - overheadBytes_;

		packetsIn_.update( timeDelta );
		packetsOut_.update( timeDelta );

		bitsIn_.update( timeDelta );
		bitsOut_.update( timeDelta );

		messagesIn_.update( timeDelta );
		messagesOut_.update( timeDelta );

		totalBytes_.update( timeDelta );
		movementBytes_.update( timeDelta );
		nonMovementBytes_.update( timeDelta );
		overheadBytes_.update( timeDelta );
	}
}


/**
 *	This method sends the current bundle to the server.
 */
void ServerConnection::send()
{
	// get out now if we are not connected
	if (this->offline())
		return;

	// if we want to try to fool a firewall by reconfiguring ports,
	//  this is a good time to do so!
	if (tryToReconfigurePorts_ && !everReceivedPacket_)
	{
		Mercury::Bundle bundle;
		bundle.startMessage( BaseAppExtInterface::authenticate, false );
		bundle << sessionKey_;
		this->nub().send( this->addr(), bundle );
	}

	// record the time we last did a send
	if (pTime_)
		lastSendTime_ = *pTime_;

	// update statistics
	const Mercury::Bundle & bundle = this->bundle();
	packetsOut_ += bundle.sizeInPackets();
	messagesOut_ += bundle.numMessages();
	bitsOut_ += (bundle.size() + UDP_OVERHEAD) * NETWORK_BITS_PER_BYTE;

	// get the channel to send the bundle
	this->channel().send();

	const int OVERFLOW_LIMIT = 1024;

	// TODO: #### Make a better check that is dependent on both the window
	// size and time since last heard from the server.
	if (this->channel().sendWindowUsage() > OVERFLOW_LIMIT)
	{
		WARNING_MSG( "ServerConnection::send: "
			"Disconnecting since channel has overflowed.\n" );

		this->disconnect();
	}
}



/**
 * 	This method primes outgoing bundles with the authenticate message once it
 * 	has been received.
 */
void ServerConnection::primeBundle( Mercury::Bundle & bundle )
{
	if (sessionKey_)
	{
		bundle.startMessage( BaseAppExtInterface::authenticate, false );
		bundle << sessionKey_;
	}
}


/**
 * 	This method returns the number of unreliable messages that are streamed on
 *  by primeBundle().
 */
int ServerConnection::numUnreliableMessages() const
{
	return sessionKey_ ? 1 : 0;
}


/**
 *	This method requests the server to send update information for the entity
 *	with the input id. This must be called after receiving an onEntityEnter
 *	message to allow message and incremental property updates to flow.
 *
 *  @param id		ID of the entity whose update is requested.
 *	@param stamps	A vector containing the known cache event stamps. If none
 *					are known, stamps is empty.
 */
void ServerConnection::requestEntityUpdate( EntityID id,
	const CacheStamps & stamps )
{
	if (this->offline())
		return;

	this->bundle().startMessage( BaseAppExtInterface::requestEntityUpdate,
		  /*isReliable:*/true );
	this->bundle() << id;

	CacheStamps::const_iterator iter = stamps.begin();

	while (iter != stamps.end())
	{
		this->bundle() << (*iter);

		iter++;
	}
}


/**
 *	This method returns the approximate round-trip time to the server.
 */
float ServerConnection::latency() const
{
	return pChannel_ ? float( pChannel_->roundTripTimeInSeconds() ) : 0.f;
}


// -----------------------------------------------------------------------------
// Section: Various Mercury InputMessageHandler implementations
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EntityMessageHandler::EntityMessageHandler()
{
}


/**
 * 	This method handles entity messages from the server and sends them to the
 *	associated ServerConnection object.
 */
void EntityMessageHandler::handleMessage(
	const Mercury::Address & /* srcAddr */,
	Mercury::UnpackedMessageHeader & header,
	BinaryIStream & data )
{
	ServerConnection * pServConn =
		(ServerConnection *)header.pNub->pExtensionData();
	pServConn->handleEntityMessage( header.identifier & 0x7F, data,
		header.length );
}


/**
 * 	@internal
 *	Objects of this type are used to handle messages destined for the client
 *	application.
 */
template <class ARGS>
class ClientMessageHandler : public Mercury::InputMessageHandler
{
public:
	/// This typedef declares a member function on the ServerConnection
	/// class that handles a single message.
	typedef void (ServerConnection::*Handler)( const ARGS & args );

	/// This is the constructor
	ClientMessageHandler( Handler handler ) : handler_( handler ) {}

private:
	/**
	 * 	This method handles a Mercury message, and dispatches it to
	 * 	the correct member function of the ServerConnection object.
	 */
	virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
	{
#ifndef _BIG_ENDIAN
		ARGS & args = *(ARGS*)data.retrieve( sizeof(ARGS) );
#else
		// Poor old big-endian clients can't just refer directly to data
		// on the network stream, they have to stream it off.
		ARGS args;
		data >> args;
#endif

		ServerConnection * pServConn =
			(ServerConnection *)header.pNub->pExtensionData();
		(pServConn->*handler_)( args );
	}

	Handler handler_;
};


/**
 * 	@internal
 *	Objects of this type are used to handle variable length messages destined
 *	for the client application.
 */
class ClientVarLenMessageHandler : public Mercury::InputMessageHandler
{
public:
	/// This typedef declares a member function on the ServerConnection
	/// class that handles a single variable lengthmessage.
	typedef void (ServerConnection::*Handler)( BinaryIStream & stream,
			int length );

	/// This is the constructor.
	ClientVarLenMessageHandler( Handler handler ) : handler_( handler ) {}

private:
	/**
	 * 	This method handles a Mercury message, and dispatches it to
	 * 	the correct member function of the ServerConnection object.
	 */
	virtual void handleMessage( const Mercury::Address & /*srcAddr*/,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data )
	{
		ServerConnection * pServConn =
			(ServerConnection *)header.pNub->pExtensionData();
		(pServConn->*handler_)( data, header.length );
	}

	Handler handler_;
};


/**
 * 	@internal
 *	Objects of this type are used to handle variable length messages destined
 *	for the client application. The handler is also passed the source address.
 */
class ClientVarLenWithAddrMessageHandler : public Mercury::InputMessageHandler
{
	public:
		/// This typedef declares a member function on the ServerConnection
		/// class that handles a single variable lengthmessage.
		typedef void (ServerConnection::*Handler)(
				const Mercury::Address & srcAddr,
				BinaryIStream & stream,
				int length );

		/// This is the constructor.
		ClientVarLenWithAddrMessageHandler( Handler handler ) :
			handler_( handler ) {}

	private:
		/**
		 * 	This method handles a Mercury message, and dispatches it to
		 * 	the correct member function of the ServerConnection object.
		 */
		virtual void handleMessage( const Mercury::Address & srcAddr,
				Mercury::UnpackedMessageHeader & header,
				BinaryIStream & data )
		{
			ServerConnection * pServConn =
				(ServerConnection *)header.pNub->pExtensionData();
			(pServConn->*handler_)( srcAddr, data, header.length );
		}

		Handler handler_;
};


// -----------------------------------------------------------------------------
// Section: Server Timing code
// -----------------------------------------------------------------------------
// TODO:PM This section is really just here to give a better time to the
// filtering. This should be reviewed.

/**
 *	This method returns the server time estimate based on the input client time.
 */
double ServerConnection::serverTime( double clientTime ) const
{
	return serverTimeHandler_.serverTime( clientTime );
}


/**
 *	This method returns the server time associated with the last packet that was
 *	received from the server.
 */
double ServerConnection::lastMessageTime() const
{
	return serverTimeHandler_.lastMessageTime();
}


/**
 * 	This method returns the game time associated with the last packet that was
 * 	received from the server.
 */
TimeStamp ServerConnection::lastGameTime() const
{
	return serverTimeHandler_.lastGameTime();
}


const double ServerConnection::ServerTimeHandler::UNINITIALISED_TIME = -1000.0;

/**
 *	The constructor for ServerTimeHandler.
 */
ServerConnection::ServerTimeHandler::ServerTimeHandler() :
	tickByte_( 0 ),
	timeAtSequenceStart_( UNINITIALISED_TIME ),
	gameTimeAtSequenceStart_( 0 )
{
}


/**
 * 	This method is called when the server sends a new gametime.
 * 	This should be after the sequence number for the current packet
 * 	has been set.
 *
 *	@param newGameTime	The current game time in ticks.
 */
void ServerConnection::ServerTimeHandler::gameTime( TimeStamp newGameTime,
		double currentTime )
{
	tickByte_ = uint8( newGameTime );
	gameTimeAtSequenceStart_ = newGameTime - tickByte_;
	timeAtSequenceStart_ = currentTime -
		double(tickByte_) / ServerConnection::updateFrequency();
}


/**
 *	This method is called when a new tick sync message is received from the
 *	server. It is used to synchronise between client and server time.
 *
 *	@param newSeqNum	The sequence number just received. This increases by one
 *						for each packets and packets should be received at 10Hz.
 *
 *	@param currentTime	This is the time that the client currently thinks it is.
 *						We need to pass it in since this file does not have
 *						access to the app file.
 */
void ServerConnection::ServerTimeHandler::tickSync( uint8 newSeqNum,
		double currentTime )
{
	const float updateFrequency = ServerConnection::updateFrequency();
	const double SEQUENCE_PERIOD = 256.0/updateFrequency;
	const int32 SEQUENCE_PERIOD_INT = 256;

	// This is how many consecutive packets can be dropped.
	const uint8 LAST_HEAD_SEQ_NUM = 256/3 - 1;
	const uint8 FIRST_TAIL_SEQ_NUM = 255 - LAST_HEAD_SEQ_NUM;

	// Have we started yet?

	if (timeAtSequenceStart_ == UNINITIALISED_TIME)
	{
		// The first one is always like this.
		// INFO_MSG( "ServerTimeHandler::sequenceNumber: "
		//	"Have not received gameTime message yet.\n" );
		return;
	}

	// If the sequence number wraps around, we need to adjust the time
	// at the start of the sequence.

	if (tickByte_ >= FIRST_TAIL_SEQ_NUM &&
		newSeqNum <= LAST_HEAD_SEQ_NUM)
	{
		timeAtSequenceStart_ += SEQUENCE_PERIOD;
		gameTimeAtSequenceStart_ += SEQUENCE_PERIOD_INT;
	}
	else if (newSeqNum >= FIRST_TAIL_SEQ_NUM &&
		tickByte_ <= LAST_HEAD_SEQ_NUM)
	{
		WARNING_MSG( "ServerTimeHandler::sequenceNumber: "
				"Got a reverse change over (%d, %d)\n",
			tickByte_, newSeqNum );
		timeAtSequenceStart_ -= SEQUENCE_PERIOD;
		gameTimeAtSequenceStart_ -= SEQUENCE_PERIOD_INT;
	}

	if (uint8( newSeqNum - tickByte_ ) > 0x80)
	{
		DEBUG_MSG( "Non-sequential sequence numbers. Wanted %d, got %d\n",
			uint8( tickByte_ + 1 ), newSeqNum );
	}

	tickByte_ = newSeqNum;

	// Want to adjust the time so that the client does not get too out of sync.
	double timeError = currentTime - this->lastMessageTime();
	const double MAX_TIME_ERROR = 0.05;
	const double MAX_TIME_ADJUST = 0.005;

	if (timeError > MAX_TIME_ERROR)
	{
		timeAtSequenceStart_ += MF_MIN( timeError, MAX_TIME_ADJUST );

		while (timeError > 2 * SEQUENCE_PERIOD/3.f)
		{
			timeAtSequenceStart_ += SEQUENCE_PERIOD;
			timeError -= SEQUENCE_PERIOD;
		}
	}
	else if (-timeError > MAX_TIME_ERROR)
	{
		timeAtSequenceStart_ += MF_MAX( timeError, -MAX_TIME_ADJUST );

		while (timeError < -2 * SEQUENCE_PERIOD/3.f)
		{
			timeAtSequenceStart_ -= SEQUENCE_PERIOD;
			timeError += SEQUENCE_PERIOD;
		}
	}

	if (timeError < -30.f || 30.f < timeError)
	{
		WARNING_MSG( "Time error is %f. Client = %.3f. Server = %.3f.\n",
			timeError,
			currentTime,
			this->lastMessageTime() );
	}
}


/**
 *	This method returns the time that this client thinks the server is at.
 */
double
ServerConnection::ServerTimeHandler::serverTime( double clientTime ) const
{
	return (gameTimeAtSequenceStart_ / ServerConnection::updateFrequency()) +
		(clientTime - timeAtSequenceStart_);
}


/**
 *	This method returns the server time associated with the last packet that was
 *	received from the server.
 */
double ServerConnection::ServerTimeHandler::lastMessageTime() const
{
	return timeAtSequenceStart_ +
		double(tickByte_) / ServerConnection::updateFrequency();
}


/**
 * 	This method returns the game time of the current message.
 */
TimeStamp ServerConnection::ServerTimeHandler::lastGameTime() const
{
	return gameTimeAtSequenceStart_ + tickByte_;
}


/**
 *	This method initialises the watcher information for this object.
 */
void ServerConnection::initDebugInfo()
{
	MF_WATCH(  "Comms/Desired bps in",
		*this,
		MF_ACCESSORS( int, ServerConnection, bandwidthFromServer ) );

	MF_WATCH( "Comms/bps in",	*this, &ServerConnection::bpsIn );
	MF_WATCH( "Comms/bps out",	*this, &ServerConnection::bpsOut );

	MF_WATCH( "Comms/PacketsSec in ", *this,
		&ServerConnection::packetsPerSecondIn );
	MF_WATCH( "Comms/PacketsSec out", *this,
		&ServerConnection::packetsPerSecondOut );

	MF_WATCH( "Comms/Messages in",	*this,
		&ServerConnection::messagesPerSecondIn );
	MF_WATCH( "Comms/Messages out",	*this,
		&ServerConnection::messagesPerSecondOut );

	MF_WATCH( "Comms/Expected Freq", ServerConnection::s_updateFrequency_,
		Watcher::WT_READ_ONLY );

	MF_WATCH( "Comms/Game Time", *this, &ServerConnection::lastGameTime );

	MF_WATCH( "Comms/Movement pct", *this, &ServerConnection::movementBytesPercent);
	MF_WATCH( "Comms/Non-move pct", *this, &ServerConnection::nonMovementBytesPercent);
	MF_WATCH( "Comms/Overhead pct", *this, &ServerConnection::overheadBytesPercent);

	MF_WATCH( "Comms/Movement total", *this, &ServerConnection::movementBytesTotal);
	MF_WATCH( "Comms/Non-move total", *this, &ServerConnection::nonMovementBytesTotal);
	MF_WATCH( "Comms/Overhead total", *this, &ServerConnection::overheadBytesTotal);

	MF_WATCH( "Comms/Movement count", *this, &ServerConnection::movementMessageCount);
	MF_WATCH( "Comms/Packet count", nub_, &Mercury::Nub::numPacketsReceived );

	MF_WATCH( "Comms/Latency", *this, &ServerConnection::latency );

}


// -----------------------------------------------------------------------------
// Section: Mercury message handlers
// -----------------------------------------------------------------------------

/**
 *	This method authenticates the server to the client. Its use is optional,
 *	and determined by the server that we are connected to upon login.
 */
void ServerConnection::authenticate(
	const ClientInterface::authenticateArgs & args )
{
	if (args.key != sessionKey_)
	{
		ERROR_MSG( "ServerConnection::authenticate: "
				   "Unexpected key! (%x, wanted %x)\n",
				   args.key, sessionKey_ );
		return;
	}
}



/**
 * 	This message handles a bandwidthNotification message from the server.
 */
void ServerConnection::bandwidthNotification(
	const ClientInterface::bandwidthNotificationArgs & args )
{
	// TRACE_MSG( "ServerConnection::bandwidthNotification: %d\n", args.bps);
	bandwidthFromServer_ = args.bps;
}


/**
 *	This method handles the message from the server that informs us how
 *	frequently it is going to send to us.
 */
void ServerConnection::updateFrequencyNotification(
		const ClientInterface::updateFrequencyNotificationArgs & args )
{
	s_updateFrequency_ = (float)args.hertz;
}



/**
 *	This method handles a tick sync message from the server. It is used as
 *	a timestamp for the messages in the packet.
 */
void ServerConnection::tickSync(
	const ClientInterface::tickSyncArgs & args )
{
	serverTimeHandler_.tickSync( args.tickByte, this->appTime() );
}


/**
 *	This method handles a setGameTime message from the server.
 *	It is used to adjust the current (server) game time.
 */
void ServerConnection::setGameTime(
	const ClientInterface::setGameTimeArgs & args )
{
	serverTimeHandler_.gameTime( args.gameTime, this->appTime() );
}


/**
 *	This method handles a resetEntities call from the server.
 */
void ServerConnection::resetEntities(
	const ClientInterface::resetEntitiesArgs & args )
{
	// proxy must have received our enableEntities if it is telling
	// us to reset entities (even if we haven't yet received any player
	// creation msgs due to reordering)
	MF_ASSERT_DEV( entitiesEnabled_ );

	// clear existing stale packet
	this->send();

	controlledEntities_.clear();
	passengerToVehicle_.clear();
	createCellPlayerMsg_.reset();

	// forget about the base player entity too if so indicated
	if (!args.keepPlayerOnBase)
	{
		id_ = 0;

		// delete proxy data downloads in progress
		for (uint i = 0; i < dataDownloads_.size(); ++i)
			delete dataDownloads_[i];
		dataDownloads_.clear();
		// but not resource downloads as they're non-entity and should continue
	}

	// refresh bundle prefix
	this->send();

	// re-enable entities, which serves to ack the resetEntities
	// and flush/sync the incoming channel
	entitiesEnabled_ = false;
	this->enableEntities();

	// and finally tell the client about it so it can clear out
	// all (or nigh all) its entities
	if (pHandler_)
	{
		pHandler_->onEntitiesReset( args.keepPlayerOnBase );
	}
}


/**
 *	This method handles a createPlayer call from the base.
 */
void ServerConnection::createBasePlayer( BinaryIStream & stream,
										int /*length*/ )
{
	// we have new player id
	EntityID playerID = 0;
	stream >> playerID;

	INFO_MSG( "ServerConnection::createBasePlayer: id %u\n", playerID );

	// this is now our player id
	id_ = playerID;

	EntityTypeID playerType = EntityTypeID(-1);
	stream >> playerType;

	if (pHandler_)
	{	// just get base data here
		pHandler_->onBasePlayerCreate( id_, playerType,
			stream );
	}

	if (createCellPlayerMsg_.remainingLength() > 0)
	{
		INFO_MSG( "ServerConnection::createBasePlayer: "
			"Playing buffered createCellPlayer message\n" );
		this->createCellPlayer( createCellPlayerMsg_,
			createCellPlayerMsg_.remainingLength() );
		createCellPlayerMsg_.reset();
	}
}


/**
 *	This method handles a createCellPlayer call from the cell.
 */
void ServerConnection::createCellPlayer( BinaryIStream & stream,
										int /*length*/ )
{
	if (id_ == 0)
	{
		WARNING_MSG( "ServerConnection::createCellPlayer: Got createCellPlayer"
			"before createBasePlayer. Buffering message\n" );

		MF_ASSERT_DEV( createCellPlayerMsg_.remainingLength() == 0 );

		createCellPlayerMsg_.transfer( stream, stream.remainingLength() );

		return;
	}
	else
	{
		INFO_MSG( "ServerConnection::createCellPlayer: id %u\n", id_ );
	}

	EntityID vehicleID;
	Position3D pos;
	Direction3D	dir;
	stream >> spaceID_ >> vehicleID >> pos >> dir;

	// assume that we control this entity too
	controlledEntities_.insert( id_ );

	this->setVehicle( id_, vehicleID );

	if (pHandler_)
	{	// just get cell data here
		pHandler_->onCellPlayerCreate( id_,
			spaceID_, vehicleID, pos, dir.yaw, dir.pitch, dir.roll,
			stream );
		// pHandler_->onEntityEnter( id_, spaceID, vehicleID );
	}

	this->detailedPositionReceived( id_, spaceID_, vehicleID, pos );

	// The channel to the server is now regular
	this->channel().isIrregular( false );
}



/**
 *	This method handles keyed data about a particular space from the server.
 */
void ServerConnection::spaceData( BinaryIStream & stream, int length )
{
	SpaceID spaceID;
	SpaceEntryID spaceEntryID;
	uint16 key;
	std::string data;

	stream >> spaceID >> spaceEntryID >> key;
	length -= sizeof(spaceID) + sizeof(spaceEntryID) + sizeof(key);
	data.assign( (char*)stream.retrieve( length ), length );

	TRACE_MSG( "ServerConnection::spaceData: space %u key %hu\n",
		spaceID, key );

	if (pHandler_)
	{
		pHandler_->spaceData( spaceID, spaceEntryID, key, data );
	}
}


/**
 *	This method handles the message from the server that an entity has entered
 *	our Area of Interest (AoI).
 */
void ServerConnection::enterAoI( const ClientInterface::enterAoIArgs & args )
{
	// Set this even if args.idAlias is NO_ID_ALIAS.
	idAlias_[ args.idAlias ] = args.id;

	if (pHandler_)
	{
		//TRACE_MSG( "ServerConnection::enterAoI: Entity = %d\n", args.id );
		pHandler_->onEntityEnter( args.id, spaceID_, 0 );
	}
}


/**
 *	This method handles the message from the server that an entity has entered
 *	our Area of Interest (AoI).
 */
void ServerConnection::enterAoIOnVehicle(
	const ClientInterface::enterAoIOnVehicleArgs & args )
{
	// Set this even if args.idAlias is NO_ID_ALIAS.
	idAlias_[ args.idAlias ] = args.id;
	this->setVehicle( args.id, args.vehicleID );

	if (pHandler_)
	{
		//TRACE_MSG( "ServerConnection::enterAoI: Entity = %d\n", args.id );
		pHandler_->onEntityEnter( args.id, spaceID_, args.vehicleID );
	}
}



/**
 *	This method handles the message from the server that an entity has left our
 *	Area of Interest (AoI).
 */
void ServerConnection::leaveAoI( BinaryIStream & stream, int /*length*/ )
{
	EntityID id;
	stream >> id;

	if (pHandler_)
	{
		CacheStamps stamps( stream.remainingLength() / sizeof(EventNumber) );

		CacheStamps::iterator iter = stamps.begin();

		while (iter != stamps.end())
		{
			stream >> (*iter);

			iter++;
		}

		pHandler_->onEntityLeave( id, stamps );
	}

	passengerToVehicle_.erase( id );

	controlledEntities_.erase( id );
}


/**
 *	This method handles a createEntity call from the server.
 */
void ServerConnection::createEntity( BinaryIStream & stream, int /*length*/ )
{
	EntityID id;
	stream >> id;

	MF_ASSERT_DEV( id != EntityID( -1 ) )	// old-style deprecated hack

	EntityTypeID type;
	stream >> type;

	Vector3 pos( 0.f, 0.f, 0.f );
	int8 compressedYaw = 0;
	int8 compressedPitch = 0;
	int8 compressedRoll = 0;

	stream >> pos >> compressedYaw >> compressedPitch >> compressedRoll;

	float yaw = int8ToAngle( compressedYaw );
	float pitch = int8ToAngle( compressedPitch );
	float roll = int8ToAngle( compressedRoll );

	EntityID vehicleID = this->getVehicleID( id );

	if (pHandler_)
	{
		pHandler_->onEntityCreate( id, type,
			spaceID_, vehicleID, pos, yaw, pitch, roll,
			stream );
	}

	this->detailedPositionReceived( id, spaceID_, vehicleID, pos );
}


/**
 *	This method handles an updateEntity call from the server.
 */
void ServerConnection::updateEntity(
	BinaryIStream & stream, int /*length*/ )
{
	if (pHandler_)
	{
		EntityID id;
		stream >> id;
		pHandler_->onEntityProperties( id, stream );
	}
}


/**
 *	This method handles voice data that comes from another client.
 */
void ServerConnection::voiceData( const Mercury::Address & srcAddr,
	BinaryIStream & stream, int /*length*/ )
{
	if (pHandler_)
	{
		pHandler_->onVoiceData( srcAddr, stream );
	}
	else
	{
		ERROR_MSG( "ServerConnection::voiceData: "
			"Got voice data before a handler has been set.\n" );
	}
}


/**
 *	This method handles a message from the server telling us that we need to
 *	restore a state back to a previous point.
 */
void ServerConnection::restoreClient( BinaryIStream & stream, int length )
{
	EntityID	id;
	SpaceID		spaceID;
	EntityID	vehicleID;
	Position3D	pos;
	Direction3D	dir;

	stream >> id >> spaceID >> vehicleID >> pos >> dir;

	if (pHandler_)
	{
		this->setVehicle( id, vehicleID );
		pHandler_->onRestoreClient( id, spaceID, vehicleID, pos, dir, stream );
	}
	else
	{
		ERROR_MSG( "ServerConnection::restoreClient: "
			"No handler. Maybe already logged off.\n" );
	}


	if (this->offline()) return;

	BaseAppExtInterface::restoreClientAckArgs args;
	// TODO: Put on a proper ack id.
	args.id = 0;
	this->bundle() << args;
	this->send();
}


/**
 *	This method is called when something goes wrong with the BaseApp and we need
 *	to recover.
 */
void ServerConnection::restoreBaseApp( BinaryIStream & stream, int length )
{
	// TODO: We should really do more here. We need to at least tell the handler
	// like we do when the cell entity is restored. It would also be good to
	// have information about what state we should restore to.

	ServerMessageHandler * pSavedHandler = pHandler_;
	this->disconnect();

	pHandler_ = pSavedHandler;
}


/**
 *  The header for a resource download from the server.
 */
void ServerConnection::resourceHeader( BinaryIStream & stream, int length )
{
	// First destream the ID and make sure it isn't already in use
	uint16 id;
	stream >> id;

	DataDownload *pDD;
	DataDownloadMap::iterator it = dataDownloads_.find( id );

	// Usually there shouldn't be an existing download for this id, so make a
	// new one and map it in
	if (it == dataDownloads_.end())
	{
		pDD = new DataDownload( id );
		dataDownloads_[ id ] = pDD;
	}
	else
	{
		// If a download with this ID already exists and has a description,
		// we've got a problem
		if (it->second->pDesc() != NULL)
		{
			ERROR_MSG( "ServerConnection::resourceHeader: "
				"Collision between new and existing download IDs (%hu), "
				"download is likely to be corrupted\n", id );
			return;
		}

		// Otherwise, it just means data for this download arrived before the
		// header, which is weird, but not necessarily a problem
		else
		{
			WARNING_MSG( "ServerConnection::resourceHeader: "
				"Data for download #%hu arrived before the header\n",
				id );

			pDD = it->second;
		}
	}

	// Destream the description
	pDD->setDesc( stream );
}


/**
 *	The server is giving us a fragment of a resource that we have requested.
 */
void ServerConnection::resourceFragment( BinaryIStream & stream, int length )
{
	uint32 argsLength = sizeof( ClientInterface::ResourceFragmentArgs );

	ClientInterface::ResourceFragmentArgs & args =
		*(ClientInterface::ResourceFragmentArgs*)stream.retrieve( argsLength );

	length -= argsLength;

	// Get existing DataDownload record if there is one
	DataDownload *pData;
	DataDownloadMap::iterator it = dataDownloads_.find( args.rid );

	if (it != dataDownloads_.end())
	{
		pData = it->second;
	}
	else
	{
		// Getting to here means header hasn't arrived by the time data arrives,
		// but there's a warning in resourceHeader() about this so we'll just
		// handle it silently here to avoid duplicate warnings.
		pData = new DataDownload( args.rid );
		dataDownloads_[ args.rid ] = pData;
	}

	DownloadSegment *pSegment = new DownloadSegment(
		(char*)stream.retrieve( length ), length, args.seq );

	pData->insert( pSegment, args.flags == 1 );

	// If this DataDownload is now complete, invoke script callback and destroy
	if (pData->complete())
	{
		MemoryOStream stream;
		pData->write( stream );

		if (pHandler_ != NULL)
			pHandler_->onStreamComplete( pData->id(), *pData->pDesc(), stream );

		dataDownloads_.erase( pData->id() );
		delete pData;
	}
}


/**
 *	This method handles a message from the server telling us that we have been
 *	disconnected.
 */
void ServerConnection::loggedOff( const ClientInterface::loggedOffArgs & args )
{
	INFO_MSG( "ServerConnection::loggedOff: "
		"The server has disconnected us. reason = %d\n", args.reason );
	this->disconnect( /*informServer:*/ false );
}


// -----------------------------------------------------------------------------
// Section: Mercury
// -----------------------------------------------------------------------------

#define DEFINE_INTERFACE_HERE
#include "common/login_interface.hpp"

#define DEFINE_INTERFACE_HERE
#include "common/baseapp_ext_interface.hpp"

#define DEFINE_SERVER_HERE
#include "common/client_interface.hpp"

// servconn.cpp
