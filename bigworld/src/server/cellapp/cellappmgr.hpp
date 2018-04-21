/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CELLAPPMGR_HPP
#define CELLAPPMGR_HPP

#include "cstdmf/stdmf.hpp"

#include "network/basictypes.hpp"
#include "network/channel.hpp"
#include "network/misc.hpp"

typedef uint8 SharedDataType;

/**
 * 	This is a simple helper class that is used to represent the remote cell
 * 	manager.
 */
class CellAppMgr : public Mercury::ChannelOwner
{
public:
	CellAppMgr( Mercury::Nub & nub );
	bool init();

	void add( const Mercury::Address & addr, uint16 viewerPort,
			Mercury::ReplyMessageHandler * pReplyHandler );

	void informOfLoad( float load );

	void handleCellAppDeath( const Mercury::Address & addr );

	void setSharedData( const std::string & key, const std::string & value,
		   SharedDataType type );
	void delSharedData( const std::string & key, SharedDataType type );
};


#endif // CELLAPPMGR_HPP
