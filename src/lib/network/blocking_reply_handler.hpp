/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BLOCKING_REPLY_HANDLER_HPP
#define BLOCKING_REPLY_HANDLER_HPP

#include "nub.hpp"
#include "channel.hpp"

namespace Mercury
{

/**
 *	This is a class to make simple blocking two-way calls easier.
 *
 *	@note You are STRONGLY discouraged from using this from within message
 *	handlers, as you are heading straight for all the common re-entrancy
 *	problems.
 *
 *	@ingroup mercury
 *	@see Bundle::startRequest
 */
class BlockingReplyHandler :
	public ReplyMessageHandler,
	public TimerExpiryHandler
{
public:
	BlockingReplyHandler( Nub & nub, ReplyMessageHandler * pHandler );

	Reason waitForReply( Channel * pChannel = NULL,
		   int maxWaitMicroSeconds = 10 * 1000000 );

protected:
	virtual int handleTimeout( TimerID id, void * arg );

	virtual void handleMessage( const Address & addr,
		UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );

	virtual void handleException( const NubException & ex, void * );

private:
	Nub &			nub_;
	bool			isDone_;
	Reason			err_;

	TimerID			timerID_;

	ReplyMessageHandler * pHandler_;
};


/**
 *	This is a template class to make simple blocking two-way calls easier.
 *
 *	To use this class, first make a request using Bundle::startRequest.
 *	Then, instantiate an object of this type, using the expected
 *	reply type as the template argument. Then, call the @ref waitForReply
 *	method, and the handler will block until a reply is received or
 *	the request times out.
 *
 *	@note You are STRONGLY discouraged from using this from within message
 *	handlers, as you are heading straight for all the common re-entrancy
 *	problems.
 *
 *	@ingroup mercury
 *	@see Bundle::startRequest
 */
template <class RETURNTYPE>
class BlockingReplyHandlerWithResult : public BlockingReplyHandler
{
public:
	BlockingReplyHandlerWithResult( Nub & nub );

	RETURNTYPE & get();

private:
	virtual void handleMessage( const Address & addr,
		UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg );

	RETURNTYPE		result_;
};

// -----------------------------------------------------------------------------
// Section: BlockingReplyHandler
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
inline BlockingReplyHandler::BlockingReplyHandler(
		Mercury::Nub & nub,
		ReplyMessageHandler * pHandler ) :
	nub_( nub ),
	isDone_( false ),
	err_( REASON_SUCCESS ),
	timerID_( TIMER_ID_NONE ),
	pHandler_( pHandler )
{
}


/**
 * 	This method until a reply is received, or the request times out.
 */
inline Reason BlockingReplyHandler::waitForReply( Channel * pChannel,
	   int maxWaitMicroSeconds )
{
	bool wasBroken = nub_.processingBroken();
	bool isRegularChannel = pChannel && !pChannel->isIrregular();

	// Since this channel might not be doing any sending while we're waiting for
	// the reply, we need to mark it as irregular temporarily to ensure ACKs are
	// sent until we're done.
	if (isRegularChannel)
		pChannel->isIrregular( true );

	if (maxWaitMicroSeconds > 0)
	{
		timerID_ = nub_.registerTimer( maxWaitMicroSeconds, this );
	}

	while (!isDone_)
	{
		try
		{
			nub_.processContinuously();
		}
		catch (NubException & ne)
		{
			err_ = ne.reason();
		}
	}

	if (timerID_ != TIMER_ID_NONE)
	{
		nub_.cancelTimer( timerID_ );
	}

	// Restore channel regularity if necessary
	if (isRegularChannel)
		pChannel->isIrregular( false );

	nub_.breakProcessing( wasBroken );

	return err_;
}


/**
 *	This method handles the max timer going off. If this is called, we have not
 *	received the response in the required time.
 */
inline int BlockingReplyHandler::handleTimeout( TimerID id, void * arg )
{
	INFO_MSG( "BlockingReplyHandler::handleTimeout: Timer expired\n" );

	nub_.cancelReplyMessageHandler( this, REASON_TIMER_EXPIRED );

	return 0;
}


/**
 * 	This method handles reply messages from Mercury.
 */
inline void BlockingReplyHandler::handleMessage(
		const Address & addr,
		UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg )
{
	if (pHandler_)
	{
		pHandler_->handleMessage( addr, header, data, arg );
	}

	err_ = REASON_SUCCESS;

	nub_.breakProcessing();
	isDone_ = true;
}


/**
 * 	This method handles exceptions from Mercury.
 */
inline void BlockingReplyHandler::handleException(
		const NubException & ex, void * )
{
	if (err_ == REASON_SUCCESS)
	{
		err_ = ex.reason();
	}

	nub_.breakProcessing();
	isDone_ = true;
}


// -----------------------------------------------------------------------------
// Section: BlockingReplyHandlerWithResult
// -----------------------------------------------------------------------------

/**
 * 	This is the constructor.
 *
 * 	@param nub	The Nub to be used for sending and receiving.
 */
template <class RETURNTYPE> inline
BlockingReplyHandlerWithResult<RETURNTYPE>::BlockingReplyHandlerWithResult(
	Nub & nub ) :
	BlockingReplyHandler( nub, NULL )
{
}


/**
 * 	This method returns the result of the request.
 */
template <class RETURNTYPE>
inline RETURNTYPE & BlockingReplyHandlerWithResult<RETURNTYPE>::get()
{
	return result_;
}

/**
 * 	This method handles reply messages from Mercury.
 * 	It unpacks the reply and stores it.
 */
template <class RETURNTYPE>
inline void BlockingReplyHandlerWithResult<RETURNTYPE>::handleMessage(
		const Address & addr,
		UnpackedMessageHeader & header,
		BinaryIStream & data,
		void * arg )
{
	data >> result_;

	this->BlockingReplyHandler::handleMessage( addr, header, data, arg );
}

} // namespace Mercury


#endif //  BLOCKING_REPLY_HANDLER_HPP
