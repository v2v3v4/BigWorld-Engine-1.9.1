/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TIMER_CONTROLLER_HPP
#define TIMER_CONTROLLER_HPP

#include "controller.hpp"
#include "cstdmf/time_queue.hpp"

#include "pyscript/script.hpp"

/**
 *	This class implements server objects owned by an entity script, that move
 *	between cells when the script object moves.
 */
class TimerController : public Controller
{
	DECLARE_CONTROLLER_TYPE( TimerController )

public:
	TimerController( TimeStamp start = 0, TimeStamp interval = 0 );

	void				writeRealToStream( BinaryOStream& stream );
	bool 				readRealFromStream( BinaryIStream& stream );

	void				handleTimeout();
	void				onHandlerRelease();

	// Controller overrides
	virtual void		startReal( bool isInitialStart );
	virtual void		stopReal( bool isFinalStop );

	static FactoryFnRet New( float initialOffset, float repeatOffset,
		int userArg = 0 );
	PY_AUTO_CONTROLLER_FACTORY_DECLARE( TimerController,
		ARG( float, OPTARG( float, 0.f, OPTARG( int, 0, END ) ) ) )

private:

	/**
	 *	Handler for a timer to go into the global time queue
	 */
	class Handler : public TimeQueueHandler
	{
	public:
		Handler( TimerController * pController );

		void pController( TimerController * pController )
									{ pController_ = pController; }

	private:
		// Overrides from TimeQueueHandler
		virtual void	handleTimeout( TimeQueueId id, void * pUser );
		virtual void	onRelease( TimeQueueId id, void  * pUser );

		TimerController *	pController_;
	};

	Handler *		pHandler_;

	TimeStamp		start_;
	TimeStamp		interval_;
	TimeQueueId		timeQueueId_;
};

#endif // TIMER_CONTROLLER_HPP
