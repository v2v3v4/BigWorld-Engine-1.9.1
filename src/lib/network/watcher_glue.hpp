/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef WATCHER_GLUE_HPP
#define WATCHER_GLUE_HPP

#include "cstdmf/config.hpp"
#if ENABLE_WATCHERS
#include "cstdmf/singleton.hpp"

#include "network/nub.hpp"
#include "network/watcher_nub.hpp"

/**
 *	This class is a singleton version of WatcherNub that receives event
 *	notifications from Mercury and uses these to process watcher events.
 *
 * 	@ingroup watcher
 */
class WatcherGlue :
	public WatcherNub,
	public Mercury::InputNotificationHandler,
	public Singleton< WatcherGlue >
{
public:
	WatcherGlue();
	virtual ~WatcherGlue();

	virtual int handleInputNotification( int fd );

private:
	StandardWatcherRequestHandler	handler_;
};

#endif /* ENABLE_WATCHERS */

#endif // WATCHER_GLUE_HPP
