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
#include "worldeditor/scripting/world_editor_script.hpp"
#ifndef CODE_INLINE
#include "worldeditor/scripting/world_editor_script.ipp"
#endif
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/editor_chunk_overlapper.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/misc/world_editor_camera.hpp"
#include "worldeditor/misc/sync_mode.hpp"
#include "worldeditor/gui/dialogs/convert_space_dlg.hpp"
#include "worldeditor/gui/dialogs/resize_maps_dlg.hpp"
#include "worldeditor/gui/pages/panel_manager.hpp"
#include "worldeditor/terrain/terrain_converter.hpp"
#include "worldeditor/terrain/terrain_locator.hpp"
#include "worldeditor/terrain/terrain_map_resizer.hpp"
#include "worldeditor/editor/snaps.hpp"
#include "common/base_camera.hpp"
#include "common/compile_time.hpp"
#include "appmgr/commentary.hpp"
#include "appmgr/module_manager.hpp"
#include "appmgr/options.hpp"
#include "chunk/chunk.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_obstacle.hpp"
#include "chunk/chunk_space.hpp"
#include "gizmo/tool_manager.hpp"
#include "gizmo/undoredo.hpp"
#include "gizmo/general_properties.hpp"
#include "gizmo/current_general_properties.hpp"
#include "input/input.hpp"
#include "particle/particle_system_manager.hpp"
#include "pyscript/py_output_writer.hpp"
#include "pyscript/res_mgr_script.hpp"
#include "pyscript/script.hpp"
#include "pyscript/script_math.hpp"
#include "resmgr/string_provider.hpp"
#include "entitydef/constants.hpp"
#include "romp/console_manager.hpp"
#include "romp/fog_controller.hpp"
#include "romp/progress.hpp"
#include "romp/weather.hpp"
#include "romp/xconsole.hpp"
#include "terrain/terrain2/terrain_block2.hpp"
#include "terrain/terrain_hole_map.hpp"
#include "terrain/terrain_settings.hpp"
#include "terrain/terrain_texture_layer.hpp"
#include "xactsnd/soundmgr.hpp"
#include "cstdmf/debug.hpp"
#include <queue>


DECLARE_DEBUG_COMPONENT2( "Script", 0 )

// -----------------------------------------------------------------------------
// Section: 'WorldEditor' Module
// -----------------------------------------------------------------------------


/*~ function WorldEditor.isKeyDown
 *	@components{ worldeditor }
 *	
 *	This function allows the script to check if a particular key has
 *	been pressed and is currently still down. The term 'key' is used here to refer
 *	to any control with an up/down status; it can refer to the keys of a
 *	keyboard, the buttons of a mouse or even that of a joystick. The complete
 *	list of keys recognised by the client can be found in the Keys module,
 *	defined in keys.py.
 *
 *	The return value is zero if the key is not being held down, and a non-zero
 *	value if it is being held down.
 *
 *	@param key	An integer value indexing the key of interest.
 *
 *	@return True (1) if the key is down, false (0) otherwise.
 *
 *	Code Example:
 *	@{
 *	if WorldEditor.isKeyDown( Keys.KEY_ESCAPE ):
 *	@}
 */
static PyObject * py_isKeyDown( PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_isKeyDown: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( InputDevices::isKeyDown( (KeyEvent::Key)key ) );
}
PY_MODULE_FUNCTION( isKeyDown, WorldEditor )


/*~ function WorldEditor.isCapsLockOn
 *	@components{ worldeditor }
 *
 *	This function returns whether CapsLock is on.
 *
 * @return Returns True (1) if Caps Lock is on, False (0) otherwise.
 */
static PyObject * py_isCapsLockOn( PyObject * args )
{
	return PyInt_FromLong(
		(::GetKeyState( VK_CAPITAL ) & 0x0001) == 0 ? 0 : 1 );
}
PY_MODULE_FUNCTION( isCapsLockOn, WorldEditor )


/*~ function WorldEditor.stringToKey
 *	@components{ worldeditor }
 *
 *	This function converts the name of a key to its corresponding
 *	key index as used by the 'isKeyDown' method. The string names
 *	for a key can be found in the keys.py file. If the name supplied is not on
 *	the list defined, the value returned is zero, indicating an error. This
 *	method has a inverse method, 'keyToString' which does the exact opposite.
 *
 *	@param string	A string argument containing the name of the key.
 *
 *	@return An integer value for the key with the supplied name.
 *
 *	Code Example:
 *	@{
 *	if BigWorld.isKeyDown( WorldEditor.stringToKey( "KEY_ESCAPE" ) ):
 *	@}
 */
static PyObject * py_stringToKey( PyObject * args )
{
	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_stringToKey: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong( KeyEvent::stringToKey( str ) );
}
PY_MODULE_FUNCTION( stringToKey, WorldEditor )


/*~ function WorldEditor.keyToString
 *	@components{ worldeditor }
 *
 *	The 'keyToString' method converts from a key index to its corresponding
 *	string name. The string names returned by the integer index can be found in
 *	the keys.py file. If the index supplied is out of bounds, an empty string
 *	will be returned.
 *
 *	@param key	An integer representing a key index value.
 *
 *	@return A string containing the name of the key supplied.
 *
 *	Code Example:
 *	@{
 *	print WorldEditor.keyToString( key ), "pressed."
 *	@}
 */
static PyObject * py_keyToString( PyObject * args )
{
	int	key;
	if (!PyArg_ParseTuple( args, "i", &key ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_keyToString: Argument parsing error." );
		return NULL;
	}

	return PyString_FromString( KeyEvent::keyToString(
		(KeyEvent::Key) key ) );
}
PY_MODULE_FUNCTION( keyToString, WorldEditor )


/*~ function WorldEditor.axisValue
 *	@components{ worldeditor }
 *
 *	This function returns the value of the given joystick axis.
 *
 *	@param Axis The given joystick axis to get the joystick axis from.
 *
 *	@return The value of the given joystick axis.
 */
static PyObject * py_axisValue( PyObject * args )
{
	int	axis;
	if (!PyArg_ParseTuple( args, "i", &axis ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_axisValue: Argument parsing error." );
		return NULL;
	}

	Joystick::Axis a = InputDevices::joystick().getAxis( (AxisEvent::Axis)axis );

	return PyFloat_FromDouble( a.value() );
}

PY_MODULE_FUNCTION( axisValue, WorldEditor )


/*~ function WorldEditor.axisDirection
 *	@components{ worldeditor }
 *
 *	This function returns the direction the specified joystick is pointing in.
 *
 *	The return value indicates which direction the joystick is facing, as
 *	follows:
 *
 *	@{
 *	- 0 down and left
 *	- 1 down
 *	- 2 down and right
 *	- 3 left
 *	- 4 centred
 *	- 5 right
 *	- 6 up and left
 *	- 7 up
 *	- 8 up and right
 *	@}
 *
 *	@param	axis	This is one of AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, with the
 *					first letter being L or R meaning left thumbstick or right
 *					thumbstick, the second, X or Y being the direction.
 *
 *	@return			An integer representing the direction of the specified
 *					thumbstick, as listed above.
 */
static PyObject * py_axisDirection( PyObject * args )
{
	int	axis;
	if (!PyArg_ParseTuple( args, "i", &axis ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_axisPosition: Argument parsing error." );
		return NULL;
	}

	int direction = InputDevices::joystick().stickDirection( (AxisEvent::Axis)axis );

	return PyInt_FromLong( direction );
}
PY_MODULE_FUNCTION( axisDirection, WorldEditor )


/*~ function WorldEditor.playFx
 *	@components{ worldeditor }
 *
 *	This function plays the named sound effect.
 *
 *	@param SoundEffect	The name of the sound effect to play.
 */
static PyObject* py_playFx( PyObject * args )
{
	char* tag;
	float x, y, z;

	if (!PyArg_ParseTuple( args, "s(fff)", &tag , &x, &y, &z ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFx: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFx(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playFx(tag, Vector3(x, y, z));

	Py_Return;
}
PY_MODULE_FUNCTION( playFx, WorldEditor )


/*~ function WorldEditor.playFxDelayed
 *	@components{ worldeditor }
 *
 *	This function plays the named sound effect with a delay.
 *
 *	@param SoundEffect	The name of the sound effect to play.
 *	@param Delayed	The delay before playing the sound effect.
 */
static PyObject * py_playFxDelayed( PyObject * args )
{
	char* tag;
	float x, y, z, delay;

	if (!PyArg_ParseTuple( args, "s(fff)f", &tag , &x, &y, &z, &delay ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playFxDelayed: Argument parsing error." );
		return NULL;
	}

	//TRACE_MSG( "py_playFxDelayed(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playFxDelayed(tag, delay, Vector3(x, y, z));

	Py_Return;
}
PY_MODULE_FUNCTION( playFxDelayed, WorldEditor )


/*~ function WorldEditor.fxSound
 *	@components{ worldeditor }
 *
 *	This function returns a reference to a loaded sound.
 *
 *	@param SoundName	The name of the loaded sound to get the reference from.
 *
 *	@return The reference to the loaded sound that has the given sound name.
 */
static PyObject* py_fxSound( PyObject* args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag )||1)// unimplement
	{
		PyErr_SetString( PyExc_TypeError, "py_fxSound: Argument parsing error." );
		return NULL;
	}

	DEBUG_MSG( "py_fxSound: %s\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
/*	PyFxSound* snd = new PyFxSound( tag );

	if (!snd->isValidForPy())
	{
		PyErr_Format( PyExc_ValueError, "py_fxSound: No such sound: %s", tag );
		Py_DECREF( snd );
		return NULL;
	}

	return snd;*/
}
PY_MODULE_FUNCTION( fxSound, WorldEditor )


/*~ function WorldEditor.playSimple
 *	@components{ worldeditor }
 *
 *	This function plays the named Simple sound.
 *
 *	@param SimpleSoundName The name of the Simple sound to play.
 */
static PyObject * py_playSimple( PyObject * args )
{
	char* tag;

	if (!PyArg_ParseTuple( args, "s", &tag ))
	{
		PyErr_SetString( PyExc_TypeError, "py_playSimple: Argument parsing error." );
		return NULL;
	}

	TRACE_MSG( "py_playSimple(%s)\n", tag );

	CRITICAL_MSG( "This needs to be fixed by Xiaoming\n" );
	//soundMgr().playSimple(tag);

	Py_Return;
}
PY_MODULE_FUNCTION( playSimple, WorldEditor )



/*~ function WorldEditor.addCommentaryMsg
 *	@components{ worldeditor }
 *
 *	This function adds a message to the	Commentary Console.
 *
 *	@param CommentaryMsg The message to display in the Commentary Console.
 */
static PyObject * py_addCommentaryMsg( PyObject * args )
{
	int id = Commentary::COMMENT;
	char* tag;

	if (!PyArg_ParseTuple( args, "s|i", &tag, &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_addCommentaryMsg: Argument parsing error." );
		return NULL;
	}

	if ( stricmp( tag, "" ) )
	{
		Commentary::instance().addMsg( std::string( tag ), id );
		dprintf( "Commentary: %s\n", tag );
	}
	else
	{
		Commentary::instance().addMsg( std::string( "NULL" ), Commentary::WARNING );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( addCommentaryMsg, WorldEditor )


/*~ function WorldEditor.push
 *	@components{ worldeditor }
 *
 *	This function pushes a module onto the application's module stack.
 *
 *	@param Module	The name of the module to push onto the application's module stack.
 */
static PyObject * py_push( PyObject * args )
{
	char* id;

	if (!PyArg_ParseTuple( args, "s", &id ))
	{
		PyErr_SetString( PyExc_TypeError, "py_push: Argument parsing error." );
		return NULL;
	}

	ModuleManager::instance().push( std::string(id) );

	Py_Return;
}
PY_MODULE_FUNCTION( push, WorldEditor )


/*~ function WorldEditor.pop
 *	@components{ worldeditor }
 *
 *	This function pops the current module from the application's module stack.
 */
static PyObject * py_pop( PyObject * args )
{
	ModuleManager::instance().pop();

	Py_Return;
}
PY_MODULE_FUNCTION( pop, WorldEditor )


/*~ function WorldEditor.pushTool
 *	@components{ worldeditor }
 *
 *	This function pushes a tool onto WorldEditor's tool stack.
 *
 *	@param tool	The tool to push onto WorldEditor's tool stack.
 */
static PyObject * py_pushTool( PyObject * args )
{
	PyObject* pTool;

	if (!PyArg_ParseTuple( args, "O", &pTool ) ||
		!Tool::Check( pTool ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_pushTool: Expected a Tool." );
		return NULL;
	}

	ToolManager::instance().pushTool( static_cast<Tool*>( pTool ) );

	Py_Return;
}
PY_MODULE_FUNCTION( pushTool, WorldEditor )


/*~	function WorldEditor.popTool
 *
 *	This function pops the current tool from WorldEditor's tool stack.
 */
static PyObject * py_popTool( PyObject * args )
{
	ToolManager::instance().popTool();

	Py_Return;
}
PY_MODULE_FUNCTION( popTool, WorldEditor )


/*~ function WorldEditor.tool
 *	@components{ worldeditor }
 *
 *	This function gets the current tool from WorldEditor's tool stack.
 *
 *	@return A reference to the current tool from WorldEditor's tool stack.
 */
static PyObject * py_tool( PyObject * args )
{
	ToolPtr spTool = ToolManager::instance().tool();

	if (spTool)
	{
		Py_INCREF( spTool.getObject() );
		return spTool.getObject();
	}
	else
	{
		Py_Return;
	}
}
PY_MODULE_FUNCTION( tool, WorldEditor )


/*~ function WorldEditor.undo
 *	@components{ worldeditor }
 *
 *	This function undoes the most recent operation, returning
 *	its description. If it is passed a positive integer argument,
 *	then it just returns the description for that level of the
 *	undo stack and doesn't actually undo anything.
 *	If there is no undo level, an empty string is returned.
 *
 *	@param undoLevel	The level of the undo stack to return the undo 
 *						description to. If not supplied, then the most recent
 *						operation will be undone.
 *
 *	@return	The description of the undo operation at the given level of the
 *			undo stack. If no description is found then an empty string is returned.
 */
static PyObject * py_undo( PyObject * args )
{
    CWaitCursor waitCursor;

	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.undo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().undoInfo( max(0,forStep) );

	if (forStep < 0) UndoRedo::instance().undo();

	return Script::getData( what );
}
PY_MODULE_FUNCTION( undo, WorldEditor )

/*~ function WorldEditor.redo
 *	@components{ worldeditor }
 *
 *	This function works exactly like undo, only it redoes the last undo operation.
 *
 *	@see WorldEditor.undo
 */
static PyObject * py_redo( PyObject * args )
{
    CWaitCursor waitCursor;

	int forStep = -1;
	if (!PyArg_ParseTuple( args, "|i", &forStep ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.redo() "
			"expects an optional integer argument" );
		return NULL;
	}

	std::string what = UndoRedo::instance().redoInfo( max(0,forStep) );

	if (forStep < 0) UndoRedo::instance().redo();

	return Script::getData( what );
}
PY_MODULE_FUNCTION( redo, WorldEditor )

/*~ function WorldEditor.addUndoBarrier
 *	@components{ worldeditor }
 *
 *	Adds an undo/redo barrier with the given name.
 *
 *	@param name The name of the undo/redo barrier to add.
 *	@param skipIfNoChange	An optional int that specifies whether to force
 *							a barrier to be added even if no changes have been made.
 *							Defaults value is False (0), otherwise setting True (1) will not add 
 *							a barrier if no changes have been made.
 */
static PyObject * py_addUndoBarrier( PyObject * args )
{

	char* name;
	int skipIfNoChange = 0;
	if (!PyArg_ParseTuple( args, "s|i", &name, &skipIfNoChange ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.addUndoBarrier() "
			"expects a string and an optional int" );
		return NULL;
	}

	// Add the undo barrier
	UndoRedo::instance().barrier( name, (skipIfNoChange != 0) );

	Py_Return;
}
PY_MODULE_FUNCTION( addUndoBarrier, WorldEditor )


/*~ function WorldEditor.saveOptions
 *	@components{ worldeditor }
 *
 *	This function saves the options file.
 *
 *	@param filename The name of the file to save the options file as. If
 *					no name is given then it will overwrite the current 
 *					options file.
 *	
 *	@return Returns True if the save operation was successful, False otherwise.
 */
static PyObject * py_saveOptions( PyObject * args )
{
	char * filename = NULL;

	if (!PyArg_ParseTuple( args, "|s", &filename ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.saveOptions() "
			"expects an optional string argument." );
		return NULL;
	}

	return Script::getData( Options::save( filename ) );
}
PY_MODULE_FUNCTION( saveOptions, WorldEditor )


/*~ function WorldEditor.camera
 *	@components{ worldeditor }
 *
 *	This function gets a WorldEditor camera.
 *	
 *	@param cameraType	The type of camera to return. If cameraType = 0 then the
 *						mouseLook camera is returned; if cameraType = 1 then the
 *						orthographic camera is returned; if cameraType is not given
 *						then the current camera is returned.
 *
 *	@return Returns a reference to a camera which corresponds to the supplied cameraType
 *			parameter. If cameraType is not supplied then the current camera is returned.
 */
static PyObject * py_camera( PyObject * args )
{
	int cameraType = -1;
	if (!PyArg_ParseTuple( args, "|i", &cameraType ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.camera() "
			"expects an optional int argument." );
		return NULL;
	}

	if (cameraType == -1)
	{
		// if no camera specified, return the current camera
		return Script::getData( &(WorldEditorCamera::instance().currentCamera()) );
	}
	else
	{
		// else return the camera specified (only one type of each camera exists
		return Script::getData( &(WorldEditorCamera::instance().camera((WorldEditorCamera::CameraType)cameraType)) );
	}
}
PY_MODULE_FUNCTION( camera, WorldEditor )


/*~ function WorldEditor.changeToCamera
 *	@components{ worldeditor }
 *
 *	This function changes the current camera to the specified cameraType.
 *	
 *	@param cameraType	The cameraType to change the current camera to. If
 *						cameraType = 0 then the mouseLook camera is used;
 *						if cameraType = 1 then the orthographic camera is used.
 */
static PyObject * py_changeToCamera( PyObject * args )
{
	int cameraType = -1;
	if (!PyArg_ParseTuple( args, "i", &cameraType ))
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.camera() "
			"expects an int argument." );
		return NULL;
	}

	if (cameraType != -1)
		WorldEditorCamera::instance().changeToCamera((WorldEditorCamera::CameraType)cameraType);

	Py_Return;
}
PY_MODULE_FUNCTION( changeToCamera, WorldEditor )


/*~ function WorldEditor.snapCameraToTerrain
 *	@components{ worldeditor }
 *
 *	This function snaps the camera to the ground.
 */
static PyObject * py_snapCameraToTerrain( PyObject * args )
{
	BaseCamera& cam = WorldEditorCamera::instance().currentCamera();

	Matrix view = cam.view();
	view.invert();
	Vector3 camPos( view.applyToOrigin() );

	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if ( space )
	{
		ClosestTerrainObstacle terrainCallback;

		// magic numbers are defined here:
		const float EXTENT_RANGE	= 5000.0f;
		const float CAM_RANGE		= 5000.0f;

		// start with the camera's vertical position at 0m
		camPos.y = 0;
		// cycle incrementing the camera's vertical position until a collision is 
		// found, or until the camera's maximum range is reached (set very high!).
		while (!terrainCallback.collided())
		{
			Vector3 extent = camPos + ( Vector3( 0, -EXTENT_RANGE, 0.f ) );

			space->collide( 
				camPos,
				extent,
				terrainCallback );

			// clamp the camera max height to something 'sensible'
			if ( camPos.y >= CAM_RANGE )
				break;

			if (!terrainCallback.collided())
			{
				// drop the camera from higher if no collision is detected
				camPos.y += 200;
			}
		}

		if (terrainCallback.collided())
		{
			camPos = camPos +
					( Vector3(0,-1,0) * terrainCallback.dist() );
			view.translation( camPos +
				Vector3( 0,
				(float)Options::getOptionFloat( "graphics/cameraHeight", 2.f ),
				0 ) );
			view.invert();
			cam.view( view );
		}
	}	

	Py_Return;
}
PY_MODULE_FUNCTION( snapCameraToTerrain, WorldEditor )

/*~ function WorldEditor.enterPlayerPreviewMode
 *	@components{ worldeditor }
 *
 *	This function enables the player preview mode view.
 */
static PyObject * py_enterPlayerPreviewMode( PyObject * args )
{
	WorldManager::instance().setPlayerPreviewMode( true );
	Py_Return;
}
PY_MODULE_FUNCTION( enterPlayerPreviewMode, WorldEditor )

/*~ function WorldEditor.leavePlayerPreviewMode
 *	@components{ worldeditor }
 *
 *	This function disables the player preview mode view.
 */
static PyObject * py_leavePlayerPreviewMode( PyObject * args )
{
	WorldManager::instance().setPlayerPreviewMode( false );
	Py_Return;
}
PY_MODULE_FUNCTION( leavePlayerPreviewMode, WorldEditor )

/*~ function WorldEditor.isInPlayerPreviewMode
 *	@components{ worldeditor }
 *
 *	This function asks WorldEditor if we are in playerPreviewMode
 *
 *	@return Returns True (1) if in player preview mode, False (0) otherwise.
 */
static PyObject * py_isInPlayerPreviewMode( PyObject * args )
{
	return PyInt_FromLong( WorldManager::instance().isInPlayerPreviewMode() );
}
PY_MODULE_FUNCTION( isInPlayerPreviewMode, WorldEditor )

/*~ function WorldEditor.fudgeOrthographicMode
 *	@components{ worldeditor }
 *
 *	This is a temporary function that simply makes the camera
 *	go top-down.
 */
static PyObject * py_fudgeOrthographicMode( PyObject * args )
{
	float height = -31000.f;
	float lag = 5.f;

	if ( !PyArg_ParseTuple( args, "|ff", &height, &lag ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.fudgeOrthographicMode() "
			"expects two optional float arguments - height and lag." );
		return NULL;
	}


	BaseCamera& cam = WorldEditorCamera::instance().currentCamera();

	Matrix view = cam.view();
	view.invert();
	Vector3 camPos( view.applyToOrigin() );

	if ( height > -30000.f )
	{
		if ( camPos.y != height )
		{
			float newCamY = ( ( (camPos.y*lag) + height ) / (lag+1.f) );
			float dy = ( newCamY - camPos.y ) * WorldManager::instance().dTime();
			camPos.y += dy;
		}
	}

	Matrix xform = cam.view();
	xform.setRotateX( 0.5f*MATH_PI ); 
	xform.postTranslateBy( camPos );
	xform.invert();
	cam.view(xform);

	Py_Return;
}
PY_MODULE_FUNCTION( fudgeOrthographicMode, WorldEditor )



/*~ function WorldEditor.ejectChunk
 *	@components{ worldeditor }
 *
 *	This function ejects the chunk under the current tool's locator.
 *	This has the effect of clearing all changes made to the chunk since the 
 *	last save.
 */
static PyObject * py_ejectChunk( PyObject * args )
{
	ToolPtr spTool = ToolManager::instance().tool();

	if (spTool && spTool->locator())
	{
		Vector3 cen = spTool->locator()->transform().applyToOrigin();
		Chunk * pChunk = ChunkManager::instance().cameraSpace()->
			findChunkFromPoint( cen );
		if (pChunk != NULL)
		{
			pChunk->loose( false );
			pChunk->eject();

			Py_Return;
		}
	}

	PyErr_SetString( PyExc_ValueError, "WorldEditor.ejectChunk() "
		"could not find the chunk to eject." );
	return NULL;
}
PY_MODULE_FUNCTION( ejectChunk, WorldEditor )


/*~ function WorldEditor.moveGroupTo
 *	@components{ worldeditor }
 *
 *	Move all current position properties to the given locator.
 *	It does not add an undo barrier, it is up to the Python code to do that.
 *
 *	@param ToolLocator	The ToolLocator object to move the current position properties to.
 */
static PyObject * py_moveGroupTo( PyObject * args )
{
	// get args
	PyObject * pPyLoc;
	if (!PyArg_ParseTuple( args, "O", &pPyLoc ) ||
		!ToolLocator::Check( pPyLoc ))
	{
		PyErr_SetString( PyExc_ValueError, "WorldEditor.moveGroupTo() "
			"expects a ToolLocator" );
		return NULL;
	}

	ToolLocator* locator = static_cast<ToolLocator*>( pPyLoc );

	//Move all group objects relatively by an offset.
	//The offset is a relative, snapped movement.
	Vector3 centrePos = CurrentPositionProperties::centrePosition();
	Vector3 locPos = locator->transform().applyToOrigin();
	Vector3 newPos = locPos - centrePos;
	SnapProvider::instance()->snapPositionDelta( newPos );

	Matrix offset;
	offset.setTranslate( newPos );

	std::vector<GenPositionProperty*> props = CurrentPositionProperties::properties();
	for (std::vector<GenPositionProperty*>::iterator i = props.begin(); i != props.end(); ++i)
	{
		Matrix m;
		(*i)->pMatrix()->recordState();
		(*i)->pMatrix()->getMatrix( m );

		m.postMultiply( offset );

		if ( WorldManager::instance().terrainSnapsEnabled() )
		{
			Vector3 pos( m.applyToOrigin() );
			//snap to terrain only
			pos = Snap::toGround( pos );
			m.translation( pos );
		}
		else if ( WorldManager::instance().obstacleSnapsEnabled() )
		{
			Vector3 normalOfSnap = SnapProvider::instance()->snapNormal( m.applyToOrigin() );
			Vector3 yAxis( 0, 1, 0 );
			yAxis = m.applyVector( yAxis );

			Vector3 binormal = yAxis.crossProduct( normalOfSnap );

			normalOfSnap.normalise();
			yAxis.normalise();
			binormal.normalise();

			float angle = acosf( Math::clamp(-1.0f, yAxis.dotProduct( normalOfSnap ), +1.0f) );

			Quaternion q( binormal.x * sinf( angle / 2.f ),
				binormal.y * sinf( angle / 2.f ),
				binormal.z * sinf( angle / 2.f ),
				cosf( angle / 2.f ) );

			q.normalise();

			Matrix rotation;
			rotation.setRotate( q );

			Vector3 pos( m.applyToOrigin() );

			m.translation( Vector3( 0.f, 0.f, 0.f ) );
			m.postMultiply( rotation );

			m.translation( pos );
		}

		Matrix worldToLocal;
		(*i)->pMatrix()->getMatrixContextInverse( worldToLocal );

		m.postMultiply( worldToLocal );

		(*i)->pMatrix()->setMatrix( m );
		(*i)->pMatrix()->commitState( false, false );
	}

	Py_Return;
}
PY_MODULE_FUNCTION( moveGroupTo, WorldEditor )

/*~ function WorldEditor.showChunkReport
 *	@components{ worldeditor }
 *
 *	This function displays the chunk report of the selected chunk
 *
 *	@param chunk A ChunkItemRevealer object to the selected chunk.
 */
static PyObject * py_showChunkReport( PyObject * args )
{
	// get args
	PyObject * pPyRev;
	if (!PyArg_ParseTuple( args, "O", &pPyRev ) ||
		!ChunkItemRevealer::Check( pPyRev ))
	{
		PyErr_SetString( PyExc_ValueError, "WorldEditor.showChunkReport() "
			"expects a ChunkItemRevealer" );
		return NULL;
	}
	
	ChunkItemRevealer* pRevealer = static_cast<ChunkItemRevealer*>( pPyRev );

	ChunkItemRevealer::ChunkItems items;
	pRevealer->reveal( items );

	uint modelCount = 0;

	ChunkItemRevealer::ChunkItems::iterator i = items.begin();
	for (; i != items.end(); ++i)
	{
		ChunkItemPtr pItem = *i;
		Chunk* pChunk = pItem->chunk();

		if (pChunk)
		{
			std::vector<DataSectionPtr>	modelSects;
			EditorChunkCache::instance( *pChunk ).pChunkSection()->openSections( "model", modelSects );

			modelCount += (int) modelSects.size();
		}
	}

	char buf[512];
	bw_snprintf( buf, sizeof(buf), "%d models in selection\n", modelCount );

	Commentary::instance().addMsg( buf );

	Py_Return;
}
PY_MODULE_FUNCTION( showChunkReport, WorldEditor )


/*~ function WorldEditor.setToolMode
 *	@components{ worldeditor }
 *
 *	This function sets the current WorldEditor tool mode.
 *
 *	@param mode The name of the tool mode to set.
 */
static PyObject * py_setToolMode( PyObject * args )
{
	char* mode = 0;
	if ( !PyArg_ParseTuple( args, "s", &mode ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.setToolMode() "
			"expects a string argument." );
		return NULL;
	}

	if ( mode )
		PanelManager::instance().setToolMode( mode );

	Py_Return;
}
PY_MODULE_FUNCTION( setToolMode, WorldEditor )


/*~ function WorldEditor.showPanel
 *	@components{ worldeditor }
 *
 *	This function shows or hides a Tool Panel.
 *
 *	@param panel	The name of the panel to show/hide.
 *	@param show		If show = 0 then the panel will be hidden, otherwise it will be shown.
 */
static PyObject * py_showPanel( PyObject * args )
{
	char* mode = 0;
	int show = -1;
	if ( !PyArg_ParseTuple( args, "si", &mode, &show ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.showPanel() "
			"expects one string and one int arguments." );
		return NULL;
	}

	if ( mode && show != -1 )
		PanelManager::instance().showPanel( mode, show );

	Py_Return;
}
PY_MODULE_FUNCTION( showPanel, WorldEditor )


/*~ function WorldEditor.isPanelVisible
 *	@components{ worldeditor }
 *
 *	This function checks whether a given panel is visible.
 *
 *	@param panel The name of the panel to query whether it is visible.
 *
 *	@return Returns True (1) if the panel is visible, False (0) otherwise.
 */
static PyObject * py_isPanelVisible( PyObject * args )
{
	char* mode = 0;
	if ( !PyArg_ParseTuple( args, "s", &mode ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.isPanelVisible() "
			"expects a string argument." );
		return NULL;
	}

	if ( mode )
		return PyInt_FromLong( PanelManager::instance().isPanelVisible( mode ) );

	return NULL;
}
PY_MODULE_FUNCTION( isPanelVisible, WorldEditor )



/**
 *	This function currently doesn't do anything.
 */
static PyObject * py_addItemToHistory( PyObject * args )
{
	char* str = 0;
	char* type = 0;
	if ( !PyArg_ParseTuple( args, "ss", &str, &type ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.addItemToHistory()"
			"expects two string arguments." );
		return NULL;
	}

	if ( str && type )
		PanelManager::instance().ualAddItemToHistory( str, type );

	Py_Return;
}
PY_MODULE_FUNCTION( addItemToHistory, WorldEditor )


/*~ function WorldEditor.launchTool
 *	@components{ worldeditor }
 *
 *	This function launches the specified tool.
 *
 *	@param tool The name of the tool to launch, e.g., ParticleEditor.
 *	@param cmd	Any startup command-line options that the tool should launch with.
 */
static PyObject* py_launchTool( PyObject * args )
{
	char* name = 0;
	char* cmdline = 0;
	if ( !PyArg_ParseTuple( args, "ss", &name, &cmdline ) )
	{
		PyErr_SetString( PyExc_TypeError, "WorldEditor.launchTool()"
			"expects two string arguments." );
		return NULL;
	}

	if ( name && cmdline )
	{
		char exe[ MAX_PATH ];
		GetModuleFileName( NULL, exe, sizeof( exe ) );
		if( std::count( exe, exe + strlen( exe ), '\\' ) > 2 )
		{
			*strrchr( exe, '\\' ) = 0;
			*strrchr( exe, '\\' ) = 0;
			std::string path = exe;
			path += "\\";
			path += name;
			std::replace( path.begin(), path.end(), '/', '\\' );

			std::string commandLine = path + "\\" + name + ".exe " + cmdline;

			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			GetStartupInfo( &si );

			TRACE_MSG( "WorldEditor.launchTool: cmdline = %s, path = %s\n", commandLine.c_str(), path.c_str() );

			if( CreateProcess( NULL, (LPSTR)commandLine.c_str(), NULL, NULL, FALSE, 0, NULL, path.c_str(),
				&si, &pi ) )
			{
				CloseHandle( pi.hThread );
				CloseHandle( pi.hProcess );
			}
		}
	}

	Py_Return;
}
PY_MODULE_FUNCTION( launchTool, WorldEditor )


/**
 *  finds a chunk with a terrain block, and returns the block
 *  TODO: this code is a bit hacky. Should get it from space.settings or
 *  similar.
 */
static Terrain::EditorBaseTerrainBlock* anyTerrainBlock()
{
	for ( std::set<Chunk*>::iterator it = EditorChunkCache::chunks_.begin();
		it != EditorChunkCache::chunks_.end() ; ++it )
	{
		if ( ChunkTerrainCache::instance( *(*it) ).pTerrain() )
		{
			return 
				static_cast<Terrain::EditorBaseTerrainBlock*>(
					ChunkTerrainCache::instance( *(*it) ).pTerrain()->block().getObject() );
		}
	}

	return NULL;
}


/*~ function WorldEditor.terrainHeightMapRes
 *	@components{ worldeditor }
 *
 *	This function returns the current terrain height map resolution.
 *
 *	@return current terrain height map resolution, or 1 if not available.
 */
static PyObject * py_terrainHeightMapRes( PyObject * args )
{
	int res = 0;

	Terrain::EditorBaseTerrainBlock* tb = anyTerrainBlock();
	if ( tb )
		res = tb->heightMap().blocksWidth();

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( terrainHeightMapRes, WorldEditor )


/*~ function WorldEditor.terrainBlendsRes
 *	@components{ worldeditor }
 *
 *	This function returns the current terrain layer blend resolution.
 *
 *	@return current terrain layer blend resolution, or 1 if not available.
 */
static PyObject * py_terrainBlendsRes( PyObject * args )
{
	int res = 0;

	Terrain::EditorBaseTerrainBlock* tb = anyTerrainBlock();
	if ( tb && tb->numberTextureLayers() )
		res = tb->textureLayer( 0 ).width() - 1;

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( terrainBlendsRes, WorldEditor )


/*~ function WorldEditor.terrainHoleMapRes
 *	@components{ worldeditor }
 *
 *	This function returns the current terrain hole map resolution.
 *
 *	@return current terrain hole map resolution, or 1 if not available.
 */
static PyObject * py_terrainHoleMapRes( PyObject * args )
{
	int res = 0;

	Terrain::EditorBaseTerrainBlock* tb = anyTerrainBlock();
	if ( tb )
		res = tb->holeMap().width();

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( terrainHoleMapRes, WorldEditor )


//
// Terrain conversion utilities
//

void convertTerrain( const std::string& space, bool reconvert )
{
	DataSectionPtr pSpaceSection = BWResource::openSection( space );
	if (pSpaceSection)
	{
		SyncMode syncMode;

		DataSectionPtr spaceSettings = BWResource::openSection(
			WorldManager::instance().chunkDirMapping()->path() + "/" + SPACE_SETTING_FILE_NAME );
		if (!spaceSettings)
		{
			ERROR_MSG( "Couldn't open space.settings file.\n" );
			return;
		}

		spaceSettings->deleteSection( "terrain" );

		DataSectionPtr terrainSettings = spaceSettings->openSection( "terrain", true );

		// init it the space terrain settings to allow proper conversion
		Terrain::TerrainSettingsPtr pTerrainSettings = new Terrain::TerrainSettings();
		pTerrainSettings->initDefaults();

		// Setup the configurable options
		pTerrainSettings->heightMapSize(
			Options::getOptionInt( "terrain2/defaults/heightMapSize", 
			pTerrainSettings->heightMapSize() ) );
		pTerrainSettings->normalMapSize(
			Options::getOptionInt( "terrain2/defaults/normalMapSize", 
			pTerrainSettings->normalMapSize() ) );
		pTerrainSettings->holeMapSize(
			Options::getOptionInt( "terrain2/defaults/holeMapSize", 
			pTerrainSettings->holeMapSize() ) );
		pTerrainSettings->shadowMapSize(
			Options::getOptionInt( "terrain2/defaults/shadowMapSize", 
			pTerrainSettings->shadowMapSize() ) );
		pTerrainSettings->blendMapSize(
			Options::getOptionInt( "terrain2/defaults/blendMapSize", 
			pTerrainSettings->blendMapSize() ) );

		pTerrainSettings->save( terrainSettings );
		if ( !spaceSettings->save() )
		{
			ERROR_MSG( "Couldn't create space.settings/terrain section.\n" );
			return;
		}

		TerrainConverter pHMC;
		pHMC.init( space, pSpaceSection );
		ProgressTask progress( WorldManager::instance().progressBar(), L("WORLDEDITOR/WORLDEDITOR/BIGBANG/BIG_BANG/CONVERTING_PROGRESS") );
		pHMC.convertAll(
			&progress,
			reconvert );

		WorldManager::instance().resetTerrainInfo();
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, convertTerrain, ARG( std::string,
						OPTARG( bool, true, END )  ), WorldEditor );


/*~ function WorldEditor.convertCurrentTerrain
 *	@components{ worldeditor }
 *
 *	This function converts the current terrain to new terrain.
 */
static void convertCurrentTerrain()
{
	if ( !WorldManager::instance().warnSpaceNotLocked() )
		return;

	std::string spaceName = WorldManager::instance().chunkDirMapping()->path();
	if ( spaceName[ spaceName.length() - 1 ] == '/' )
		spaceName = spaceName.substr( 0, spaceName.length() - 1 );

	if ( !WorldManager::instance().canClose( "&Convert" ) )
	{
		// avoid converting to avoid loosing changes.
		return;
	}

	ConvertSpaceDlg dlg;
	if ( dlg.DoModal() != IDOK )
		return;

	convertTerrain( spaceName, false );

	// it's the same space, so reload all chunks
	WorldManager::instance().reloadAllChunks( false );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, convertCurrentTerrain, END, WorldEditor )


/*~ function WorldEditor.canConvertCurrentTerrain
 *	@components{ worldeditor }
 *
 *	This function returns 1 if the current terrain can be converted
 *
 *	@return 1 if the current terrain can be converted, or 0 if not.
 */
static PyObject * py_canConvertCurrentTerrain( PyObject * args )
{
	int res =
		WorldManager::instance().pTerrainSettings()->version() == 100 ? 1 : 0;

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( canConvertCurrentTerrain, WorldEditor )


void convertTerrainSingle( const std::string& space, const std::string& chunkId )
{
	DataSectionPtr pSpaceSection = BWResource::openSection( space );
	if (pSpaceSection)
	{
		// convert chunk id to grid
		int32 x, y;
		if( ChunkTerrain::outsideChunkIDToGrid( chunkId, x, y ) )
		{
			TerrainConverter pHMC;
			pHMC.init( space, pSpaceSection );
			pHMC.convertSingle( x, y, true );
		}
		else
		{
			PyErr_Format( PyExc_ValueError, "chunkId %s is not the right format", chunkId.c_str() );
		}
	}
	else
	{
		PyErr_Format( PyExc_ValueError, "space %s not found", space.c_str() );
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, convertTerrainSingle, ARG( std::string, ARG( std::string, END  ) ), WorldEditor );


void wipeTerrain( const std::string& space )
{
	DataSectionPtr pSpaceSection = BWResource::openSection( space );
	if (pSpaceSection)
	{
		TerrainConverter* pHMC = new TerrainConverter();
		pHMC->init( space, pSpaceSection );
		pHMC->wipeAll();
		delete pHMC;
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, wipeTerrain, ARG( std::string, END  ), WorldEditor );


void wipeTerrainRect( const std::string& space, int xStart, int xEnd,
					   int zStart, int zEnd )
{
	DataSectionPtr pSpaceSection = BWResource::openSection( space );
	if (pSpaceSection)
	{
		TerrainConverter* pHMC = new TerrainConverter();
		pHMC->init( space, pSpaceSection );
		pHMC->wipeRect( xStart, xEnd, zStart, zEnd );
		delete pHMC;
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, wipeTerrainRect, ARG( std::string, ARG( int,
						ARG( int, ARG( int, ARG( int, END  ))))), WorldEditor );


/*~ function WorldEditor.resizeSpaceTerrainMaps
 *	@components{ worldeditor }
 *
 *	@param space				space to resize
 *	@param heightMapSize		new height map size, or 0 to keep the old size.
 *	@param normalMapSize		new normal map size, or 0 to keep the old size.
 *	@param shadowMapSize		new shadow map size, or 0 to keep the old size.
 *	@param holeMapSize			new hole map size, or 0 to keep the old size.
 *	@param blends				new blends map size, or 0 to keep the old size.
 */
static void resizeSpaceTerrainMaps( const std::string& space,
   uint32 height, uint32 normal, uint32 shadow, uint32 hole, uint32 blends )
{
	SyncMode syncMode;

	TerrainMapResizer resizer;

	TerrainMapResizer::MapSizeInfo newSizes;
	newSizes.heightMap_ = height;
	newSizes.normalMap_ = normal;
	newSizes.shadowMap_ = shadow;
	newSizes.holeMap_ = hole;
	newSizes.blendMap_ = blends;

	ProgressTask progress( WorldManager::instance().progressBar(), L("WORLDEDITOR/WORLDEDITOR/BIGBANG/BIG_BANG/RESIZEMAPS_PROGRESS") );

	if ( !resizer.resize( space, newSizes, &progress ) )
	{
		ERROR_MSG( "Couldn't resize maps for space '%s'\n", space.c_str() );
		return;
	}
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resizeSpaceTerrainMaps, ARG( std::string,
						ARG( uint32, ARG( uint32, ARG( uint32, ARG( uint32, ARG( uint32,
						END ) ) ) ) ) ), WorldEditor );


/*~ function WorldEditor.resizeTerrainMaps
 *	@components{ worldeditor }
 *
 *	
 */
static void resizeTerrainMaps()
{
	if ( !WorldManager::instance().warnSpaceNotLocked() )
		return;

	std::string spaceName = WorldManager::instance().chunkDirMapping()->path();
	if ( spaceName[ spaceName.length() - 1 ] == '/' )
		spaceName = spaceName.substr( 0, spaceName.length() - 1 );

	if ( !WorldManager::instance().canClose( "&Resize Maps" ) )
	{
		// avoid converting to avoid loosing changes.
		return;
	}

	ResizeMapsDlg dlg;
	if ( dlg.DoModal() != IDOK )
		return;

	resizeSpaceTerrainMaps( spaceName,
		0, 0, 0, 0, dlg.blendsMapSize() );

	// it's the same space, so reload all chunks
	WorldManager::instance().reloadAllChunks( false );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resizeTerrainMaps, END, WorldEditor );


/*~ function WorldEditor.canResizeCurrentTerrain
 *	@components{ worldeditor }
 *
 *	This function returns 1 if the current terrain maps can be resized.
 *
 *	@return 1 if the current terrain maps can be resized, or 0 if not.
 */
static PyObject * py_canResizeCurrentTerrain( PyObject * args )
{
	int res =
		WorldManager::instance().pTerrainSettings()->version() == 100 ? 0 : 1;

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( canResizeCurrentTerrain, WorldEditor )


/*~ function WorldEditor.resaveAllTerrainBlocks
 *	@components{ worldeditor }
 *
 *	This function resaves all terrain blocks in the space. This is used when the
 *	file format changes and the client does not support the same format.
 */
static void resaveAllTerrainBlocks()
{
	WorldManager::instance().resaveAllTerrainBlocks();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, resaveAllTerrainBlocks, END, WorldEditor )

/*~ function WorldEditor.restitchAllTerrainBlocks
 *	@components{ worldeditor }
 *
 *	This function restitches all chunks in the space to eliminate seams in the terrain.
 */
static void restitchAllTerrainBlocks()
{
	WorldManager::instance().restitchAllTerrainBlocks();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, restitchAllTerrainBlocks, END, WorldEditor )

/*~ function WorldEditor.regenerateThumbnails
 *	@components{ worldeditor }
 *
 *	This function goes through all chunks, both loaded and unloaded, and
 *	recalculates the thumbnails then saves them directly to disk. Chunks
 *	that were unloaded are ejected when it finishes with them, so large
 *	spaces can be regenerated. The downside is that there is no undo/redo, and
 *	the .cdata files are modified directly. It also assumes that the shadow
 *	data is up to date.
 *
 *	This function also deletes the time stamps and dds files.
 */
static void regenerateThumbnails()
{
	WorldManager::instance().regenerateThumbnailsOffline();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, regenerateThumbnails, END, WorldEditor )


/*~ function WorldEditor.convertSpaceToZip
 *	@components{ worldeditor }
 *
 *	This function goes through all .cdata files of the current space and 
 *	converts them to use zip sections.  It is okay to call this even if
 *	the current space uses zip sections.
 */
static void convertSpaceToZip()
{
	WorldManager::instance().convertSpaceToZip();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, convertSpaceToZip, END, WorldEditor )


/*~ function WorldEditor.regenerateLODs
 *	@components{ worldeditor }
 *
 *	This function goes through all chunks, both loaded and unloaded, and
 *	recalculates the terrain LOD textures then saves them directly to disk. 
 *	Chunks that were unloaded are ejected when it finishes with them, so large
 *	spaces can be regenerated. The downside is that there is no undo/redo, and
 *	the .cdata files are modified directly. 
 */
static void regenerateLODs()
{
	if (WorldManager::instance().pTerrainSettings()->version() >= 200)
		WorldManager::instance().regenerateLODsOffline();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, regenerateLODs, END, WorldEditor )


/*~ function WorldEditor.canRegenerateLODs
 *	@components{ worldeditor }
 *
 *	This function returns 1 if the terrain LODs can be regenerated.
 *
 *	@return 1 if the current terrain LODs can be regenerated, or 0 if not.
 */
static PyObject * py_canRegenerateLODs( PyObject * args )
{
	int res =
		WorldManager::instance().pTerrainSettings()->version() >= 200 ? 1 : 0;

	return PyInt_FromLong( res );
}
PY_MODULE_FUNCTION( canRegenerateLODs, WorldEditor )


/*~ function WorldEditor.terrainCollide
 *	@components{ worldeditor }
 *
 *	This function tests if a given line collides with the terrain, and returns
 *  the distance traveled from the start to the collision point.
 *
 *  @param start	Start point of the line to collide against the terrain.
 *  @param end		End point of the line to collide against the terrain.
 *	@return	Distance from start to the collision point, -1 if no collision.
 */
static float terrainCollide( Vector3 start, Vector3 end )
{
	ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
	if ( !space )
		return -1.0f;

	ClosestTerrainObstacle terrainCallback;
	space->collide( start, end, terrainCallback );
	if (terrainCallback.collided())
	{
		return terrainCallback.dist();
	}
	else
	{
		return -1.0;
	}
}
PY_AUTO_MODULE_FUNCTION( RETDATA, terrainCollide, ARG( Vector3, ARG( Vector3, END) ), WorldEditor );


/*~ function WorldEditor.touchAllChunks
 *	@components{ worldeditor }
 *
 *	This function marks all chunks in the current space as dirty.
 */
static void touchAllChunks()
{
	WorldManager::instance().touchAllChunks();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, touchAllChunks, END, WorldEditor );


// -----------------------------------------------------------------------------
// Section: Common stuff (should be elsewhere...)
// -----------------------------------------------------------------------------

/// Yes, these are globals
struct TimerRecord
{
	/**
	 *	This method returns whether or not the input record occurred later than
	 *	this one.
	 *
	 *	@return True if input record is earlier (higher priority),
	 *		false otherwise.
	 */
	bool operator <( const TimerRecord & b ) const
	{
		return b.time < this->time;
	}

	float		time;			///< The time of the record.
	PyObject	* function;		///< The function associated with the record.
};

typedef std::priority_queue<TimerRecord>	Timers;
Timers	gTimers;


/*~ function WorldEditor.callback
 *	@components{ worldeditor }
 *
 *	Registers a callback function to be called after a certain time,
 *	but not before the next tick. (If registered during a tick
 *	and it has expired then it will go off still - add a minuscule
 *	amount of time to BigWorld.time() to prevent this if unwanted)
 *	Non-positive times are interpreted as offsets from the current time.
 *
 *	@param time The amount of time to pass before the function is called.
 *	@param function The callback function.
 */
static PyObject * py_callback( PyObject * args )
{
	float		time = 0.f;
	PyObject *	function = NULL;

	if (!PyArg_ParseTuple( args, "fO", &time, &function ) ||
		function == NULL || !PyCallable_Check( function ) )
	{
		PyErr_SetString( PyExc_TypeError, "py_callback: Argument parsing error." );
		return NULL;
	}

	if (time < 0) time = 0.f;

	//TODO
	//time = EntityManager::getTimeNow() + time;
	Py_INCREF( function );

	TimerRecord		newTR = { time, function };
	gTimers.push( newTR );

	Py_Return;
}
PY_MODULE_FUNCTION( callback, WorldEditor )


/**
 *	This class implements a PyOutputWriter with the added functionality of
 *	writing to the Python console.
 */
class BWOutputWriter : public PyOutputWriter
{
public:
	BWOutputWriter( const char * prefix, const char * fileText ) :
		PyOutputWriter( fileText, /*shouldWritePythonLog = */true )
	{
	}

protected:
	virtual void printMessage( const std::string & msg );
};


/**
 *	This method implements the default behaviour for printing a message. Derived
 *	class should override this to change the behaviour.
 */
void BWOutputWriter::printMessage( const std::string & msg )
{
	XConsole * pXC = ConsoleManager::instance().find( "Python" );
	if (pXC != NULL) 
	{
		pXC->print( msg );
	}

	this->PyOutputWriter::printMessage( msg );
}



// -----------------------------------------------------------------------------
// Section: WorldEditorScript namespace functions
// -----------------------------------------------------------------------------


namespace
{
	PyObject* s_keyModule = NULL;
}

/**
 *	This method initialises the WorldEditor script.
 */
bool WorldEditorScript::init( DataSectionPtr pDataSection )
{
	// Particle Systems are creatable from Python code
	MF_VERIFY( ParticleSystemManager::init() );

	std::string scriptPath = "resources/scripts;";
	scriptPath += EntityDef::Constants::entitiesEditorPath();
	scriptPath += ";";
	scriptPath += EntityDef::Constants::userDataObjectsEditorPath();
	// Call the general init function
	if (!Script::init( scriptPath, "editor" ))
	{
		CRITICAL_MSG( "WorldEditorScript::init: Failed to init Script.\n" );
		return false;
	}

	// We implement our own stderr / stdout so we can see Python's output
	PyObject *pSysModule = PyImport_AddModule( "sys" );	// borrowed

	char fileText[ 256 ];
	
	#ifdef _DEBUG
		const char * config = "Debug";
	#elif defined( _HYBRID )
		const char * config = "Hybrid";
	#else
		const char * config = "Release";
	#endif

	time_t aboutTime = time( NULL );

	bw_snprintf( fileText, sizeof(fileText), "WorldEditor %s (compiled on %s) starting on %s",
		config,
		aboutCompileTimeString,
		ctime( &aboutTime ) );

	PyObject * pStdErrWriter = new BWOutputWriter( "stderr: ", fileText );
	PyObject * pStdOutWriter = new BWOutputWriter( "stdout: ", fileText );

	PyObject_SetAttrString( pSysModule, "stderr", pStdErrWriter );
	PyObject_SetAttrString( pSysModule,	"stdout", pStdOutWriter );

	s_keyModule = PyImport_ImportModule( "keys" );
	
	PyObject * pInit =
		PyObject_GetAttrString( s_keyModule, "init" );
	if (pInit != NULL)
	{
		PyRun_SimpleString( PyString_AsString(pInit) );
		Py_DECREF( pInit );
	}

	Py_DECREF( pStdErrWriter );
	Py_DECREF( pStdOutWriter );

	PyErr_Clear();

	return true;
}


/**
 *	This method does the script clean up.
 */
void WorldEditorScript::fini()
{
	Py_XDECREF( s_keyModule );

	Script::fini();
	ParticleSystemManager::fini();
}
