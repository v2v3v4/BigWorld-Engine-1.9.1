/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef ALERT_MANAGER_HPP
#define ALERT_MANAGER_HPP

#include "cstdmf/main_loop_task.hpp"

class SimpleGUIComponent;
typedef SmartPointer< SimpleGUIComponent > SimpleGUIComponentPtr;

/**
 *	The AlertManager object is a singleton object that acts as a wrapper
 *	for the different alert Icons that pop up as a result of certain
 *	conditions being met.
 */
class AlertManager : public MainLoopTask
{
public:
	///	@name Enumerated Types.
	//@{
	enum AlertID
	{
		ALERT_FRAME_TEXTURE_MEM,
		ALERT_SCENE_TEXTURE_MEM,
		ALERT_PRIMITIVES,
		ALERT_MESH_MEM,
		ALERT_AMIM_LOAD,	// animations loaded per second
		ALERT_FRAME_RATE,

		MAXIMUM_ALERT_ID
	};
	//@}


	///	@name Constructors and Destructor.
	//@{
	AlertManager();
	~AlertManager();
	//@}

	static AlertManager & instance();

	///	@name MainLoopTask methods.
	//@{
	bool init();
	void tick( float dTime );
	void draw();
	//@}


	///	@name Methods for flagging alerts and updating them.
	//@{
	bool alertStatus( AlertID alert );
	void alertStatus( AlertID alert, bool status );
	void signalAlert( AlertID alert);
	//@}

private:
	///	@name Private Helper Methods.
	//@{
	void checkInbuiltAlerts( float dTime );
	void calculatePosition( int n );
	//@}

	float	dTime_;

	bool alertStatus_[ MAXIMUM_ALERT_ID ];
	bool signaledStatus_[ MAXIMUM_ALERT_ID ];

	SimpleGUIComponentPtr alertIcons_[ MAXIMUM_ALERT_ID ];
};

#ifdef CODE_INLINE
#include "alert_manager.ipp"
#endif

#endif // ALERT_MANAGER_HPP
