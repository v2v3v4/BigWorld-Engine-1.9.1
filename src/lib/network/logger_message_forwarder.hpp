/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LOGGER_MESSAGE_FORWARDER_HPP
#define LOGGER_MESSAGE_FORWARDER_HPP

#include "cstdmf/config.hpp"
#if ENABLE_WATCHERS

#include "cstdmf/singleton.hpp"

#include "network/channel.hpp"
#include "network/endpoint.hpp"
#include "network/interfaces.hpp"
#include "network/watcher_nub.hpp"
#include "network/watcher_glue.hpp"

#include "network/forwarding_string_handler.hpp"

#define MESSAGE_LOGGER_VERSION 	6
#define MESSAGE_LOGGER_NAME 	"message_logger"

enum
{
	MESSAGE_LOGGER_MSG = WATCHER_MSG_EXTENSION_START, // 107
	MESSAGE_LOGGER_REGISTER,
	MESSAGE_LOGGER_PROCESS_BIRTH,
	MESSAGE_LOGGER_PROCESS_DEATH,
	MESSAGE_LOGGER_APP_ID
};

const int LOGGER_MSG_SIZE = 2048;

#pragma pack( push, 1 )
/**
 *  The header section that appears at the start of each message sent to
 *  MessageLogger.
 */
struct LoggerMessageHeader
{
	// TODO: move componentPriority_ into LoggerComponentMessage
	uint8		componentPriority_;
	uint8		messagePriority_;
};
#pragma pack( pop )

#pragma pack( push, 1 )
/**
 * The message that is sent to MessageLogger to register with it.
 */
class LoggerComponentMessage
{
public:
	uint8					version_;
	uint8					loggerID_;
	uint16					uid_;
	uint32					pid_;
	std::string				componentName_;

	void write( BinaryOStream &os ) const;
	void read( BinaryIStream &is );
};
#pragma pack( pop )

/* A SimpleLoggerMessageForwarder is like the other kind but doesn't support
   suppression. It was created for the Web Integration module because it doesn't
   have a nub-centred execution path and changes its nub regularly. */

class SimpleLoggerMessageForwarder :
	public DebugMessageCallback
{
public:
	SimpleLoggerMessageForwarder(
		std::string appName,
		Endpoint & endpoint,
		uint8 loggerID = 0,
		bool enabled = true );

	virtual ~SimpleLoggerMessageForwarder();

	void registerAppID( int id );
protected:

	virtual bool handleMessage( int componentPriority,
		int messagePriority, const char * format, va_list argPtr );


	void addLogger( const Mercury::Address & addr );
	void delLogger( const Mercury::Address & addr );
	void sendAppID( const Mercury::Address & addr );

	void findLoggerInterfaces();

	class FindLoggerHandler : public MachineGuardMessage::ReplyHandler
	{
	public:
		FindLoggerHandler( SimpleLoggerMessageForwarder &lmf ) : lmf_( lmf ) {}
		virtual bool onProcessStatsMessage(
			ProcessStatsMessage &psm, uint32 addr );

	private:
		SimpleLoggerMessageForwarder &lmf_;
	};


	void parseAndSend( ForwardingStringHandler * pHandler,
		int componentPriority, int messagePriority, va_list argPtr );

	void parseAndSend( ForwardingStringHandler * pHandler,
		int componentPriority, int messagePriority, ... );

	typedef std::vector< Mercury::Address > Loggers;
	Loggers loggers_;

	std::string appName_;

	// The ID used by the process when registering with MessageLoggers.
    // If this ID does not match a MessageLogger's filter, the process will 
	// not log to that MessageLogger.
	uint8 loggerID_;
	
	int appID_;
	bool enabled_;

	/// This is the socket that is actually used for sending log messages.
	Endpoint & endpoint_;

	/// The collection of format string handlers that we have already seen.
	typedef std::map< std::string, ForwardingStringHandler* > HandlerCache;
	HandlerCache handlerCache_;

	ForwardingStringHandler* findForwardingStringHandler( const char * format );

	virtual bool isSuppressible( const std::string & format ) const;
};

/**
 *	This class is used to forward log messages to any attached loggers.
 */
class LoggerMessageForwarder :
	public SimpleLoggerMessageForwarder,
	public Mercury::TimerExpiryHandler,
	public Singleton< LoggerMessageForwarder >
{
public:
	LoggerMessageForwarder(
		std::string appName,
		Endpoint & endpoint,
		Mercury::Nub & nub,
		uint8 loggerID = 0,
		bool enabled = true,
		unsigned spamFilterThreshold = 10 );

	virtual ~LoggerMessageForwarder();

	std::string suppressionWatcherHack() const { return std::string(); }
	void addSuppressionPattern( std::string prefix );
	void delSuppressionPattern( std::string prefix );

protected:
	virtual bool handleMessage( int componentPriority,
		int messagePriority, const char * format, va_list argPtr );

	virtual int handleTimeout( Mercury::TimerID id, void * arg );

private:
	void init();

	Mercury::Address watcherHack() const;

	void watcherAddLogger( Mercury::Address addr ) { this->addLogger( addr ); }
	void watcherDelLogger( Mercury::Address addr ) { this->delLogger( addr ); }

	int size() const	{ return loggers_.size(); }

	bool isSuppressible( const std::string & format ) const;

	bool isSpamming( ForwardingStringHandler * pHandler ) const
	{
		return (spamFilterThreshold_ > 0) &&
			pHandler->isSuppressible() &&
			(pHandler->numRecentCalls() > spamFilterThreshold_);
	}

	void updateSuppressionPatterns();

	/// This is the nub we register a timer with for managing spam suppression.
	Mercury::Nub & nub_;

	/// A list of the format string prefixes that we will suppress.
	typedef std::vector< std::string > SuppressionPatterns;
	SuppressionPatterns suppressionPatterns_;

	/// The timer ID for managing spam suppression.
	Mercury::TimerID spamTimerID_;

	/// The maximum number of times a particular format string can be emitted
	/// each second.
	unsigned spamFilterThreshold_;

	/// The forwarding string handler that is used for sending spam summaries.
	ForwardingStringHandler spamHandler_;

	/// A collection of all the handlers that have been used since the last time
	/// handleTimeout() was called.
	typedef std::vector< ForwardingStringHandler* > RecentlyUsedHandlers;
	RecentlyUsedHandlers recentlyUsedHandlers_;
};


/*
 *	This method is used to add the ability to forward messages to loggers.
 */
#define BW_MESSAGE_FORWARDER2( NAME, CONFIG_PATH, ENABLED, NUB )			\
	std::string monitoringInterfaceName =									\
		BWConfig::get( #CONFIG_PATH "/monitoringInterface",					\
				BWConfig::get( "monitoringInterface", "" ) );				\
																			\
	NUB.setLossRatio( BWConfig::get( #CONFIG_PATH "/internalLossRatio",		\
				BWConfig::get( "internalLossRatio", 0.f ) ) );				\
	NUB.setLatency(															\
			BWConfig::get( #CONFIG_PATH "/internalLatencyMin",				\
				BWConfig::get( "internalLatencyMin", 0.f ) ),				\
			BWConfig::get( #CONFIG_PATH "/internalLatencyMax",				\
				BWConfig::get( "internalLatencyMax", 0.f ) ) );				\
																			\
	NUB.setIrregularChannelsResendPeriod(									\
			BWConfig::get( #CONFIG_PATH "/irregularResendPeriod",			\
				BWConfig::get( "irregularResendPeriod",						\
					1.5f / BWConfig::get( "gameUpdateHertz", 10.f ) ) ) );	\
																			\
	NUB.shouldUseChecksums(													\
		BWConfig::get( #CONFIG_PATH "/shouldUseChecksums",					\
			BWConfig::get( "shouldUseChecksums", true ) ) );				\
																			\
	Mercury::Channel::setInternalMaxOverflowPackets(						\
		BWConfig::get( "maxChannelOverflow/internal",						\
		Mercury::Channel::getInternalMaxOverflowPackets() ));				\
																			\
	Mercury::Channel::setIndexedMaxOverflowPackets(							\
		BWConfig::get( "maxChannelOverflow/indexed",						\
		Mercury::Channel::getIndexedMaxOverflowPackets() ));				\
																			\
	Mercury::Channel::setExternalMaxOverflowPackets(						\
		BWConfig::get( "maxChannelOverflow/external",						\
		Mercury::Channel::getExternalMaxOverflowPackets() ));				\
																			\
	Mercury::Channel::assertOnMaxOverflowPackets(							\
		BWConfig::get( "maxChannelOverflow/isAssert",						\
		Mercury::Channel::assertOnMaxOverflowPackets() ));					\
																			\
	if (monitoringInterfaceName == "")										\
	{																		\
		monitoringInterfaceName =											\
						inet_ntoa( (struct in_addr &)NUB.address().ip );	\
	}																		\
																			\
	WatcherGlue watcherGlue;												\
	watcherGlue.init( monitoringInterfaceName.c_str(), 0 );					\
																			\
	unsigned spamFilterThreshold =											\
		BWConfig::get( #CONFIG_PATH "/logSpamThreshold",					\
			BWConfig::get( "logSpamThreshold", 20 ) );						\
																			\
	LoggerMessageForwarder lForwarder( #NAME,								\
		watcherGlue.socket(), NUB,											\
		BWConfig::get( "loggerID", 0 ), ENABLED, spamFilterThreshold );		\
																			\
	DataSectionPtr pSuppressionPatterns =									\
		BWConfig::getSection( #CONFIG_PATH "/logSpamPatterns",				\
			BWConfig::getSection( "logSpamPatterns" ) );					\
																			\
	if (pSuppressionPatterns)												\
	{																		\
		for (DataSectionIterator iter = pSuppressionPatterns->begin();		\
			 iter != pSuppressionPatterns->end(); ++iter)					\
		{																	\
			lForwarder.addSuppressionPattern( (*iter)->asString() );		\
		}																	\
	}																		\
																			\
	if (BWConfig::isBad())													\
	{																		\
		return 0;															\
	}																		\
	(void)0

#define BW_MESSAGE_FORWARDER( NAME, CONFIG_PATH, NUB )							\
	BW_MESSAGE_FORWARDER2( NAME, CONFIG_PATH, true, NUB )

#endif /* ENABLE_WATCHERS */

#endif // LOGGER_MESSAGE_FORWARDER_HPP
