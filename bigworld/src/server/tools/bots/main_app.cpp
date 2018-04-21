/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "main_app.hpp"

#include "client_app.hpp"
#include "py_bots.hpp"
#include "patrol_graph.hpp"
#include "bots_interface.hpp"

#include "network/watcher_glue.hpp"
#include "server/bwconfig.hpp"
#include "server/python_server.hpp"
#include "pyscript/py_output_writer.hpp"

#include "entity.hpp"

#include "entitydef/constants.hpp"

#include <signal.h>
#include <memory>

#ifdef unix
#include <dlfcn.h>
#endif


DECLARE_DEBUG_COMPONENT2( "Bots", 0 )

namespace
{
// These options are related to splitting the sends up over each tick.
const int TICK_FRAGMENTS = 1; // Currently not used.
const int TICK_FREQUENCY = 10;
const int TICK_TIMEOUT = 1000000/TICK_FREQUENCY/TICK_FRAGMENTS;
const float TICK_PERIOD = 1.f/TICK_FREQUENCY;
const float MINI_TICK_PERIOD = 0.1f / TICK_FRAGMENTS;
}

// -----------------------------------------------------------------------------
// Section: Static data
// -----------------------------------------------------------------------------

/// Bots Application Singleton.
BW_SINGLETON_STORAGE( MainApp )

extern int Math_token;
extern int ResMgr_token;
extern int PyScript_token;
static int s_moduleTokens =
	Math_token | ResMgr_token | PyScript_token;

extern int PyPatrolPath_token;
static int s_patrolPathToken = PyPatrolPath_token;
extern int PyUserDataObject_token;
extern int UserDataObjectDescriptionMap_Token;
static int s_udoTokens = PyUserDataObject_token | UserDataObjectDescriptionMap_Token;

/**
 *	 This function handles the SIGINT signal.
 */
void intSignalHandler( int /*sigNum*/ )
{
	MainApp * pApp = MainApp::pInstance();

	if (pApp != NULL)
	{
		pApp->shutDown();
	}
}

// -----------------------------------------------------------------------------
// Section: class MainApp
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
MainApp::MainApp() :
		nub_(),
		stop_( false ),
		serverName_(),
		username_( "Bot" ),
		password_(),
		tag_( "Default" ),
		randomName_( true ),
		port_( 0 ),
		useScripts_( false ),
		standinEntity_( "DefaultEntity" ),
		localTime_( 0.0 ),
		timerID_( 0 ),
		controllerType_( "Patrol" ),
		controllerData_( "server/bots/test.bwp" ),
		pPythonServer_( NULL ),
		clientTickIndex_( bots_.end() )
{
	BWConfig::update( "bots/username", username_ );
	BWConfig::update( "bots/password", password_ );
	BWConfig::update( "bots/publicKey", publicKeyPath_ );
	BWConfig::update( "bots/serverName", serverName_ );
	BWConfig::update( "bots/port", port_ );
	BWConfig::update( "bots/randomName", randomName_ );
	BWConfig::update( "bots/scripts", useScripts_ );
	BWConfig::update( "bots/standinEntity", standinEntity_ );
	BWConfig::update( "bots/controllerType", controllerType_ );
	BWConfig::update( "bots/controllerData", controllerData_ );

	std::string digest;
	BWConfig::update( "bots/loginMD5Digest", digest );

	loginDigest_.unquote( digest );

	srand( (unsigned int)timestamp() );
}


/**
 *	Destructor.
 */
MainApp::~MainApp()
{
	nub_.cancelTimer( timerID_ );

	bots_.clear();
	Py_XDECREF( pPythonServer_ );
	pPythonServer_ = NULL;

	Script::fini();
}


/**
 *	This method initialises the application.
 *
 *	@return True on success, otherwise false.
 */
bool MainApp::init( int argc, char * argv[] )
{
	// Get any command line arguments
	for (int i = 0; i < argc; i++)
	{
		if (strcmp( "-serverName", argv[i] ) == 0)
		{
			i++;
			serverName_ = ( i < argc ) ? argv[ i ] : "";
			INFO_MSG( "Server name is %s\n", serverName_.c_str() );
		}
		else if (strcmp( "-username", argv[i] ) == 0)
		{
			i++;
			username_ = ( i < argc ) ? argv[ i ] : username_;
			INFO_MSG( "Username is %s\n", username_.c_str() );
		}
		else if (strcmp( "-password", argv[i] ) == 0)
		{
			i++;
			password_ = ( i < argc ) ? argv[ i ] : password_;
		}
		else if (strcmp( "-port", argv[i] ) == 0)
		{
			i++;
			port_ = ( i < argc ) ? atoi(argv[ i ]) : port_;
		}
		else if (strcmp( "-randomName", argv[i] ) == 0)
		{
			randomName_ = true;
		}
		else if (strcmp( "-scripts", argv[i] ) == 0)
		{
			useScripts_ = true;
		}
	}

	if (serverName_.empty())
	{
		Mercury::Address addr;
		if (nub_.findInterface( "LoginInterface", 0, addr, 4 ) == 0)
		{
			serverName_ = addr.c_str();
			/* ignore port_ config as we've got that from LoginInterface */
			port_ = 0;
			printf( "Found via bwmachined at %s. "
					"Use server port from bwmachined instead of specified one\n",
					serverName_.c_str() );
		}
	}

	if (serverName_.empty())
	{
		char inputServerName[128];
		std::cout << "Input server name: ";
		if (scanf( "%128s", inputServerName ) > 0)
		{
			serverName_ = inputServerName;
		}
	}

	if (port_ != 0)
	{
		char portNum[10];
		snprintf( portNum, 10, "%d", port_ );
		serverName_.append( ":" ).append( portNum );
	}

	timerID_ = nub_.registerTimer( TICK_TIMEOUT, this );

	signal( SIGINT, ::intSignalHandler );

	Script::init( EntityDef::Constants::botScriptsPath(), "bot" );
	PyOutputWriter::overrideSysMembers(
		BWConfig::get( "bots/writePythonLog", false ) );

	PyObject * pModule = PyImport_AddModule( "BigWorld" );

	if (pModule)
	{
		PyObject * pBots = new PyBots;

		if (PyObject_SetAttrString( pModule, "bots", pBots ) == -1)
		{
			ERROR_MSG( "MainApp::init: Failed to set BigWorld.bots\n" );
			PyErr_Clear();
		}
		Py_DECREF( pBots );
	}

	// Initialise the entity descriptions.
	// Read entities scripts anyway as we need to create player entities
	// for logoff purpose
	if (EntityType::init( standinEntity_ ) == -1)
	{
		ERROR_MSG( "MainApp::init: Could not initialise entity data.Abort!\n" );
		return false;
	}

	Script::call(
		PyObject_GetAttrString( this->getPersonalityModule().getObject(), "onBotsReady" ),
		PyTuple_New(0), "onBotsReady", true );

	pPythonServer_ = new PythonServer( "Welcome to the Bot process" );
	pPythonServer_->startup( nub_, 0 );
	PyRun_SimpleString( "import BigWorld" );

	INFO_MSG( "Python Server Port is %d\n", pPythonServer_->port() );

	// Register the watcher
	BW_REGISTER_WATCHER( 0, "bots", "Bot App", "bots", nub_ );

	MF_WATCH( "command/addBots", *this,
			MF_WRITE_ACCESSOR( int, MainApp, addBots ) );
	MF_WATCH( "command/delBots", *this,
			MF_WRITE_ACCESSOR( int, MainApp, delBots ) );

	MF_WATCH( "tag", tag_ );
	MF_WATCH( "command/delTaggedEntities", *this,
			MF_WRITE_ACCESSOR( std::string, MainApp, delTaggedEntities ) );

	MF_WATCH( "numBots", bots_, &Bots::size );

	MF_WATCH( "pythonServerPort", *pPythonServer_, &PythonServer::port );

	/* */
	MF_WATCH( "defaultControllerType", controllerType_ );
			// MF_ACCESSORS( std::string, MainApp, controllerType ) );
	MF_WATCH( "defaultControllerData", controllerData_ );
			// MF_ACCESSORS( std::string, MainApp, controllerData ) );
	MF_WATCH( "defaultStandinEntity", standinEntity_ );
	MF_WATCH( "loginMD5Digest", *this,
			MF_ACCESSORS( std::string, MainApp, loginMD5Digest ) );
	MF_WATCH( "command/updateMovement", *this,
			MF_WRITE_ACCESSOR( std::string, MainApp, updateMovement ) );
	MF_WATCH( "command/runPython", *this,
			MF_WRITE_ACCESSOR( std::string, MainApp, runPython ) );

	Watcher::rootWatcher().addChild( "nub", Mercury::Nub::pWatcher(), &nub_ );

	BotsInterface::registerWithNub( nub_ );

	return true;
}


/**
 *	This method performs the main loop of the application.
 */
void MainApp::run()
{
	for (;;)
	{
		try
		{
			nub_.processContinuously();
			if (stop_) break;

			TRACE_MSG( "MainApp::run: "
				"processContinuously returned normally (looping back in)\n" );
		}
		catch (Mercury::NubException & ne)
		{
			WARNING_MSG( "MainApp::run: "
				"processContinuously returned unexpectedly (%s).\n",
				Mercury::reasonToString( (Mercury::Reason)ne.reason() ) );
			// TODO: Stop when we get REASON_NO_SUCH_PORT ... or at least get
			// rid of all the bots on that server if the exceptions persist
		}
	}

	TRACE_MSG( "MainApp::run: stopped due to user request\n" );
}


/**
 *	This method shuts down this application.
 */
void MainApp::shutDown()
{
	stop_ = true;
	nub_.breakProcessing();
}

/**
 *	This method adds another simulated client to this application.
 */
void MainApp::addBot()
{
	std::string bname = this->username_;
	std::string bpass = this->password_;
	if (this->randomName_)
	{
		char randName[10];
		bw_snprintf( randName, sizeof( randName ), "_%08x", rand() );
		bname.append( randName );
	}

	bots_.push_back( new ClientApp( nub_, bname, bpass, tag_ ) );
}


/**
 *	This method adds a number of simulated clients to this application.
 */
void MainApp::addBots( int num )
{
	if (num <= 0)
		return;

	for (int i = 0; i < num; ++i)
	{
		this->addBot();
	}
}

void MainApp::addBotsWithName( PyObjectPtr logInfoData )
{
	if (!logInfoData || logInfoData == Py_None)
	{
		PyErr_SetString( PyExc_TypeError,
			"Bots::addBotsWithName: empty log info. "
			"Argument must be list of tuple." );
		return;
	}
	if (!PyList_Check( logInfoData.getObject() ))
	{
		PyErr_SetString( PyExc_TypeError,
			"Bots::addBotsWithName: Argument must be list of tuple." );
		return;
	}
	Py_ssize_t listSize = PyList_Size( logInfoData.getObject() );
	for (Py_ssize_t i = 0; i < listSize; ++i)
	{
		PyObject * loginItem = PyList_GetItem( logInfoData.getObject(), i );
		if (!PyTuple_Check( loginItem ) || PyTuple_Size( loginItem ) != 2)
		{
			PyErr_Format( PyExc_TypeError,
				"Bots::addBotsWithName: Argument list item %zd must "
				"be tuple of two string.", i );
			return;
		}
		PyObject * nameInfo = PyTuple_GetItem( loginItem, 0 );
		PyObject * passwdInfo = PyTuple_GetItem( loginItem, 1 );
		if (!PyString_Check( nameInfo ) || !PyString_Check( passwdInfo ))
		{
			PyErr_Format( PyExc_TypeError,
				"Bots::addBotsWithName: Argument list item %zd must "
				"be tuple of two string.", i );
			return;
		}
		bots_.push_back( new ClientApp( nub_, 
			std::string( PyString_AsString( nameInfo ) ),
			std::string( PyString_AsString( passwdInfo ) ),
			tag_ ) );
	}
}

/**
 *	This method removes a number of simulated clients from this application.
 */
void MainApp::delBots( int num )
{
	while (num-- > 0 && !bots_.empty())
	{
		Bots::iterator iter = bots_.begin();
		if (iter == clientTickIndex_)
		{
			++clientTickIndex_;
		}
		Py_DECREF( iter->get() );
		bots_.pop_front();
	}
}


/**
 *	This method updates the movement controllers of all bots matching the input
 *	tag based on the current default values.
 *
 *	If the input tag is empty, all bots are changed.
 */
void MainApp::updateMovement( std::string tag )
{
	Bots::iterator iter = bots_.begin();

	while (iter != bots_.end())
	{
		if (tag.empty() || ((*iter)->tag() == tag))
		{
			bool ok = (*iter)->setMovementController( this->controllerType(),
											this->controllerData() );
			if (!ok)
			{
				PyErr_Print();
			}
		}
		++iter;
	}
}


/**
 *	This method runs the input string.
 */
void MainApp::runPython( std::string command )
{
	if (PyRun_SimpleString( command.c_str() ) != 0)
	{
		ERROR_MSG( "MainApp::runPython: Couldn't execute '%s'\n",
				command.c_str() );
		PyErr_Print();
	}
}

void MainApp::loginMD5Digest( std::string quoteDigest )
{
	if (!loginDigest_.unquote( quoteDigest ))
	{
		PyErr_SetString( PyExc_ValueError,
			"Bots::setLoginMD5Digest: Login MD5 digest in text format "
			"should be 32 character long" );
		PyErr_Print();
	}
}
/**
 *	This method deletes tagged entities.
 */
void MainApp::delTaggedEntities( std::string tag )
{
	Bots::iterator iter = bots_.begin();
	Bots condemnedBots; //Call destructors when going out of scope

	while (iter != bots_.end())
	{
		Bots::iterator oldIter = iter++;
		if ((*oldIter)->tag() == tag)
		{
			if (oldIter == clientTickIndex_)
			{
				++clientTickIndex_;
			}
			condemnedBots.push_back(*oldIter);
			Py_DECREF( oldIter->get() );
			bots_.erase( oldIter );
		}
	}
}


/*
 *	Override from TimerExpiryHandler.
 */
int MainApp::handleTimeout( Mercury::TimerID, void * )
{
	static bool inTick = false;

	if (inTick)
	{
		// This can occur because the tick method of ClientApp can
		// processPendingEvents
		WARNING_MSG( "MainApp::handleTimeout: Called recursively\n" );
		return 0;
	}

	inTick = true;

	// give Bots personality script a chance to handle
	Script::call( 
		PyObject_GetAttrString( this->getPersonalityModule().getObject(), "onTick" ),
		PyTuple_New( 0 ), "BWPersonality.onTick", true );

	static int remainder = 0;
	int numberToUpdate = (bots_.size() + remainder) / TICK_FRAGMENTS;
	remainder = (bots_.size() + remainder) % TICK_FRAGMENTS;

	localTime_ += MINI_TICK_PERIOD;

	while (numberToUpdate-- > 0 && !bots_.empty())
	{
		if (clientTickIndex_ == bots_.end())
		{
			clientTickIndex_ = bots_.begin();	
		}
		Bots::iterator iter = clientTickIndex_++;

		if (!(*iter)->tick( TICK_PERIOD ))
		{
			bots_.erase( iter );
		}
	}

	inTick = false;

	return 0;
}


/**
 *	Thie method returns personality module
 */
PyObjectPtr MainApp::getPersonalityModule() const
{
	std::string moduleName = BWConfig::get( "personality", "BWPersonality" );

	PyObjectPtr pModule(
		PyImport_ImportModule( (char*)moduleName.c_str() ),
		PyObjectPtr::STEAL_REFERENCE );

	if (!pModule)
	{
		PyObject * pErrType = PyErr_Occurred();

		if (pErrType == PyExc_ImportError)
		{
			INFO_MSG( "No personality module - '%s'\n", moduleName.c_str() );
			PyErr_Clear();
		}
		else
		{
			ERROR_MSG( "MainApp::getPersonalityModule: "
						"Failed to import personality module '%s'\n",
					moduleName.c_str() );
			PyErr_Print();
		}
	}

	return pModule;
}

namespace
{
typedef std::map< std::string, MovementFactory * > MovementFactories;
MovementFactories * s_pMovementFactories;
}

/**
 *	This method returns a default movement controller instance.
 */
MovementController * MainApp::createDefaultMovementController(
	float & speed, Vector3 & position )
{
	return this->createMovementController( speed, position,
										controllerType_, controllerData_ );
}


/**
 *	This method creates a movement controller corresponding to the input
 *	arguments.
 */
MovementController *
	MainApp::createMovementController( float & speed, Vector3 & position,
							   const std::string & controllerTypeIn,
							   const std::string & controllerData )
{
	std::string controllerType = controllerTypeIn;

	if (controllerType == "None") return NULL;

#ifdef unix
	uint soPos = controllerType.find( ".so:" );
   	if (soPos < controllerType.length())
	{
		std::string soName = controllerType.substr( 0, soPos+3 );
		static std::set<std::string> loadedLibs;
		if (loadedLibs.find( soName ) == loadedLibs.end())
		{
			loadedLibs.insert( soName );
			std::string soPath = "bots-extensions/"+soName;
			void * handle = dlopen( soPath.c_str(), RTLD_LAZY | RTLD_GLOBAL );
			if (handle == NULL)
			{
				ERROR_MSG( "MainApp::createMovementController: "
					"Failed to load dyn lib '%s' since %s\n",
					soName.c_str(), dlerror() );
			}
			else
			{
				INFO_MSG( "MainApp::createMovementController: "
					"Loaded dyn lib '%s'\n", soName.c_str() );
			}
		}

		controllerType = controllerType.substr( soPos+4 );
	}
#endif

	if (s_pMovementFactories != NULL)
	{
		MovementFactories::iterator iter =
				s_pMovementFactories->find( controllerType );

		if (iter != s_pMovementFactories->end())
		{
			return iter->second->create( controllerData, speed,
						position );
		}
	}

	PyErr_Format( PyExc_TypeError, "No such controller type '%s'",
		controllerType.c_str() );
	return NULL;
}


/**
 *	This static method registers a MovementFactory.
 */
void MainApp::addFactory( const std::string & name, MovementFactory & factory )
{
	if (s_pMovementFactories == NULL)
	{
		s_pMovementFactories = new MovementFactories;
	}

	(*s_pMovementFactories)[ name ] = &factory;
}


// -----------------------------------------------------------------------------
// Section: Script related
// -----------------------------------------------------------------------------

/**
 *	This method returns the client application with the input id.
 */
ClientApp * MainApp::findApp( EntityID id ) const
{
	// This is inefficient. Could look at making a map of these but it should
	// not be used this way very often.(??)
	Bots::const_iterator iter = bots_.begin();

	while (iter != bots_.end())
	{
		ClientApp * pApp = iter->get();

		// When deleting multiple bots, there may be holes in the bots_ vector
		// temporarily, so jump over any NULL pointers we encounter
		if (pApp && pApp->id() == id)
		{
			Py_INCREF( pApp );
			return pApp;
		}

		++iter;
	}

	return NULL;
}


/**
 *	This method populates a list with the IDs of available apps.
 */
void MainApp::appsKeys( PyObject * pList ) const
{
	Bots::const_iterator iter = bots_.begin();

	while (iter != bots_.end())
	{
		PyObject * pInt = PyInt_FromLong( (*iter)->id() );
		PyList_Append( pList, pInt );
		Py_DECREF( pInt );

		++iter;
	}
}


/**
 *	This method populates a list with available apps.
 */
void MainApp::appsValues( PyObject * pList ) const
{
	Bots::const_iterator iter = bots_.begin();

	while (iter != bots_.end())
	{
		PyList_Append( pList, iter->get() );

		++iter;
	}
}


/**
 *	This method populates a list with id, value pairs of the available apps.
 */
void MainApp::appsItems( PyObject * pList ) const
{
	Bots::const_iterator iter = bots_.begin();

	while (iter != bots_.end())
	{
		PyObject * pTuple = PyTuple_New( 2 );
		PyTuple_SetItem( pTuple, 0, PyInt_FromLong( (*iter)->id() ) );
		Py_INCREF( *iter );
		PyTuple_SetItem( pTuple, 1, iter->get() );
		PyList_Append( pList, pTuple );
		Py_DECREF( pTuple );

		++iter;
	}
}


// -----------------------------------------------------------------------------
// Section: BigWorld script functions
// -----------------------------------------------------------------------------

namespace
{
void addBots( int count )
{
	MainApp::instance().addBots( count );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, addBots, ARG( int, END ), BigWorld )

void addBotsWithName( PyObjectPtr logInfoData )
{
	// validate input log info. it should be a list of duple of
	// user name and password.
	MainApp::instance().addBotsWithName( logInfoData );
	if (PyErr_Occurred())
	{
		PyErr_Print();
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, addBotsWithName, ARG( PyObjectPtr, END ), BigWorld )


void setLoginMD5Digest( std::string quoteDigest )
{
	MainApp::instance().loginMD5Digest( quoteDigest );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, setLoginMD5Digest, ARG( std::string, END ), BigWorld )

void delBots( int count )
{
	MainApp::instance().delBots( count );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, delBots, ARG( int, END ), BigWorld )

#define DEFAULT_ACCESSOR( N1, N2 )											\
std::string getDefault##N2()												\
{																			\
	return MainApp::instance().N1();										\
}																			\
PY_AUTO_MODULE_FUNCTION( RETDATA, getDefault##N2, END, BigWorld )			\
																			\
void setDefault##N2( const std::string & v )								\
{																			\
	MainApp::instance().N1( v );											\
}																			\
																			\
PY_AUTO_MODULE_FUNCTION( RETVOID,											\
		setDefault##N2, ARG( std::string, END ), BigWorld )					\

DEFAULT_ACCESSOR( serverName, Server )
DEFAULT_ACCESSOR( username, Username )
DEFAULT_ACCESSOR( password, Password )
DEFAULT_ACCESSOR( tag, Tag )
DEFAULT_ACCESSOR( controllerType, ControllerType )
DEFAULT_ACCESSOR( controllerData, ControllerData )

class BotAdder : public Mercury::TimerExpiryHandler
{
public:
	BotAdder( int total, float period, int perTick ) :
		remaining_( total ),
		perTick_( perTick )
	{
		MainApp::instance().nub().registerTimer( int(period * 1000000), this );
	}

	int handleTimeout( Mercury::TimerID id, void * )
	{
		MainApp::instance().addBots( std::min( remaining_, perTick_ ) );
		remaining_ -= perTick_;

		if (remaining_ <= 0)
		{
			MainApp::instance().nub().cancelTimer( id );
			delete this;
		}

		return 1;
	}

private:
	int remaining_;
	int perTick_;
};

void addBotsSlowly( int count, float period, int perTick )
{
	new BotAdder( count, period, perTick );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, addBotsSlowly,
		ARG( int, OPTARG( float, 1.f, OPTARG( int, 1, END ) ) ), BigWorld )


} // namespace

#define DEFINE_SERVER_HERE
#include "bots_interface.hpp"

// main_app.cpp
