/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "reviver.hpp"
#include "reviver_interface.hpp"

#include "cstdmf/intrusive_object.hpp"
#include "network/portmap.hpp"
#include "network/watcher_glue.hpp"
#include "resmgr/bwresource.hpp"
#include "server/bwconfig.hpp"
#include "server/reviver_common.hpp"

// Interface includes
#include "dbmgr/db_interface.hpp"
#include "cellappmgr/cellappmgr_interface.hpp"
#include "baseappmgr/baseappmgr_interface.hpp"
#include "loginapp/login_int_interface.hpp"

#include <set>

/// Reviver Singleton.
BW_SINGLETON_STORAGE( Reviver )

DECLARE_DEBUG_COMPONENT2( "Reviver", 0 )

// -----------------------------------------------------------------------------
// Section: ComponentReviver
// -----------------------------------------------------------------------------

ComponentRevivers * g_pComponentRevivers;

/**
 *	This class is used to monitor and revive a single, server component.
 */
class ComponentReviver : public Mercury::ReplyMessageHandler,
	public Mercury::TimerExpiryHandler,
	public Mercury::InputMessageHandler,
	public IntrusiveObject< ComponentReviver >
{
public:
	ComponentReviver( const char * configName, const char * name,
			const char * interfaceName, const char * createParam ) :
		IntrusiveObject< ComponentReviver >( g_pComponentRevivers ),
		pBirthMessage_( NULL ),
		pDeathMessage_( NULL ),
		pPingMessage_( NULL ),
		pNub_( NULL ),
		addr_( 0, 0 ),
		configName_( configName ),
		name_( name ),
		interfaceName_( interfaceName ),
		createParam_( createParam ),
		priority_( 0 ),
		timerID_( 0 ),
		pingsToMiss_( 0 ),
		maxPingsToMiss_( 3 ),
		pingPeriod_( 0 ),		// Make this configuarable
		isAttached_( false ),
		isEnabled_( true )
	{
	}

	bool init( Mercury::Nub & nub )
	{
		bool isOkay = true;

		MF_ASSERT( pNub_ == NULL );
		pNub_ = &nub;

		std::string prefix = "reviver/";

		float pingPeriodInSeconds =
			BWConfig::get( (prefix + configName_ + "/pingPeriod").c_str(), -1.f );

		if (pingPeriodInSeconds < 0.f)
			pingPeriod_ = Reviver::pInstance()->pingPeriod();
		else
			pingPeriod_ = int(pingPeriodInSeconds * 1000000);

		maxPingsToMiss_ =
			BWConfig::get( (prefix + configName_ + "/timeoutInPings").c_str(),
								Reviver::pInstance()->maxPingsToMiss() );

		// This initialisation of the interface elements needs to be delayed
		// because VC++ has troubles getting pointers to the values before they
		// have been created globally.
		this->initInterfaceElements();

		if (nub.findInterface( interfaceName_.c_str(), 0, addr_, 4 ) !=
											Mercury::REASON_SUCCESS)
		{
			ERROR_MSG( "ComponentReviver::init: "
				"failed to find %s\n", interfaceName_.c_str() );
			isOkay = false;
		}

		nub.registerBirthListener( *pBirthMessage_,
			const_cast<char *>( interfaceName_.c_str() ) );
		nub.registerDeathListener( *pDeathMessage_,
			const_cast<char *>( interfaceName_.c_str() ) );

		return isOkay;
	}

	void revive()
	{
		bool wasAttached = isAttached_;

		this->deactivate();
		addr_.ip = 0;
		addr_.port = 0;

		if (wasAttached)
		{
			INFO_MSG( "Reviving %s\n", name_.c_str() );
			Reviver::pInstance()->revive( createParam_ );
		}
	}

	bool activate( ReviverPriority priority )
	{
		isAttached_ = false;

		if ((timerID_ == 0) && (addr_.ip != 0))
		{
			pingsToMiss_ = maxPingsToMiss_;
			timerID_ = pNub_->registerTimer( pingPeriod_, this );
			priority_ = priority;
			return true;
		}

		return false;
	}

	bool deactivate()
	{
		if (isAttached_)
		{
			Reviver::pInstance()->markAsDirty();
			INFO_MSG( "ComponentReviver: %s (%s) has detached\n",
				(char *)addr_, name_.c_str() );
			isAttached_ = false;
		}

		if (timerID_ != 0)
		{
			pNub_->cancelTimer( timerID_ );
			timerID_ = 0;
			priority_ = 0;
			return true;
		}

		return false;
	}

	// Handles the death message
	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data )
	{
		MF_ASSERT( (header.identifier == pBirthMessage_->id()) ||
					(header.identifier == pDeathMessage_->id()) );

		Mercury::Address addr;
		data >> addr;

		if (header.identifier == pBirthMessage_->id())
		{
			addr_ = addr;
			INFO_MSG( "ComponentReviver::handleMessage: "
					"%s at %s has started.\n",
				name_.c_str(),
				(char *)addr );
			return;
		}

		INFO_MSG( "ComponentReviver::handleMessage: %s at %s has died.\n",
			name_.c_str(),
			(char *)addr );

		if (addr == addr_)
		{
			this->revive();
		}
		else if (isAttached_)
		{
			std::string currAddrStr = (char *)addr_;
			ERROR_MSG( "ComponentReviver::handleMessage: "
					"%s component died at %s. Expected %s\n",
				name_.c_str(),
				(char *)addr,
				currAddrStr.c_str() );
		}
	}

	// Reply handler
	virtual void handleMessage( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg )
	{
		uint8 returnCode;
		data >> returnCode;
		if (returnCode == REVIVER_PING_YES)
		{
			pingsToMiss_ = maxPingsToMiss_;

			if (!isAttached_)
			{
				Reviver::pInstance()->markAsDirty();
				INFO_MSG( "ComponentReviver: %s (%s) has attached.\n",
					(char *)addr_, name_.c_str() );
				isAttached_ = true;
			}
		}
		else
		{
			this->deactivate();
		}
	}

	virtual int handleTimeout( Mercury::TimerID /*id*/, void * /*arg*/ )
	{
		if (pingsToMiss_ > 0)
		{
			--pingsToMiss_;
			Mercury::Bundle bundle;
			bundle.startRequest( *pPingMessage_, this );
			bundle << priority_;
			pNub_->send( addr_, bundle );
		}
		else
		{
			INFO_MSG( "ComponentReviver::handleTimeout: Missed too many\n" );
			this->revive();
		}

		return 0;
	}

	virtual void handleException( const Mercury::NubException & ne,
		void * /*arg*/ )
	{
		// We should really be detached if we get an exception.
		if (isAttached_)
		{
			ERROR_MSG( "ReviverReplyHandler::handleMessage: "
										"%s got an exception (%s).\n",
					name_.c_str(),
					Mercury::reasonToString( ne.reason() ) );
		}
	}

	ReviverPriority priority() const	{ return priority_; }
	void priority( ReviverPriority p )	{ priority_ = p; }

	bool isAttached() const				{ return isAttached_; }
	const std::string & name() const	{ return name_; }
	const Mercury::Address & addr() const	{ return addr_; }

	bool isEnabled() const				{ return isEnabled_; }
	void isEnabled( bool v )			{ isEnabled_ = v; }

	const std::string & configName() const		{ return configName_; }
	const char * createName() const				{ return createParam_; }

	int maxPingsToMiss() const					{ return maxPingsToMiss_; }
	int pingPeriod() const						{ return pingPeriod_; }

protected:
	virtual void initInterfaceElements() = 0;

	const Mercury::InterfaceElement * pBirthMessage_;
	const Mercury::InterfaceElement * pDeathMessage_;
	const Mercury::InterfaceElement * pPingMessage_;

private:
	Mercury::Nub * pNub_;
	Mercury::Address addr_;

	std::string configName_;
	std::string name_;
	std::string interfaceName_;
	const char * createParam_;

	ReviverPriority priority_;

	Mercury::TimerID timerID_;
	int pingsToMiss_;
	int maxPingsToMiss_;
	int pingPeriod_;

	// Indicates that we are active and have received a positive response.
	bool isAttached_;

	bool isEnabled_;
};


#define MF_REVIVER_HANDLER( CONFIG, COMPONENT, CREATE_WHAT )				\
	MF_REVIVER_HANDLER2( CONFIG, COMPONENT, COMPONENT, CREATE_WHAT )

#define MF_REVIVER_HANDLER2( CONFIG, COMPONENT, COMPONENT2, CREATE_WHAT )	\
/** @internal */															\
class COMPONENT##Reviver : public ComponentReviver							\
{																			\
public:																		\
	COMPONENT##Reviver() :													\
		ComponentReviver( #CONFIG, #COMPONENT, #COMPONENT2 "Interface",		\
				CREATE_WHAT )												\
	{}																		\
	virtual void initInterfaceElements()									\
	{																		\
		pBirthMessage_ = &ReviverInterface::handle##COMPONENT##Birth;		\
		pDeathMessage_ = &ReviverInterface::handle##COMPONENT##Death;		\
		pPingMessage_ = &COMPONENT2##Interface::reviverPing;				\
	}																		\
} g_reviverOf##COMPONENT;													\


MF_REVIVER_HANDLER( cellAppMgr, CellAppMgr, "cellappmgr" )
MF_REVIVER_HANDLER( baseAppMgr, BaseAppMgr, "baseappmgr" )
MF_REVIVER_HANDLER( dbMgr,      DB,         "dbmgr" )
MF_REVIVER_HANDLER2( loginApp,   Login, LoginInt,   "loginapp" )


// -----------------------------------------------------------------------------
// Section: Construction/Destruction
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
Reviver::Reviver( Mercury::Nub & nub ) :
	nub_( nub ),
	pingPeriod_( 0 ),
	maxPingsToMiss_( DEFAULT_REVIVER_TIMEOUT_IN_PINGS ),
	shuttingDown_( false ),
	shutDownOnRevive_( true ),
	isDirty_( true )
{
}


/**
 *	Destructor.
 */
Reviver::~Reviver()
{
}


/**
 *	This method initialises the reviver.
 */
bool Reviver::init( int argc, char * argv[] )
{
	// Shouldn't be initialised yet.
	MF_ASSERT( components_.empty() );

	float reattachPeriod = BWConfig::get( "reviver/reattachPeriod", 10.f );
	float pingPeriodInSeconds = BWConfig::get( "reviver/pingPeriod", 0.1f );
	pingPeriod_ = int(pingPeriodInSeconds * 1000000);
	BWConfig::update( "reviver/shutDownOnRevive", shutDownOnRevive_ );
	BWConfig::update( "reviver/timeoutInPings", maxPingsToMiss_ );

	INFO_MSG( "\tNub address         = %s\n", nub_.c_str() );
	INFO_MSG( "\tReattach Period     = %.1f seconds\n", reattachPeriod );
	INFO_MSG( "\tDefault Ping Period = %.1f seconds\n", pingPeriodInSeconds );
	INFO_MSG( "\tDefault Timeout     = %d pings\n", maxPingsToMiss_ );
	INFO_MSG( "\tShut down on revive = %s\n",
									shutDownOnRevive_ ? "True" : "False" );

	ReviverInterface::registerWithNub( nub_ );

	if (ReviverInterface::registerWithMachined( nub_, 0 ) !=
		Mercury::REASON_SUCCESS)
	{
		WARNING_MSG( "Reviver::init: Unable to register with nub\n" );
		return false;
	}

	// TODO: Make it configurable which processes this is able to revive.
	if (g_pComponentRevivers == NULL)
	{
		ERROR_MSG( "Reviver::init: No component revivers\n" );
		return false;
	}

	components_ = *g_pComponentRevivers;

	if (!this->queryMachinedSettings())
	{
		return false;
	}

	// TODO: Support watcher nub.
	// Need to have a unique ID (or change watcher), expose values etc.
	// BW_REGISTER_WATCHER( 0, "reviver", "Reviver", "reviver", nub_ );

	ComponentRevivers::iterator endIter = components_.end();

	bool isFirstAdd = true;
	bool isFirstDel = true;

	for (int i = 1; i < argc - 1; ++i)
	{
		const bool isAdd = (strcmp( argv[i], "--add" ) == 0);
		const bool isDel = (strcmp( argv[i], "--del" ) == 0);

		if (isAdd || isDel)
		{
			++i;

			if (isAdd && isFirstAdd)
			{
				isFirstAdd = false;
				ComponentRevivers::iterator iter = components_.begin();

				while (iter != endIter)
				{
					(*iter)->isEnabled( false );

					++iter;
				}
			}

			{
				bool found = false;
				ComponentRevivers::iterator iter = components_.begin();

				while (iter != endIter)
				{
					if (((*iter)->configName() == argv[i]) ||
							// Support the lower-case version too.
							strcmp( (*iter)->createName(), argv[i] ) == 0)
					{
						found = true;
						(*iter)->isEnabled( isAdd );
					}

					++iter;
				}

				if (!found)
				{
					ERROR_MSG( "Reviver::init: Invalid command line. "
							"No such component %s\n", argv[i] );
					return false;
				}
			}
		}
	}

	if (!isFirstAdd && !isFirstDel)
	{
		ERROR_MSG( "Reviver::init: "
					"Cannot mix --add and --del command line options\n" );
		return false;
	}

	// Initialise the ComponentRevivers.
	{
		ComponentRevivers::iterator iter = components_.begin();

		while (iter != endIter)
		{
			if ((*iter)->isEnabled())
			{
				(*iter)->init( nub_ );
			}

			++iter;
		}
	}

	// Information about which types are supported.
	{
		INFO_MSG( "Monitoring the following component types:\n" );
		ComponentRevivers::iterator iter = components_.begin();

		while (iter != endIter)
		{
			if ((*iter)->isEnabled())
			{
				INFO_MSG( "\t%s\n", (*iter)->name().c_str() );
				INFO_MSG( "\t\tPing Period = %.1f seconds\n",
						double((*iter)->pingPeriod())/1000000.f );
				INFO_MSG( "\t\tTimeout     = %d pings\n",
						(*iter)->maxPingsToMiss() );
			}

			++iter;
		}
	}

	// Activate the ComponentRevivers.
	{
		ReviverPriority priority = 0;

		ComponentRevivers::iterator iter = components_.begin();

		while (iter != endIter)
		{
			if ((*iter)->isEnabled())
			{
				(*iter)->activate( ++priority );
			}

			++iter;
		}
	}

	nub_.registerTimer( int(reattachPeriod * 1000000),
			this,
			(void *)TIMEOUT_REATTACH );

	return true;
}

bool Reviver::TagsHandler::onTagsMessage( TagsMessage &tm, uint32 addr )
{
	if (tm.exists_)
	{
		Tags &tags = tm.tags_;
		ComponentRevivers::iterator iter = reviver_.components_.begin();
		ComponentRevivers::iterator endIter = reviver_.components_.end();

		while (iter != endIter)
		{
			ComponentReviver & component = **iter;

			if (std::find( tags.begin(), tags.end(), component.createName() )
				!= tags.end() ||
				std::find( tags.begin(), tags.end(), component.configName() )
				!= tags.end())
			{
				component.isEnabled( true );
			}
			else
			{
				INFO_MSG( "\t%s disabled via bwmachined's Components tags\n",
					   component.name().c_str() );
				component.isEnabled( false );
			}

			++iter;
		}
	}
	else
	{
		ERROR_MSG( "Reviver::init: BWMachined has no Components tags\n" );
	}

	return false;
}

/**
 *	This method queries the local bwmachined for its tags associated with
 *	Components. This is the set of Component types that the machine can run.
 *	This is used to restrict the types of components that this can revive.
 */
bool Reviver::queryMachinedSettings()
{
	TagsMessage query;
	query.tags_.push_back( std::string( "Components" ) );

	TagsHandler handler( *this );
	int reason;

	if ((reason = query.sendAndRecv( 0, LOCALHOST, &handler )) !=
		Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "Reviver::queryMachinedSettings: MGM query failed (%s)\n",
			Mercury::reasonToString( (Mercury::Reason&)reason ) );
		return false;
	}

	return true;
}


/**
 *	This method handles timer events.
 */
int Reviver::handleTimeout( Mercury::TimerID id, void * arg )
{
	typedef std::map< ReviverPriority, ComponentReviver * > Map;

	switch (uintptr(arg))
	{
		case TIMEOUT_REATTACH:
		{
			Map activeSet;
			ComponentRevivers deactive;

			components_ = *g_pComponentRevivers;
			ComponentRevivers::iterator iter = components_.begin();
			ComponentRevivers::iterator endIter = components_.end();

			while (iter != endIter)
			{
				if ((*iter)->isEnabled())
				{
					ReviverPriority priority = (*iter)->priority();

					if (priority > 0)
					{
						activeSet[ priority ] = (*iter);
					}
					else
					{
						deactive.push_back( *iter );
					}
				}

				++iter;
			}

			// Adjust priorities
			{
				Map::iterator mapIter = activeSet.begin();
				ReviverPriority priority = 0;

				while (mapIter != activeSet.end())
				{
					++priority;
					if (mapIter->first != priority)
					{
						mapIter->second->priority( priority );
					}

					++mapIter;
				}

				std::random_shuffle( deactive.begin(), deactive.end() );

				iter = deactive.begin();
				endIter = deactive.end();

				while (iter != endIter)
				{
					(*iter)->activate( ++priority );
					++iter;
				}
			}

			if (isDirty_)
			{
				INFO_MSG( "---- Attached components summary ----\n" );

				if (!activeSet.empty())
				{

					Map::iterator mapIter = activeSet.begin();

					while (mapIter != activeSet.end())
					{
						INFO_MSG( "%d: (%s) %s\n",
							mapIter->second->priority(),
							(char *)mapIter->second->addr(),
							mapIter->second->name().c_str() );

						++mapIter;
					}
				}
				else
				{
					INFO_MSG( "No attached components\n" );
				}

				isDirty_ = false;
			}
		}
		break;
	}

	return 0;
}


// -----------------------------------------------------------------------------
// Section: Misc
// -----------------------------------------------------------------------------

/**
 *	This method runs the main loop of this process.
 */
void Reviver::run()
{
	if (this->hasEnabledComponents())
	{
		nub_.processUntilBreak();
	}
	else
	{
		INFO_MSG( "Reviver::run:"
				"No components enabled to revive. Shutting down.\n" );
	}
}


/**
 *	This method shuts this process down.
 */
void Reviver::shutDown()
{
	shuttingDown_ = true;
	nub_.breakProcessing();
}


/**
 *	This method sends a message to machined so that the input process is revived.
 */
void Reviver::revive( const char * createComponent )
{
	if (shuttingDown_)
	{
		INFO_MSG( "Reviver::revive: "
			"Trying to revive a process while shutting down.\n" );
		return;
	}


	CreateMessage cm;
	cm.uid_ = getUserId();
	cm.recover_ = 1;
	cm.name_ = createComponent;
#ifdef _DEBUG
	cm.config_ = "Debug";
#endif
#ifdef _HYBRID
	cm.config_ = "Hybrid";
#endif
#ifdef _RELEASE
	cm.config_ = "Release";
#endif

	// TODO: The original output forwarding will not be preserved by this, and
	// we aren't ever using multicast

	// TODO: It would be good to know if machined actually successfully starts
	// the process. One reason that it can fail is if machined.conf is not
	// set up correctly.

	uint32 srcaddr = 0, destaddr = htonl( 0x7f000001U );
	if (cm.sendAndRecv( srcaddr, destaddr ) != Mercury::REASON_SUCCESS)
	{
		ERROR_MSG( "ComponentReviver::revive: Could not send request.\n" );
	}

	if (shutDownOnRevive_)
	{
		shuttingDown_ = true;
		this->shutDown();
	}
}


/**
 * This method checks if there any enabled components
 */
bool Reviver::hasEnabledComponents() const
{
	ComponentRevivers::const_iterator iter = components_.begin();
	ComponentRevivers::const_iterator endIter = components_.end();

	while (iter != endIter)
	{
		if ((*iter)->isEnabled())
		{
			return true;
		}
		iter++;
	}

	return false;
}


// -----------------------------------------------------------------------------
// Section: Interfaces
// -----------------------------------------------------------------------------

#include "loginapp/login_int_interface.hpp"
#define DEFINE_INTERFACE_HERE
#include "loginapp/login_int_interface.hpp"

// We serve this interface
#define DEFINE_SERVER_HERE
#include "reviver_interface.hpp"

// reviver.cpp
