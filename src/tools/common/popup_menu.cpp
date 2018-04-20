/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*
 *  Generic popup menu helper class
 */

#include "pch.hpp"
#include <stack>
#include "popup_menu.hpp"


PopupMenu::PopupMenu()
{
}

PopupMenu::PopupMenu( Items& items ) :
	items_( items )
{
}

int PopupMenu::doModal( HWND parent )
{
	if ( items_.empty() )
		return 0;

	CPoint pt;
	GetCursorPos( &pt );
	HMENU menu = ::CreatePopupMenu();
	MENUITEMINFO info = { sizeof(info),
		MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_STATE,
		MFT_STRING, MFS_DEFAULT, 0, 0, 0, 0, 0, "", 0, 0 };
	int pos = 0;

	std::stack<HMENU> menuStack;
	menuStack.push( menu );
	for( PopupMenu::Items::iterator i = items_.begin();
		i != items_.end(); ++i )
	{
		HMENU curMenu = menuStack.top();
		char *str = const_cast<char*>( (*i).first.c_str() );
		bool checked = false;
		if ( strlen( str ) >= 2 && str[0] == '#' && str[1] == '#' )
		{
			str += 2;
			checked = true;
		}
		if ( (*i).second == PopupMenu::Separator )
		{
			info.fType = MFT_SEPARATOR;
			::InsertMenuItem( curMenu, pos++, TRUE, &info );
		}
		else if ( (*i).second == PopupMenu::StartSubmenu )
		{
			info.dwTypeData = str;
			info.fMask |= MIIM_SUBMENU;
			info.fType = MFT_STRING;
			info.fState = checked ? MFS_CHECKED : 0;
			info.wID = (*i).second;
			info.hSubMenu = ::CreateMenu();
			::InsertMenuItem( curMenu, pos++, TRUE, &info );
			info.fMask = info.fMask & ~MIIM_SUBMENU;
			menuStack.push( info.hSubMenu );
		}
		else if ( (*i).second == PopupMenu::EndSubmenu )
		{
			if ( menuStack.size() > 1 )
				menuStack.pop();
		}
		else
		{
			info.dwTypeData = str;
			info.fState = checked ? MFS_CHECKED : 0;
			info.fType = MFT_STRING;
			info.wID = (*i).second;
			::InsertMenuItem( curMenu, pos++, TRUE, &info );
		}
	}

	int result = 0;
	if ( ::GetMenuItemCount( menu ) )
	{
		result = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_LEFTBUTTON,
			pt.x, pt.y, 0, parent, NULL );
	}

	DestroyMenu( menu );

	return result;
}

//statics
void PopupMenu::addItem( Items& in, const std::string& name, int id )
{
	in.push_back( PopupMenu::Item( name, id ) );
}

// adds all items in the Items vector
void PopupMenu::addItems( Items& in, Items& items )
{
	for( PopupMenu::Items::iterator i = items.begin();
		i != items.end(); ++i )
		in.push_back( *i );
}
