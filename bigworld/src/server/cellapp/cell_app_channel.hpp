/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CELL_APP_CHANNEL_HPP
#define CELL_APP_CHANNEL_HPP

#include "network/channel.hpp"

#include <map>

/**
 *	This class is used to represent a connection between two cell applications.
 */
class CellAppChannel : public Mercury::ChannelOwner
{
public:
	typedef std::map< Mercury::Address, CellAppChannel * > Map;
	typedef Map::iterator iterator;

	// ---- Static methods ----
	static void init( int microseconds );
	static void sendAll();
	static CellAppChannel * get( const Mercury::Address & addr,
		bool shouldCreate = true );

	static iterator begin()	{ return s_map_.begin(); }
	static iterator end()	{ return s_map_.end(); }

	static void remoteFailure( const Mercury::Address & addr );

	// ---- User stuff ----
	bool isOverloaded() const;

	int mark() const				{ return mark_; }
	void mark( int v )				{ mark_ = v; }
	int offloadCapacity() const		{ return offloadCapacity_; }
	void offloadCapacity( int v )	{ offloadCapacity_ = v; }
	int ghostingCapacity() const	{ return ghostingCapacity_; }
	void ghostingCapacity( int v )	{ ghostingCapacity_ = v; }
	bool isGood() const 			{ return !this->channel().hasRemoteFailed(); }

	void addHaunt();
	void removeHaunt();

private:
	CellAppChannel( const Mercury::Address & addr );

	int		mark_;
	int		offloadCapacity_;
	int		ghostingCapacity_;
	int		numHaunts_;

	static Map s_map_;

	static const int RECENTLY_DEAD_PERIOD = 10;

	typedef std::set< Mercury::Address > RecentlyDead;
	static RecentlyDead s_recentlyDead_;

	static uint64 s_lastTimeOfDeath_;
};

#endif // CELL_APP_CHANNEL_HPP
