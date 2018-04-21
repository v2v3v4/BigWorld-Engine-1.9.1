/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "bwlog.hpp"
#include "network/machine_guard.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/timestamp.hpp"

#include <libgen.h>

DECLARE_DEBUG_COMPONENT( 0 );

/*
 * log version 0: binary logging but using monolithic user entry and args files
 * log version 1: user entries file are segmented but args still monolithic
 * log version 2: argument blob files are segmented and componentnames file
 *                is no longer a FileStream (text only now)
 * log version 3: 'componentnames' file is now named 'component_names'
 * log version 4: first entry offset recorded with each component, appID tracked
 */

// This constant represents the format version of the log directory and
// contained files. It is independant of MESSAGE_LOGGER_VERSION as defined
// in 'src/lib/network/logger_message_forwarder.hpp' which represents the
// protocol version of messages sent between components and the logger.
const int BWLog::LOG_FORMAT_VERSION = 4;

// -----------------------------------------------------------------------------
// Section: PyObject stuff
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( BWLog )

PY_BEGIN_METHODS( BWLog )

 	PY_METHOD( fetch )

 	PY_METHOD( getComponentNames )

	PY_METHOD( getHostnames )

 	PY_METHOD( getUsers )

	PY_METHOD( getUserLog )

	PY_METHOD( getStrings )

PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( BWLog )

	PY_ATTRIBUTE( root )

PY_END_ATTRIBUTES()

PyObject* BWLog::pyNew( PyObject *args )
{
	const char *dirname = NULL, *mode = "r";

	// Enable error messages from a python script to go to syslog
	DebugMsgHelper::shouldWriteToSyslog( true );

	// We don't support writing via Python
	if (!PyArg_ParseTuple( args, "|s", &dirname ))
		return NULL;

	BWLogPtr pLog( new BWLog(), BWLogPtr::STEAL_REFERENCE );
	bool status;

	if (dirname == NULL)
	{
		char buf[ 1024 ];
		if (snprintf(buf, 1023, "%s/current", pLog->root()) <= 0)
		{
			status = false;
		}
		else
		{
			status = pLog->init( buf, mode );
		}
	}
	else
		status = pLog->init( dirname, mode );

	if (!status)
	{
		PyErr_Format( PyExc_IOError,
			"Log init failed in %s, see errors in logs",
			(dirname) ? dirname : pLog->root() );
		return NULL;
	}

	Py_INCREF( pLog.getObject() );
	return pLog.getObject();
}

// -----------------------------------------------------------------------------
// Section: BWLog
// -----------------------------------------------------------------------------

BWLog::BWLog() :
	PyObjectPlus( &BWLog::s_type_ ),
	writeToStdout_( false ),
	writeTextLogs_( false )
{}

BWLog::~BWLog()
{
	if (int( pid_ ) == mf_getpid())
	{
		// Clean up the PID file so someone else can write here
		if (unlink( pid_.filename() ))
		{
			ERROR_MSG( "BWLog::~BWLog(): "
				"Couldn't clean up PID file (%s): %s\n",
				pid_.filename(), strerror( errno ) );
		}

		// Clean up the active_files
		if (unlink( activeFiles_.filename() ))
		{
			ERROR_MSG( "BWLog::~BWLog(): "
				"Couldn't clean up active_files: %s\n",
				strerror( errno ) );
		}
	}
}

/**
 *  @param root  The path which logs should be written to and read from. NULL
 *               indicates usages of the default logging path.
 *
 *  @param mode  The mode in which to open logs. "r" for reading, "a+" for writing.
 *
 *  @returns     @c true if successfully initialised, @c false on error.
 */
bool BWLog::init( const char *root, const char *mode, const char *config )
{
	// Read config in append mode only, since the Python will always pass a
	// 'root' parameter in read mode
	if (!strcmp( mode, "a+" ) && !this->readConfig( config ))
	{
		ERROR_MSG( "BWLog::init: Failed to read config file\n" );
		return false;
	}

	// Only use logdir from the config file if none has been provided
	if (root == NULL)
		root = config_.logDir_.c_str();

	// If the path is given relatively, convert it to an absolute path
	if (root[0] != '/')
	{
		char buf[ 512 ];
		getcwd( buf, sizeof( buf ) );
		root_ = buf;
		root_ += '/';
		root_ += root;
	}
	else
		root_ = root;

	mode_ = mode;

	if (mode_ != "r" && mode_ != "a+")
	{
		ERROR_MSG( "Unable to open logs in mode '%s', try 'r' or 'a+'\n",
			mode );
		return false;
	}

	// Make sure the root directory has the access we want
	if ((mode_ == "r"  && !this->isAccessible( root_.c_str() )) ||
		(mode_ == "a+" && !this->softMkDir( root_.c_str() )))
	{
		ERROR_MSG( "Root logdir (%s) not accessible in mode '%s'\n",
			root_.c_str(), mode_.c_str() );
		return false;
	}

	// Make sure another logger isn't already logging to this directory
	if (mode_ == "a+" &&
		!pid_.init( pid_.join( root, "pid" ), mode, mf_getpid() ))
	{
		ERROR_MSG( "BWLog::init: Another logger seems to be writing to %s\n",
			root );
		return false;
	}

	if (!version_.init( version_.join( root, "version" ),
			mode, BWLog::LOG_FORMAT_VERSION ))
	{
		ERROR_MSG( "Couldn't init version file\n" );
		return false;
	}

	if (!componentNames_.init( root, mode ))
	{
		ERROR_MSG( "Couldn't init component names mapping\n" );
		return false;
	}


	if (!hostnames_.init( root, mode ))
	{
		ERROR_MSG( "Couldn't init hostnames mapping\n" );
		return false;
	}

	if (!strings_.init( root, mode ))
	{
		ERROR_MSG( "Couldn't init strings mapping\n" );
		return false;
	}

	// Load up all existing UserLogs when writing.  This isn't too expensive in
	// terms of file handles as we only keep open the most recent entries/args
	// files when writing
	DIR *rootdir = opendir( root );
	struct dirent *dirent;
	while ((dirent = readdir( rootdir )) != NULL)
	{
		char fname[ 1024 ];
		struct stat statinfo;

		bw_snprintf( fname, sizeof( fname ), "%s/%s/uid",
			root, dirent->d_name );
		if (stat( fname, &statinfo ))
			continue;

		IntFile uidfile;
		if (!uidfile.init( fname, mode ) || (int)uidfile == -1)
		{
			ERROR_MSG( "BWLog::init: Skipping dir with invalid uid file %s\n",
				fname );
			continue;
		}

		uint16 uid = (int)uidfile;
		std::string username = dirent->d_name;

		// In write mode we actually load up the UserLog, since write mode
		// UserLogs only keep a pair of file handles open.
		if (mode_ == "a+")
		{
			UserLogPtr pUserLog( new UserLog( *this, uid, username, mode ),
				UserLogPtr::STEAL_REFERENCE );

			if (pUserLog->good())
				userLogs_[ uid ] = pUserLog;
		}

		// In read mode we just make a record of the uid->username mapping
		else
		{
			usernames_[ uid ] = username;
		}
	}
	closedir( rootdir );

	return true;
}

bool BWLog::readConfig( const char *config )
{
	std::string confFile;
	struct stat statinfo;

	// Provided config takes precedence
	if (config)
	{
		confFile = config;
	}

	// Next use global toolsdir and look for config in std location
	else if (stat( "/etc/bigworld.conf", &statinfo ) == 0)
	{
		BigWorldConfig bwconf;

		if (!bwconf.init( "/etc/bigworld.conf", "r" ))
		{
			ERROR_MSG( "Error whilst reading config from /etc/bigworld.conf\n" );
			return false;
		}
		else
		{
			confFile = bwconf.toolsDir_ +
				"/message_logger/message_logger.conf";

			if (stat( confFile.c_str(), &statinfo ))
			{
				ERROR_MSG(
					"Config file doesn't exist in std location '%s'\n",
					confFile.c_str() );
				return false;
			}
		}
	}

	// Next is config in current directory
	else if (stat( "./message_logger.conf", &statinfo ) == 0)
	{
		confFile = "./message_logger.conf";
	}

	else
	{
		ERROR_MSG( "BWLog::readConfig: No valid configuration file found\n" );
		return false;
	}

	// Read config
	if (config_.init( confFile.c_str(), "r" ))
	{
		INFO_MSG( "BWLog::init: Read config from %s\n", confFile.c_str() );
		return true;
	}
	else
	{
		ERROR_MSG( "Couldn't read config file %s\n", confFile.c_str() );
		return false;
	}
}

bool BWLog::resume()
{
	if (strings_.dirty())
		if (!strings_.refresh())
			return false;

	if (hostnames_.dirty())
		if (!hostnames_.refresh())
			return false;

	if (componentNames_.dirty())
		if (!componentNames_.refresh())
			return false;

	return true;
}

bool BWLog::delComponent( const Mercury::Address &addr )
{
	// Search through all the UserLogs and remove that component
	for (UserLogs::iterator it = userLogs_.begin(); it != userLogs_.end(); it++)
		if (it->second->components_.erase( addr ))
			return true;

	return false;
}

bool BWLog::setAppID( const Mercury::Address &addr, int id )
{
	Component *pComponent = this->getComponent( addr );

	if (pComponent == NULL)
	{
		ERROR_MSG( "BWLog::setAppID: "
			"Can't set app ID for unknown address %s\n", (char*)addr );
		return false;
	}

	return pComponent->setAppID( id );
}

bool BWLog::addEntry( const LoggerComponentMessage &msg,
	const Mercury::Address &addr, MemoryIStream &is )
{
	uint16 uid = msg.uid_;

	// Stream off the header and the format string
	LoggerMessageHeader header;
	std::string format;
	int len = is.remainingLength();
	is >> header >> format;

	if (is.error())
	{
		ERROR_MSG( "BWLog::addEntry: Log message from %s was too short "
			"(%d bytes)\n", (char*)addr, len );
		return false;
	}

	// Get the format string handler
	LoggingStringHandler *pHandler = strings_.resolve( format );
	if (pHandler == NULL)
	{
		ERROR_MSG( "BWLog::addEntry: Couldn't add fmt %s to mapping\n",
			format.c_str() );
		return false;
	}

	// Cache the IP address -> Hostname mapping if neccessary
	if (!hostnames_.resolve( addr.ip ))
	{
		ERROR_MSG( "BWLog::addEntry: Error resolving %s\n",
			(char*)addr );
		return false;
	}

	// Get the user log segment
	UserLogPtr pUserLog = this->getUserLog( uid );
	if (pUserLog == NULL)
	{
		std::string result;
		Mercury::Reason reason;

		if ((reason = this->resolveUid( uid, addr.ip, result )) ==
			Mercury::REASON_SUCCESS)
		{
			pUserLog = UserLogPtr(
				new UserLog( *this, uid, result, mode_.c_str() ),
				UserLogPtr::STEAL_REFERENCE );

			if (!pUserLog->good())
			{
				ERROR_MSG( "BWLog::addEntry: "
					"UserLog for %s failed to init\n", result.c_str() );
				return false;
			}

			userLogs_[ uid ] = pUserLog;
		}
		else
		{
			ERROR_MSG( "BWLog::addEntry: Couldn't resolve uid %d (%s). "
				"UserLog not started.\n",
				uid, Mercury::reasonToString( reason ) );
			return false;
		}
	}

	Component &component = *pUserLog->components_.resolve( msg, addr );

	struct Entry entry;
	struct timeval tv;
	gettimeofday( &tv, NULL );
	entry.time_.secs_ = tv.tv_sec;
	entry.time_.msecs_ = tv.tv_usec/1000;
	entry.componentId_ = component.id_;
	entry.messagePriority_ = header.messagePriority_;
	entry.stringOffset_ = pHandler->fileOffset();

	// Dump output to stdout if required
	if (writeToStdout_)
	{
		std::cout << pUserLog->format( component, entry, *pHandler, is );
	}

	if (!pUserLog->addEntry( component, entry, *pHandler, is ))
	{
		ERROR_MSG( "BWLog::addEntry: Failed to add entry to user log\n" );
		return false;
	}

	return true;
}


/**
 *  Terminates all current log segments.
 */
bool BWLog::roll()
{
	INFO_MSG( "Rolling logs\n" );

	UserLogs::iterator iter = userLogs_.begin();

	while (iter != userLogs_.end())
	{
		UserLog &userLog = *iter->second;

		if (!userLog.segments_.empty())
		{
			delete userLog.segments_.back();
			userLog.segments_.pop_back();
		}

		// Since the userlog now has no active segments, drop the object
		// entirely since it's likely we will be mlrm'ing around this time which
		// could blow away this user's directory.  If that happens, then a new
		// UserLog object must be created when the next log message arrives.
		UserLogs::iterator oldIter = iter++;
		userLogs_.erase( oldIter );
	}

	return activeFiles_.write( *this );
}


PyObject* BWLog::py_getComponentNames( PyObject *args )
{
	PyObject *results = PyList_New( 0 );
	for (ComponentNames::iterator it = componentNames_.begin();
		 it != componentNames_.end(); ++it)
	{
		PyList_Append( results,	PyString_FromString( it->c_str() ) );
	}
	return results;
}


PyObject* BWLog::py_getHostnames( PyObject *args )
{
	PyObject *dict = PyDict_New();

	for (Hostnames::iterator it = hostnames_.begin();
		 it != hostnames_.end(); ++it)
	{
		PyDict_SetItemString( dict,
			inet_ntoa( (in_addr&)it->first ),
			PyString_FromString( it->second.c_str() ) );
	}

	return dict;
}


PyObject* BWLog::py_getStrings( PyObject *args )
{
	PyObject *list = PyList_New( strings_.formatMap_.size() );
	int i = 0;

	for (Strings::FormatMap::iterator it = strings_.formatMap_.begin();
		 it != strings_.formatMap_.end(); ++it, ++i)
	{
		PyList_SET_ITEM( list, i, PyString_FromString( it->first.c_str() ) );
	}

	PyList_Sort( list );

	return list;
}


PyObject* BWLog::py_getUsers( PyObject *args )
{
	PyObject *dict = PyDict_New();

	for (Usernames::iterator it = usernames_.begin();
		 it != usernames_.end(); ++it)
	{
		PyDict_SetItemString( dict,
			it->second.c_str(),
			PyInt_FromLong( it->first ) );
	}

	return dict;
}

PyObject* BWLog::py_getUserLog( PyObject *args )
{
	int uid;
	if (!PyArg_ParseTuple( args, "i", &uid ))
		return NULL;

	UserLogPtr pUserLog = this->getUserLog( uid );
	if (pUserLog == NULL)
	{
		PyErr_Format( PyExc_KeyError,
			"No entries for uid %d in this log", uid );
		return NULL;
	}

	PyObject *ret = pUserLog.getObject();
	Py_INCREF( ret );
	return ret;
}

PyObject* BWLog::py_fetch( PyObject *args, PyObject *kwargs )
{
	QueryParams *pParams = new QueryParams( args, kwargs, *this );
	if (!pParams->good())
	{
		delete pParams;
		return NULL;
	}

	if (pParams->start_ > pParams->end_)
	{
		PyErr_Format( PyExc_RuntimeError, "BWLog::fetchEntries: "
			"start time (%d.%d) is greater than end time (%d.%d)",
			(int)pParams->start_.secs_, pParams->start_.msecs_,
			(int)pParams->end_.secs_, pParams->end_.msecs_ );
		delete pParams;
		return NULL;
	}

	UserLogPtr pUserLog = this->getUserLog( pParams->uid_ );
	if (pUserLog == NULL)
	{
		PyErr_Format( PyExc_RuntimeError,
			"BWLog::fetchEntries: No user log for uid %d\n",
			pParams->uid_ );
		delete pParams;
		return NULL;
	}

	return new Query( this, pParams, pUserLog.getObject() );
}

namespace
{
class UsernameHandler : public MachineGuardMessage::ReplyHandler
{
public:
	virtual ~UsernameHandler() {}
	virtual bool onUserMessage( UserMessage &um, uint32 addr )
	{
		if (um.uid_ != UserMessage::UID_NOT_FOUND)
			username_ = um.username_;

		return false;
	}
	std::string username_;
};
}

Mercury::Reason BWLog::resolveUid( uint16 uid, uint32 addr, std::string &result )
{
	int reason;
	uint32 queryaddr = addr;
	UsernameHandler handler;
	UserMessage um;
	um.uid_ = uid;
	um.param_ = um.PARAM_USE_UID;

	while ((reason = um.sendAndRecv( 0, queryaddr, &handler )) !=
		Mercury::REASON_SUCCESS)
	{
		if (queryaddr == BROADCAST)
			return (Mercury::Reason&)reason;

		ERROR_MSG( "BWLog::resolveUid: UserMessage query to %s for uid %d "
					"failed: %s\n",
			inet_ntoa( (in_addr&)queryaddr ), uid,
			Mercury::reasonToString( (Mercury::Reason&)reason ) );
		INFO_MSG( "BWLog::resolveUid: Retrying UID query for %d as "
					"broadcast.\n",	uid );
		queryaddr = BROADCAST;
	}

	if (!handler.username_.empty())
	{
		result = handler.username_;
	}

	// If we couldn't resolve the username, just use his UID as his username
	else
	{
		WARNING_MSG( "BWLog::resolveUid: "
			"Couldn't resolve UID %hu, using UID as username\n", uid );

		char buf[ 128 ];
		bw_snprintf( buf, sizeof( buf ), "%hu", uid );
		result = buf;
	}

	return (Mercury::Reason&)reason;
}

bool BWLog::softMkDir( const char *path ) const
{
	struct stat statinfo;
	if (stat( path, &statinfo ))
	{
		if (mkdir( path, 0777 ))
		{
			ERROR_MSG( "BWLog::softMkDir: Couldn't make log directory '%s': %s\n",
				path, strerror( errno ) );
			return false;
		}
	}
	else if (!S_ISDIR( statinfo.st_mode ))
	{
		ERROR_MSG( "BWLog::softMkDir: %s already exists and is not a directory\n",
			path );
		return false;
	}
	else if (!(statinfo.st_mode & S_IRWXU))
	{
		ERROR_MSG( "BWLog::softMkDir: Insufficient permissions for %s (%o)\n",
			path, statinfo.st_mode );
		return false;
	}
	else if (statinfo.st_uid != geteuid())
	{
		ERROR_MSG( "BWLog::softMkDir: %s is not owned by me (uid:%d)\n",
			path, geteuid() );
		return false;
	}

	return true;
}

bool BWLog::isAccessible( const char *path ) const
{
	struct stat statinfo;
	if (stat( path, &statinfo ))
	{
		ERROR_MSG( "BWLog::isAccessible: Directory %s doesn't exist\n", path );
		return false;
	}

	if (!S_ISDIR( statinfo.st_mode ))
	{
		ERROR_MSG( "BWLog::isAccessible: "
			"%s already exists and is not a directory\n", path );
		return false;
	}

	if (!(statinfo.st_mode & S_IROTH))
	{
		ERROR_MSG( "BWLog::isAccessible: %s is not readable\n", path );
		return false;
	}

	return true;
}

PyObject * BWLog::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

BWLog::Component* BWLog::getComponent( const Mercury::Address &addr )
{
	Component *pComponent;

	for (UserLogs::iterator it = userLogs_.begin(); it != userLogs_.end(); ++it)
		if ((pComponent = it->second->components_.resolve( addr )) != NULL)
			return pComponent;

	return NULL;
}

/**
 *  Returns the UserLog object for this uid.  In write mode this should always
 *  be in memory already.  In read mode it may not be, so we might have to load
 *  it up on demand.
 */
BWLog::UserLogPtr BWLog::getUserLog( uint16 uid )
{
	UserLogs::iterator it = userLogs_.find( uid );
	if (it != userLogs_.end())
		return it->second;

	if (mode_ == "r")
	{
		Usernames::iterator uit = usernames_.find( uid );
		if (uit == usernames_.end())
			return NULL;

		UserLogPtr pUserLog(
			new UserLog( *this, uid, uit->second, mode_.c_str() ),
			UserLogPtr::STEAL_REFERENCE );

		if (pUserLog->good())
		{
			userLogs_[ uid ] = pUserLog;
			return pUserLog;
		}
	}

	return NULL;
}

// -----------------------------------------------------------------------------
// Section: BWLog::EntryAddress
// -----------------------------------------------------------------------------

void BWLog::EntryAddress::write( BinaryOStream &os ) const
{
	os << suffix_ << index_;
}


void BWLog::EntryAddress::read( BinaryIStream &is )
{
	is >> suffix_ >> index_;
}


void BWLog::EntryAddress::parseTuple( PyObject *tuple )
{
	suffix_ = PyString_AsString( PyTuple_GetItem( tuple, 0 ) );
	index_ = PyInt_AsLong( PyTuple_GetItem( tuple, 1 ) );
}


bool BWLog::EntryAddress::operator<( const EntryAddress &other ) const
{
	return suffix_ < other.suffix_ ||
		(suffix_ == other.suffix_ && index_ < other.index_);
}


// -----------------------------------------------------------------------------
// Section: BWLog::Component
// -----------------------------------------------------------------------------

BWLog::Component::Component( Components &components ) :
	components_( components )
{}

BWLog::Component::Component( Components &components,
	const Mercury::Address &addr, const LoggerComponentMessage &msg,
	int ttypeid ) :
	components_( components ),
	addr_( addr ),
	id_( components.getId() ),
	appid_( 0 ),
	typeid_( ttypeid ),
	msg_( msg ),
	fileOffset_( -1 )
{}

/**
 *  This method is not const as you'd probably expect because it needs to set
 *  this object's fileOffset_ field prior to writing.
 */
void BWLog::Component::write( FileStream &os )
{
	fileOffset_ = os.tell();
	os << addr_ << id_ << appid_ << typeid_;
	msg_.write( os );
	firstEntry_.write( os );
	os.commit();
}

void BWLog::Component::read( FileStream &is )
{
	fileOffset_ = is.tell();
	is >> addr_ >> id_ >> appid_ >> typeid_;
	msg_.read( is );
	firstEntry_.read( is );
}

bool BWLog::Component::setAppID( int id )
{
	appid_ = id;

	// If this component has already been written to disk (which is almost
	// certain) then we need to overwrite the appid field in the components file
	if (this->written())
	{
		FileStream file( components_.filename(), "r+" );

		// Seek to the exact offset of the app id
		file.seek( fileOffset_ );
		file >> addr_ >> id_;

		// Overwrite old ID
		file << appid_;
		file.commit();
	}

	return true;
}

std::string BWLog::Component::str() const
{
	char buf[ 256 ];
	bw_snprintf( buf, sizeof( buf ), "%s%02d (id:%d) %s",
		msg_.componentName_.c_str(), appid_, id_, (char*)addr_ );
	return std::string( buf );
}

// -----------------------------------------------------------------------------
// Section: BWLog::Segment
// -----------------------------------------------------------------------------

BWLog::Segment::Segment( UserLog &userLog,
	const char *mode, const char *suffix ) :
	userLog_( userLog ),
	good_( true ),
	mode_( mode ),
	pText_( NULL )
{
	char buf[ 1024 ];

	// Generate a suffix if none provided
	if (suffix == NULL)
	{
		time_t now = time( NULL );
		struct tm *pTime = localtime( &now );
		strftime( buf, sizeof( buf ) - 1, "%Y-%m-%d-%T", pTime );
		suffix_ = buf;
	}
	else
		suffix_ = suffix;

	bw_snprintf( buf, sizeof( buf ), "%s/entries.%s",
		userLog_.path_.c_str(), suffix_.c_str() );
	pEntries_ = new FileStream( buf, mode );
	if (!pEntries_->good())
	{
		ERROR_MSG( "BWLog::Segment::init: "
			"Couldn't open entries file %s in mode %s: %s\n",
			buf, mode, pEntries_->strerror() );
		good_ = false;
		return;
	}

	bw_snprintf( buf, sizeof( buf ), "%s/args.%s",
		userLog_.path_.c_str(), suffix_.c_str() );
	pArgs_ = new FileStream( buf, mode );
	if (!pArgs_->good())
	{
		ERROR_MSG( "BWLog::Segment::init: "
			"Couldn't open args file %s in mode %s: %s\n",
			buf, mode, pArgs_->strerror() );
		good_ = false;
		return;
	}

	if (userLog_.log_.writeTextLogs_)
	{
		bw_snprintf( buf, sizeof( buf ), "%s/text.%s",
			userLog_.path_.c_str(), suffix_.c_str() );
		pText_ = fopen( buf, mode );
		if (pText_ == NULL)
		{
			ERROR_MSG( "BWLog::Segment::init: "
				"Couldn't open text file %s in mode %s: %s\n",
				buf, mode, strerror( errno ) );
			good_ = false;
			return;
		}
	}

	this->calculateLengths();
}

BWLog::Segment::~Segment()
{
	if (pEntries_)
		delete pEntries_;

	if (pArgs_)
		delete pArgs_;

	if (pText_)
		fclose( pText_ );
}

void BWLog::Segment::calculateLengths()
{
	nEntries_ = pEntries_->length() / sizeof( Entry );
	argsSize_ = pArgs_->length();

	Entry entry;

	// Work out current start and end times
	if (nEntries_ > 0)
	{
		this->readEntry( 0, entry );
		start_ = entry.time_;
		this->readEntry( nEntries_ - 1, entry );
		end_ = entry.time_;
	}
}

bool BWLog::Segment::full() const
{
	return int( nEntries_ * sizeof( Entry ) ) + argsSize_ >=
		(userLog_.log_.config_.segmentSize_);
}

/**
 *  Returns true if this segment's underlying files have been modified since the
 *  last call to calculateLengths().
 */
bool BWLog::Segment::dirty() const
{
	return int( nEntries_ * sizeof( Entry ) ) < pEntries_->length();
}

/**
 *  Add an entry to this segment.
 */
bool BWLog::Segment::addEntry( Component &component, Entry &entry,
	LoggingStringHandler &handler, MemoryIStream &is )
{
	// Dump text output if necessary
	if (pText_ != NULL)
	{
		fputs( userLog_.format( component, entry, handler, is, true ), pText_ );
		fflush( pText_ );
	}

	entry.argsOffset_ = pArgs_->length();

	LoggingStringHandler::LogWritingParser parser( *pArgs_ );
	if (!handler.streamToLog( parser, is ))
	{
		ERROR_MSG( "BWLog::Segment::addEntry: "
			"Error whilst destreaming args\n" );
		return false;
	}

	argsSize_ = pArgs_->length();
	entry.argsLen_ = argsSize_ - entry.argsOffset_;

	// If this is the component's first log entry, we need to write the
	// component to disk as well.
	if (!component.written())
	{
		component.firstEntry_.suffix_ = suffix_;
		component.firstEntry_.index_ = nEntries_;
 		userLog_.components_.write( component );
		if (!component.written())
		{
			ERROR_MSG( "BWLog::UserLog::addEntry: "
				"Failed to write %s to components file: %s\n",
				component.str().c_str(),
				userLog_.components_.pFile()->strerror() );
		}
	}

	*pEntries_ << entry;
	pEntries_->commit();

	if (nEntries_ == 0)
		start_ = entry.time_;
	end_ = entry.time_;
	nEntries_++;

	return true;
}

bool BWLog::Segment::readEntry( int n, Entry &entry )
{
	pEntries_->seek( n * sizeof( Entry ) );
	*pEntries_ >> entry;
	if (pEntries_->error())
	{
		ERROR_MSG( "BWLog::Segment::readEntry: Failed to read entry: %s\n",
			pEntries_->strerror() );
		return false;
	}
	else
		return true;
}

/**
 *  Returns the entry # of the smallest time >= time (with direction == 1) or
 *  greatest time <= time (with direction == -1) or -1 if none found.
 */
int BWLog::Segment::find( LogTime &time, int direction )
{
	// Early return if time is outside the range of this segment (which happens
	// often when searching from the beginning or to the end of a log).
	if (direction == 1 && time <= start_)
		return 0;
	if (direction == -1 && time >= end_)
		return nEntries_-1;

	// Now do binary search
	int left = 0, right = nEntries_ - 1, mid;
	LogTime midtime;

	while (1)
	{
		mid = direction == 1 ? (left+right)/2 : (left+right+1)/2;
		pEntries_->seek( mid * sizeof( Entry ) );
		*pEntries_ >> midtime;

		if (left >= right)
			break;

		if (direction > 0)
		{
			if (time <= midtime)
			{
				right = mid;
			}
			else
			{
				left = mid+1;
			}
		}
		else
		{
			if (time < midtime)
			{
				right = mid-1;
			}
			else
			{
				left = mid;
			}
		}
	}

	if ((direction == 1 && time <= midtime) ||
		(direction == -1 && midtime <= time))
	{
		return mid;
	}
	else
		return -1;
}

// -----------------------------------------------------------------------------
// Section: BWLog::Range::iterator
// -----------------------------------------------------------------------------

/**
 *  This iterator class is intended to remove some of the annoying (and error
 *  prone) handling of BWLog::Range::direction_ from this module's code.
 *  As far as this class is concerned, the positive direction is always towards
 *  the end of the search, whether the search is running forwards or backwards.
 */
BWLog::Range::iterator::iterator( Range &range, int segmentNum, int entryNum,
	int metaOffset ) :
	pRange_( &range ),
	segmentNum_( segmentNum ),
	entryNum_( entryNum ),
	metaOffset_( metaOffset )
{}

/**
 *  'Goodness' is tied into the segmentNum_ field.  We use the entryNum_ field
 *  for saying that an out of range iterator is out of range to the left or
 *  right.
 */
bool BWLog::Range::iterator::good() const
{
	return segmentNum_ != -1 && entryNum_ != -1;
}


bool BWLog::Range::iterator::operator<( const iterator &other ) const
{
	return (*this) - other < 0;
}


bool BWLog::Range::iterator::operator<=( const iterator &other ) const
{
	return (*this < other) || (*this == other);
}


bool BWLog::Range::iterator::operator==( const iterator &other ) const
{
	if (pRange_ != other.pRange_)
	{
		ERROR_MSG( "BWLog::Range::iterator::operator<:"
			"Trying to compare iterators from two different ranges!\n" );
		return false;
	}

	return segmentNum_ == other.segmentNum_ &&
		entryNum_ == other.entryNum_ &&
		metaOffset_ == other.metaOffset_;
}


BWLog::Range::iterator& BWLog::Range::iterator::operator++()
{
	this->step( pRange_->direction_ );
	return *this;
}


BWLog::Range::iterator& BWLog::Range::iterator::operator--()
{
	this->step( -pRange_->direction_ );
	return *this;
}


/**
 *  Helper method for operator++ and operator--.
 */
void BWLog::Range::iterator::step( int direction )
{
	int d = direction;

	// If we've got a metaoffset and it's in the opposite direction, negate it
	if (metaOffset_ == -d)
	{
		metaOffset_ = 0;
		return;
	}

	entryNum_ += d;

	// If we've gone off the end of a segment, try to select the next one
	if (entryNum_ < 0 || entryNum_ >= this->segment().nEntries_)
	{
		segmentNum_ += d;

		// If we go off the end of the log, revert fields
		if (segmentNum_ < 0 ||
			segmentNum_ >= (int)pRange_->userLog_.segments_.size())
		{
			entryNum_ -= d;
			segmentNum_ -= d;

			// If we've hit the end of the search, set metaOffset in case we
			// resume() at some later time.
			if (d == pRange_->direction_)
			{
				metaOffset_ = d;
				return;
			}

			// Resuming a query never alters the beginning of the range, so
			// there's not much point doing anything with metaOffset here.
			else
			{
				return;
			}
		}

		// Not off the end of the log, select next segment
		else
		{
			entryNum_ = d == FORWARDS ? 0 : this->segment().nEntries_ - 1;
		}
	}

	metaOffset_ = 0;
}


/**
 *  This returns the offset (in entries) between this iterator and another
 *  iterator.
 */
int BWLog::Range::iterator::operator-( const iterator &other ) const
{
	int d = pRange_->direction_;
	if (segmentNum_ == other.segmentNum_)
	{
		return d * (entryNum_ - other.entryNum_	+
			metaOffset_ - other.metaOffset_);
	}
	else
	{
		// Count first and last segments
		int count = segmentNum_ > other.segmentNum_ ?
			entryNum_ + other.segment().nEntries_ - other.entryNum_ :
			this->segment().nEntries_ - entryNum_ + other.entryNum_;

		// Intervening segments
		for (int i = d == FORWARDS ?
				 other.segmentNum_ + 1 : other.segmentNum_ - 1;
			 d == FORWARDS ? i < segmentNum_ : i > segmentNum_;
			 i += d)
		{
			count += pRange_->userLog_.segments_[i]->nEntries_;
		}

		// Flip the count if this iterator precedes the other iterator
		if (d * segmentNum_ < d * other.segmentNum_)
			count = -count;

		return count + metaOffset_ - other.metaOffset_;
	}
}


BWLog::Segment& BWLog::Range::iterator::segment()
{
	return *pRange_->userLog_.segments_[ segmentNum_ ];
}


const BWLog::Segment& BWLog::Range::iterator::segment() const
{
	return *pRange_->userLog_.segments_[ segmentNum_ ];
}

BWLog::EntryAddress BWLog::Range::iterator::addr() const
{
	return EntryAddress( this->segment().suffix_, entryNum_ );
}


std::string BWLog::Range::iterator::str() const
{
	char buf[ 32 ];
	bw_snprintf( buf, sizeof( buf ), "%d:%d:%d",
		segmentNum_, entryNum_, metaOffset_ );
	return std::string( buf );
}

// -----------------------------------------------------------------------------
// Section: BWLog::Range
// -----------------------------------------------------------------------------

BWLog::Range::Range( UserLog &userLog, QueryParams &params ) :
	userLog_( userLog ),
	startTime_( params.start_ ),
	endTime_( params.end_ ),
	startAddress_( params.startAddress_ ),
	endAddress_( params.endAddress_ ),
	direction_( params.direction_ ),
	begin_( *this ),
	curr_( *this ),
	end_( *this ),
	args_( *this )
{
	// Find the start point for the query
	begin_ = curr_ = this->findSentinel( direction_ );

	// Find the end point for the query.  Even though this might seem expensive
	// because there's no guarantee our search will actually reach the end, it's
	// better to do this now as it makes checking for search termination during
	// each call to getNextEntry() cheap.
	end_ = this->findSentinel( -direction_ );
}

/**
 *  Used to locate the first entry that this range should inspect, either coming
 *  from the left (1) or right (-1) direction.
 */
BWLog::Range::iterator BWLog::Range::findSentinel( int direction )
{
	// Check for manually specified offsets first
	if (direction == 1 && startAddress_.valid())
	{
		int segmentNum = userLog_.getSegment( startAddress_.suffix_.c_str() );
		return iterator( *this, segmentNum, startAddress_.index_ );
	}

	if (direction == -1 && endAddress_.valid())
	{
		int segmentNum = userLog_.getSegment( endAddress_.suffix_.c_str() );
		return iterator( *this, segmentNum, endAddress_.index_ );
	}

	// Do a linear search through the list of segments and return an iterator
	// pointing into the right one
	for (int i = direction == 1 ? 0 : userLog_.segments_.size() - 1;
		 direction == 1 ? (i < (int)userLog_.segments_.size()) : (i >= 0);
		 i += direction)
	{
		Segment &segment = *userLog_.segments_[i];

		// This segment is a candidate if either the start or end falls within
		// its limits, or if it's spanned by the start and end
		bool starteq = startAddress_.valid() ?
			startAddress_.suffix_ == segment.suffix_ :
			segment.start_ <= startTime_ && startTime_ <= segment.end_;

		bool endeq = endAddress_.valid() ?
			endAddress_.suffix_ == segment.suffix_ :
			segment.start_ <= endTime_ && endTime_ <= segment.end_;

		bool startlt = startAddress_.valid() ?
			startAddress_.suffix_ < segment.suffix_ :
			startTime_ < segment.start_;

		bool endgt = endAddress_.valid() ?
			segment.suffix_ < endAddress_.suffix_ :
			segment.end_ < endTime_;

		if (starteq || endeq || (startlt && endgt))
		{
			int entryNum = segment.find(
				direction == 1 ? startTime_ : endTime_, direction );

			return iterator( *this, i, entryNum );
		}
	}

	return iterator::error( *this );
}

bool BWLog::Range::getNextEntry( Entry &entry )
{
	if (!begin_.good() || !end_.good() || !curr_.good() || !(curr_ <= end_))
		return false;

	// Read off entry and set args iterator
	curr_.segment().seek( curr_.entryNum_ );
	*curr_.segment().pEntries_ >> entry;
	args_ = iterator( *this, curr_.segmentNum_, entry.argsOffset_ );

	++curr_;
	return true;
}

/**
 *  Returns a FileStream positioned at the args blob corresponding to the most
 *  recent entry fetched by getNextEntry().
 */
BinaryIStream* BWLog::Range::getArgs()
{
	args_.segment().pArgs_->seek( args_.argsOffset_ );
	return args_.segment().pArgs_;
}

bool BWLog::Range::seek( int segmentNum, int entryNum, int metaOffset,
	int postIncrement )
{
	iterator query( *this, segmentNum, entryNum );
	if (begin_ <= query && query <= end_)
	{
		curr_ = iterator( *this, segmentNum, entryNum, metaOffset );
		for (int i=0; i < postIncrement; i++)
			++curr_;
		return true;
	}
	else
		return false;
}

void BWLog::Range::rewind()
{
	--curr_;
}

void BWLog::Range::resume()
{
	end_ = this->findSentinel( -direction_ );

	if (!begin_.good())
		begin_ = curr_ = this->findSentinel( direction_ );
}

// -----------------------------------------------------------------------------
// Section: BWLog::UserLog
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( BWLog::UserLog )

PY_BEGIN_METHODS( BWLog::UserLog )

	PY_METHOD( getSegments )

	PY_METHOD( getComponents )

	PY_METHOD( getEntry )

PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( BWLog::UserLog )

	PY_ATTRIBUTE( uid )

	PY_ATTRIBUTE( username )

	PY_ATTRIBUTE( log )

PY_END_ATTRIBUTES()

BWLog::UserLog::UserLog( BWLog &log, uint16 uid,
	std::string &username, const char *mode ) :
	PyObjectPlus( &s_type_ ),
	log_( log ),
	uid_( uid ),
	username_( username ),
	good_( false ),
	components_( log )
{
	path_ = log.root_;
	path_.push_back( '/' );
	path_ += username_;

	if ((!strcmp( mode, "r" )  && !log.isAccessible( path_.c_str() )) ||
		(!strcmp( mode, "a+" ) && !log.softMkDir( path_.c_str() )))
	{
		ERROR_MSG( "User logdir is not accessible in mode %s: %s\n",
			mode, path_.c_str() );
		return;
	}

	if (!uidfile_.init( uidfile_.join( path_.c_str(), "uid" ), mode, uid ))
	{
		ERROR_MSG( "BWLog::UserLog::UserLog: "
			"Failed to init uid file in %s\n", path_.c_str() );
		return;
	}

	if (!components_.init( path_.c_str(), mode ))
	{
		ERROR_MSG( "BWLog::UserLog::UserLog: "
			"Failed to read components mapping from %s\n",
			components_.filename() );
		return;
	}

	if (!strcmp( mode, "r" ) && !this->loadSegments())
	{
		ERROR_MSG( "BWLog::UserLog::UserLog: Failed to load segments\n" );
		return;
	}

	// Touch active_files, because non-existence of that file is supposed to
	// indicate that message_logger is in the process of rolling.
	if (!strcmp( mode, "a+" ) && !log.activeFiles_.write( log ))
	{
		ERROR_MSG( "BWlog::UserLog::UserLog: Failed to touch active_files\n" );
		return;
	}

	good_ = true;
}

BWLog::UserLog::~UserLog()
{
	for (unsigned i=0; i < segments_.size(); i++)
		delete segments_[i];

}

static void free_dirents( struct dirent **list, int numentries )
{
	for (int i=0; i < numentries; i++)
		free( list[i] );
	free( list );
}

/**
 *  Returns the index of an already loaded segment with the given suffix, or -1
 *  if no segment with that suffix is already loaded.
 */
int BWLog::UserLog::getSegment( const char *suffix ) const
{
	for (unsigned i=0; i < segments_.size(); i++)
		if (segments_[i]->suffix_ == suffix)
			return i;

	return -1;
}

bool BWLog::UserLog::loadSegments()
{
	struct dirent **namelist = NULL;
	int numentries = scandir( path_.c_str(), &namelist,
		Segment::filter, NULL );

	if (numentries == -1)
	{
		ERROR_MSG( "UserLog::loadSegments: Failed to scan user log "
			"directory to load existing entries segments.\n");
		return false;
	}

	for (int i=0; i < numentries; i++)
	{
		const char *filename = namelist[i]->d_name;

		// Everything in the filename after the '.' should be the time.
		// Both the entries file and args file are assumed to have the
		// same suffix.
		const char *suffix = strchr( filename, '.' );
		if (suffix == NULL)
		{
			ERROR_MSG( "BWLog::UserLog::loadSegments: "
				"Entries file found with bad filename: %s\n", filename );
			free_dirents( namelist, numentries );
			return false;
		}
		else
			suffix++;

		int existingIndex = this->getSegment( suffix );
		if (existingIndex != -1)
		{
			if (segments_[ existingIndex ]->dirty())
			{
				segments_[ existingIndex ]->calculateLengths();
			}
		}
		else
		{
			Segment *pSegment = new Segment( *this, log_.mode_.c_str(), suffix );
			if (pSegment->good())
				segments_.push_back( pSegment );
			else
			{
				ERROR_MSG( "BWLog::UserLog::loadSegments: "
					"Dropping segment %s due to load error\n",
					pSegment->suffix_.c_str() );
				delete pSegment;
			}
		}
	}

	// We order the segments by sorting on their start times instead of doing an
	// alphasort in scandir() above, because the filenames are generated from
	// localtime() and may not be strictly in the right order around daylight
	// savings or other similar time changes.
	Segment::cmp cmp;
	std::sort( segments_.begin(), segments_.end(), cmp );

	free_dirents( namelist, numentries );
	return true;
}

bool BWLog::UserLog::resume()
{
	if (!this->loadSegments())
	{
		ERROR_MSG( "BWLog::UserLog::resume: "
			"Failed to reload segments\n" );
		return false;
	}

	if (!components_.refresh())
	{
		ERROR_MSG( "BWLog::UserLog::resume: "
			"Failed to reload components\n" );
		return false;
	}

	return true;
}

/**
 *  Add an Entry to the end of the segment file.
 */
bool BWLog::UserLog::addEntry( Component &component, Entry &entry,
	LoggingStringHandler &handler, MemoryIStream &is )
{
	// Make sure segment is ready to be written to
	if (segments_.empty() || segments_.back()->full())
	{
		// If segments_ is empty, there's a potential race condition here.  For
		// the time between the call to 'new Segment()' and the call to
		// 'activeFiles_.write()', the newly created segment could be
		// accidentally rolled by mltar.  To avoid this, we blow away
		// active_files so that mltar knows not to roll at the moment.  That's
		// OK because activeFiles_.write() regenerates the whole file anyway.
		if (segments_.empty())
		{
			if (unlink( log_.activeFiles_.filename() ) && errno != ENOENT)
			{
				ERROR_MSG( "BWLog::UserLog::addEntry: "
					"Error whilst blowing away active_files: %s\n",
					strerror( errno ) );
			}
		}

		Segment *pSegment = new Segment( *this, log_.mode_.c_str() );
		if (pSegment->good())
		{
			// Drop full segments as we don't need em around anymore and they're
			// just eating up memory and file handles
			while (!segments_.empty())
			{
				delete segments_.back();
				segments_.pop_back();
			}

			segments_.push_back( pSegment );

			// Update active_files
			if (!log_.activeFiles_.write( log_ ))
			{
				ERROR_MSG( "BWLog::UserLog::addEntry: "
					"Couldn't update active_files\n" );
				return false;
			}
		}
		else
		{
			ERROR_MSG( "BWLog::UserLog::addEntry: "
				"Couldn't create new segment %s; dropping msg with fmt '%s'\n",
				pSegment->suffix_.c_str(), handler.fmt().c_str() );
			delete pSegment;
			return false;
		}
	}

	MF_ASSERT( segments_.size() == 1 );
	return segments_.back()->addEntry( component, entry, handler, is );
}

/**
 *  Extract the Entry corresponding to the given EntryAddress in this UserLog.
 *  If ppSegment is passed as non-NULL, the Segment the Entry is located in will
 *  be written to that pointer.
 */
bool BWLog::UserLog::getEntry( const EntryAddress &addr, Entry &result,
	Segment **ppSegment, Range **ppRange, bool warn )
{
	int segmentNum = this->getSegment( addr.suffix_.c_str() );
	if (segmentNum == -1)
	{
		if (warn)
		{
			ERROR_MSG( "BWLog::UserLog::getEntry: "
				"There is no segment with suffix '%s' in %s's log\n",
				addr.suffix_.c_str(), username_.c_str() );
		}

		return false;
	}

	// Read off the entry
	Segment &segment = *segments_[ segmentNum ];
	if (!segment.readEntry( addr.index_, result ))
	{
		ERROR_MSG( "BWLog::UserLog::getEntry: "
			"Couldn't read entry %d from log segment %s\n",
			addr.index_, addr.suffix_.c_str() );
		return false;
	}
	else
	{
		if (ppSegment)
		{
			*ppSegment = &segment;
		}

		// If a range was provided, set its args iterator to the right place
		if (ppRange)
		{
			Range &range = **ppRange;
			range.args_ = Range::iterator( range, segmentNum, result.argsOffset_ );
		}

		return true;
	}
}

/**
 *  Extract the Entry at the given extremity of this UserLog.  Either LOG_BEGIN
 *  or LOG_END must be passed as the first argument to this method.
 */
bool BWLog::UserLog::getEntry( double time, Entry &result )
{
	if (time != LOG_BEGIN && time != LOG_END)
	{
		ERROR_MSG( "BWLog::UserLog::getEntry: "
			"Invalid time passed as first argument: %f\n", time );
		return false;
	}

	// If we encounter the extremely unlikely case where a user's log directory
	// has been created but the first log segment hasn't been written yet, abort
	// here.
	if (segments_.empty())
	{
		ERROR_MSG( "BWLog::UserLog::getEntry: "
			"User's log is currently empty, can't proceed with getEntry()\n" );
		return false;
	}

	Segment &segment = time == LOG_BEGIN ?
		*segments_.front() : *segments_.back();

	int entryNum = time == LOG_END ? 0 : segment.nEntries_ - 1;

	return segment.readEntry( entryNum, result );
}

/**
 *  Format an entry into a line of text.  Makes a copy of the input memory stream
 *  and will not disturb any of the arguments.
 */
const char *BWLog::UserLog::format( const Component &component,
	const Entry &entry, LoggingStringHandler &handler,
	MemoryIStream &is, bool useOldFormat ) const
{
	std::string msg;
	MemoryIStream args( is.data(), is.remainingLength() );

	handler.streamToString( args, msg );
	Result *pResult = new Result(
		entry, log_, *this, component, msg );

	// This dodgy return is OK because the pointer is to a static buffer
	const char *text = useOldFormat ? pResult->formatOld() : pResult->format();
	Py_DECREF( pResult );
	return text;
}


PyObject* BWLog::UserLog::pyGetAttribute( const char *attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

/**
 *  Returns a list of tuples corresponding to the segments in this UserLog.  Each
 *  tuple takes the format (suffix, starttime, endtime, nEntries, size of entries
 *  file, size of args file).
 */
PyObject* BWLog::UserLog::py_getSegments( PyObject *args )
{
	PyObject *list = PyList_New( segments_.size() );

	for (unsigned i=0; i < segments_.size(); i++)
	{
		Segment &segment = *segments_[i];
		PyObject *rec = Py_BuildValue( "sddiii",
			segment.suffix_.c_str(),
			(double)segment.start_, (double)segment.end_,
			segment.nEntries_,
			segment.pEntries_->length(), segment.pArgs_->length() );

		PyList_SET_ITEM( list, i, rec );
	}

	return list;
}

/**
 *  Returns a list of tuples corresponding to the components in this user's log.
 *  Each tuple takes the format (name, pid, appid, firstEntry).  The
 *  'firstEntry' member is a 2-tuple of (suffix, index).
 */
PyObject* BWLog::UserLog::py_getComponents( PyObject *args )
{
	const Components::IdMap &map = components_.idMap();
	PyObject *list = PyList_New( map.size() );
	int i = 0;

	for (Components::IdMap::const_iterator it = map.begin();
		 it != map.end(); ++it)
	{
		const Component &component = *it->second;
		PyObject *rec = Py_BuildValue( "sii(si)",
			component.msg_.componentName_.c_str(),
			component.msg_.pid_,
			component.appid_,
			component.firstEntry_.suffix_.c_str(),
			component.firstEntry_.index_ );

		PyList_SET_ITEM( list, i++, rec );
	}

	return list;
}

/**
 *  Returns a Result object corresponding to a given (suffix, index) tuple.
 */
PyObject* BWLog::UserLog::py_getEntry( PyObject *args )
{
	const char *suffix;
	int index;

	if (!PyArg_ParseTuple( args, "(si)", &suffix, &index ))
		return NULL;

	EntryAddress addr( suffix, index );
	Entry entry;
	Segment *pSegment;

	// We might not be able to get an entry because the segment it lives in may
	// have been rolled.
	if (!this->getEntry( addr, entry, &pSegment, NULL, false ))
	{
		Py_RETURN_NONE;
	}

	// Get the fmt string to go with it
	LoggingStringHandler *pHandler =
		log_.strings_.resolve( entry.stringOffset_ );
	if (pHandler == NULL)
	{
		PyErr_Format( PyExc_LookupError,
			"Unknown string offset: %d", (int)entry.stringOffset_ );
		return NULL;
	}

	// Get the Component to go with it
	const Component *pComponent = components_.resolve( entry.componentId_ );
	if (pComponent == NULL)
	{
		PyErr_Format( PyExc_LookupError,
			"Unknown component id: %d", entry.componentId_ );
		return NULL;
	}

	// Get args stream and interpolate message
	std::string msg;
	pSegment->pArgs_->seek( entry.argsOffset_ );
	pHandler->streamToString( *pSegment->pArgs_, msg );

	return new Result( entry, log_, *this, *pComponent, msg );
}

// -----------------------------------------------------------------------------
// Section: BWLog::FileHandler
// -----------------------------------------------------------------------------

/**
 *  Don't call this from subclass implementations of init() until you are ready
 *  to read (if you're in read mode).
 */
bool BWLog::FileHandler::init( const char *path, const char *mode )
{
	filename_ = path;
	mode_ = mode;
	length_ = this->length();
	return this->read();
}

bool BWLog::FileHandler::dirty()
{
	return length_ != this->length();
}

const char *BWLog::FileHandler::filename() const
{
	return filename_.c_str();
}

bool BWLog::FileHandler::refresh()
{
	this->flush();
	bool success = this->read();
	length_ = this->length();
	return success;
}

char BWLog::FileHandler::s_pathBuf_[ 1024 ];

const char *BWLog::FileHandler::join( const char *dir, const char *filename )
{
	bw_snprintf( s_pathBuf_, sizeof( s_pathBuf_ ), "%s/%s", dir, filename );
	return s_pathBuf_;
}

// -----------------------------------------------------------------------------
// Section: BWLog::BinaryFileHandler
// -----------------------------------------------------------------------------

BWLog::BinaryFileHandler::BinaryFileHandler() :
	pFile_( NULL )
{}

BWLog::BinaryFileHandler::~BinaryFileHandler()
{
	if (pFile_)
		delete pFile_;
}

bool BWLog::BinaryFileHandler::init( const char *path, const char *mode )
{
	pFile_ = new FileStream( path, mode );
	if (pFile_->error())
	{
		ERROR_MSG( "BWLog::BinaryFileHandler::init: "
			"Couldn't open %s in mode %s: %s\n",
			path, mode, pFile_->strerror() );
		return false;
	}

	return FileHandler::init( path, mode );
}

long BWLog::BinaryFileHandler::length()
{
	return pFile_->length();
}

// -----------------------------------------------------------------------------
// Section: BWLog::TextFileHandler
// -----------------------------------------------------------------------------

BWLog::TextFileHandler::TextFileHandler() :
	fp_( NULL )
{}

BWLog::TextFileHandler::~TextFileHandler()
{
	if (fp_)
		fclose( fp_ );
}

bool BWLog::TextFileHandler::init( const char *filename, const char *mode )
{
	if ((fp_ = fopen( filename, mode )) == NULL)
	{
		ERROR_MSG( "TextFileHandler::init: Unable to open file "
			"'%s' in mode '%s': %s\n", filename, mode, strerror( errno ) );
		return false;
	}

	return FileHandler::init( filename, mode );
}

bool BWLog::TextFileHandler::close()
{
	if (fp_)
	{
		fclose( fp_ );
		fp_ = NULL;
		return true;
	}
	else
	{
		ERROR_MSG( "BWLog::TextFileHandler::close: "
			"Tried to close a closed filehandle (%s)\n",
			this->filename() );
		return false;
	}
}

bool BWLog::TextFileHandler::read()
{
	char *line = NULL;
	size_t len = 0;
	bool ok = true;

	if (fseek( fp_, 0, 0 ))
	{
		ERROR_MSG( "BWLog::TextFileHandler::read: "
			"Couldn't rewind '%s': %s\n",
			filename_.c_str(), strerror( errno ) );
		return false;
	}

	while (getline( &line, &len, fp_ ) != -1)
	{
		// Chomp where necessary
		size_t slen = strlen( line );
		if (line[ slen-1 ] == '\n')
			line[ slen-1 ] = '\0';

		if (!this->handleLine( line ))
		{
			ok = false;
			ERROR_MSG( "BWLog::TextFileHandler::read: "
				"Aborting due to failure in handleLine()\n" );
			break;
		}
	}

	if (line)
		free( line );

	return ok;
}

long BWLog::TextFileHandler::length()
{
	if ( !fp_ )
		return 0;

	long pos = ftell( fp_ );
	fseek( fp_, 0, SEEK_END );
	long currsize = ftell( fp_ );
	fseek( fp_, pos, SEEK_SET );
	return currsize;
}

bool BWLog::TextFileHandler::writeLine( const char *line )
{
	if (mode_.find( 'r' ) != std::string::npos)
	{
		ERROR_MSG( "BWLog::TextFileHandler::writeLine: "
			"Can't write to file %s in mode '%s'\n",
			filename_.c_str(), mode_.c_str() );
		return false;
	}

	if (fprintf( fp_, "%s\n", line ) == -1)
	{
		ERROR_MSG( "BWLog::TextFileHandler::writeLine: "
			"Unable to write line '%s' to file %s: %s\n",
			line, filename_.c_str(), strerror(errno) );
		return false;
	}

	fflush( fp_ );
	return true;
}

// -----------------------------------------------------------------------------
// Section: BWLog::Config
// -----------------------------------------------------------------------------

BWLog::Config::Config() :
	inSection_( false ),
	segmentSize_( 100 << 20 )
{}

bool BWLog::Config::handleLine( const char *line )
{
	if (!inSection_ && !strcmp( line, "[message_logger]" ))
		inSection_ = true;
	else if (inSection_ && strlen( line ) > 1)
	{
		if ((line[0] == '[') && (line[strlen(line) - 1] == ']'))
			inSection_ = false;
	}

	if (inSection_)
	{
		char buf[ 1024 ];

		if (sscanf( line, "segment_size = %d", &segmentSize_ ) == 1)
			;

		// If logdir begins with a slash, it is absolute, otherwise it is
		// relative to the directory the config file resides in
		else if (sscanf( line, "logdir = %s", buf ) == 1)
		{
			if (buf[0] != '/')
			{
				char cwd[ 512 ], confDirBuf[ 512 ];
				char *confDir;

				memset( cwd, 0, sizeof(cwd) );

				// If the path to the config file isn't absolute...
				if (filename_.c_str()[0] != '/')
				{
					getcwd( cwd, sizeof( cwd ) );
				}
				strcpy( confDirBuf, filename_.c_str() );
				confDir = dirname( confDirBuf );

				if (cwd[0])
				{
					logDir_ = cwd;
					logDir_.push_back( '/' );
				}
				logDir_ += confDir;
				logDir_ .push_back( '/' );
				logDir_ += buf;
			}
			else
			{
				logDir_ = buf;
			}
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
// Section: BWLog::BigWorldConfig
// -----------------------------------------------------------------------------

bool BWLog::BigWorldConfig::handleLine( const char *line )
{
	if (!inSection_ && !strcmp( line, "[tools]" ))
		inSection_ = true;
	else if (inSection_ && line[0] == '[')
		inSection_ = false;

	if (inSection_)
	{
		char buf[ 1024 ];

		if (sscanf( line, "location = %s", buf ) == 1)
			toolsDir_ = buf;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Section: BWLog::IntFile
// -----------------------------------------------------------------------------

BWLog::IntFile::IntFile() :
	v_( -1 )
{}

/**
 *  IntFile's init() accepts an extra int arg unlike the init() methods of
 *  related classes, and it means a few things.  In read mode, it is the value
 *  you expect the file to have in it.  In append mode, if the file doesn't
 *  exist, the value will be written to the file, and if it does exist, it means
 *  the same as it does in read mode (i.e. a sync check).
 */
bool BWLog::IntFile::init( const char *path, const char *mode, int v )
{
	if (!TextFileHandler::init( path, mode ))
		return false;

	if ((!strcmp( mode, "r" )  && v_ != v) ||
		(!strcmp( mode, "a+" ) && v_ != -1 && v_ != v))
	{
		ERROR_MSG( "BWLog::IntFile::init: "
			"Value in %s (%d) does not match %d\n", path, v_, v );
		return false;
	}

	if (!strcmp( mode, "a+" ) && v_ == -1)
		return this->set( v );
	else
		return true;
}

bool BWLog::IntFile::handleLine( const char *line )
{
	if (v_ != -1)
	{
		ERROR_MSG( "BWLog::IntFile::handleLine: "
			"There is more than one number in %s!\n", filename_.c_str() );
		return false;
	}

	return sscanf( line, "%d", &v_ ) == 1;
}

void BWLog::IntFile::flush()
{
	v_ = -1;
}

bool BWLog::IntFile::set( int v )
{
	if (fprintf( fp_, "%d\n", v ) < 0)
		return false;
	fflush( fp_ );
	v_ = v;
	return true;
}


// -----------------------------------------------------------------------------
// Section: BWLog::ActiveFiles
// -----------------------------------------------------------------------------

/**
 *  We never read this file.
 */
bool BWLog::ActiveFiles::read()
{
	return true;
}

bool BWLog::ActiveFiles::handleLine( const char *line )
{
	return true;
}

/**
 *  Makes a record of all current entries and args files.  Blows away the
 *  previous entries.
 */
bool BWLog::ActiveFiles::write( const BWLog &log )
{
	char buf[ 512 ];

	// Open file and clobber
	if (!this->init( this->join( log.root(), "active_files" ), "w" ))
	{
		ERROR_MSG( "BWLog::ActiveFiles::write: "
			"Couldn't open %s for writing: %s\n",
			this->filename(), strerror( errno ) );
		return false;
	}

	// Dump current entries and args files
	for (UserLogs::const_iterator it = log.userLogs_.begin();
		 it != log.userLogs_.end(); ++it)
	{
		// Skip userlogs with no active segment
		if (it->second->segments_.empty())
			continue;

		snprintf( buf, sizeof( buf ), "%s/entries.%s",
			it->second->username_.c_str(),
			it->second->segments_[0]->suffix_.c_str() );

		if (!this->writeLine( buf ))
		{
			ERROR_MSG( "BWLog::ActiveFiles::write: "
				"Couldn't write '%s': %s\n",
				buf, strerror( errno ) );
			this->close();
			return false;
		}

		snprintf( buf, sizeof( buf ), "%s/args.%s",
			it->second->username_.c_str(),
			it->second->segments_[0]->suffix_.c_str() );

		if (!this->writeLine( buf ))
		{
			ERROR_MSG( "BWLog::ActiveFiles::write: "
				"Couldn't write '%s': %s\n",
				buf, strerror( errno ) );
			this->close();
			return false;
		}
	}

	this->close();
	return true;
}

// -----------------------------------------------------------------------------
// Section: BWLog::Strings
// -----------------------------------------------------------------------------

BWLog::Strings::~Strings()
{
	for (OffsetMap::iterator it = offsetMap_.begin();
		 it != offsetMap_.end(); it++)
		delete it->second;
}

bool BWLog::Strings::init( const char *root, const char *mode )
{
	return BinaryFileHandler::init( this->join( root, "strings" ), mode );
}

bool BWLog::Strings::read()
{
	long len = pFile_->length();
	pFile_->seek( 0 );

	while (pFile_->tell() < len)
	{
		LoggingStringHandler *pHandler = new LoggingStringHandler();
		pHandler->read( *pFile_ );

		// This could happen if we're reading and the logger was halfway
		// through dumping a fmt when we calculated 'len'.
		if (pFile_->error())
		{
			WARNING_MSG( "BWLog::Strings::read: "
				"Error while reading strings file (%s): %s\n",
				filename_.c_str(), pFile_->strerror() );
			return false;
		}
		else
		{
			formatMap_[ pHandler->fmt() ] = pHandler;
			offsetMap_[ pHandler->fileOffset() ] = pHandler;
		}
	}

	return true;
}

void BWLog::Strings::flush()
{
	for (FormatMap::iterator it = formatMap_.begin();
		 it != formatMap_.end(); ++it)
	{
		delete it->second;
	}

	formatMap_.clear();
	offsetMap_.clear();
}

/**
 *  If we're in write mode and the fmt string passed in does not currently exist
 *  in the mapping, it will be added to the mapping and written to disk.
 */
LoggingStringHandler* BWLog::Strings::resolve( const std::string &fmt )
{
	FormatMap::iterator it = formatMap_.find( fmt );
	if (it != formatMap_.end())
	{
		return it->second;
	}
	else
	{
		if (mode_ == "a+")
		{
			LoggingStringHandler *pHandler = new LoggingStringHandler( fmt );
			pHandler->write( *pFile_ );
			pFile_->commit();
			formatMap_[ fmt ] = pHandler;
			offsetMap_[ pHandler->fileOffset() ] = pHandler;
			return pHandler;
		}
		else
			return NULL;
	}
}

LoggingStringHandler* BWLog::Strings::resolve( uint32 offset )
{
	OffsetMap::iterator it = offsetMap_.find( offset );
	return it != offsetMap_.end() ? it->second : NULL;
}

// -----------------------------------------------------------------------------
// Section: BWLog::Hostnames
// -----------------------------------------------------------------------------

bool BWLog::Hostnames::init( const char *root, const char *mode )
{
	return TextFileHandler::init( this->join( root, "hostnames" ), mode );
}

void BWLog::Hostnames::flush()
{
	this->clear();
}

bool BWLog::Hostnames::handleLine( const char *line )
{
	char hostIp[64];
	char hostName[1024];
	struct in_addr addr;

	if (sscanf( line, "%63s %1023s", hostIp, hostName ) != 2)
	{
		ERROR_MSG( "Unable to read hostnames file entry (%s)\n", line );
		return false;
	}

	if (inet_aton( hostIp, &addr ) == 0)
	{
		ERROR_MSG( "Unable to convert hostname entry '%s' "
			"to a valid IPv4 address\n", hostIp );
		return false;
	}
	else
	{
		(*this)[ addr.s_addr ] = hostName;
		return true;
	}
}

const char * BWLog::Hostnames::resolve( uint32 addr )
{
	iterator it = this->find( addr );
	if (it == this->end())
	{
		struct hostent *ent = gethostbyaddr( &addr, sizeof( addr ), AF_INET );
		const char *hostname;

		// Unable to resolve the hostname, store the IP address as a string
		if (ent == NULL)
		{
			const char *reason = NULL;
			hostname = inet_ntoa( (in_addr&)addr );

			switch (h_errno)
			{
				case HOST_NOT_FOUND:
					reason = "HOST_NOT_FOUND"; break;
				case NO_DATA:
					reason = "NO_DATA"; break;
				case NO_RECOVERY:
					reason = "NO_RECOVERY"; break;
				case TRY_AGAIN:
					reason = "TRY_AGAIN"; break;
				default:
					reason = "Unknown reason";
			}

			WARNING_MSG( "BWLog::addEntry: Unable to resolve hostname of %s (%s)\n",
				hostname, reason );
		}
		else
		{
			char *firstdot = strstr( ent->h_name, "." );
			if (firstdot != NULL)
				*firstdot = '\0';
			hostname = ent->h_name;
		}

		// Write the mapping to disk
		const char *ipstr = inet_ntoa( (in_addr&)addr );
		char line[ 2048 ];
		bw_snprintf( line, sizeof( line ), "%s %s", ipstr, hostname );
		if (!this->writeLine( line ))
		{
			CRITICAL_MSG( "BWLog::Hostnames::resolve: "
				"Couldn't write hostname mapping for %s\n", line );
			return NULL;
		}
		else
		{
			(*this)[ addr ] = hostname;
			return (*this)[ addr ].c_str();
		}
	}
	else
	{
		return it->second.c_str();
	}
}

uint32 BWLog::Hostnames::resolve( const char *hostname ) const
{
	std::string s = hostname;
	for (const_iterator it = this->begin(); it != this->end(); ++it)
	{
		if (it->second == s)
		{
			return it->first;
		}
	}

	return 0;
}

// -----------------------------------------------------------------------------
// Section: BWLog::ComponentNames
// -----------------------------------------------------------------------------

bool BWLog::ComponentNames::init( const char *root, const char *mode )
{
	return BWLog::TextFileHandler::init(
		this->join( root, "component_names" ), mode );
}

void BWLog::ComponentNames::flush()
{
	this->clear();
}

bool BWLog::ComponentNames::handleLine( const char *line )
{
	std::string name = line;

	// Ignore any components past our maximum limit
	if (this->size() >= this->MAX_COMPONENTS)
	{
		CRITICAL_MSG( "BWLog::ComponentNames::handleLine:"
			"Dropping component '%u'; max number of components reached",
			this->MAX_COMPONENTS );
	}

	this->push_back( name );
	return true;
}

int BWLog::ComponentNames::resolve( const std::string &componentName )
{
	int id = 0;

	// Search for existing records
	for (iterator it = this->begin(); it != this->end(); ++it, ++id)
	{
		if (*it == componentName)
			return id;
	}

	// Make a new entry if none existed
	if (id < (int)MAX_COMPONENTS)
	{
		this->push_back( componentName );
		this->writeLine( componentName.c_str() );
	}
	else
	{
		CRITICAL_MSG( "BWLog::ComponentNames::resolve: "
			"You have registered more components than is supported (%d)\n",
			MAX_COMPONENTS );
	}

	return id;
}

const char * BWLog::ComponentNames::resolve( int ttypeid ) const
{
	if (ttypeid < (int)this->size())
		return (*this)[ ttypeid ].c_str();
	else
	{
		ERROR_MSG( "BWLog::ComponentNames::resolve:"
			"Cannot resolve unknown typeid (%d) from %zu known records\n",
			ttypeid, this->size() );
		return NULL;
	}
}

// -----------------------------------------------------------------------------
// Section: BWLog::Components
// -----------------------------------------------------------------------------

BWLog::Components::Components( BWLog &log ) :
	log_( log ),
	idTicker_( 0 )
{}

BWLog::Components::~Components()
{
	for (IdMap::iterator it = idMap_.begin(); it != idMap_.end(); ++it)
		delete it->second;
}

bool BWLog::Components::init( const char *root, const char *mode )
{
	// We store the filename in this object because we might need it later when
	// we're trying to insert app IDs into the file retrospectively.
	filename_ = this->join( root, "components" );

	return BinaryFileHandler::init( filename_.c_str(), mode );
}

void BWLog::Components::flush()
{
	for (IdMap::iterator it = idMap_.begin(); it != idMap_.end(); ++it)
		delete it->second;

	idMap_.clear();
	addrMap_.clear();
}

bool BWLog::Components::read()
{
	long len = pFile_->length();
	pFile_->seek( 0 );

	while (pFile_->tell() < len)
	{
		Component *pComponent = new Component( *this );
		pComponent->read( *pFile_ );

		if (pFile_->error())
		{
			ERROR_MSG( "BWLog::Components::read: "
				"Error whilst reading %s: %s\n",
				filename_.c_str(), pFile_->strerror() );
			return false;
		}

		addrMap_[ pComponent->addr_ ] = pComponent;
		idMap_[ pComponent->id_ ] = pComponent;

		// Keep the ticker ahead of any components we read from disk so that we
		// don't re-use existing id's when new components register.
		if (pComponent->id_ >= idTicker_)
			idTicker_ = pComponent->id_ + 1;
	}

	return true;
}

bool BWLog::Components::write( Component &component )
{
	component.write( *pFile_ );
	return pFile_->good();
}

/**
 *  Returns the Component object for a particular LCM and address, and adds the
 *  component to the mapping if it doesn't already exist.
 */
BWLog::Component* BWLog::Components::resolve(
	const LoggerComponentMessage &msg, const Mercury::Address &addr )
{
	Component *pComponent = NULL;

	AddrMap::iterator it = addrMap_.find( addr );
	if (it != addrMap_.end())
	{
		// Remove existing entries for that address if it's a different process
		Component &existing = *it->second;

		if ((existing.msg_.version_ != msg.version_) ||
			(existing.msg_.uid_ != msg.uid_) ||
			(existing.msg_.pid_ != msg.pid_) ||
			(existing.msg_.componentName_ != msg.componentName_))
		{
			addrMap_.erase( it );
			idMap_.erase( idMap_.find( existing.id_ ) );
			delete &existing;
		}
		else
			pComponent = &existing;
	}

	// If the component doesn't exist, create it and make entries for it in the
	// runtime mappings, but don't dump it to disk yet ... this is done in
	// UserLog::addEntry once we know the offset of the first log entry.
	if (pComponent == NULL)
	{
		pComponent = new Component( *this, addr, msg,
			log_.componentNames_.resolve( msg.componentName_ ) );
		addrMap_[ addr ] = pComponent;
		idMap_[ pComponent->id_ ] = pComponent;
	}

	return pComponent;
}


/**
 *  Returns the component for a particular address, but obviously can't make the
 *  entry if it doesn't already exist.
 */
BWLog::Component* BWLog::Components::resolve( const Mercury::Address &addr )
{
	AddrMap::iterator it = addrMap_.find( addr );
	return it != addrMap_.end() ? it->second : NULL;
}

BWLog::Component* BWLog::Components::resolve( int id )
{
	IdMap::iterator it = idMap_.find( id );
	return it != idMap_.end() ? it->second : NULL;
}

bool BWLog::Components::erase( const Mercury::Address &addr )
{
	AddrMap::iterator adit = addrMap_.find( addr );
	if (adit != addrMap_.end())
	{
		Component *pComponent = adit->second;
		addrMap_.erase( adit );

		IdMap::iterator idit = idMap_.find( pComponent->id_ );
		if (idit != idMap_.end())
		{
			idMap_.erase( idit );
		}
		else
		{
			ERROR_MSG( "BWLog::Components::erase: "
				"%s wasn't in the ID map!\n",
				pComponent->str().c_str() );
		}

		delete pComponent;
		return true;
	}
	else
		return false;
}

// -----------------------------------------------------------------------------
// Section: BWLog::QueryParams
// -----------------------------------------------------------------------------

// Helper function for compiling regexs
static regex_t *reCompile( const char *pattern, bool casesens )
{
	regex_t *re = new regex_t();
	int reFlags = REG_EXTENDED | REG_NOSUB | (casesens ? 0 : REG_ICASE);
	int reError;
	char reErrorBuf[ 256 ];

	if ((reError = regcomp( re, pattern, reFlags )) != 0)
	{
		regerror( reError, re, reErrorBuf, sizeof( reErrorBuf ) );

		PyErr_Format( PyExc_SyntaxError,
			"Failed to compile regex '%s': %s\n",
			pattern, reErrorBuf );

		delete re;
		return NULL;
	}
	else
		return re;
}


/**
 *  The only mandatory argument to this method is the uid.  Everything else has
 *  (reasonably) sensible defaults.
 *
 *  Also, be aware that if you are searching backwards (i.e. end time > start
 *  time) then your results will be in reverse order.  This is because results
 *  are generated on demand, not in advance.
 *
 *  This class now automatically swaps start/end and startaddr/endaddr if they
 *  are passed in reverse order.  This is so we can avoid repeating all the
 *  reordering logic in higher level apps.
 *
 *  @param addr	IP Address of the records to find (0 for all records)
 *  @param max   The maximum number of records to return (0 for not limit)
 */
BWLog::QueryParams::QueryParams( PyObject *args, PyObject *kwargs, BWLog &log ) :
	start_( LOG_BEGIN ),
	end_( LOG_END ),
	pid_( 0 ),
	appid_( 0 ),
	procs_( -1 ),
	severities_( -1 ),
	pInclude_( NULL ),
	pExclude_( NULL ),
	interpolate_( PRE_INTERPOLATE ),
	casesens_( true ),
	direction_( FORWARDS ),
	context_( 0 ),
	good_( false )
{
	static const char *kwlist[] = { "uid", "start", "end", "startaddr", "endaddr",
							  "period", "host", "pid", "appid",
							  "procs", "severities", "message", "exclude",
							  "interpolate", "casesens", "direction", "context",
							  NULL };

	double start = LOG_BEGIN, end = LOG_END;
	const char *host = "", *include = "", *exclude = "", *cperiod = "";
	PyObject *startAddr = NULL, *endAddr = NULL;

	if (!PyArg_ParseTupleAndKeywords( args, kwargs, "H|ddO!O!ssHHiissibii",
			const_cast<char **>(kwlist),
			&uid_, &start, &end,
			&PyTuple_Type, &startAddr, &PyTuple_Type, &endAddr,
			&cperiod, &host, &pid_, &appid_, &procs_, &severities_,
			&include, &exclude, &interpolate_, &casesens_, &direction_, &context_ ))
	{
		return;
	}

	addr_ = *host ? log.hostnames_.resolve( host ) : 0;
	if (*host && !addr_)
	{
		PyErr_Format( PyExc_LookupError,
			"Queried host '%s' was not known in the logs", host );
		return;
	}

	// Regex compilation
	if (*include && (pInclude_ = reCompile( include, casesens_ )) == NULL)
		return;

	if (*exclude && (pExclude_ = reCompile( exclude, casesens_ )) == NULL)
		return;

	std::string period = cperiod;

	UserLogPtr pUserLog = log.getUserLog( uid_ );
	if (pUserLog == NULL)
	{
		PyErr_Format( PyExc_LookupError,
			"UID %d doesn't have any entries in this log", uid_ );
		return;
	}

	// If this user's log has no segments (i.e. they have been rolled away) then
	// bail out now
	if (pUserLog->segments_.empty())
	{
		WARNING_MSG( "BWLog::QueryParams::QueryParams: "
			"%s's log has no segments, they may have been rolled\n",
			pUserLog->username_.c_str() );

		good_ = true;
		return;
	}

	// Start address take precedences over start time if both are specified.
	if (startAddr != NULL)
	{
		startAddress_.parseTuple( startAddr );

		// Obtain the start time here to ensure that inference of direction
		// based on start and end time is always correct.
		Entry entry;

		if (!pUserLog->getEntry( startAddress_, entry ))
		{
			PyErr_Format( PyExc_RuntimeError,
				"Couldn't determine time for %s's entry address %s:%d",
				pUserLog->username_.c_str(), startAddress_.suffix_.c_str(),
				startAddress_.index_ );
			return;
		}

		start_ = entry.time_;
	}
	else
	{
		start_ = start;

		// If start time was given as either extremity and we're using a fixed
		// period (i.e. period isn't to beginning or present), we need the
		// actual time for the same reason as above.
		if (period.size() && period != "to beginning" && period != "to present" &&
			(start == LOG_BEGIN || start == LOG_END))
		{
			Entry entry;

			if (!pUserLog->getEntry( start, entry ))
			{
				char buf[1024];
				bw_snprintf( buf, sizeof( buf ), "Couldn't determine time for %s's extremity %f",
					pUserLog->username_.c_str(), start );
				PyErr_SetString( PyExc_RuntimeError, buf );
				return;
			}

			start_ = entry.time_;
		}
	}

	// endAddr takes the highest priority for figuring out the endpoint,
	// followed by period, and then by end time.
	if (endAddr != NULL)
	{
		endAddress_.parseTuple( endAddr );

		// Obtain the end time here to ensure that inference of direction
		// based on start and end time is always correct.
		Entry entry;

		if (!pUserLog->getEntry( endAddress_, entry ))
		{
			PyErr_Format( PyExc_RuntimeError,
				"Couldn't determine time for %s's entry address %s:%d",
				pUserLog->username_.c_str(), endAddress_.suffix_.c_str(),
				endAddress_.index_ );
			return;
		}

		end_ = entry.time_;
	}
	else if (period.size())
	{
		if (period == "to beginning")
		{
			end_ = 0;
		}
		else if (period == "to present")
		{
			end_ = -1;
		}
		else
		{
			end_ = double( start_ ) + atof( cperiod );

			if (period[0] != '+'&& period[0] != '-')
			{
				start_ = double( start_ ) - atof( cperiod );
			}
		}
	}
	else
	{
		end_ = end;
	}

	// Re-order times if passed in reverse order.  TODO: Actually verify that
	// the segment numbers are in order instead of using lex cmp on suffixes.
	if (end_ < start_ ||
		(startAddress_.valid() && endAddress_.valid() &&
			endAddress_ < startAddress_))
	{
		LogTime lt = end_; end_ = start_; start_ = lt;
		EntryAddress ea = endAddress_;
		endAddress_ = startAddress_; startAddress_ = ea;
		direction_ *= -1;
	}

	good_ = true;
}

BWLog::QueryParams::~QueryParams()
{
	if (pInclude_ != NULL)
	{
		regfree( pInclude_ );
		delete pInclude_;
	}

	if (pExclude_ != NULL)
	{
		regfree( pExclude_ );
		delete pExclude_;
	}
}

// -----------------------------------------------------------------------------
// Section: Query
// -----------------------------------------------------------------------------

static PyObject *query_iter( PyObject *pQuery )
{
	Py_INCREF( pQuery );
	return pQuery;
}

static PyObject *query_iternext( PyObject *pIter )
{
	BWLog::Query *pQuery = static_cast< BWLog::Query* >( pIter );
	return pQuery->next();
}

PY_TYPEOBJECT_WITH_ITER( BWLog::Query, query_iter, query_iternext );

PY_BEGIN_METHODS( BWLog::Query )

	PY_METHOD( get )

	PY_METHOD( inReverse )

	PY_METHOD( getProgress )

	PY_METHOD( resume )

	PY_METHOD( tell )

	PY_METHOD( seek )

	PY_METHOD( step )

	PY_METHOD( setTimeout )

PY_END_METHODS();

PY_BEGIN_ATTRIBUTES( BWLog::Query );

PY_END_ATTRIBUTES();

BWLog::Query::Query( BWLog *pLog, QueryParams *pParams, UserLog *pUserLog ) :
	PyObjectPlus( &s_type_ ),
	pLog_( pLog ), pRange_( new Range( *pUserLog, *pParams ) ),
	pParams_( pParams ), pUserLog_( pUserLog ),
	pContextResult_( NULL ),
	contextPoint_( *pRange_ ), contextCurr_( *pRange_ ), mark_( *pRange_ ),
	separatorReturned_( false ),
	pCallback_( NULL ), timeout_( 0 ), timeoutGranularity_( 0 )
{}


PyObject* BWLog::Query::pyGetAttribute( const char *attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}


PyObject* BWLog::Query::next()
{
	// If we're fetching context, we don't need to search
	if (pContextResult_)
	{
		// If there's a gap between the start of this context and the last
		// result returned, give back a separator line
		if (!separatorReturned_ && mark_.good() && contextCurr_ - mark_ > 1)
		{
			separatorReturned_ = true;
			return new Result();
		}

		Result *pResult = NULL;

		// If we're positioned over the actual context point, re-use the Result
		// we fetched earlier
		if (contextCurr_ == contextPoint_)
		{
			pResult = pContextResult_.getObject();
			Py_INCREF( pResult );
		}

		// Otherwise read it from disk like normal
		else
		{
			Entry entry;
			Range *pRange = pRange_.getObject();

			if (!pUserLog_->getEntry(
					contextCurr_.addr(), entry, NULL, &pRange ))
			{
				PyErr_Format( PyExc_LookupError,
					"Couldn't fetch context entry @ %s",
					contextCurr_.str().c_str() );
				return NULL;
			}

			pResult = this->getResultForEntry( entry, false );

			if (pResult == NULL)
				return NULL;
		}

		mark_ = contextCurr_;
		++contextCurr_;

		// Terminate context stuff if we've got enough
		if (contextCurr_.metaOffset_ > 0 ||
			contextCurr_ - contextPoint_ > pParams_->context_)
		{
			pContextResult_ = NULL;
			separatorReturned_ = false;
		}

		return pResult;
	}

	uint64 startTime = timestamp();
	Entry entry;

	for (int i=0; pRange_->getNextEntry( entry ); i++)
	{
		// Trigger timeout callback if necessary
		if (pCallback_ != NULL &&
			i % timeoutGranularity_ == 0 &&
			(timestamp() - startTime)/stampsPerSecondD() > timeout_)
		{
			// If the callback raises an exception, terminate the search.
			if (PyObject_CallFunction( pCallback_.getObject(), "O", this ) ==
				NULL)
			{
				return NULL;
			}

			startTime = timestamp();
		}

		Result *pResult = this->getResultForEntry( entry, true );

		// Skip to next entry if this one was filtered out
		if (pResult == NULL)
			continue;

		// If we need context, set the context fields and re-execute this method
		if (pParams_->context_)
		{
			pContextResult_ = ResultPtr( pResult, ResultPtr::STEAL_REFERENCE );

			contextPoint_ = pRange_->curr_;
			--contextPoint_;

			contextCurr_ = contextPoint_;

			// Cycle contextCurr_ backwards if it's ahead of the mark_
			if (!mark_.good() || mark_ < contextCurr_)
			{
				for (int j=0; j < pParams_->context_; j++)
				{
					if (mark_.good() && contextCurr_ - mark_ <= 1)
						break;
					else
						--contextCurr_;
				}
			}

			// Otherwise cycle it in front of the mark_
			else
			{
				contextCurr_ = mark_;
				++contextCurr_;
			}

			// If the context has advanced past the end of the range, we're done
			if (pRange_->end_ < contextCurr_)
			{
				PyErr_SetNone( PyExc_StopIteration );
				return NULL;
			}

			return this->next();
		}

		// Remember the position of the most recent result actually returned
		mark_ = pRange_->curr_;
		--mark_;

		return pResult;
	}

	PyErr_SetNone( PyExc_StopIteration );
	return NULL;
}


/**
 *  Returns the Result* for the provided entry, or NULL if filter is true and
 *  the entry doesn't pass the filter criteria.
 */
BWLog::Result* BWLog::Query::getResultForEntry( Entry &entry, bool filter )
{
	// Get the fmt string to go with it
	LoggingStringHandler *pHandler =
		pLog_->strings_.resolve( entry.stringOffset_ );

	if (pHandler == NULL)
	{
		PyErr_Format( PyExc_LookupError,
			"Unknown string offset: %d", (int)entry.stringOffset_ );
		return NULL;
	}

	// Get the Component to go with it
	const Component *pComponent =
		pUserLog_->components_.resolve( entry.componentId_ );
	if (pComponent == NULL)
	{
		PyErr_Format( PyExc_LookupError,
			"Unknown component id: %d", entry.componentId_ );
		return NULL;
	}

	std::string matchText = pHandler->fmt();

	if (pParams_->interpolate_ == PRE_INTERPOLATE)
		this->interpolate( *pHandler, pRange_, matchText );

	// Filter
	if (filter)
	{
		if (pParams_->addr_ && pComponent->addr_.ip != pParams_->addr_)
			return NULL;

		if (pParams_->pid_ && pComponent->msg_.pid_ != pParams_->pid_)
			return NULL;

		if (pParams_->appid_ && pComponent->appid_ != pParams_->appid_)
			return NULL;

		if (pParams_->procs_ != -1 &&
			!(pParams_->procs_ & (1 << pComponent->typeid_)))
			return NULL;

		if (pParams_->severities_ != -1 &&
			!(pParams_->severities_ & (1 << entry.messagePriority_)))
			return NULL;

		if (pParams_->pInclude_ &&
			regexec( pParams_->pInclude_, matchText.c_str(), 0, NULL, 0 ) != 0)
			return NULL;

		if (pParams_->pExclude_ &&
			regexec( pParams_->pExclude_, matchText.c_str(), 0, NULL, 0 ) == 0)
			return NULL;
	}

	if (pParams_->interpolate_ == POST_INTERPOLATE)
		this->interpolate( *pHandler, pRange_, matchText );

	return new Result( entry, *pLog_, *pUserLog_, *pComponent, matchText );
}


void BWLog::Query::interpolate( LoggingStringHandler &handler,
	RangePtr pRange, std::string &dest )
{
	BinaryIStream &argsStream = *pRange_->getArgs();
	dest.clear();
	handler.streamToString( argsStream, dest );
}

/**
 *  Fetch at most the next 'n' search results.  Passing 0 as the argument means
 *  fetch all possible results.
 */
PyObject *BWLog::Query::py_get( PyObject *args )
{
	int n = 0;

	if (!PyArg_ParseTuple( args, "|i", &n ))
		return NULL;

	PyObject *list = PyList_New( 0 );
	for (int i=0; n == 0 || i < n; i++)
	{
		PyObject *result = this->next();

		if (result == NULL)
		{
			PyErr_Clear();
			return list;
		}
		else
			PyList_Append( list, result );
	}

	return list;
}

/**
 *  Returns true if this query will return results in reverse order
 */
PyObject *BWLog::Query::py_inReverse( PyObject *args )
{
	if (pParams_->direction_ == BACKWARDS)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

/**
 *  Returns (number of entries seen, total number of entries in range)
 */
PyObject *BWLog::Query::py_getProgress( PyObject *args )
{
	if (!pRange_->begin_.good() || !pRange_->end_.good())
		return Py_BuildValue( "(ii)", 0, 0 );
	else
		return Py_BuildValue( "(ii)",
			pRange_->curr_ - pRange_->begin_,
			pRange_->end_ - pRange_->begin_ + 1);
}

/**
 *  Resume a query that was started earlier, recalculating the endpoint of the
 *  search and updating any mappings that might have changed.
 */
PyObject *BWLog::Query::py_resume( PyObject *args )
{
	pLog_->resume();
	pUserLog_->resume();
	pRange_->resume();

	// If we had exhausted the previous range, then we need to step off the
	// last record
	if (pRange_->curr_.metaOffset_ == pRange_->direction_)
		++pRange_->curr_;

	Py_RETURN_NONE;
}

/**
 *  If you pass 'True' as an argument to this method, it will return the address
 *  of the end of the query.  Without an argument, the expected behaviour of
 *  returning the current position of the query is exhibited.
 */
PyObject *BWLog::Query::py_tell( PyObject *args )
{
	char tellEnd = 0;
	if (!PyArg_ParseTuple( args, "|b", &tellEnd ))
		return NULL;

	Range::iterator it = tellEnd ? pRange_->end_ : pRange_->curr_;

	if (it.good())
	{
		Segment &segment = *pUserLog_->segments_[ it.segmentNum_ ];
		return Py_BuildValue( "(sii)",
			segment.suffix_.c_str(), it.entryNum_, it.metaOffset_ );
	}
	else
	{
		Py_RETURN_NONE;
	}
}

PyObject *BWLog::Query::py_seek( PyObject *args )
{
	const char *suffix;
	int segmentNum, entryNum, metaOffset, postIncrement = 0;

	if (!PyArg_ParseTuple( args, "(sii)|i",
			&suffix, &entryNum, &metaOffset, &postIncrement ))
		return NULL;

	if ((segmentNum = pUserLog_->getSegment( suffix )) == -1)
	{
		PyErr_Format( PyExc_KeyError,
			"Unknown segment suffix '%s'", suffix );
		return NULL;
	}

	if (pRange_->seek( segmentNum, entryNum, metaOffset, postIncrement ))
	{
		Py_RETURN_NONE;
	}
	else
	{
		PyErr_Format( PyExc_RuntimeError,
			"(%d,%d) is not within the current extents "
			"of this query (%d,%d) -> (%d,%d)",
			segmentNum, entryNum,
			pRange_->begin_.segmentNum_, pRange_->begin_.entryNum_,
			pRange_->end_.segmentNum_, pRange_->end_.entryNum_ );
		return NULL;
	}
}


/**
 *  Basically an implementation of fseek( fp, +/-1, SEEK_CUR ) for queries.
 *  Seeking forwards can be done as many times as you want, but you can only
 *  seek backwards once, much like ungetc().  Note that passing FORWARDS for the
 *  argument means search forwards with respect to the query's direction, as
 *  opposed to towards the end of the log.  Same goes for BACKWARDS.
 */
PyObject *BWLog::Query::py_step( PyObject *args )
{
	int offset;

	if (!PyArg_ParseTuple( args, "i", &offset ))
		return NULL;

	if (offset == BACKWARDS)
	{
		--pRange_->curr_;
		Py_RETURN_NONE;
	}
	else if (offset == FORWARDS)
	{
		++pRange_->curr_;
		Py_RETURN_NONE;
	}
	else
	{
		PyErr_Format( PyExc_ValueError,
			"You must pass either FORWARDS or BACKWARDS to step()" );
		return NULL;
	}
}


/**
 *  This method sets a timeout callback that will be called inside Query::next()
 *  at a specified interval.  If the callback raises an exception, this will
 *  cause Query::next() to abort prematurely.  The primary use of this mechanism
 *  is to limit the runtime of sparse queries.
 */
PyObject *BWLog::Query::py_setTimeout( PyObject *args )
{
	PyObject *pFunc = NULL;
	float timeout = 0;
	int granularity = 1000;

	if (!PyArg_ParseTuple( args, "fO|i", &timeout, &pFunc, &granularity ))
		return NULL;

	if (!PyCallable_Check( pFunc ))
	{
		PyErr_Format( PyExc_TypeError,
			"Callback argument is not callable" );
		return NULL;
	}

	// Insert new callback
	timeout_ = timeout;
	pCallback_ = pFunc;
	timeoutGranularity_ = granularity;
	Py_RETURN_NONE;
}

// -----------------------------------------------------------------------------
// Section: Result
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( BWLog::Result );

PY_BEGIN_METHODS( BWLog::Result )

	PY_METHOD( format );

PY_END_METHODS();

PY_BEGIN_ATTRIBUTES( BWLog::Result )

	PY_ATTRIBUTE( time )
	PY_ATTRIBUTE( host )
	PY_ATTRIBUTE( pid )
	PY_ATTRIBUTE( appid )
	PY_ATTRIBUTE( username )
	PY_ATTRIBUTE( component )
	PY_ATTRIBUTE( severity )
	PY_ATTRIBUTE( message )
	PY_ATTRIBUTE( stringOffset )

PY_END_ATTRIBUTES();

char BWLog::Result::s_linebuf_[ 2048 ];


BWLog::Result::Result() :
	PyObjectPlus( &s_type_ ),
	host_( NULL )
{
}


BWLog::Result::Result( const Entry &entry, BWLog &log,
	const UserLog &userLog, const Component &component,
	const std::string &message ) :
	PyObjectPlus( &s_type_ ),
	time_( entry.time_ ),
	host_( log.hostnames_.resolve( component.addr_.ip ) ),
	pid_( component.msg_.pid_ ),
	appid_( component.appid_ ),
	username_( userLog.username_.c_str() ),
	component_( log.componentNames_.resolve( component.typeid_ ) ),
	severity_( entry.messagePriority_ ),
	message_( message ),
	stringOffset_( entry.stringOffset_ )
{
}


BWLog::Result::~Result()
{
}


PyObject * BWLog::Result::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}

int BWLog::Result::pySetAttribute( const char * attr, PyObject * value )
{
	PY_SETATTR_STD();

	return PyObjectPlus::pySetAttribute( attr, value );
}

PyObject* BWLog::Result::py_format( PyObject *args )
{
	unsigned flags = SHOW_ALL;
	int len;

	if (!PyArg_ParseTuple( args, "|I", &flags ))
		return NULL;

	const char *line = this->format( flags, &len );
 	return PyString_FromStringAndSize( line, len );
}

/**
 *  These little functions used to be macros in the body of the following method,
 *  but are implemented as functions to avoid macro ugliness.
 */

static int buf_remain( const char *cursor, const char *end )
{
	return end - cursor - 1;
}

static bool truncate( char *end, int *&pLen )
{
	const char truncmsg[] = "<== message truncated!\n";
	strcpy( end - strlen( truncmsg ) - 1, truncmsg );
	if (pLen != NULL) *pLen = sizeof( BWLog::Result::s_linebuf_ ) - 1;
	return false;
}

static bool seek_cursor( char *&cursor, char *end, bool &previous, int *&pLen,
	int n=0 )
{
	if (n)
	{
		if (cursor + n >= end)
			return truncate( end, pLen );
		cursor += n;
	}
	else
	{
		while (cursor < end && *cursor) { cursor++; }
		if (cursor == end)
			return truncate( end, pLen );
	}
	previous = true;
	return true;
}

static void pad_prior( char *&cursor, char *end, bool &previous, int *&pLen )
{
	if (previous)
	{
		*cursor = ' ';
		seek_cursor( cursor, end, previous, pLen, 1 );
	}
}

/**
 *  Format this log result according to the supplied display flags (see
 *  DisplayFlags in bwlog.hpp).  Uses a static buffer, so you must copy the
 *  result of this call away if you want to preserve it.  Writes the length of
 *  the formatted string to pLen if it is non-NULL.
 */
const char *BWLog::Result::format( unsigned flags, int *pLen )
{
	char *cursor = s_linebuf_;
	char *end = s_linebuf_ + sizeof( s_linebuf_ );
	bool previous = false;

	// If this is a pad line, just chuck in -- like grep does
	if (host_ == NULL)
	{
		bw_snprintf( s_linebuf_, sizeof( s_linebuf_ ), "--\n" );
		if (pLen)
			*pLen = 3;
		return s_linebuf_;
	}

	if (flags & (SHOW_DATE | SHOW_TIME))
	{
		struct tm breakdown;
		LogTime time( time_ );
		localtime_r( &time.secs_, &breakdown );
		if (flags & SHOW_DATE)
		{
			strftime( cursor, buf_remain( cursor, end ), "%a %d %b %Y ",
				&breakdown );
			seek_cursor( cursor, end, previous, pLen );
		}
		if (flags & SHOW_TIME)
		{
			strftime( cursor, buf_remain( cursor, end ), "%T", &breakdown );
			if (!seek_cursor( cursor, end, previous, pLen ))
				return s_linebuf_;
			snprintf( cursor, buf_remain( cursor, end ), ".%03d ",
				time.msecs_ );
			if (!seek_cursor( cursor, end, previous, pLen, 5 ))
				return s_linebuf_;
		}
	}

	if (flags & SHOW_HOST)
	{
		pad_prior( cursor, end, previous, pLen );
		snprintf( cursor, buf_remain( cursor, end ), "%-15s", host_ );
		if (!seek_cursor( cursor, end, previous, pLen, 15 ))
			return s_linebuf_;
	}

	if (flags & SHOW_USER)
	{
		pad_prior( cursor, end, previous, pLen );
		snprintf( cursor, buf_remain( cursor, end ), "%-10s", username_ );
		if (!seek_cursor( cursor, end, previous, pLen, 10 ))
			return s_linebuf_;
	}

	if (flags & SHOW_PID)
	{
		pad_prior( cursor, end, previous, pLen );
		snprintf( cursor, buf_remain( cursor, end ), "%-5d", pid_ );
		if (!seek_cursor( cursor, end, previous, pLen, 5 ))
			return s_linebuf_;
	}

	if (flags & SHOW_PROCS)
	{
		pad_prior( cursor, end, previous, pLen );
		snprintf( cursor, buf_remain( cursor, end ), "%-10s", component_ );
		if (!seek_cursor( cursor, end, previous, pLen, 10 ))
			return s_linebuf_;
	}

	if (flags & SHOW_APPID)
	{
		pad_prior( cursor, end, previous, pLen );

		if (appid_)
			snprintf( cursor, buf_remain( cursor, end ), "%-3d", appid_ );
		else
			snprintf( cursor, buf_remain( cursor, end ), "   " );

		if (!seek_cursor( cursor, end, previous, pLen, 3 ))
			return s_linebuf_;
	}

	if (flags & SHOW_SEVERITY)
	{
		pad_prior( cursor, end, previous, pLen );
		snprintf( cursor, buf_remain( cursor, end ), "%-8s",
			messagePrefix( (DebugMessagePriority)severity_ ) );
		if (!seek_cursor( cursor, end, previous, pLen, 8 ))
			return s_linebuf_;
	}

	if (flags & SHOW_MESSAGE)
	{
		pad_prior( cursor, end, previous, pLen );
		int msgcol = (int)(cursor - s_linebuf_);
		const char *c = message_.c_str();

		while (*c)
		{
			*cursor = *c++;
			if (!seek_cursor( cursor, end, previous, pLen, 1 ))
				return s_linebuf_;

			// If the character just read was a newline and we're not done, fill
			// out the left hand side so the message columns line up
			if (cursor[-1] == '\n' && *c)
			{
				char *padpoint = cursor;
				if (!seek_cursor( cursor, end, previous, pLen, msgcol ))
					return s_linebuf_;
				memset( padpoint, ' ', msgcol );
			}
		}
	}

	if (cursor[-1] != '\n')
	{
		*cursor = '\n';
		if (!seek_cursor( cursor, end, previous, pLen, 1 ))
			return s_linebuf_;
		*cursor = '\0';
	}
	else
		*cursor = '\0';

	if (pLen != NULL)
		*pLen = (int)(cursor - s_linebuf_);

	return s_linebuf_;
}

/**
 *  Format this log entry in the old text log format.  Source adapted from 1.7.x
 *  logger.cpp.
 */
const char *BWLog::Result::formatOld()
{
	struct tm breakdown;
	struct timeval now;
	char timestr[64];
	size_t n;

	gettimeofday( &now, NULL );
	localtime_r( &now.tv_sec, &breakdown );
	n = strftime( timestr, sizeof(timestr), "%a %d %b %Y %T", &breakdown );

	bw_snprintf( timestr+n, sizeof( timestr ) - n,
			".%03ld", now.tv_usec/1000 );

	bw_snprintf( s_linebuf_, sizeof( s_linebuf_ ),
		"%s, %15s, %3d, %10s, %5d, %10s, %8s, %s",
		timestr,
		host_,
		0, // TODO: actually put the component priority in here
		username_,
		pid_,
		component_,
		messagePrefix( (DebugMessagePriority)severity_ ),
		message_.c_str() );

	return s_linebuf_;
}

// -----------------------------------------------------------------------------
// Section: Python glue common to all exposed objects
// -----------------------------------------------------------------------------

PyMODINIT_FUNC initbwlog()
{
	// Suppress warnings about Python C API version mismatches between this
	// module (compiled with Python 2.5) and the python interpreter on the
	// system (which is likely to be Python 2.4 for quite some time).  We can
	// remove this hack as soon as Python 2.5 is shipped with the distributions
	// we support.
	PyRun_SimpleString( "import warnings;"
		"warnings.filterwarnings( 'ignore', '.*API version mismatch.*bwlog', "
		"RuntimeWarning )" );

	if (PyType_Ready( &BWLog::s_type_ ) < 0)
		return;

	PyObject *m = Py_InitModule3( "bwlog", NULL, "Interface to BWLog files");

	if (m == NULL)
		return;

#	define INSERT_CLASS( CLASS, NAME )								\
	Py_INCREF( &CLASS::s_type_ );									\
	PyModule_AddObject( m, NAME, (PyObject*)&CLASS::s_type_ );		\

	INSERT_CLASS( BWLog, "BWLog" );
	INSERT_CLASS( BWLog::Result, "Result" );

	// Add in all the flags the Python will need
#	define INSERT_BWLOG_CONSTANT( NAME )							\
	PyModule_AddIntConstant( m, #NAME, (int)BWLog::NAME );

	INSERT_BWLOG_CONSTANT( SHOW_DATE );
	INSERT_BWLOG_CONSTANT( SHOW_TIME );
	INSERT_BWLOG_CONSTANT( SHOW_HOST );
	INSERT_BWLOG_CONSTANT( SHOW_USER );
	INSERT_BWLOG_CONSTANT( SHOW_PID );
	INSERT_BWLOG_CONSTANT( SHOW_APPID );
	INSERT_BWLOG_CONSTANT( SHOW_PROCS );
	INSERT_BWLOG_CONSTANT( SHOW_SEVERITY );
	INSERT_BWLOG_CONSTANT( SHOW_MESSAGE );
	INSERT_BWLOG_CONSTANT( SHOW_ALL );

	INSERT_BWLOG_CONSTANT( DONT_INTERPOLATE );
	INSERT_BWLOG_CONSTANT( PRE_INTERPOLATE );
	INSERT_BWLOG_CONSTANT( POST_INTERPOLATE );

	INSERT_BWLOG_CONSTANT( LOG_BEGIN );
	INSERT_BWLOG_CONSTANT( LOG_END );

	INSERT_BWLOG_CONSTANT( FORWARDS );
	INSERT_BWLOG_CONSTANT( BACKWARDS );

#	define INSERT_CONSTANT( NAME )							\
	PyModule_AddIntConstant( m, #NAME, NAME );

	INSERT_CONSTANT( MESSAGE_LOGGER_MSG );
	INSERT_CONSTANT( MESSAGE_LOGGER_REGISTER );
	INSERT_CONSTANT( MESSAGE_LOGGER_PROCESS_BIRTH );
	INSERT_CONSTANT( MESSAGE_LOGGER_PROCESS_DEATH );

	// Add in what this version of bwlogger is called
	PyModule_AddStringConstant( m, "VERSION_NAME", "message_logger" );


	/* Add the severity levels to the python module */
	PyObject *severityLevels = PyDict_New();
	PyModule_AddObject( m, "SEVERITY_LEVELS", severityLevels );

#	define INSERT_SEVERITY( LEVEL )									\
	PyDict_SetItemString( severityLevels, messagePrefix( LEVEL ),	\
		PyInt_FromLong( LEVEL ) );

	INSERT_SEVERITY( MESSAGE_PRIORITY_TRACE );
	INSERT_SEVERITY( MESSAGE_PRIORITY_DEBUG );
	INSERT_SEVERITY( MESSAGE_PRIORITY_INFO );
	INSERT_SEVERITY( MESSAGE_PRIORITY_NOTICE );
	INSERT_SEVERITY( MESSAGE_PRIORITY_WARNING );
	INSERT_SEVERITY( MESSAGE_PRIORITY_ERROR );
	INSERT_SEVERITY( MESSAGE_PRIORITY_CRITICAL );
	INSERT_SEVERITY( MESSAGE_PRIORITY_HACK );
	INSERT_SEVERITY( MESSAGE_PRIORITY_SCRIPT );

	// Add in the minimal set of component names that can be registered
	PyObject *minNames = PyList_New( 0 );
#	define INSERT_COMPONENT_NAME( NAME )				\
	{													\
		PyObject *str = PyString_FromString( NAME );	\
		PyList_Append( minNames, str );					\
		Py_DECREF( str );								\
	}

	INSERT_COMPONENT_NAME( "CellApp" );
	INSERT_COMPONENT_NAME( "BaseApp" );
	INSERT_COMPONENT_NAME( "LoginApp" );
	INSERT_COMPONENT_NAME( "DBMgr" );
	INSERT_COMPONENT_NAME( "CellAppMgr" );
	INSERT_COMPONENT_NAME( "BaseAppMgr" );

	if (PyModule_AddObject( m, "BASE_COMPONENT_NAMES", minNames ) == -1)
		ERROR_MSG( "initbwlog: Unable to add baseComponentNames to module\n" );
}
