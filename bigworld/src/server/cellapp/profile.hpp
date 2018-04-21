/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CELLAPP_PROFILE_HPP
#define CELLAPP_PROFILE_HPP

#include "cellapp.hpp"
#include "cstdmf/profile.hpp"

/**
 *	This class stores a value in stamps but has access functions in seconds.
 */
class CPUStamp
{
	uint64	stamp_;
public:
	CPUStamp( uint64 stamp = 0 ) : stamp_( stamp ) {}
	CPUStamp( double seconds ) : stamp_( fromSeconds( seconds ) ) {}

	uint64 get() const 			{ return stamp_; }
	void set( uint64 stamp ) 	{ stamp_ = stamp; }

	double getInSeconds() const 		{ return toSeconds( stamp_ ); }
	void setInSeconds( double seconds ) { stamp_ = fromSeconds( seconds ); }

private:
	// Utility methods
	static double toSeconds( uint64 stamp )
	{
		return double( stamp )/stampsPerSecondD();
	}
	static uint64 fromSeconds( double seconds )
	{
		return uint64( seconds * stampsPerSecondD() );
	}
};

// __kyl__(30/7/2007) Special profile thresholds for T2CN
extern CPUStamp 	g_profileInitGhostTimeLevel;
extern int			g_profileInitGhostSizeLevel;
extern CPUStamp 	g_profileInitRealTimeLevel;
extern int			g_profileInitRealSizeLevel;
extern CPUStamp 	g_profileOnloadTimeLevel;
extern int			g_profileOnloadSizeLevel;
extern int			g_profileBackupSizeLevel;

#define START_PROFILE( PROFILE ) CellProfileGroup::PROFILE.start();

#define IF_PROFILE_LONG( PROFILE )											\
	if (!CellProfileGroup::PROFILE.running() &&								\
		CellProfileGroup::PROFILE.lastTime_ *								\
		CellApp::instance().updateHertz() > stampsPerSecond())				\

#define STOP_PROFILE( PROFILE )												\
	{																		\
		CellProfileGroup::PROFILE.stop();									\
	IF_PROFILE_LONG( PROFILE )												\
	{																		\
		WARNING_MSG( "%s:%d: Profile " #PROFILE " took %.2f seconds\n",		\
			__FILE__, __LINE__,												\
			CellProfileGroup::PROFILE.lastTime_  / stampsPerSecondD() );	\
	}																		\
}


#define STOP_PROFILE_WITH_CHECK( PROFILE )									\
	STOP_PROFILE( PROFILE )													\
	IF_PROFILE_LONG( PROFILE )


#define STOP_PROFILE_WITH_DATA( PROFILE, DATA )								\
	CellProfileGroup::PROFILE.stop( DATA );

#define IS_PROFILE_RUNNING( PROFILE )										\
	CellProfileGroup::PROFILE.running();

namespace CellProfileGroup
{
	void init();
}

namespace CellProfileGroup
{
extern ProfileVal RUNNING;

extern ProfileVal CREATE_ENTITY;
extern ProfileVal CREATE_GHOST;
extern ProfileVal ONLOAD_ENTITY;
extern ProfileVal UPDATE_CLIENT;
extern ProfileVal UPDATE_CLIENT_PREPARE;
extern ProfileVal UPDATE_CLIENT_LOOP;
extern ProfileVal UPDATE_CLIENT_POP;
extern ProfileVal UPDATE_CLIENT_APPEND;
extern ProfileVal UPDATE_CLIENT_PUSH;
extern ProfileVal UPDATE_CLIENT_SEND;
extern ProfileVal UPDATE_CLIENT_UNSEEN;
extern ProfileVal OFFLOAD_ENTITY;
extern ProfileVal DELETE_GHOST;

extern ProfileVal AVATAR_UPDATE;
extern ProfileVal GHOST_AVATAR_UPDATE;
extern ProfileVal GHOST_OWNER;
extern ProfileVal SCRIPT_MESSAGE;
extern ProfileVal SCRIPT_CALL;

extern ProfileVal LOAD_BALANCE;
extern ProfileVal BOUNDARY_CHECK;
extern ProfileVal DELIVER_GHOSTS;

extern ProfileVal INIT_REAL;
extern ProfileVal INIT_GHOST;
extern ProfileVal FORWARD_TO_REAL;
extern ProfileVal POPULATE_KNOWN_LIST;
extern ProfileVal FIND_ENTITY;
extern ProfileVal PICKLE;
extern ProfileVal UNPICKLE;

extern ProfileVal ON_TIMER;
extern ProfileVal ON_MOVE;
extern ProfileVal ON_NAVIGATE;
extern ProfileVal CAN_NAVIGATE_TO;
extern ProfileVal FIND_PATH;
extern ProfileVal SHUFFLE_ENTITY;
extern ProfileVal SHUFFLE_TRIGGERS;
extern ProfileVal SHUFFLE_AOI_TRIGGERS;
extern ProfileVal VISION_UPDATE;
extern ProfileVal ENTITIES_IN_RANGE;

extern ProfileVal CHUNKS_MAIN_THREAD;

extern ProfileVal TICK_SLUSH;

extern ProfileVal GAME_TICK;

extern ProfileVal CALC_BOUNDARY;
extern ProfileVal CALL_TIMERS;
extern ProfileVal CALL_UPDATES;

extern ProfileVal WRITE_TO_DB;

extern ProfileVal BACKUP;
};

/**
 *	This function stops the given profile and returns the duration of the
 * 	last run (in stamps). Will return 0 if the profile is still running i.e.
 * 	there were nested starts.
 */
inline uint64 STOP_PROFILE_GET_TIME( ProfileVal & profile )
{
	profile.stop();
	return profile.running() ? 0 : profile.lastTime_;
}

#ifdef CODE_INLINE
#include "profile.ipp"
#endif

#endif // CELLAPP_PROFILE_HPP
