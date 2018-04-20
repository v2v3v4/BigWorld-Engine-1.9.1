/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BUNDLE_HPP
#define BUNDLE_HPP

#include "misc.hpp"
#include "packet.hpp"
#include "basictypes.hpp"
#include "cstdmf/binary_stream.hpp"

#include <map>

const bool MERCURY_DEFAULT_RELIABLE = true;

class Endpoint;

namespace Mercury
{

class Bundle;
class InterfaceElement;
class ReplyMessageHandler;
class Channel;

/**
 *	@internal
 *	This is the default request timeout in microseconds.
 */
const int DEFAULT_REQUEST_TIMEOUT = 5000000;


/**
 * 	This structure is returned by Mercury when delivering
 * 	messages to a client.
 *
 * 	@ingroup mercury
 */
class UnpackedMessageHeader
{
public:
	static const int NO_REPLY = -1;

	MessageID		identifier;		///< The message identifier.
	char			flags;			///< Message header flags.
	ReplyID			replyID;		///< A unique ID, used for replying.
	int				length;			///< The number of bytes in this message.
	Nub *			pNub;			///< The nub that received this message.
	Channel *		pChannel;		///< The channel that received this message.

	UnpackedMessageHeader() :
		identifier( 0 ), flags( 0 ),
		replyID( REPLY_ID_NONE ), length( 0 ), pNub( NULL ), pChannel( NULL )
	{}

	const char * msgName() const;
};



/**
 *	This class defines an interface that can be used to receive
 *	a callback whenever an incoming or outgoing packet passes
 *	through Mercury.
 *
 *	@see Nub::setPacketMonitor
 *
 *	@ingroup mercury
 */
class PacketMonitor
{
public:
	virtual ~PacketMonitor() {};

	/**
	 *	This method is called when Mercury sends a packet.
	 *
	 *	@param addr 	The destination address of the packet.
	 *	@param packet	The actual packet.
	 */
	virtual void packetOut(const Address& addr, const Packet& packet) = 0;

	/**
	 * 	This method is called when Mercury receives a packet, before
	 * 	it is processed.
	 *
	 * 	@param addr		The source address of the packet.
	 * 	@param packet	The actual packet.
	 */
	virtual void packetIn(const Address& addr, const Packet& packet) = 0;
};


/**
 * 	@internal
 * 	This structure is used to describe a reliable message. When
 * 	a message is added to a Bundle, it is streamed onto the end
 * 	of the last packet, and it is not easy to extract it. However
 * 	when a packet containing reliable data is dropped on a
 * 	connection between client and server, only the reliable data
 * 	is resent. The ReliableOrder structure is used to extract
 *	the reliable messages from a bundle that has already been
 *	sent.
 */
class ReliableOrder
{
public:
	uint8	* segBegin;				///< Pointer to the reliable segment
	uint16	segLength;				///< Length of the segment
	uint16 	segPartOfRequest;		///< True if it is part of a request
};

/**
 * 	@internal
 * 	The ReliableVector type is just a vector of ReliableOrders.
 */
typedef std::vector<ReliableOrder> ReliableVector;

/**
 *	There are three types of reliability. RELIABLE_PASSENGER messages will
 *	only be sent so long as there is at least one RELIABLE_DRIVER in the
 *	same Bundle.  RELIABLE_CRITICAL means the same as RELIABLE_DRIVER, however,
 *	starting a message of this type also marks the Bundle as critical.
 */
enum ReliableTypeEnum
{
	RELIABLE_NO = 0,
	RELIABLE_DRIVER = 1,
	RELIABLE_PASSENGER = 2,
	RELIABLE_CRITICAL = 3
};


/**
 *	This struct wraps a @see ReliableTypeEnum value.
 */
struct ReliableType
{
	ReliableType( ReliableTypeEnum e ) : e_( e ) { }
	ReliableType( bool b ) : e_( b ? RELIABLE_DRIVER : RELIABLE_NO ) { }

	bool isReliable() const	{ return e_ != RELIABLE_NO; }

	// Leveraging the fact that only RELIABLE_DRIVER and RELIABLE_CRITICAL share
	// the 0x1 bit.
	bool isDriver() const	{ return e_ & RELIABLE_DRIVER; }

	bool operator== (const ReliableTypeEnum e) { return e == e_; }

	ReliableTypeEnum e_;
};


/**
 * 	A bundle is a sequence of messages. You stream or otherwise
 * 	add your messages onto the bundle. When you want to send
 * 	a group of messages (possibly just one), you tell a nub
 * 	to send the bundle. Bundles can be sent multiple times
 * 	to different hosts, but beware that any requests inside
 * 	will also be made multiple times.
 *
 * 	@ingroup mercury
 */
class Bundle : public BinaryOStream
{
public:
	Bundle( uint8 spareSize = 0, Channel * pChannel = NULL );

	Bundle( Packet * packetChain );
	virtual ~Bundle();

	void clear( bool firstTime = false );

	bool isEmpty() const;

	int size() const;
	int sizeInPackets() const;
	bool isMultiPacket() const { return firstPacket_->next() != NULL; }
	int freeBytesInPacket();
	int numMessages() const		{ return numMessages_; }

	bool hasDataFooters() const;

	void startMessage( const InterfaceElement & ie,
		ReliableType reliable = MERCURY_DEFAULT_RELIABLE );

	void startRequest( const InterfaceElement & ie,
		ReplyMessageHandler * handler,
		void * arg = NULL,
		int timeout = DEFAULT_REQUEST_TIMEOUT,
		ReliableType reliable = MERCURY_DEFAULT_RELIABLE );

	void startReply( ReplyID id, ReliableType reliable = MERCURY_DEFAULT_RELIABLE );

	void * startStructMessage( const InterfaceElement & ie,
		ReliableType reliable = MERCURY_DEFAULT_RELIABLE );

	void * startStructRequest( const InterfaceElement & ie,
		ReplyMessageHandler * handler,
		void * arg = NULL,
		int timeout = DEFAULT_REQUEST_TIMEOUT,
		ReliableType reliable = MERCURY_DEFAULT_RELIABLE );

	void reliable( ReliableType currentMsgReliabile );
	bool isReliable() const;

	bool isCritical() const { return isCritical_; }

	bool isOnExternalChannel() const;

	Channel * pChannel() { return pChannel_; }

	virtual void * reserve( int nBytes );
	virtual void addBlob( const void * pBlob, int size );
	INLINE void * qreserve( int nBytes );

	void reliableOrders( Packet * p,
		const ReliableOrder *& roBeg, const ReliableOrder *& roEnd );

 	bool piggyback( SeqNum seq, const ReliableVector& reliableOrders,
		Packet *p );

	/**
	 * 	@internal
	 *	This class is used to iterate over the messages in a bundle.
	 *	Mercury uses this internally when unpacking a bundle and
	 *	delivering messages to the client.
	 */
	class iterator
	{
	public:
		iterator(Packet * first);
		iterator(const iterator & i);
		~iterator();

		const iterator & operator=( const iterator & i );

		MessageID msgID() const;

		// Note: You have to unpack before you can call
		// 'data' or 'operator++'

		UnpackedMessageHeader & unpack( const InterfaceElement & ie );
		const char * data();

		void operator++(int);
		bool operator==(const iterator & x) const;
		bool operator!=(const iterator & x) const;
	private:
		void nextPacket();

		Packet		* cursor_;
		uint16		bodyEndOffset_;
		uint16		offset_;
		uint16		dataOffset_;
		int			dataLength_;
		char 		* dataBuffer_;

		uint16	nextRequestOffset_;

		UnpackedMessageHeader	curHeader_;
	};

	/**
	 * Get some iterators
	 */
	iterator begin();
	iterator end();

	/**
	 * Clean up any loose ends (called only by nub)
	 */
	void finalise();

private:
public:	// public so streaming operators work (they can't
		// be in-class due to VC's problem with template
		// member functions)

	// per bundle stuff
	PacketPtr firstPacket_;		///< The first packet in the bundle
	Packet	* currentPacket_;	///< The current packet in the bundle
	bool	finalised_;			///< True if the bundle has been finalised
	bool	reliableDriver_;	///< True if any driving reliable messages added
	uint8	extraSize_;			///< Size of extra bytes needed for e.g. filter

	/**
	 * 	@internal
	 * 	This structure represents a request that requires a reply.
	 * 	It is used internally by Mercury.
	 */
	struct ReplyOrder
	{
		/// The user reply handler.
		ReplyMessageHandler * handler;

		/// User argument passed to the handler.
		void * arg;

		/// Timeout in microseconds.
		int microseconds;

		/// Pointer to the reply ID for this request, written in Nub::send().
		ReplyID * pReplyID;
	};

	/// This vector stores all the requests for this bundle.
	typedef std::vector<ReplyOrder>	ReplyOrders;

	ReplyOrders	replyOrders_;

	/// This vector stores all the reliable messages for this bundle.
	ReliableVector	reliableOrders_;
	int				reliableOrdersExtracted_;

	/// If true, this Bundle's packets will be considered to be 'critical' by
	/// the Channel.
	bool			isCritical_;

	/**
	 *  @internal
	 *  A Piggyback is the data structure used to represent a piggyback packet
	 *  between the call to Bundle::piggyback() and the data actually being
	 *  streamed onto the packet footers during Nub::send().
	 */
	class Piggyback
	{
	public:
		Piggyback( Packet * pPacket,
				Packet::Flags flags,
				SeqNum seq,
				int16 len ) :
			pPacket_( pPacket ),
			flags_( flags ),
			seq_( seq ),
			len_( len )
		{}

		PacketPtr 		pPacket_;  ///< Original packet messages come from
		Packet::Flags	flags_;     ///< Header for the piggyback packet
		SeqNum			seq_;       ///< Sequence number of the piggyback packet
		int16			len_;       ///< Length of the piggyback packet
		ReliableVector	rvec_;      ///< Reliable messages to go onto the packet
	};

	typedef std::vector< Piggyback* > Piggybacks;
	Piggybacks piggybacks_;

	/**
	 * 	@internal
	 * 	This structure represents an Ack.
	 */
	struct AckOrder
	{
		Packet		* p;		///< The packet in which this ack will be sent.
		SeqNum		forseq;		///< The sequence number being acknowledged.
	};

	/// This  vector stores all the Acks being sent with this bundle.
	typedef std::vector<AckOrder> AckOrders;
	AckOrders ackOrders_;

private:
	/// This is the Channel that owns this Bundle, or NULL if not on a Channel.
	Channel * pChannel_;

	// per message stuff
	InterfaceElement	const * curIE_;
	int		msglen_;
	int		msgextra_;
	uint8	* msgbeg_;
	uint16	msgChunkOffset_;
	bool	msgReliable_;
	bool	msgRequest_;

	// Statistics
	int		numMessages_;
	int		numReliableMessages_;

public:
	int addAck( SeqNum seq );

private:
	void * sreserve( int nBytes );
	void reserveFooter( int nBytes, Packet::Flags flag );
	void dispose();
	void startPacket( Packet * p );
	void endPacket(bool multiple);
	void endMessage();
	char * newMessage( int extra = 0 );
	void addReliableOrder();

	Packet::Flags packetFlags() const { return currentPacket_->flags(); }

	Bundle( const Bundle & );
	Bundle & operator=( const Bundle & );
};


/**
 *  This class is useful when you have a lot of data you want to send to a
 *  collection of other apps, but want to group the sends to each app together.
 */
class BundleSendingMap
{
public:
	BundleSendingMap( Nub & nub ) : nub_( nub ) {}
	Bundle & operator[]( const Address & addr );
	void sendAll();

private:
	Nub & nub_;

	typedef std::map< Address, Channel* > Channels;
	Channels channels_;
};



} // namespace Mercury

#ifdef CODE_INLINE
#include "bundle.ipp"
#endif

#endif // BUNDLE_HPP
