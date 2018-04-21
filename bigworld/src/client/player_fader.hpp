/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __BWCLIENT_PLAYER_FADER_HPP__

#include "romp/full_screen_back_buffer.hpp"
#include "cstdmf/singleton.hpp"

/**
 *	This class is a helper for fading out the player.  It checks whether or
 *	not the near-plane clips through the player and, if so, makes the
 *	player's model invisible.
 *	At final composite time it uses the FullScreenBackBuffer as a buffer
 *	to draw the character and then copies it back onto the real BackBuffer
 *	translucently.
 */

class PlayerFader : public FullScreenBackBuffer::User, public Singleton< PlayerFader >
{
public:
	PlayerFader();
	~PlayerFader();

	void update();

	void init();
	void fini();

	bool isEnabled();
	void beginScene();
	void endScene();	
	bool doTransfer( bool alreadyTransferred )	{ return false; }
	void doPostTransferFilter();
	
private:	
	float transparency_;
	///player transparency power
	float ptp_;
	///max player transparency
	float maxPt_;			
};

#endif // PLAYER_HPP