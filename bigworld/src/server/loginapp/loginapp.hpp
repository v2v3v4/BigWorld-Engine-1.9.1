/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LOGINAPP_HPP
#define LOGINAPP_HPP

#include "cstdmf/memory_stream.hpp"
#include "cstdmf/singleton.hpp"

#include "network/mercury.hpp"
#include "network/netmask.hpp"
#include "network/public_key_cipher.hpp"
#include "network/interfaces.hpp"

#include "resmgr/datasection.hpp"
#include "server/stream_helper.hpp"

#include "common/doc_watcher.hpp"
#include "math/ema.hpp"

#include "login_int_interface.hpp"


typedef Mercury::ChannelOwner DBMgr;

/**
 *	This class implements the main singleton object in the login application.
 */
class LoginApp : public Singleton< LoginApp >
{
public:
	LoginApp( Mercury::Nub & intNub, uint16 loginPort );

	bool init( int argc, char * argv[], uint16 loginPort );
	void run();

	// external methods

	virtual void login( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	virtual void probe( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

	// internal methods
	void controlledShutDown( const Mercury::Address & source,
		Mercury::UnpackedMessageHeader & header, BinaryIStream & data );

//	void systemOverloadStatus(
//		const LoginIntInterface::systemOverloadStatusArgs & args );


	const NetMask& 	netMask() const			{ return netMask_; }
	unsigned long	externalIP() const 		{ return externalIP_; }

public:
	void sendFailure( const Mercury::Address & addr,
		Mercury::ReplyID replyID, int status, const char * msg = NULL,
		LogOnParamsPtr pParams = NULL );

	void sendAndCacheSuccess( const Mercury::Address & addr,
			Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
			LogOnParamsPtr pParams );

	Mercury::Nub &	intNub()		{ return intNub_; }
	Mercury::Nub &	extNub()		{ return extNub_; }

	DBMgr & dbMgr()					{ return *dbMgr_.pChannelOwner(); }
	const DBMgr & dbMgr() const		{ return *dbMgr_.pChannelOwner(); }

	bool isDBReady() const
	{
		return this->dbMgr().channel().isEstablished();
	}

	void controlledShutDown()
	{
		isControlledShutDown_ = true;
		intNub_.breakProcessing();
	}

	uint64 maxLoginDelay() const	{ return maxLoginDelay_; }

	uint8 systemOverloaded() const
	{ return systemOverloaded_; }

	void systemOverloaded( uint8 status )
	{
		systemOverloaded_ = status;
		systemOverloadedTime_ = timestamp();
	}


private:
	/**
	 *	This class is used to store a recent, successful login. It is used to
	 *	handle the case where the reply to the client is dropped.
	 */
	class CachedLogin
	{
	public:
		// We set creationTime_ to 0 to indicate that the login is pending.
		CachedLogin() : creationTime_( 0 ) {}

		bool isTooOld() const;
		bool isPending() const;

		void pParams( LogOnParamsPtr pParams ) { pParams_ = pParams; }
		LogOnParamsPtr pParams() { return pParams_; }

		void replyRecord( const LoginReplyRecord & record );
		const LoginReplyRecord & replyRecord() const { return replyRecord_; }

		/// This method re-initialises the cache object to indicate that it is
		/// pending.
		void reset() { creationTime_ = 0; }

	private:
		uint64 creationTime_;
		LogOnParamsPtr pParams_;
		LoginReplyRecord replyRecord_;
	};

	bool handleResentPendingAttempt( const Mercury::Address & addr,
		Mercury::ReplyID replyID );
	bool handleResentCachedAttempt( const Mercury::Address & addr,
		LogOnParamsPtr pParams, Mercury::ReplyID replyID );

	void sendSuccess( const Mercury::Address & addr,
		Mercury::ReplyID replyID, const LoginReplyRecord & replyRecord,
		const std::string & encryptionKey );

	void rateLimitSeconds( uint newPeriod )
	{ rateLimitDuration_ = newPeriod * stampsPerSecond(); }
	uint rateLimitSeconds() const
	{ return rateLimitDuration_ / stampsPerSecond(); }

	static uint32 updateStatsPeriod() { return UPDATE_STATS_PERIOD; }

#ifdef USE_OPENSSL
	Mercury::PublicKeyCipher privateKey_;
#endif
	Mercury::Nub &		intNub_;
	Mercury::Nub		extNub_;

	NetMask 			netMask_;
	unsigned long 		externalIP_;

	bool				isControlledShutDown_;
	bool				isProduction_;

	uint8				systemOverloaded_;
	uint64				systemOverloadedTime_;

	bool				allowLogin_;
	bool				allowProbe_;
	bool				logProbes_;

	typedef std::map< Mercury::Address, CachedLogin > CachedLoginMap;
	CachedLoginMap 		cachedLoginMap_;

	AnonymousChannelClient dbMgr_;

	uint64 				maxLoginDelay_;

	bool 				allowUnencryptedLogins_;

	// Rate Limiting state

	// the time of the start of the last time block
	uint64 				lastRateLimitCheckTime_;
	// the number of logins left for this time block
	uint				numAllowedLoginsLeft_;
	// the number of logins allowed per time block
	int					loginRateLimit_;
	// the length of each time block for rate limiting
	uint64				rateLimitDuration_;

	static LoginApp * pInstance_;

	static const uint32 UPDATE_STATS_PERIOD;
	Mercury::TimerID 	statsTimerID_;

	/**
	 *	This class represents login statistics. These statistics are exposed to
	 *	watchers.
	 */
	class LoginStats: public Mercury::TimerExpiryHandler
	{
	public:
		/**
		 *	Constructor.
		 */
		LoginStats();

		/**
		 *	Overridden from TimerExpiryHandler.
		 */
		virtual int handleTimeout( Mercury::TimerID id, void * arg )
		{
			this->update();
			return 0;
		}

		// Incrementing accessors

		/**
		 *	Increment the count for rate-limited logins.
		 */
		void incRateLimited() 	{ ++all_.value(); ++rateLimited_.value(); }

		/**
		 *	Increment the count for failed logins.
		 */
		void incFails() 		{ ++all_.value(); ++fails_.value(); }

		/**
		 *	Increment the count for repeated logins (duplicate logins that came
		 *	in from the client while the original was pending.
		 */
		void incPending() 		{ ++all_.value(); ++pending_.value(); }

		/**
		 *	Increment the count for successful logins.
		 */
		void incSuccesses() 	{ ++all_.value(); ++successes_.value(); }

		// Average accessors

		/**
		 *	Return the failed logins per second average.
		 */
		float fails() const 		{ return fails_.average(); }

		/**
		 *	Return the rate-limited logins per second average.
		 */
		float rateLimited() const 	{ return rateLimited_.average(); }

		/**
		 *	Return the repeated logins (due to already pending login) per
		 *	second average.
		 */
		float pending() const 		{ return pending_.average(); }

		/**
		 *	Return the successful logins per second average.
		 */
		float successes() const 	{ return successes_.average(); }

		/**
		 *	Return the logins per second average.
		 */
		float all() const 			{ return all_.average(); }

		/**
		 *	This method updates the averages to the accumulated values.
		 */
		void update()
		{
			fails_.sample();
			rateLimited_.sample();
			successes_.sample();
			pending_.sample();
			all_.sample();
		}

	private:
		/// Failed logins.
		AccumulatingEMA< uint32 > fails_;
		/// Rate-limited logins.
		AccumulatingEMA< uint32 > rateLimited_;
		/// Repeated logins that matched a pending login.
		AccumulatingEMA< uint32 > pending_;
		/// Successful logins.
		AccumulatingEMA< uint32 > successes_;
		/// All logins.
		AccumulatingEMA< uint32 > all_;

		/// The bias for all the exponential averages.
		static const float BIAS;
	};

	LoginStats loginStats_;
};

#endif // LOGINAPP_HPP
