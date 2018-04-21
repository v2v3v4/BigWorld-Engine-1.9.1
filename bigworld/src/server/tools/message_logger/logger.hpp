/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LOGGER_HPP
#define LOGGER_HPP

// #include "network/endpoint.hpp"
#include "network/logger_message_forwarder.hpp"
#include "network/watcher_nub.hpp"
#include "network/machine_guard.hpp"

#include "logging_string_handler.hpp"
#include "bwlog.hpp"

/**
 *	This is the main class of the message_logger process. It is responsible for
 *	receiving log messages from other components.
 */
class BWLog;

class Logger : public StandardWatcherRequestHandler
{
public:
	Logger();
	virtual ~Logger();

	bool init( int argc, char * argv[] );
	bool handleNextMessage();

	BWLogPtr pLog() { return pLog_; }

	void shouldRoll( bool status ) { shouldRoll_ = status; }

protected:
	virtual void processExtensionMessage( int messageID,
			char * data, int dataLen, const Mercury::Address & addr );

public:
	class Component : public LoggerComponentMessage
	{
	public:
		static Watcher & watcher();
		const char *name() const { return componentName_.c_str(); }

		// TODO: Fix this
		bool commandAttached() const	{ return true; }
		void commandAttached( bool value );
	};

private:
	class FindHandler : public MachineGuardMessage::ReplyHandler
	{
	public:
		FindHandler( Logger &logger ) : logger_( logger ) {}
		virtual bool onProcessStatsMessage(
			ProcessStatsMessage &psm, uint32 addr );
	private:
		Logger &logger_;
	};

	void findComponents();

	void handleBirth( const Mercury::Address & addr );
	void handleDeath( const Mercury::Address & addr );

	void handleLogMessage( MemoryIStream &is, const Mercury::Address & addr );
	void handleRegisterRequest(
			char * data, int dataLen, const Mercury::Address & addr );

	bool shouldConnect( const Component & component ) const;

	void sendAdd( const Mercury::Address & addr );
	void sendDel( const Mercury::Address & addr );

	void delComponent( const Mercury::Address & addr, bool send = true );
	void delComponent( Component * pComponent );

	bool commandReattachAll() const { return true; }
	void commandReattachAll( bool value );

	bool resetFileDescriptors();

	Endpoint & socket()		{ return watcherNub_.socket(); }

	// Watcher
	int size() const	{ return components_.size(); }

	std::string interfaceName_;
	WatcherNub watcherNub_;

	// ID of the processes whose messages should be logged — value
    // range is 0-255. The default is 0, a special value that causes
	// logging all processes, regardless of loggerID.
	int loggerID_;
	
	uint logUser_;
	bool logAllUsers_;
	std::vector< std::string > logNames_;
	std::vector< std::string > doNotLogNames_;
	bool quietMode_;
	bool daemonMode_;
	bool shouldRoll_;

	std::string outputFilename_;
	std::string errorFilename_;

	std::string addLoggerData_;
	std::string delLoggerData_;

	typedef std::map< Mercury::Address, Component > Components;
	Components components_;

	bool shouldLogMessagePriority_[ NUM_MESSAGE_PRIORITY ];

	BWLogPtr pLog_;
};


#endif // LOGGER_HPP
