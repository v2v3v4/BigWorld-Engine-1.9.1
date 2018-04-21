/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef HISTORY_EVENT_HPP
#define HISTORY_EVENT_HPP

#include "entitydef/data_description.hpp"
#include "network/basictypes.hpp"
#include "network/mercury.hpp"


/**
 * 	This class is used to store an event in the event history.
 */
class HistoryEvent
{
public:
	/**
	 *	This class is a bit of a hack. For state change events, we want to use
	 *	a detail level, while for messages (that is, events with no state
	 *	change), we want to store a priority.
	 *
	 */
	class Level
	{
	public:
		Level() {}
		Level( int i ) : detail( i ) {};
		Level( float f ) : priority( f ) {};

		union
		{
			float priority;
			int detail;
		};
	};

	HistoryEvent( Mercury::MessageID msgID, EventNumber number,
		void * msg, int msgLen, Level level,
		const std::string * pName = NULL );

	~HistoryEvent();

	EventNumber number() const;

	void addToBundle( Mercury::Bundle & bundle );

	bool shouldSend( float threshold, int detailLevel ) const;

//private:
	HistoryEvent( const HistoryEvent & );
	HistoryEvent & operator=( const HistoryEvent & );

	bool isStateChange() const;

#if ENABLE_WATCHERS
	void pChangedDescription( MemberDescription * pChangedDescription )
	{ pChangedDescription_ = pChangedDescription; }
#endif
public:
	Level level_;

//private:
	Mercury::InterfaceElement msgIE_;
	EventNumber number_;
	char * msg_;
public:
	/// This member stores the length of the message (in bytes).
	int msgLen_;

	/// __glenc__ Hack to hold onto the name for event tracking.
	const std::string * pName_;

#if ENABLE_WATCHERS
	MemberDescription * pChangedDescription_;
#endif
};


/**
 *	This class is used to store a queue of history events.
 */
class EventHistory
{
public:
	typedef std::deque< HistoryEvent * > Container;

	typedef Container::const_iterator			const_iterator;
	typedef Container::const_reverse_iterator	const_reverse_iterator;

	EventHistory();
	~EventHistory();

	void add( HistoryEvent * pEvent );
	void trim();
	void clear();

	const_iterator begin() const			{ return container_.begin(); }
	const_iterator end() const				{ return container_.end(); }

	const_reverse_iterator rbegin() const	{ return container_.rbegin(); }
	const_reverse_iterator rend() const		{ return container_.rend(); }

	bool empty() const						{ return container_.empty(); }

private:
	Container container_;
	Container::size_type trimSize_;
};

#ifdef CODE_INLINE
#include "history_event.ipp"
#endif

#endif // HISTORY_EVENT_HPP
