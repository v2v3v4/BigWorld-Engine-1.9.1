/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef NETWORK_INTERFACES_HPP
#define NETWORK_INTERFACES_HPP

#include "misc.hpp"

namespace Mercury
{

/**
 *	This class declares an interface for receiving Mercury messages.
 *	Classes that can handle general messages from Mercury needs to
 *	implement this.
 *
 *	@ingroup mercury
 */
class InputMessageHandler
{
public:
	virtual ~InputMessageHandler() {};

	/**
	 * 	This method is called by Mercury to deliver a message.
	 *
	 * 	@param source	The address at which the message originated.
	 * 	@param header	This contains the message type, size, and flags.
	 * 	@param data		The actual message data.
	 */
	virtual void handleMessage( const Address & source,
		UnpackedMessageHeader & header,
		BinaryIStream & data ) = 0;
};


/**
 *	This class declares an interface for receiving reply messages.
 *	When a client issues a request, an interface of this type should
 *	be provided to handle the reply.
 *
 *	@see Bundle::startRequest
 *	@see Bundle::startReply
 *
 *	@ingroup mercury
 */
class ReplyMessageHandler
{
public:
	virtual ~ReplyMessageHandler() {};

	/**
	 * 	This method is called by Mercury to deliver a reply message.
	 *
	 * 	@param source	The address at which the message originated.
	 * 	@param header	This contains the message type, size, and flags.
	 * 	@param data		The actual message data.
	 * 	@param arg		This is user-defined data that was passed in with
	 * 					the request that generated this reply.
	 */
	virtual void handleMessage( const Address & source,
		UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg ) = 0;

	/**
	 * 	This method is called by Mercury when the request fails. The
	 * 	normal reason for this happening is a timeout.
	 *
	 * 	@param exception	The reason for failure.
	 * 	@param arg			The user-defined data associated with the request.
	 */
	virtual void handleException( const NubException & exception,
		void * arg ) = 0;
};


/**
 *	This class defines an interface for receiving timer events.
 *	When a client requests a timer notification, it must provide
 *	an interface of this type to receive the callbacks.
 *
 *	@see Nub::registerTimer
 *
 *	@ingroup mercury
 */
class TimerExpiryHandler
{
public:
	virtual ~TimerExpiryHandler() {};

	/**
	 * 	This method is called when a timer expires.
	 *
	 * 	@param id	Unique id assigned when this timer was created.
	 * 	@param arg	User data that was passed in when this timer was created.
	 *
	 * 	@return If this method returns a nonzero value, the Mercury
	 * 			'processContinuously' loop will break. Otherwise, it
	 * 			will keep going as normal.
	 *
	 *	@todo 	Make return value work.
	 */
	virtual int handleTimeout( TimerID id, void * arg ) = 0;
};


/**
 * 	This class defines an interface for receiving socket events.
 * 	Since Mercury runs the event loop, it is useful to be able
 * 	to register additional file descriptors, and receive callbacks
 * 	when they are ready for IO.
 *
 * 	@see Nub::registerFileDescriptor
 *
 * 	@ingroup mercury
 */
class InputNotificationHandler
{
public:
	virtual ~InputNotificationHandler() {};

	/**
	 *	This method is called when a file descriptor is ready for
	 *	reading.
	 *
	 *	@param fd	The file descriptor
	 *
	 * 	@return If this method returns a nonzero value, the Mercury
	 * 			'processContinuously' loop will break. Otherwise, it
	 * 			will keep going as normal.
	 *
	 *	@todo 	Make return value work.
	 */
	virtual int handleInputNotification( int fd ) = 0;
};


/**
 *	This class declares an interface for receiving notification
 *	when a bundle is complete.
 *
 *	@see Nub::registerBundleFinishHandler
 *
 *	@ingroup mercury
 */
class BundleFinishHandler
{
public:
	virtual ~BundleFinishHandler() {};

	/**
	 * 	This method is called after all messages in a bundle have
	 * 	been delivered.
	 */
	virtual void onBundleFinished() = 0;
};


/**
 *  This class defines an interface for objects used for priming bundles on
 *  channels with data.  It is used by ServerConnection and Proxy to write the
 *  'authenticate' message to the start of each bundle.
 *
 *  @see Channel::bundlePrimer
 */
class BundlePrimer
{
public:
	virtual ~BundlePrimer() {}

	/**
	 *  This method is called by the channel just after the bundle is cleared.
	 */
	virtual void primeBundle( Bundle & bundle ) = 0;

	/**
	 *  This method should return the number of non RELIABLE_DRIVER messages
	 *  that the primer writes to the bundle.
	 */
	virtual int numUnreliableMessages() const = 0;
};


} // namespace Mercury

#endif // NETWORK_INTERFACES_HPP
