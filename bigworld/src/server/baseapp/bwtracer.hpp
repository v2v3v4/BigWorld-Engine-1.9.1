/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef _BWTRACER_HEADER
#define _BWTRACER_HEADER

#include "network/bundle.hpp"

class BWTracer : public Mercury::PacketMonitor
{
public:
	BWTracer();

	void startLogging(Mercury::Nub& nub, const char* filename);
	void stopLogging();

	void packetIn(const Mercury::Address& addr, const Mercury::Packet& packet);
	void packetOut(const Mercury::Address& addr, const Mercury::Packet& packet);

	void setFlushMode(bool flushMode)		{ flushMode_ = flushMode; }
	void setHexMode(bool hexMode)			{ hexMode_ = hexMode; }
	void setFilterAddr(uint32 filterAddr)	{ filterAddr_ = filterAddr; }

private:

	// Helper functions
	void dumpTimeStamp();
	void dumpHeader();
	void dumpMessages();
	void hexDump();
	int32 readVarLength(int bytes);

	// Client message handlers
	int clientBandwidthNotification();
	int clientTickSync();
	int clientEnterAoI();
	int clientLeaveAoI();
	int clientCreateEntity();

	// Proxy message handlers
	int proxyAuthenticate();
	int proxyAvatarUpdateImplicit();
	int proxyAvatarUpdateExplicit();
	int proxySwitchInterface();
	int proxyRequestEntityUpdate();
	int proxyEnableEntities();

	// General message handlers
	int entityMessage();
	int replyMessage();
	int piggybackMessage();

private:

	FILE* 			pFile_;	
	Mercury::Nub*	pNub_;
	const char* 	pData_;
	int				len_;
	uint8			msgId_;
	bool			flushMode_;
	bool			hexMode_;
	uint32			filterAddr_;

	typedef int (BWTracer::*MsgHandler)();

	struct MsgDef
	{
		MsgHandler 	handler;
		int			minBytes;
	};

	MsgDef*			msgTable_;
	MsgDef			proxyMsg_[256];
	MsgDef			clientMsg_[256];
};

#endif
