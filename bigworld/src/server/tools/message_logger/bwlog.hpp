/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 * Similar in spirit to the space viewer log, we have one file with fixed
 * length records that indexes other files with variable length records. Each
 * user gets a separate directory in the log.
 *
 * The 'strings' file contains all the format strings and parsing info to go
 * with them. It lives in the root directory of the log and is shared amongst
 * all users.
 *
 * The per-user 'entries' files contain all the fixed length stuff from each log
 * entry, and the per-user 'args' files contain the arguments to each log
 * message.
 *
 * The log is divided into segments defined by the maximum length of either the
 * strings or args file. They are named entries.000, entries.001 etc. The
 * strings file is monolithic and is shared between all log segments.
 *
 * TODO: Update this comment with the segmented structure.
 */
#ifndef BWLOG_HPP
#define BWLOG_HPP

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "cstdmf/smartpointer.hpp"
#include "cstdmf/stdmf.hpp"
#include "network/logger_message_forwarder.hpp"
#include "logging_string_handler.hpp"

#include <sys/types.h>
#include <regex.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>

//------------------------------------------------------------------------
// Section: BWLog
//------------------------------------------------------------------------

class BWLog : public PyObjectPlus
{
	Py_InstanceHeader( BWLog )

public:
	enum DisplayFlags
	{
		SHOW_DATE = 1 << 0,
		SHOW_TIME = 1 << 1,
		SHOW_HOST = 1 << 2,
		SHOW_USER = 1 << 3,
		SHOW_PID = 1 << 4,
		SHOW_APPID = 1 << 5,
		SHOW_PROCS = 1 << 6,
		SHOW_SEVERITY = 1 << 7,
		SHOW_MESSAGE = 1 << 8,
		SHOW_ALL = 0x1FF
	};

	enum InterpolateFlags
	{
		DONT_INTERPOLATE = 0,
		POST_INTERPOLATE = 1,
		PRE_INTERPOLATE = 2
	};

	static const double LOG_BEGIN = 0;
	static const double LOG_END = -1;

	enum SearchDirection
	{
		FORWARDS = 1,
		BACKWARDS = -1
	};

	static const int LOG_FORMAT_VERSION;

	BWLog();

	bool init( const char *root = NULL, const char *mode = "w",
		const char *config = NULL );
	bool readConfig( const char *config );
	bool resume();

	void writeToStdout( bool flag ){ writeToStdout_ = flag; }
	void writeTextLogs( bool flag ){ writeTextLogs_ = flag; }
	const char *root() const { return root_.c_str(); }

	bool delComponent( const Mercury::Address &addr );
	bool setAppID( const Mercury::Address &addr, int id );

	bool addEntry( const LoggerComponentMessage &msg,
		const Mercury::Address &addr, MemoryIStream &is );

	bool roll();

	// API exposed to Python (i.e. the useful stuff)
	PyObject* pyGetAttribute( const char *attr );
	PY_METHOD_DECLARE( py_getUsers );
	PY_METHOD_DECLARE( py_getUserLog );
	PY_METHOD_DECLARE( py_getComponentNames );
	PY_METHOD_DECLARE( py_getHostnames );
	PY_METHOD_DECLARE( py_getStrings );
	PY_KEYWORD_METHOD_DECLARE( py_fetch );
	PY_RO_ATTRIBUTE_DECLARE( root_, root );
	PY_FACTORY_DECLARE();

	/**
	 * These two small small structures are streamed to disk as part of each log
	 * entry.
	 */
#	pragma pack( push, 1 )
	struct LogTime
	{
		LogTime () {}

		// Pass -1 here to generate the positive infinity time
		LogTime( double ftime )
		{
			if (ftime == LOG_END)
			{
				secs_ = LONG_MAX;
				msecs_ = 0;
			}
			else
			{
				secs_ = (time_t)ftime;
				msecs_ = (uint16)((ftime - secs_) * 1000 + 0.5);
			}
		}

		bool operator>( const LogTime &other ) const
		{
			return secs_ > other.secs_ ||
				(secs_ == other.secs_ && msecs_ > other.msecs_);
		}

		bool operator>=( const LogTime &other ) const
		{
			return (secs_ == other.secs_ && msecs_ == other.msecs_) ||
				*this > other;
		}

		bool operator<( const LogTime &other ) const
		{
			return secs_ < other.secs_ ||
				(secs_ == other.secs_ && msecs_ < other.msecs_);
		}

		bool operator<=( const LogTime &other ) const
		{
			return (secs_ == other.secs_ && msecs_ == other.msecs_) ||
				*this < other;
		}

		operator char*() const { return ctime( &secs_ ); }
		operator double() const { return secs_ + msecs_/1000.0; }

		time_t secs_;
		uint16 msecs_;
	};

	/**
	 * The fixed-length portion of a log entry (i.e. the bit that gets written
	 * to the 'entries' file).
	 */
	struct Entry
	{
		LogTime time_;
		int componentId_;
		uint8 messagePriority_;
		uint32 stringOffset_;
		uint32 argsOffset_;
		uint16 argsLen_;
	};

	/**
	 * The address of a log entry.  Notice that we reference by suffix instead
	 * of segment number to handle segment deletion on disk.
	 */
	struct EntryAddress
	{
		EntryAddress() {}

		EntryAddress( const char *suffix, int index ) :
			suffix_( suffix ), index_( index ) {}

		EntryAddress( const std::string &suffix, int index ) :
			suffix_( suffix ), index_( index ) {}

		void write( BinaryOStream &os ) const;
		void read( BinaryIStream &is );
		bool valid() const { return !suffix_.empty(); }
		void parseTuple( PyObject *tuple );
		bool operator<( const EntryAddress &other ) const;

		std::string suffix_;
		int index_;
	};

#	pragma pack( pop )

protected:

	Mercury::Reason resolveUid( uint16 uid, uint32 addr, std::string &result );
	bool softMkDir( const char *path ) const;
	bool isAccessible( const char *path ) const;

	std::string root_;
	std::string currentPath_;
	std::string archivePath_;
	std::string mode_;

	bool writeToStdout_;
	bool writeTextLogs_;

	/**
	 * Subclasses of FileHandler are used for managing pretty much all the files
	 * generated by the logs except for the actual log entry / args blob files.
	 * Note that each class defines an 'init' method ... those with a first
	 * argument called 'path' expect the exact filename of the file to be
	 * passed, whereas those whose first argument is 'root' expect the
	 * containing directory name to passed; they work out their filename from
	 * that path.
	 */
	class FileHandler
	{
	public:
		bool init( const char *path, const char *mode );
		bool dirty();
		const char *filename() const;
		virtual bool read() = 0;
		virtual long length() = 0;
		virtual void flush() {}
		bool refresh();
		static const char *join( const char *dir, const char *filename );

	protected:
		std::string filename_;
		std::string mode_;

		// This is only really tracked in "r" mode
		long length_;

		static char s_pathBuf_[];
	};

	/**
	 * Wraps binary-format files that are accessed via a FileStream.
	 */
	class BinaryFileHandler : public FileHandler
	{
	public:
		BinaryFileHandler();
		virtual ~BinaryFileHandler();
		bool init( const char *path, const char *mode );
		virtual long length();
		const FileStream *pFile() const { return pFile_; }

	protected:
		FileStream *pFile_;
	};

	/**
	 * Wraps line-based ASCII files.
	 */
	class TextFileHandler : public FileHandler
	{
	public:
		TextFileHandler();
		virtual ~TextFileHandler();

		bool init( const char *filename, const char *mode );
		bool close();
		virtual bool read();
		virtual long length();
		virtual bool handleLine( const char *line ) = 0;
		bool writeLine( const char *line );

	protected:
		FILE *fp_;
	};

	/**
	 * The message_logger specific config file
	 */
	struct Config : public TextFileHandler
	{
		Config();
		virtual bool handleLine( const char *line );

		bool inSection_;
		int segmentSize_;
		std::string logDir_;
	};

	Config config_;

	/**
	 * The global configuration file (/etc/bigworld.conf)
	 */
	struct BigWorldConfig : public TextFileHandler
	{
		BigWorldConfig() : inSection_( false ), toolsDir_( "./" ) {}
		virtual bool handleLine( const char *line );

		bool inSection_;
		std::string toolsDir_;
	};


	/**
	 * A file containing a single number represented in ascii.  The version file
	 * and the uid file in each user log directory use this.
	 */
	class IntFile : public TextFileHandler
	{
	public:
		IntFile();
		using TextFileHandler::init;
		bool init( const char *path, const char *mode, int v );
		virtual bool handleLine( const char *line );
		virtual void flush();
		bool set( int v );
		operator int() const { return v_; }

	protected:
		int v_;
	};

	IntFile version_;
	IntFile pid_;


	/**
	 * This file tracks the entries and args files that are currently being
	 * written to.  mltar.py respects this list in --move mode and doesn't touch
	 * the files in it.
	 */
	class UserLog;
	class ActiveFiles : public TextFileHandler
	{
	public:
		virtual bool read();
		virtual bool handleLine( const char *line );
		bool write( const BWLog &log );
	};

	ActiveFiles activeFiles_;

	/**
	 * Handles the global format strings mapping and file.
	 */
	class Strings : public BinaryFileHandler
	{
	public:
		virtual ~Strings();
		bool init( const char *root, const char *mode );
		virtual bool read();
		virtual void flush();
		LoggingStringHandler* resolve( const std::string &fmt );
		LoggingStringHandler* resolve( uint32 offset );

		// Mapping from format string -> handler (used when writing log entries)
		typedef std::map< std::string, LoggingStringHandler* > FormatMap;
		FormatMap formatMap_;

		// Mapping from strings file offset -> handler (for reading)
		typedef std::map< uint32, LoggingStringHandler* > OffsetMap;
		OffsetMap offsetMap_;
	};

	Strings strings_;

	/**
	 * Handles the mapping between IP addresses and hostnames
	 */
	struct Hostnames : public TextFileHandler,
					   public std::map< uint32, std::string >
	{
		bool init( const char *root, const char *mode );
		virtual bool handleLine( const char *line );
		virtual void flush();
		const char * resolve( uint32 addr );
		uint32 resolve( const char *hostname ) const;
	};

	Hostnames hostnames_;

	/**
	 * Handles the mapping between ids and component names, i.e. 0 -> cellapp,
	 * 1 -> baseapp etc.  This is shared amongst all users, and is based on the
	 * order in which messages from unique components are delivered.
	 */
	struct ComponentNames : public TextFileHandler,
							public std::vector< std::string >
	{
		static const unsigned MAX_COMPONENTS = 31;

		bool init( const char *root, const char *mode );
		virtual void flush();
		virtual bool handleLine( const char *line );
		int resolve( const std::string &name );
		const char * resolve( int ttypeid ) const;
	};

	ComponentNames componentNames_;

	/**
	 * The registry of processes within each UserLog.  Contains static info
	 * about each process such as name, pid, etc.
	 */
	class Component;
	class Components : public BinaryFileHandler
	{
	public:
		Components( BWLog &log );
		virtual ~Components();
		bool init( const char *root, const char *mode );
		virtual bool read();
		virtual void flush();
		bool write( Component &component );
		Component *resolve( int id );
		Component *resolve( const LoggerComponentMessage& msg,
			const Mercury::Address &addr );
		Component *resolve( const Mercury::Address &addr );
		bool erase( const Mercury::Address &addr );
		int getId() { return idTicker_++; }
		const char *filename() const { return filename_.c_str(); }

		typedef std::map< int, Component* > IdMap;
		typedef std::map< Mercury::Address, Component* > AddrMap;

		const IdMap &idMap() const { return idMap_; }

	protected:
		std::string filename_;
		AddrMap addrMap_;
		IdMap idMap_;
		BWLog &log_;
		int idTicker_;
	};

	/**
	 * A Component represents a persistent process somewhere that is sending to
	 * this log.
	 */
	struct Component
	{
		Component( Components &components );
		Component( Components &components, const Mercury::Address &addr,
			const LoggerComponentMessage &msg, int ttypeid );

		Components &components_;
		Mercury::Address addr_;
		int id_;		// Unique ID per Component object
		int appid_;		// ID known as amongst server components, eg. cellapp01
		int typeid_;	// Process-type ID assigned to cellapp, baseapp etc
		LoggerComponentMessage msg_;
		EntryAddress firstEntry_;
		int fileOffset_;

		void write( FileStream &out );
		void read( FileStream &in );
		bool written() const { return fileOffset_ != -1; }
		bool setAppID( int id );
		std::string str() const;
	};

	Component* getComponent( const Mercury::Address &addr );

	/**
	 * A segment of a user's log.  This really means a pair of entries and args
	 * files.  NOTE: At the moment, each Segment always has two FileStreams
	 * open, which means that two Queries can't be executed at the same time in
	 * the same process, and also that if a log has many segments, then the
	 * number of open file handles for this process will be excessive.
	 */
	struct Segment
	{
		Segment( UserLog &userLog, const char *mode,
			const char *suffix = NULL );
		~Segment();

		void calculateLengths();
		inline bool good() const { return good_; }
		inline bool seek( int n )
		{
			return pEntries_->seek( n * sizeof( Entry ) );
		}

		struct cmp
		{
			bool operator() ( const Segment *a, const Segment *b )
			{
				return a->start_ < b->start_;
			}
		};

		static int filter( const struct dirent *ent )
		{
			return !strncmp( "entries.", ent->d_name, 8 );
		}

		bool full() const;
		bool dirty() const;
		bool addEntry( Component &component, Entry &entry,
			LoggingStringHandler &handler, MemoryIStream &is );
		bool readEntry( int n, Entry &entry );
		int find( LogTime &time, int direction );

		UserLog &userLog_;
		bool good_;
		std::string suffix_;
		std::string mode_;
		FileStream *pEntries_, *pArgs_;
		FILE *pText_;
		int nEntries_;
		int argsSize_;
		LogTime start_, end_;

		static char s_filenameBuf_[ 1024 ];
	};

	typedef std::vector< Segment* > Segments;

	/**
	 * An iterator over a specified range of a user's log.
	 */
	struct QueryParams;
	struct Range : public SafeReferenceCount
	{
		struct iterator
		{
			iterator( Range &range, int segmentNum = -1, int entryNum = -1,
				int metaOffset = 0 );
			bool good() const;
			static iterator error( Range &range ) { return iterator( range ); }
			bool operator<( const iterator &other ) const;
			bool operator<=( const iterator &other ) const;
			bool operator==( const iterator &other ) const;
			iterator& operator++();
			iterator& operator--();
			void step( int direction );
			int operator-( const iterator &other ) const;
			Segment& segment();
			const Segment& segment() const;
			EntryAddress addr() const;
			std::string str() const;
			Range *pRange_;
			int segmentNum_;
			union
			{
				int entryNum_;
				int argsOffset_;
			};
			int metaOffset_;
		};

		Range( UserLog &userLog, QueryParams &params );

		iterator findSentinel( int direction );
		bool getNextEntry( Entry &entry );
		BinaryIStream* getArgs();
		bool seek( int segmentNum, int entryNum, int metaOffset,
			int postIncrement = 0 );
		void rewind();
		void resume();

		UserLog &userLog_;
		LogTime startTime_, endTime_;
		EntryAddress startAddress_, endAddress_;
		int direction_;
		iterator begin_, curr_, end_, args_;
	};

	typedef SmartPointer< Range > RangePtr;

	/**
	 * A UserLog manages a single user's section of a log.  It is mainly
	 * responsible for managing the monolithic files in the user's directory.
	 * The segmented files are managed by Segments.
	 */
	class UserLog : public PyObjectPlus
	{
		Py_InstanceHeader( UserLog );

	public:
		UserLog( BWLog &log, uint16 uid,
			std::string &username, const char *mode );

	private:
		~UserLog();

	public:
		bool good() const { return good_; }
		BWLog *pLog() const { return &log_; }

		int getSegment( const char *suffix ) const;
		bool loadSegments();
		bool resume();

		bool addEntry( Component &component, Entry &entry,
			LoggingStringHandler &handler, MemoryIStream &is );

		bool getEntry( const EntryAddress &addr, Entry &result,
			Segment **ppSegment = NULL, Range **ppRange = NULL,
			bool warn = true );

		bool getEntry( double time, Entry &result );

		const char *format( const Component &component,
			const Entry &entry,	LoggingStringHandler &handler,
			MemoryIStream &is, bool useOldFormat = false ) const;

		BWLog &log_;
		uint16 uid_;
		std::string username_;
		std::string path_;
		bool good_;

		Segments segments_;
		Components components_;
		IntFile uidfile_;

		PyObject* pyGetAttribute( const char *attr );
		PY_RO_ATTRIBUTE_DECLARE( uid_, uid );
		PY_RO_ATTRIBUTE_DECLARE( username_, username );
		PY_RO_ATTRIBUTE_DECLARE( pLog(), log );
		PY_METHOD_DECLARE( py_getSegments );
		PY_METHOD_DECLARE( py_getComponents );
		PY_METHOD_DECLARE( py_getEntry );
	};

	typedef SmartPointer< UserLog > UserLogPtr;

	typedef std::map< uint16, UserLogPtr > UserLogs;
	UserLogs userLogs_;

	typedef std::map< uint16, std::string > Usernames;
	Usernames usernames_;

	UserLogPtr getUserLog( uint16 uid );

	/**
	 * Query parameters.
	 */
	struct QueryParams : public SafeReferenceCount
	{
		QueryParams( PyObject *args, PyObject *kwargs, BWLog &log );
		~QueryParams();

		inline bool good() const { return good_; }

		uint16 uid_;
		LogTime start_, end_;
		EntryAddress startAddress_, endAddress_;
		uint32 addr_;
		uint16 pid_;
		uint16 appid_;
		int procs_, severities_;
		regex_t *pInclude_, *pExclude_;
		int interpolate_;
		bool casesens_;
		int direction_;
		int context_;

		bool good_;
	};

public:
	/**
	 * This is the python object returned from a Query.
	 */
	class Result : public PyObjectPlus
	{
		Py_InstanceHeader( Result );

	public:
		Result();
		Result( const Entry &entry, BWLog &log, const UserLog &userLog,
			const Component &component, const std::string &message );

	private:
		~Result();

	public:
		double time_;
		const char * host_;
		int pid_;
		int appid_;
		const char * username_;
		const char * component_;
		int severity_;

		// message_ needs its own memory since an outer context may well
		// overwrite the buffer provided during construction
		std::string message_;

		// We also store the format string offset as this makes computing
		// histograms faster.
		uint32 stringOffset_;

		PyObject* pyGetAttribute( const char *attr );
		int pySetAttribute( const char * attr, PyObject * value );

		// TODO: Might be useful to include the file offset of the format string
		// here for easy format string grouping

		PY_RO_ATTRIBUTE_DECLARE( time_, time );
		PY_RO_ATTRIBUTE_DECLARE( host_, host );
		PY_RO_ATTRIBUTE_DECLARE( pid_, pid );
		PY_RO_ATTRIBUTE_DECLARE( appid_, appid );
		PY_RO_ATTRIBUTE_DECLARE( username_, username );
		PY_RO_ATTRIBUTE_DECLARE( component_, component );
		PY_RO_ATTRIBUTE_DECLARE( severity_, severity );
		PY_RO_ATTRIBUTE_DECLARE( message_, message );
		PY_RO_ATTRIBUTE_DECLARE( stringOffset_, stringOffset );

		const char *format( unsigned flags = SHOW_ALL, int *pLen = NULL );
		const char *formatOld();
		PY_METHOD_DECLARE( py_format );

		static char s_linebuf_[];
	};

	typedef SmartPointer< Result > ResultPtr;

	/**
	 * Generator-style object for iterating over query results.
	 */
	class Query : public PyObjectPlus
	{
		Py_InstanceHeader( Query );

	public:
		Query( BWLog *pLog, QueryParams *pParams, UserLog *pUserLog );

		PyObject* pyGetAttribute( const char *attr );
		PyObject *next();
		Result* getResultForEntry( Entry &entry, bool filter );
		void interpolate( LoggingStringHandler &handler,
			RangePtr pRange, std::string &dest );

		PY_METHOD_DECLARE( py_get );
		PY_METHOD_DECLARE( py_inReverse );
		PY_METHOD_DECLARE( py_getProgress );
		PY_METHOD_DECLARE( py_resume );
		PY_METHOD_DECLARE( py_tell );
		PY_METHOD_DECLARE( py_seek );
		PY_METHOD_DECLARE( py_step );
		PY_METHOD_DECLARE( py_setTimeout );

	protected:
		SmartPointer< BWLog > pLog_;
		SmartPointer< Range > pRange_;
		SmartPointer< QueryParams > pParams_;
		SmartPointer< UserLog > pUserLog_;

		ResultPtr pContextResult_;
		Range::iterator contextPoint_, contextCurr_, mark_;
		bool separatorReturned_;

		PyObjectPtr pCallback_;
		float timeout_;
		int timeoutGranularity_;
	};

	typedef SmartPointer< Query > QueryPtr;

private:
	// Deriving from PyObjectPlus, use Py_DECREF rather than delete
	~BWLog();
};

typedef SmartPointer< BWLog > BWLogPtr;

#endif
