/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"

#pragma warning(disable:4786)	// turn off truncated identifier warnings

#include "alert_manager.hpp"
#include "app_config.hpp"
#include "ashes/simple_gui.hpp"
#include "ashes/simple_gui_component.hpp"
#include "moo/render_context.hpp"
#include "moo/texture_manager.hpp"

#ifndef CODE_INLINE
#include "alert_manager.ipp"
#endif


// -----------------------------------------------------------------------------
// AlertManager:: Constructors and Destructor.
// -----------------------------------------------------------------------------

static bool s_inbuiltEnabled = false;
static double primitives_AlertLevel		= 150 * 1000.0;	// Numbers of Primitives
static double sceneTexMem_AlertLevel	= 16.5;			// MBytes
static double frameTexMem_AlertLevel	= 8.0;			// MBytes
static double meshMem_AlertLevel		= 8.0;			// MBytes
static double animLoad_AlertLevel		= 10.0;			// anims per second
static double frameRate_AlertLevel		= 20.0;			// frames per second


/**
 *	Constructor for AlertManager. Initialises the alert flags.
 */
AlertManager::AlertManager() :
	dTime_( 0.f )
{
	BW_GUARD;
	MainLoopTasks::root().add( this, "GUI/Alerts", "<App", NULL );
}

/**
 *	Destructor
 */
AlertManager::~AlertManager()
{
	BW_GUARD;
	/*MainLoopTasks::root().del( this, "GUI/Alerts" );*/
}

/**
 *	Returns a reference to the singleton instance of the
 *	AlertManager.
 *
 *	@return	A reference to the singleton instance.
 */
AlertManager & AlertManager::instance()
{
	static AlertManager singleton;

	return singleton;
}


// -----------------------------------------------------------------------------
// AlertManager:: MainLoopTask
// -----------------------------------------------------------------------------

/**
 *	MainLoopTask init method
 */
bool AlertManager::init()
{
	BW_GUARD;
	s_inbuiltEnabled = AppConfig::instance().pRoot()->readBool(
		"alertsEnabled", s_inbuiltEnabled );

	std::vector<std::string> iconTextures;
	AppConfig::instance().pRoot()->readStrings( "ui/alertTexture", iconTextures );


	MF_WATCH( "Client Settings/Alerts/enabled", s_inbuiltEnabled, Watcher::WT_READ_WRITE, 
				"When this flag is set to true, AlertManager will check for the primitive count, "
				"the total texture memory usage, the texture memory used in this frame and "
				"the frame rate, it is default to false" );
	MF_WATCH( "Client Settings/Alerts/primitives", primitives_AlertLevel, Watcher::WT_READ_WRITE, 
				"When Client Settings/Alerts/enabled is set to true and the primitive drawn in a "
				"certain frame is larger than the value specified by it, an alert will be signaled "
				"it is default to 150000" );
	MF_WATCH( "Client Settings/Alerts/sceneTexMem", sceneTexMem_AlertLevel, Watcher::WT_READ_WRITE, 
				"When Client Settings/Alerts/enabled is set to true and the texture memory used by "
				"the whole scene ( in MB ) is larger than the value specified by it, an alert will be signaled "
				"it is default to 16.5 MB" );
	MF_WATCH( "Client Settings/Alerts/frameTexMem", frameTexMem_AlertLevel,Watcher::WT_READ_WRITE, 
				"When Client Settings/Alerts/enabled is set to true and the texture memory used by "
				"the current frame ( in MB ) is larger than the value specified by it, an alert will be signaled "
				"it is default to 8 MB" );
	MF_WATCH( "Client Settings/Alerts/frameRate", frameRate_AlertLevel, Watcher::WT_READ_WRITE, 
				"When Client Settings/Alerts/enabled is set to true and the frame rate is lower "
				"than the value specified by it, an alert will be signaled "
				"it is default to 20 fps" );

	for ( int i = 0; i < MAXIMUM_ALERT_ID; i++ )
	{
		// Set status and signal status.
		alertStatus_[i] = false;
		signaledStatus_[i] = false;

		// Build the icon for the alert.
		if (iconTextures.size())
		{
			alertIcons_[i] = SimpleGUIComponentPtr( new SimpleGUIComponent( iconTextures[i%iconTextures.size()] ), true );
			calculatePosition( i );
			alertIcons_[i]->materialFX( SimpleGUIComponent::FX_BLEND );
			alertIcons_[i]->visible( alertStatus_[i] );
			// Add the icon to the GUI.
			SimpleGUI::instance().addSimpleComponent( *alertIcons_[i] );
		}
		else
		{
			alertIcons_[i] = NULL;
		}
	}

	return true;
}

/**
 *	MainLoopTask tick method
 */
void AlertManager::tick( float dTime )
{
	dTime_ = dTime;
}


/**
 *	Updates the signaled status of the alerts.
 */
void AlertManager::draw()
{
	BW_GUARD;
	// Check current global alerts
	if (s_inbuiltEnabled)
		this->checkInbuiltAlerts( dTime_ );

	// Need to reposition ourselves if the screen resolution.
	if ( SimpleGUI::instance().hasResolutionChanged() )
	{
		for ( int i = 0; i < MAXIMUM_ALERT_ID; i++ )
			calculatePosition( i );
	}

	// Display and/or hide the icons depending on their status.
	for ( int i = 0; i < MAXIMUM_ALERT_ID; i++ )
	{
		if (alertIcons_[i])
		{
			alertIcons_[i]->visible( alertStatus_[i] );			
		}

		if ( signaledStatus_[i] )
			alertStatus_[i] = signaledStatus_[i] = false;
	}	
}


// -----------------------------------------------------------------------------
// AlertManager:: Private Helper Methods.
// -----------------------------------------------------------------------------


/**
 *	This method checks the inbuilt alerts
 */
void AlertManager::checkInbuiltAlerts( float dTime )
{
	BW_GUARD;
	if ( Moo::rc().lastFrameProfilingData().nPrimitives_ > primitives_AlertLevel )
	{
		this->signalAlert( ALERT_PRIMITIVES );
	}

	if ( (float) Moo::TextureManager::instance()->textureMemoryUsed() /
						( 1024.0f * 1024.0f ) > sceneTexMem_AlertLevel )
	{
		this->signalAlert( ALERT_SCENE_TEXTURE_MEM );
	}

	if ( float(Moo::ManagedTexture::totalFrameTexmem_) / ( 1024.f * 1024.f ) >
						frameTexMem_AlertLevel )
	{
		this->signalAlert( ALERT_FRAME_TEXTURE_MEM );
	}

	if (dTime > 1.f/frameRate_AlertLevel)
	{
		this->signalAlert( ALERT_FRAME_RATE );
	}
}


/**
 *	Calculates the positions of the nth icon on the screen.
 *
 *	@param	n	The nth icon counting from zero.
 */
void AlertManager::calculatePosition( int n )
{
	BW_GUARD;
	if (alertIcons_[n])
	{
		const float clipSize = 1.0f / 10.0f;
		float aspectRatio = Moo::rc().camera().aspectRatio();

		alertIcons_[n]->anchor( SimpleGUIComponent::ANCHOR_H_RIGHT,
			SimpleGUIComponent::ANCHOR_V_TOP );

		//alertIcons_[n]->position(
		//	Moo::Vector3( -1.0f + ( clipSize / aspectRatio ) * n,
		//	1.0f, -1.0f ) );

		alertIcons_[n]->position(
			Vector3( 0.8125f, 0.8f - clipSize * n , 0.9f ) );

		alertIcons_[n]->height( clipSize );
		alertIcons_[n]->heightMode( SimpleGUIComponent::SIZE_MODE_LEGACY );

		alertIcons_[n]->width( clipSize / aspectRatio );
		alertIcons_[n]->widthMode( SimpleGUIComponent::SIZE_MODE_LEGACY );
	}
}

// alert_manager.cpp