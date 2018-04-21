/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "web_integration.hpp"

#include "autotrace.hpp"
#include "mailbox.hpp"

#include "cstdmf/debug.hpp"
#include "pyscript/script.hpp"
#include "entitydef/mailbox_base.hpp"
#include "network/basictypes.hpp"
#include "network/mercury.hpp"
#include "network/bundle.hpp"
#include "resmgr/bwresource.hpp"
#include "entitydef/constants.hpp"

#include "dbmgr/db_interface.hpp"
#include "common/login_interface.hpp"

#define ENUM_CASE_STRING(NAME) case NAME: return #NAME

/// Web Integration module Singleton.
BW_SINGLETON_STORAGE( WebIntegration )

namespace // anonymous
{
// ----------------------------------------------------------------------------
// Section: Helper method implementations
// ----------------------------------------------------------------------------

const char* logOnStatusAsString( const LogOnStatus::Status & status );
PyObject * getLogOnStatusException( const LogOnStatus& status );

/**
 *	Returns a string that describes the given login status value.
 *
 *	@param status	the log on status object
 *	@return a C-string describing the status object
 */
const char* logOnStatusAsString( const LogOnStatus::Status & status )
{

	switch (status)
	{
		case LogOnStatus::NOT_SET:
			return "Log on status not set";
		case LogOnStatus::LOGGED_ON:
			return "Logged on";
		case LogOnStatus::CONNECTION_FAILED:
			return "Connection failed";
		case LogOnStatus::DNS_LOOKUP_FAILED:
			return "DNS lookup failed";
		case LogOnStatus::UNKNOWN_ERROR:
			return "Unknown error";
		case LogOnStatus::CANCELLED:
			return "Cancelled";
		case LogOnStatus::ALREADY_ONLINE_LOCALLY:
			return "Already online locally";
		case LogOnStatus::PUBLIC_KEY_LOOKUP_FAILED:
			return "Public key lookup failed";

		case LogOnStatus::LOGIN_MALFORMED_REQUEST:
			return "Malformed request";
		case LogOnStatus::LOGIN_BAD_PROTOCOL_VERSION:
			return "Bad protocol version";

		case LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER:
			return "No such user";
		case LogOnStatus::LOGIN_REJECTED_INVALID_PASSWORD:
			return "Invalid password";
		case LogOnStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN:
			return "Already logged in";
		case LogOnStatus::LOGIN_REJECTED_BAD_DIGEST:
			return "Bad digest";
		case LogOnStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE:
			return "DB general failure";
		case LogOnStatus::LOGIN_REJECTED_DB_NOT_READY:
			return "DB not ready";
		case LogOnStatus::LOGIN_REJECTED_ILLEGAL_CHARACTERS:
			return "Illegal characters";
		case LogOnStatus::LOGIN_REJECTED_SERVER_NOT_READY:
			return "Server not ready";
		case LogOnStatus::LOGIN_REJECTED_NO_BASEAPPS:
			return "No BaseApps";
		case LogOnStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD:
			return "BaseApps overloaded";
		case LogOnStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD:
			return "CellApps overloaded";
		case LogOnStatus::LOGIN_REJECTED_BASEAPP_TIMEOUT:
			return "BaseApp timeout";
		case LogOnStatus::LOGIN_REJECTED_BASEAPPMGR_TIMEOUT:
			return "BaseAppMgr timeout";
		case LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD:
			return "DBMgr overloaded";

		case LogOnStatus::LOGIN_CUSTOM_DEFINED_ERROR:
			return "Custom defined error";
		default:
			return "(unknown logon status)";
	}
}


/**
 *	Returns the appropriate exception object for a given logon status value.
 *
 *	@param status the LogOnStatus value
 *	@return the appropriate Python exception object
 */
PyObject * getLogOnStatusException( const LogOnStatus& status )
{
		PyObject* exception = NULL;
		switch (status)
		{
			case LogOnStatus::CONNECTION_FAILED:
			case LogOnStatus::DNS_LOOKUP_FAILED:
			case LogOnStatus::LOGIN_REJECTED_ILLEGAL_CHARACTERS:
				exception = PyExc_IOError;
				break;

			case LogOnStatus::UNKNOWN_ERROR:
			case LogOnStatus::PUBLIC_KEY_LOOKUP_FAILED:
			case LogOnStatus::LOGIN_REJECTED_BAD_DIGEST:
			case LogOnStatus::LOGIN_REJECTED_DB_GENERAL_FAILURE:
			case LogOnStatus::LOGIN_REJECTED_DB_NOT_READY:
			case LogOnStatus::LOGIN_REJECTED_SERVER_NOT_READY:
			case LogOnStatus::LOGIN_REJECTED_NO_BASEAPPS:
			case LogOnStatus::LOGIN_REJECTED_BASEAPP_OVERLOAD:
			case LogOnStatus::LOGIN_REJECTED_CELLAPP_OVERLOAD:
			case LogOnStatus::LOGIN_REJECTED_BASEAPP_TIMEOUT:
			case LogOnStatus::LOGIN_REJECTED_BASEAPPMGR_TIMEOUT:
			case LogOnStatus::LOGIN_REJECTED_DBMGR_OVERLOAD:
				exception = PyExc_SystemError;
				break;

			case LogOnStatus::LOGIN_REJECTED_NO_SUCH_USER:
			case LogOnStatus::LOGIN_REJECTED_INVALID_PASSWORD:
			case LogOnStatus::CANCELLED:
			case LogOnStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN:
				exception = PyExc_ValueError;
				break;

			default:
				exception = PyExc_RuntimeError;

		}
	return exception;
}

}; // namespace anonymous


// ----------------------------------------------------------------------------
// Section: BlockingReplyHandler
// ----------------------------------------------------------------------------

/**
 *	Blocking reply handler that doesn't stream any objects off - this is left
 *	entirely up to subclasses.
 */
class BlockingReplyHandler : public Mercury::ReplyMessageHandler
{
public:
	/**
	 *	Constructor.
	 *
	 *	@param nub		the nub to wait on when blocking
	 */
	BlockingReplyHandler( Mercury::Nub & nub ):
		nub_( nub ),
		err_( Mercury::REASON_SUCCESS ),
		done_( false )
	{}

	/**
	 *	Destructor.
	 */
	virtual ~BlockingReplyHandler() {}

public: // from Mercury::ReplyMessageHandler

	virtual void handleMessage( const Mercury::Address &srcAddr,
			Mercury::UnpackedMessageHeader& header,
			BinaryIStream& data,
			void* args );

	virtual void handleException( const Mercury::NubException& exception,
			void* args );



public: // new methods

	/**
	 *	Template method to handle a message. Same contract as
	 *	Mercury::ReplyMessageHandler::handleMessage. Subclasses should override
	 *	this.
	 */
	virtual void doHandleMessage( const Mercury::Address &srcAddr,
			Mercury::UnpackedMessageHeader& header,
			BinaryIStream& data,
			void* args )
	{}


	/**
	 *	Template method to handle an exception from the nub handling the
	 *	request.
	 *	Same contract as Mercury::ReplyMessageHandler::handleException.
	 *	Subclasses should override this.
	 */
	virtual void doHandleException( const Mercury::NubException& exception,
			void* args )
	{}


	void await();


	/**
	 *	Accessor for the nub for this handler.
	 */
	Mercury::Nub& nub() { return nub_; }

	/**
	 *	Returns the Mercury result.
	 *
	 *	@return Mercury::Reason
	 */
	int err() const { return err_; }

	/**
	 *	Returns whether the handler has completed.
	 *
	 *	@return whether the handler has completed.
	 */
	bool done() const { return done_; }

protected:
	/** The handler nub. */
	Mercury::Nub & 			nub_;

	/** The handler Mercury error status. */
	int 					err_;

	/** The handler's current finish state. */
	bool					done_;
};


/**
 *	Blocks until the handler has received a message, or an exception is
 *	detected.
 */
void BlockingReplyHandler::await()
{
	AutoTrace _at( "BlockingReplyHandler::await()" );
	bool wasBroken = nub_.processingBroken();

	while (!done_)
	{
		try
		{
			nub_.processContinuously();
			nub_.processUntilChannelsEmpty();
		}
		catch (Mercury::NubException & ne)
		{
			err_ = ne.reason();
			done_ = true;
		}
	}

	nub_.breakProcessing( wasBroken );
}


/**
 *	Handle the Mercury reply message.
 */
void BlockingReplyHandler::handleMessage( const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * args )
{
	err_ = Mercury::REASON_SUCCESS;
	done_ = true;
	nub_.breakProcessing();
	this->doHandleMessage( srcAddr, header, data, args );
}


/**
 *	Handle a Mercury exception.
 */
void BlockingReplyHandler::handleException( const Mercury::NubException & ne,
		void * args )
{
	AutoTrace _at( "BlockingReplyHandler::handleException()" );
	err_ = ne.reason();
	done_ = true;
	nub_.breakProcessing();
	this->doHandleException( ne, args );
}

// ----------------------------------------------------------------------------
// Section: BlockingDbLookUpHandler
// ----------------------------------------------------------------------------

/**
 *	Class that handles replies to lookupEntity requests from the DbMgr. It can
 *	block until it receives a response.
 */
class BlockingDbLookUpHandler : public BlockingReplyHandler
{
public:
	/**
	 *	Possible result states from lookups on the database.
	 */
	enum Result
	{
		OK,					/**< mailbox received OK */
		PENDING,			/**< pending response from DbMgr */
		TIMEOUT,			/**< timeout waiting for response */
		DOES_NOT_EXIST,		/**< entity does not exist */
		NOT_CHECKED_OUT, 	/**< DbMgr reports that entity is not checked out */
		GENERAL_ERROR		/**< General communications error,
									maybe DbMgr down */
	};


public:
	/**
	 *	Constructor.
	 *
	 *	@param nub the nub to listen on
	 */
	BlockingDbLookUpHandler( Mercury::Nub & nub ):
		BlockingReplyHandler( nub ),
		result_( PENDING ),
		mailbox_()
	{}


	/**
	 *	Destructor.
	 */
	virtual ~BlockingDbLookUpHandler()
	{}

public: // From BlockingReplyHandler

	virtual void doHandleMessage(
			const Mercury::Address & srcAddr,
			Mercury::UnpackedMessageHeader & header,
			BinaryIStream & data,
			void * args );

	virtual void doHandleException(
			const Mercury::NubException & ne,
			void * arg );
	/**
	 *	Returns the mailbox object bound to this handler.
	 *	@return a reference to the mailbox
	 */
	const EntityMailBoxRef & mailbox() const { return mailbox_; }


	/**
	 *	Returns the result of the lookup.
	 *	@return the result of the lookup.
	 */
	const Result & result() const { return result_; }

	/**
	 *	Returns a string description of a result.
	 *
	 *	@return a string description of the result of the lookup.
	 */
	static const char* getResultString( Result result )
	{
		switch (result)
		{
			ENUM_CASE_STRING( OK );
			ENUM_CASE_STRING( PENDING );
			ENUM_CASE_STRING( TIMEOUT );
			ENUM_CASE_STRING( NOT_CHECKED_OUT );
			ENUM_CASE_STRING( GENERAL_ERROR );
			default: return "(UNKNOWN)";
		}
	}

private:
	/** The lookup result. */
	Result 				result_;

	/** The mailbox bound to this handler. */
	EntityMailBoxRef  	mailbox_;

};

/**
 *	Handles an incoming reply to a lookup request.
 *
 *	@param srcAddr		the source address of the reply
 *	@param header		the message header
 *	@param data			the data stream for the reply
 *	@param args			the opaque data token associated with the initial
 *						request
 */
void BlockingDbLookUpHandler::doHandleMessage(
		const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * /*args*/ )
{
	if (data.remainingLength() == 0)
	{
		//TRACE_MSG( "Entity exists but is not checked out\n" );
		// entity exists but is not checked out
		result_ = BlockingDbLookUpHandler::NOT_CHECKED_OUT;
	}
	else if (data.remainingLength() == sizeof( mailbox_ ))
	{
		// we found it!
		data >> mailbox_;
		//TRACE_MSG( "Got a mailbox: %s/%d\n",
		//	(char*)mailbox_.addr, mailbox_.id );
		result_ = BlockingDbLookUpHandler::OK;
	}
	else if (data.remainingLength() == sizeof( int ))
	{
		//TRACE_MSG( "could not lookup entity\n" );
		int err;
		data >> err;

		if (err != -1)
		{
			WARNING_MSG( "Got back an integer value that was not -1: 0x%x\n",
				err );
			result_ = BlockingDbLookUpHandler::GENERAL_ERROR;
			return;
		}

		result_ = BlockingDbLookUpHandler::DOES_NOT_EXIST;
	}
	else
	{
		// bad data size
		ERROR_MSG( "BlockingDbLookUpHandler::doHandleMessage: "
				"got bad data size=%d\n",
			data.remainingLength() );
		result_ = BlockingDbLookUpHandler::GENERAL_ERROR;
	}
}


/**
 *	Handles an exception while waiting for the corresponding request's reply.
 *
 *	@param ne		the nub exception
 *	@param args		the opaque token argument associated with the original
 *					request
 */
void BlockingDbLookUpHandler::doHandleException(
		const Mercury::NubException & ne,
		void* /*args*/ )
{
	AutoTrace _at( "BlockingDBLookUpHandler::doHandleException()" );
	if (ne.reason() == Mercury::REASON_TIMER_EXPIRED)
	{
		result_ = BlockingDbLookUpHandler::TIMEOUT;
	}
	else
	{
		result_ = BlockingDbLookUpHandler::GENERAL_ERROR;
	}
}

// ----------------------------------------------------------------------------
// Section: BlockingDbLogonHandler
// ----------------------------------------------------------------------------

/**
 *	Class that blocks on a DBMgr logon request.
 */
class BlockingDbLogonHandler: public BlockingReplyHandler
{
public:
	/**
	 *	Constructor.
	 */
	BlockingDbLogonHandler( Mercury::Nub& nub ):
		BlockingReplyHandler( nub ),
		status_( LogOnStatus::NOT_SET ),
		baseAppAddr_( 0, 0 ),
		errString_()
	{}

	/**
	 *	Destructor.
	 */
	virtual ~BlockingDbLogonHandler() {}

	/**
	 *	Returns the logon status.
	 *
	 *	@return the logon status
	 *	@see LogOnStatus::Status
	 */
	const LogOnStatus::Status &status() const { return status_; }

	/**
	 *	Returns the server address returned from the dbmgr on a logOn request.
	 */
	const Mercury::Address& baseAppAddress() const { return baseAppAddr_; }

	/**
	 *	Returns an error string describing what went wrong with a failed logon
	 *	attempt.
	 */
	const std::string& errString() const { return errString_; }

public: // from BlockingReplyHandler

	virtual void doHandleMessage( const Mercury::Address& srcAddr,
			Mercury::UnpackedMessageHeader &header,
			BinaryIStream& data,
			void* args );

private:
	/** The log on status */
	LogOnStatus::Status 	status_;

	/** The retrieved server address that the client must connect to. */
	Mercury::Address 		baseAppAddr_;

	/** The error string describing a failed logon attempt. */
	std::string 			errString_;
};


/**
 *	Handles a reply message corresponding to a previously issued logon request.
 *
 *	@param srcAddr		the source address of the reply
 *	@param header		the reply message header
 *	@param data			the reply data stream
 *	@param args			the token opaque argument associated with the original
 *						request
 */
void BlockingDbLogonHandler::doHandleMessage(
		const Mercury::Address & srcAddr,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * /*args*/ )
{
	uint8 statusByte;
	data >> statusByte;

	status_ = (LogOnStatus::Status) statusByte;

	if (status_ == LogOnStatus::LOGGED_ON)
	{
		if (data.remainingLength() >= int(sizeof( Mercury::Address ) ))
		{
			data >> baseAppAddr_;
			// session key is returned if this entity is a proxy
			SessionKey sessionKey;
			if (data.remainingLength() == sizeof( SessionKey ))
			{
				data >> sessionKey;
			}
		}
		else
		{
			ERROR_MSG( "Database returned unexpected message size.\n" );
		}
	}
	else
	{
		INFO_MSG( "Could not log on: %s (%d)\n",
			logOnStatusAsString( status_ ), status_ );
		if (data.remainingLength())
		{
			// text error message (?)
			data >> errString_;
			//TRACE_MSG( "error (%d): %s\n",
			//	remainingSize, errString_.c_str() );
		}
	}
}

// ----------------------------------------------------------------------------
// Section: WebIntegration
// ----------------------------------------------------------------------------

/**
 *	Constructor.
 */
WebIntegration::WebIntegration():
		pNub_( NULL ), 	// pNub_ is created on-demand, and nubPid_ is used to
						// track whether we need to recreate because we've been
						// forked, see definition of WebIntegration::nub()
		nubPid_( 0 ),
		dbMgrAddr_( 0, 0 ),
		pEntities_( NULL ),
		hasInited_( false ),
		loggerSocket_(),
		loggerMessageForwarder_( "Web", loggerSocket_ )
{
}


/**
 *	Destructor.
 */
WebIntegration::~WebIntegration()
{
	if (pNub_)
	{
		delete pNub_;
	}

	if (pEntities_)
	{
		pEntities_->clear();
		delete pEntities_;
	}
}


/**
 *	Initialise the web integration singleton. Returns false and sets an
 *	appropriate Python exception on initialisation failure.
 *
 *	@return true if initialisation success, otherwise
 */
bool WebIntegration::init()
{
	if (hasInited_)
	{
		PyErr_SetString( PyExc_EnvironmentError,
			"web integration module already initialised" );
		return false;
	}

	pEntities_ = new EntityDescriptionMap();

	DataSectionPtr pEntityDefData = BWResource::openSection(
		EntityDef::Constants::entitiesFile() );

	if (!pEntityDefData)
	{
		ERROR_MSG( "WebIntegration::init: "
			"Could not open %s to parse entity definitions\n",
			EntityDef::Constants::entitiesFile() );
		PyErr_Format( PyExc_EnvironmentError,
			"Could not open %s to parse entity definitions\n",
			EntityDef::Constants::entitiesFile() );
		return false;
	}

	if (!pEntities_->parse( pEntityDefData ))
	{
		ERROR_MSG( "WebIntegration::init: "
			"Failed to parse entity definitions\n" );
		PyErr_SetString( PyExc_EnvironmentError,
			"Failed to parse entity definitions\n" );
		return false;
	}

	WebEntityMailBox::initMailboxFactory();

	return true;
}


/**
 *	Sets the nub port, and invalidates the local copy of the dbmgr's address.
 *
 *	@param port the port, or 0 for a random port.
 *
 */
void WebIntegration::setNubPort( uint16 port )
{
	if (pNub_)
	{
		delete pNub_;
	}
	if (!port)
	{
		pNub_ = new Mercury::Nub();
	}
	else
	{
		pNub_ = new Mercury::Nub( port );
	}

	// also reset addresses
	dbMgrAddr_.ip = 0;

}


/**
 *	Return the last known address for the DbMgr component. If forget is true,
 *	then (re)-query the machine daemon for the DbMgr address, store, then
 *	return the retrieved address.
 *
 *	@param forget		whether to forget the last known address, and retrieve
 *						the DbMgr by querying the machine daemon.
 *
 *	@return the last known DbMgr address. The returned address has an ip field
 *	of 0 if it has/could not be retrieved.
 */
Mercury::Address & WebIntegration::dbMgrAddr( bool forget )
{
	if (forget || dbMgrAddr_.ip == 0)
	{
		// do the look up
		int res = pNub_->findInterface( "DBInterface", 0, dbMgrAddr_ );
		if (res != Mercury::REASON_SUCCESS)
		{
			ERROR_MSG( "Could not get DbMgr interface address\n" );
			dbMgrAddr_.ip = 0;
		}
	}
	return dbMgrAddr_;
}


/**
 *	This method authenticates a user, identified by the given username and
 *	password, and checks out the corresponding user entity if it has not
 *	already been - the mailbox can then be retrieved via the
 *	BigWorld.lookUpEntityByName() call.
 *
 *	Already checked out user entities have the BaseApp script
 *	callback Base.onLogOnAttempt() called on.
 *
 *	@param username					the user name of the entity being logged
 *									on
 *	@param password					the password of the entity being logged on
 *	@param allowAlreadyLoggedOn		whether to throw an exception if the user
 *									entity already exists at the time of the
 *									log on
 */
int WebIntegration::logOn( const std::string & username,
		const std::string & password, bool allowAlreadyLoggedOn )
{
	const Mercury::Address& dbMgrAddr = this->dbMgrAddr();
	if (!dbMgrAddr.ip)
	{
		this->dbMgrAddr( true );
	}
	if (!dbMgrAddr.ip)
	{
		PyErr_SetString( PyExc_IOError, "Server not running" );
		return -1;
	}

	BlockingDbLogonHandler logonHandler =
		BlockingDbLogonHandler( this->nub() );

	Mercury::Bundle request;

	request.startRequest( DBInterface::logOn, &logonHandler );

	// supply a blank address for a non-client proxy instance
	Mercury::Address blank( 0, 0 );
	request << blank;

	request << true; // off-channel

	// logon params
	std::string encryptionKey; // empty encryption key
	LogOnParamsPtr pParams = new LogOnParams(
		username, password, encryptionKey );

	// calculate digest
	MD5 md5;
	pEntities_->addToMD5( md5 );
	MD5::Digest digest;
	md5.getDigest( digest );
	pParams->digest( digest );

	request << *pParams;

	pNub_->send( this->dbMgrAddr(), request );

	logonHandler.await();

	if (logonHandler.err() != Mercury::REASON_SUCCESS)
	{
		PyErr_SetString( PyExc_IOError,
			Mercury::reasonToString(
				( Mercury::Reason ) logonHandler.err() ) );
		return -1;
	}

	if (logonHandler.status() == LogOnStatus::LOGGED_ON)
	{
		return 0;
	}
	else if (allowAlreadyLoggedOn &&
		logonHandler.status() ==
			LogOnStatus::LOGIN_REJECTED_ALREADY_LOGGED_IN)
	{
		return 0;
	}
	else
	{
		PyObject *exception = getLogOnStatusException( logonHandler.status() );
		std::string errString;
		if (logonHandler.errString().size())
		{
			errString = logonHandler.errString();
		}
		else
		{
			errString = logOnStatusAsString( logonHandler.status() );
		}
		PyErr_SetString( exception, errString.c_str() );

		return -1;
	}
}


/**
 *	This method looks up a checked out entity by its entity type and
 *	identifier string.
 *
 *	@param entityTypeName	the name of the entity type
 *	@param entityName		the value of the identifier property string
 *
 *	@return the mailbox if such an entity exists and has been checked out, the
 *	Python object for True if such an entity exists but has not been checked
 *	out, the Python object for False if such an entity does not exist in the
 *	database, or NULL if an exception occurred.
 */
PyObject * WebIntegration::lookUpEntityByName(
		const std::string & entityTypeName, const std::string & entityName )
{
	EntityTypeID entityTypeID;
	if (!this->lookUpEntityTypeByName( entityTypeName, entityTypeID ))
	{
		return NULL;
	}

	BlockingDbLookUpHandler handler( this->nub() );

	Mercury::Bundle b;
	b.startRequest( DBInterface::lookupEntityByName, &handler );
	b << entityTypeID << std::string( entityName ) << true /*offChannel*/;

	return this->lookUpEntityComplete( handler, b );
}

/**
 *	This method looks up a checked out entity by its entity type and
 *	database ID.
 *
 *	@param entityTypeName	the name of the entity type
 *	@param dbID				the value of the database ID
 *
 *	@return the mailbox if such an entity exists and has been checked out, the
 *	Python object for True if such an entity exists but has not been checked
 *	out, the Python object for False if such an entity does not exist in the
 *	database, or NULL if an exception occurred.
 */
PyObject * WebIntegration::lookUpEntityByDBID(
		const std::string & entityTypeName, DatabaseID dbID )
{
	EntityTypeID entityTypeID;
	if (!this->lookUpEntityTypeByName( entityTypeName, entityTypeID ))
	{
		return NULL;
	}

	BlockingDbLookUpHandler handler( this->nub() );
	Mercury::Bundle b;

	DBInterface::lookupEntityArgs & lea = lea.startRequest(
		b, &handler );
	lea.entityTypeID = entityTypeID;
	lea.dbid = dbID;
	lea.offChannel = true;

	return this->lookUpEntityComplete( handler, b );
}


/**
 *	This method completes the lookup operation after the given bundle has had
 *	the appropriate request streamed onto it.
 *
 *	@param handler	the request reply handler for the look up request
 *	@param bundle	the bundle holding the streamed request
 *
 *	@return	the Python object to return back to script, or NULL if an exception
 *	has occurred.
 */
PyObject * WebIntegration::lookUpEntityComplete(
		BlockingDbLookUpHandler & handler, Mercury::Bundle & bundle )
{
	Mercury::Address dbMgrAddr = this->dbMgrAddr();

	Mercury::Reason networkError = Mercury::REASON_SUCCESS;

	int retries = 3;
	while (retries--)
	{
		try
		{
			if (!dbMgrAddr.ip)
			{
				dbMgrAddr = this->dbMgrAddr( true );
			}

			if (!dbMgrAddr.ip)
			{
				networkError = Mercury::REASON_NO_SUCH_PORT;
			}
			else
			{
				handler.nub().send( dbMgrAddr, bundle );
			}
		}
		catch( Mercury::NubException & ne )
		{
			networkError = ne.reason();
		}
		if (networkError == Mercury::REASON_NO_SUCH_PORT)
		{
			// TODO: if server has gone away, we might want to return exception
			// here. Signal to the web server that the session is invalid.
			dbMgrAddr.ip = 0;
			continue;
		}
		break;
	}

	if (!networkError)
	{
		handler.await();
		networkError = (Mercury::Reason)handler.err();
	}

	if (networkError == Mercury::REASON_TIMER_EXPIRED)
	{
		PyErr_SetString( PyExc_IOError, "database timeout" );
		return NULL;
	}
	else if (networkError != Mercury::REASON_SUCCESS)
	{
		PyErr_Format( PyExc_IOError, "while requesting lookup for entity: %s",
			Mercury::reasonToString( ( Mercury::Reason ) handler.err() ) );
		return NULL;
	}

	// check the result
	BlockingDbLookUpHandler::Result result = handler.result();

	if (result == BlockingDbLookUpHandler::OK)
	{
		const EntityMailBoxRef & mbox = handler.mailbox();
		TRACE_MSG( "Mailbox: %s/id=%u,type=%d,component=%d\n",
			( char* ) mbox.addr,
			mbox.id,
			mbox.type(),
			mbox.component()
		);


		PyObject* mboxObj = Script::getData( mbox );
		if (mboxObj == Py_None)
		{
			ERROR_MSG( "Script::getData() returned None object\n" );
		}

		return mboxObj;

	}

	// we don't have a mailbox, try and find out why
	if (result == BlockingDbLookUpHandler::NOT_CHECKED_OUT)
	{
		Py_RETURN_TRUE;
	}

	if (result == BlockingDbLookUpHandler::DOES_NOT_EXIST)
	{
		Py_RETURN_FALSE;
	}

	if (result == BlockingDbLookUpHandler::TIMEOUT)
	{
		PyErr_SetString( PyExc_IOError,
			BlockingDbLookUpHandler::getResultString( result ) );
		return NULL;
	}
	if (result == BlockingDbLookUpHandler::PENDING)
	{
		PyErr_SetString( PyExc_RuntimeError, "handler is still pending" );
		return NULL;
	}

	if (result == BlockingDbLookUpHandler::GENERAL_ERROR)
	{
		PyErr_SetString( PyExc_SystemError,
			BlockingDbLookUpHandler::getResultString( result ) );
		return NULL;
	}
	PyErr_SetString( PyExc_SystemError, "unknown error" );
	return NULL;
}


/**
 *	This method looks up the entity type ID of the given entity type.
 *	If it fails, it returns false.
 *
 *	@param name		the entity type name string
 *	@param id		if successful, this will be filled with the corresponding
 *					entity type ID value
 *	@return			success or failure
 */
bool WebIntegration::lookUpEntityTypeByName(
		const std::string & name, EntityTypeID & id )
{
	if (!pEntities_->nameToIndex( name,
			id ))
	{
		PyErr_Format( PyExc_Exception, "No such entity type: %s",
			name.c_str() );
		return false;
	}
	return true;
}


/**
 *	Return the nub used for this component.
 *
 *	This nub is created on demand to cater for Apache forking.
 *
 *	@return the nub
 */
Mercury::Nub & WebIntegration::nub()
{
	// Because Apache preforks processes, we don't want to use the nub that was
	// created in the parent process. We create the nub on-demand in the child
	// Apache processes here.

	if (!nubPid_ || nubPid_ != getpid())
	{
		INFO_MSG( "WebIntegration::nub: (re-)creating nub\n" );
		delete pNub_;
		pNub_ = new Mercury::Nub();
		nubPid_ = getpid();
	}
	return *pNub_;
}

WebIntegration::LoggerEndpoint::LoggerEndpoint() :
	Endpoint()
{
	switchSocket();
}

bool WebIntegration::LoggerEndpoint::switchSocket()
{
	if (this->good())
	{
		this->close();
	}

	// open the socket
	this->socket(SOCK_DGRAM);
	if(!this->good())
	{
		ERROR_MSG( "AnyEndpoint::switchSocket: socket() failed\n" );
		return true;
	}

	if (this->setnonblocking(true))	// make it nonblocking
	{
		ERROR_MSG( "AnyEndpoint::switchSocket: fcntl(O_NONBLOCK) failed\n" );
		return true;
	}

	if (this->bind( 0, INADDR_ANY ))
	{
		ERROR_MSG( "AnyEndpoint::switchSocket: bind() failed\n" );
		this->close();
		return true;
	}

	return false;
}


// web_integration.cpp
