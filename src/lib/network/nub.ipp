/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef CODE_INLINE
    #define INLINE    inline
#else
	/// INLINE macro.
    #define INLINE
#endif

namespace Mercury
{

// -----------------------------------------------------------------------------
// Section: NubException
// -----------------------------------------------------------------------------

/**
 * 	This is the default constructor for a NubException.
 */
INLINE NubException::NubException( Reason reason ) :
	reason_(reason)
{}


/**
 * 	This method returns the reason for the exception.
 *
 * 	@see Mercury::Reason
 */
INLINE Reason NubException::reason() const
{
	return reason_;
}


/**
 * 	This method returns the address for which this exception
 * 	occurred. There is no address associated with the base
 * 	class of this exception, and this method is intended to
 * 	be overridden.
 */
INLINE bool NubException::getAddress( Address & /*addr*/ ) const
{
	return false;
}


/**
 * 	This is the default constructor for a NubExceptionWithAddress.
 *
 * 	@param reason	Reason for the exception.
 * 	@param addr		Address associated with this exception.
 */
INLINE NubExceptionWithAddress::NubExceptionWithAddress(
	Reason reason, const Address & addr ) :
		NubException(reason), address_(addr)
{
}


/**
 * 	This method returns the address associated with this
 * 	exception.
 *
 *	@param addr		The address is returned here.
 *
 * 	@return This method will always return true, since there
 * 			will always be a valid address for this type
 * 			of exception.
 */
INLINE bool NubExceptionWithAddress::getAddress( Address & addr ) const
{
	addr = address_;
	return true;
}


// -----------------------------------------------------------------------------
// Section: Nub
// -----------------------------------------------------------------------------

/**
 * 	This method returns the socket associated with the Nub.
 */
INLINE int Nub::socket() const
{
	return socket_;
}


/**
 *	This method increments the corrupted packet count associated with the Nub
 *	and returns the appropriate Mercury::Reason.
 */
INLINE Mercury::Reason Nub::receivedCorruptedPacket()
{
	++numCorruptedPacketsReceived_;
	return REASON_CORRUPTED_PACKET;
}



/**
 * Call the handler every microseconds.
 *
 * Timers cannot be longer than 30 minutes (size of int)
 *
 * @return ID of timer
 */
INLINE TimerID Nub::registerTimer( int microseconds,
	TimerExpiryHandler * handler, void * arg )
{
	return this->newTimer( microseconds, handler, arg, true );
}


/**
 * Call the handler once after microseconds.
 *
 * Timers cannot be longer than 30 minutes (size of int)
 *
 * @return ID of timer
 */
INLINE TimerID Nub::registerCallback( int microseconds,
	TimerExpiryHandler * handler, void * arg )
{
	return this->newTimer( microseconds, handler, arg, false );
}


/**
 *	This method sets the minimum and maximum latency associated with this
 *	Nub. If non-zero, packets will be randomly delayed before they are sent.
 *
 *	@param latencyMin	The minimum latency in seconds
 *	@param latencyMax	The maximum latency in seconds
 */
INLINE void Nub::setLatency( float latencyMin, float latencyMax )
{
	// Convert to milliseconds
	artificialLatencyMin_ = int( latencyMin * 1000 );
	artificialLatencyMax_ = int( latencyMax * 1000 );
}


/**
 * 	This method sets the average packet loss for this Nub.  If non-zero, packets
 * 	will be randomly dropped, and not sent.
 *
 * 	@param lossRatio	The ratio of packets to drop. Setting to 0.0 disables
 *		artificial packet dropping. 1.0 means all packets are dropped.
 */
INLINE void Nub::setLossRatio( float lossRatio )
{
	artificialDropPerMillion_ = int( lossRatio * 1000000 );
}


/**
 *	This method registers a handler to monitor sent/received packets.
 *	Only a single monitor is supported currently. To remove the
 *	monitor, set it to NULL.
 *
 *	@param pPacketMonitor	An object implementing the PacketMonitor
 *							interface.
 */
INLINE void Nub::setPacketMonitor(PacketMonitor* pPacketMonitor)
{
	pPacketMonitor_ = pPacketMonitor;
}


/**
 * 	This method returns the total number of packets received
 * 	by the Nub.
 *
 * 	@return Number of packets received.
 */
INLINE unsigned int Nub::numPacketsReceived() const
{
	return numPacketsReceived_;
}


/**
 * 	This method returns the total number of messages received
 * 	by the Nub.
 *
 * 	@return Number of messages received.
 */
INLINE unsigned int Nub::numMessagesReceived() const
{
	return numMessagesReceived_;
}


/**
 * 	This method returns the total number of bytes received
 * 	by the Nub.
 *
 * 	@return Number of bytes received.
 */
INLINE unsigned int Nub::numBytesReceived() const
{
	return numBytesReceived_;
}


/**
 * 	This method returns the total number of bytes received
 * 	by the Nub that not part of any message.
 *
 * 	@return Number of overhead bytes received.
 */
INLINE unsigned int Nub::numOverheadBytesReceived() const
{
	return numOverheadBytesReceived_;
}


/**
 *	This method returns whether this nub should be verbose with its log
 *	messages.
 */
INLINE bool Nub::isVerbose() const
{
	return isVerbose_;
}


/**
 *	This method sets whether this nub should be verbose with its log messages.
 */
INLINE void Nub::isVerbose( bool value )
{
	isVerbose_ = value;
}


// -----------------------------------------------------------------------------
// Section: Helper methods for Bytes watcher
// -----------------------------------------------------------------------------

/**
 *  This method returns the average bytes sent per second
 *  from the Nub was started
 *
 *  @return Bytes sent per second
 */
INLINE double Nub::bytesSentPerSecondAvg() const
{
	return numBytesSent_ / delta();
}


/**
 *  This method returns the average bytes sent per second
 *  from the last time this method was called
 *
 *  @return Bytes sent per second
 */
INLINE double Nub::bytesSentPerSecondPeak() const
{
	return peakCalculator( LVT_BytesSent, numBytesSent_, lastNumBytesSent_ );
}


/**
 *  This method returns the average bytes received per second
 *  from the Nub was started
 *
 *  @return Bytes received per second
 */
INLINE double Nub::bytesReceivedPerSecondAvg() const
{
	return numBytesReceived_ / delta();
}


/**
 *  This method returns the average bytes received per second
 *  from the last time this method was called
 *
 *  @return Bytes received per second
 */
INLINE double Nub::bytesReceivedPerSecondPeak() const
{
	return peakCalculator( LVT_BytesReceived, numBytesReceived_, lastNumBytesReceived_ );
}

// -----------------------------------------------------------------------------
// Section: Helper methods for Packets watcher
// -----------------------------------------------------------------------------

/**
 *  This method returns the average packets sent per second
 *  from the Nub was started
 *
 *  @return Packets sent per second
 */
INLINE double Nub::packetsSentPerSecondAvg() const
{
	return numPacketsSent_ / delta();
}


/**
 *  This method returns the average packets sent per second
 *  from the last time this method was called
 *
 *  @return Packets sent per second
 */
INLINE double Nub::packetsSentPerSecondPeak() const
{
	return peakCalculator( LVT_PacketsSent, numPacketsSent_, lastNumPacketsSent_ );
}


/**
 *  This method returns the average packets received per second
 *  from the Nub was started
 *
 *  @return Packets received per second
 */
INLINE double Nub::packetsReceivedPerSecondAvg() const
{
	return numPacketsReceived_ / delta();
}


/**
 *  This method returns the average packets received per second
 *  from the last time this method was called
 *
 *  @return Packets received per second
 */
INLINE double Nub::packetsReceivedPerSecondPeak() const
{
	return peakCalculator( LVT_PacketsReceived, numPacketsReceived_, lastNumPacketsReceived_ );
}

// -----------------------------------------------------------------------------
// Section: Helper methods for Bundles watcher
// -----------------------------------------------------------------------------


/**
 *  This method returns the average bundles sent per second
 *  from the Nub was started
 *
 *  @return Bundles sent per second
 */
INLINE double Nub::bundlesSentPerSecondAvg() const
{
	return numBundlesSent_ / delta();
}


/**
 *  This method returns the average bundles sent per second
 *  from the last time this method was called
 *
 *  @return Bundles sent per second
 */
INLINE double Nub::bundlesSentPerSecondPeak() const
{
	return peakCalculator( LVT_BundlesSent, numBundlesSent_, lastNumBundlesSent_ );
}


/**
 *  This method returns the average bundles received per second
 *  from the Nub was started
 *
 *  @return Bundles received per second
 */
INLINE double Nub::bundlesReceivedPerSecondAvg() const
{
	return numBundlesReceived_ / delta();
}


/**
 *  This method returns the average bundles received per second
 *  from the last time this method was called
 *
 *  @return Bundles sent per second
 */
INLINE double Nub::bundlesReceivedPerSecondPeak() const
{
	return peakCalculator( LVT_BundlesReceived, numBundlesReceived_, lastNumBundlesReceived_ );
}


// -----------------------------------------------------------------------------
// Section: Helper methods for Messages watcher
// -----------------------------------------------------------------------------


/**
 *  This method returns the average messages sent per second
 *  from the Nub was started
 *
 *  @return Messages sent per second
 */
INLINE double Nub::messagesSentPerSecondAvg() const
{
	return numMessagesSent_ / delta();
}


/**
 *  This method returns the average messages sent per second
 *  from the last time this method was called
 *
 *  @return Messages sent per second
 */
INLINE double Nub::messagesSentPerSecondPeak() const
{
	return peakCalculator( LVT_MessagesSent, numMessagesSent_, lastNumMessagesSent_ );
}


/**
 *  This method returns the average messages received per second
 *  from the Nub was started
 *
 *  @return Messages received per second
 */
INLINE double Nub::messagesReceivedPerSecondAvg() const
{
	return numMessagesReceived_ / delta();
}


/**
 *  This method returns the average messages received per second
 *  from the last time this method was called
 *
 *  @return Messages received per second
 */
INLINE double Nub::messagesReceivedPerSecondPeak() const
{
	return peakCalculator( LVT_MessagesReceived, numMessagesReceived_, lastNumMessagesReceived_ );
}


/**
 * 	This method returns the total number of bytes received
 * 	by the Nub for a certain message ID
 *
 * 	@return Number of bytes received.
 */
INLINE unsigned int Nub::numBytesReceivedForMessage( uint8 id ) const
{
	return interfaceTable_[id].numBytesReceived();
}


/**
 * 	This method returns the total number of messages received
 * 	by the Nub for a certain message ID
 *
 * 	@return Number of messages received.
 */
INLINE unsigned int Nub::numMessagesReceivedForMessage( uint8 id ) const
{
	return interfaceTable_[id].numMessagesReceived();
}


/**
 *	This is the constructor.
 */
INLINE Nub::MiniTimer::MiniTimer() :
	total( 0 ),
	last( 0 ),
	min( 1000000000 ),
	max( 0 ),
	count( 0 )
{
}


/**
 * 	This method should be called before starting the operation
 * 	that is being timed.
 */
INLINE void Nub::MiniTimer::start()
{
	sofar = timestamp();
}


/**
 *	This method should be called after the operation is
 *	complete. If the operation is complete (and is not just
 *	being paused), the increment parameter should be true.
 *
 *	@param increment	true if the operation is complete.
 */
INLINE void Nub::MiniTimer::stop( bool increment )
{
	last += timestamp() - sofar;
	if (increment)
	{
		if (last > max) max = last;
		if (last < min) min = last;
		total += last;
		count++;
		last = 0;
	}
}


/**
 *	This method resets all the counters and timers to zero.
 */
INLINE void Nub::MiniTimer::reset()
{
	*this = MiniTimer();
}


/**
 *	This method returns the minimum duration in seconds.
 */
INLINE double Nub::MiniTimer::getMinDurationSecs() const
{
	return double(min)/stampsPerSecond();
}


/**
 *	This method returns the maximum duration in seconds.
 */
INLINE double Nub::MiniTimer::getMaxDurationSecs() const
{
	return double(max)/stampsPerSecond();
}


/**
 *	This method returns the average duration in seconds.
 */
INLINE double Nub::MiniTimer::getAvgDurationSecs() const
{
	return (count > 0) ? (double(total)/count)/stampsPerSecond() : 0.0;
}


/**
 *	This is the constructor.
 *
 *	@param resetPeriodSecs The number of seconds between resets (sort of).
 */
INLINE Nub::TransientMiniTimer::TransientMiniTimer(int resetPeriodSecs)
	: Nub::MiniTimer()
	, resetPeriodStamp_(resetPeriodSecs * stampsPerSecond())
	, resetTimeStamp_(timestamp())
{}


/**
 * 	This method should be called before starting the operation
 * 	that is being timed.
 */
INLINE void Nub::TransientMiniTimer::start()
{
	if ((timestamp() - resetTimeStamp_) > resetPeriodStamp_)
		reset();
	Nub::MiniTimer::start();
}


/**
 *	This method should be called after the operation is
 *	complete.
 */
INLINE void Nub::TransientMiniTimer::stop()
{
	Nub::MiniTimer::stop(true);
}

/**
 *	This method resets all the counters and timers to zero.
 */
INLINE void Nub::TransientMiniTimer::reset()
{
	resetTimeStamp_ = timestamp();
	Mercury::Nub::MiniTimer::reset();
}


/**
 *	This method returns the number of seconds since our last reset.
 */
INLINE double Nub::TransientMiniTimer::getElapsedResetSecs() const
{
	return double(timestamp() - resetTimeStamp_)/stampsPerSecond();
}


/**
 *	This method returns the rate of operations.
 *
 *	@return	Number of operations per second i.e. the number of start/stops
 *	per seconds.
 */
INLINE double Nub::TransientMiniTimer::getCountPerSec() const
{
	return double(count)/getElapsedResetSecs();
}


/**
 * 	@internal
 * 	This method compares two FragmentedBundle::Key objects.
 * 	It is needed for storing fragmented bundles in a map.
 */
INLINE bool operator==( const Nub::FragmentedBundle::Key & a,
	const Nub::FragmentedBundle::Key & b )
{
	return (a.firstFragment_ == b.firstFragment_) && (a.addr_ == b.addr_);
}


/**
 * 	@internal
 * 	This method compares two FragmentedBundle::Key objects.
 * 	It is needed for storing fragmented bundles in a map.
 */
INLINE bool operator<( const Nub::FragmentedBundle::Key & a,
	const Nub::FragmentedBundle::Key & b )
{
	return (a.firstFragment_ < b.firstFragment_) ||
		(a.firstFragment_ == b.firstFragment_ && a.addr_ < b.addr_);
}


/**
 *	This method prepares this nub for shutting down this process. It will stop
 *	pinging anonymous channels.
 */
INLINE void Nub::prepareToShutDown()
{
	keepAliveChannels_.stopMonitoring( *this );
}

}; // namespace Mercury

// nub.ipp
