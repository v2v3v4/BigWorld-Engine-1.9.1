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

// This file contains code that uses both the pyscript and physics2 libraries
// and is common to many processes. This is not big enough to be its own
// library.

// BW Tech Headers
#include "moo/graphics_settings.hpp"

#include "pyscript/script.hpp"
#include "pyscript/py_data_section.hpp"

int PyMoo_token = 0;

/*~ function BigWorld.graphicsSettings
 *	@components{ client }
 *
 *  Returns list of registered graphics settings
 *	@return	list of 4-tuples in the form (label : string, index to active
 *			option in options list: int, options : list, desc : string). Each option entry is a
 *			3-tuple in the form (option label : string, support flag : boolean, desc : string).
 */
static PyObject * graphicsSettings()
{
	typedef Moo::GraphicsSetting::GraphicsSettingVector GraphicsSettingVector;
	const GraphicsSettingVector & settings = Moo::GraphicsSetting::settings();
	PyObject * settingsList = PyList_New(settings.size());

	GraphicsSettingVector::const_iterator setIt  = settings.begin();
	GraphicsSettingVector::const_iterator setEnd = settings.end();
	while (setIt != setEnd)
	{
		typedef Moo::GraphicsSetting::StringStringBoolVector StringStringBoolVector;
		const StringStringBoolVector & options = (*setIt)->options();
		PyObject * optionsList = PyList_New(options.size());
		StringStringBoolVector::const_iterator optIt  = options.begin();
		StringStringBoolVector::const_iterator optEnd = options.end();
		while (optIt != optEnd)
		{
			PyObject * optionItem = PyTuple_New(3);
			PyTuple_SetItem(optionItem, 0, Script::getData(optIt->first)); // Label
			PyTuple_SetItem(optionItem, 1, Script::getData(optIt->second.second)); // Enabled
			PyTuple_SetItem(optionItem, 2, Script::getData(optIt->second.first)); // Description

			int optionIndex = std::distance(options.begin(), optIt);
			PyList_SetItem(optionsList, optionIndex, optionItem);
			++optIt;
		}
		PyObject * settingItem = PyTuple_New(4);
		PyTuple_SetItem(settingItem, 0, Script::getData((*setIt)->label()));
		PyTuple_SetItem(settingItem, 3, Script::getData((*setIt)->desc()));

		// is setting is pending, use value stored in pending
		// list. Otherwise, use active option in setting
		int activeOption = 0;
		if (!Moo::GraphicsSetting::isPending(*setIt, activeOption))
		{
			activeOption = (*setIt)->activeOption();
		}
		PyTuple_SetItem(settingItem, 1, Script::getData(activeOption));
		PyTuple_SetItem(settingItem, 2, optionsList);

		int settingIndex = std::distance(settings.begin(), setIt);
		PyList_SetItem(settingsList, settingIndex, settingItem);
		++setIt;
	}

	return settingsList;
}
PY_AUTO_MODULE_FUNCTION( RETOWN, graphicsSettings, END, BigWorld )


/*~ function BigWorld.setGraphicsSetting
 *	@components{ client }
 *
 *  Sets graphics setting option.
 *
 *  Raises a ValueError if the given label does not name a graphics setting, if
 *  the option index is out of range, or if the option is not supported.
 *
 *	@param	label		    string - label of setting to be adjusted.
 *	@param	optionIndex		int - index of option to set.
 */
static bool setGraphicsSetting( const std::string label, int optionIndex )
{

	bool result = false;
	typedef Moo::GraphicsSetting::GraphicsSettingVector GraphicsSettingVector;
	GraphicsSettingVector settings = Moo::GraphicsSetting::settings();
	GraphicsSettingVector::const_iterator setIt  = settings.begin();
	GraphicsSettingVector::const_iterator setEnd = settings.end();
	while (setIt != setEnd)
	{
		if ((*setIt)->label() == label)
		{
			if (optionIndex < int( (*setIt)->options().size() ))
			{
				if ((*setIt)->options()[optionIndex].second.second)
				{
					(*setIt)->selectOption( optionIndex );
					result = true;
				}
				else
				{
					PyErr_SetString( PyExc_ValueError,
						"Option is not supported." );
				}
			}
			else
			{
				PyErr_SetString( PyExc_ValueError,
					"Option index out of range." );
			}
			break;
		}
		++setIt;
	}
	if (setIt == setEnd)
	{
		PyErr_SetString( PyExc_ValueError,
			"No setting found with given label." );
	}

	return result;
}
PY_AUTO_MODULE_FUNCTION( RETOK, setGraphicsSetting,
	ARG( std::string, ARG( int, END ) ), BigWorld )


/*~ function BigWorld.commitPendingGraphicsSettings
 *	@components{ client }
 *
 *  This function commits any pending graphics settings. Some graphics
 *  settings, because they may block the game for up to a few minutes when
 *  coming into effect, are not committed immediately. Instead, they are
 *  flagged as pending and require commitPendingGraphicsSettings to be called
 *  to actually apply them.
 */
static void commitPendingGraphicsSettings()
{
	Moo::GraphicsSetting::commitPending();
}
PY_AUTO_MODULE_FUNCTION( RETVOID, commitPendingGraphicsSettings, END, BigWorld )


/*~ function BigWorld.hasPendingGraphicsSettings
 *	@components{ client }
 *
 *  This function returns true if there are any pending graphics settings.
 *  Some graphics settings, because they may block the game for up to a few
 *  minutes when coming into effect, are not committed immediately. Instead,
 *  they are flagged as pending and require commitPendingGraphicsSettings to be
 *  called to actually apply them.
 */
static bool hasPendingGraphicsSettings()
{
	return Moo::GraphicsSetting::hasPending();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, hasPendingGraphicsSettings, END, BigWorld )


/*~ function BigWorld.graphicsSettingsNeedRestart
 *	@components{ client }
 *
 *  This function returns true if any recent graphics setting change
 *	requires the client to be restarted to take effect. If that's the
 *	case, restartGame can be used to restart the client. The need restart
 *	flag is reset when this method is called.
 */
static bool graphicsSettingsNeedRestart()
{
	return Moo::GraphicsSetting::needsRestart();
}
PY_AUTO_MODULE_FUNCTION( RETDATA, graphicsSettingsNeedRestart, END, BigWorld )


/*~ function BigWorld.autoDetectGraphicsSettings
 *	@components{ client }
 *  
 *	Automatically detect the graphics settings
 *	based on the client's system properties.
 */
static void autoDetectGraphicsSettings()
{
	// Init GraphicsSettings with an empty DataSection to autoDetect settings.
	Moo::GraphicsSetting::init( NULL );
}
PY_AUTO_MODULE_FUNCTION( RETVOID, autoDetectGraphicsSettings, END, BigWorld )


/*~ function BigWorld.rollBackPendingGraphicsSettings
 *	@components{ client }
 *
 *  This function rolls back any pending graphics settings.
 */
static void rollBackPendingGraphicsSettings()
{
	return Moo::GraphicsSetting::rollbackPending();
}
PY_AUTO_MODULE_FUNCTION( RETVOID,
		rollBackPendingGraphicsSettings, END, BigWorld )

// py_moo.cpp
