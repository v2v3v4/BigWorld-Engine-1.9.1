/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	UalManager: Manages interaction between the App and the Ual Dialog(s)
 */


#include "pch.hpp"
#include "ual_dialog.hpp"
#include "ual_favourites.hpp"
#include "ual_history.hpp"
#include "ual_manager.hpp"

#include "common/string_utils.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );

/// Asset Locator library Singleton.
BW_SINGLETON_STORAGE( UalManager )


UalManager::UalManager()
	: GUI::ActionMaker<UalManager>( "UalActionRefresh", &UalManager::guiActionRefresh )
	, GUI::ActionMaker<UalManager,1>( "UalActionLayout", &UalManager::guiActionLayout )
	, thumbnailManager_( new ThumbnailManager() )
	, timerID_( 0 )
	, itemClickCallback_( 0 )
	, itemDblClickCallback_( 0 )
	, startPopupMenuCallback_( 0 )
	, endPopupMenuCallback_( 0 )
	, startDragCallback_( 0 )
	, updateDragCallback_( 0 )
	, endDragCallback_( 0 )
	, focusCallback_( 0 )
	, errorCallback_( 0 )
{
	favourites_.setChangedCallback( new UalFunctor0< UalManager >( this, &UalManager::favouritesCallback ) );
	history_.setChangedCallback( new UalFunctor0< UalManager >( this, &UalManager::historyCallback ) );

	timerID_ = SetTimer( 0, 0, 100, onTimer );
}

UalManager::~UalManager()
{
	KillTimer( 0, timerID_ );
}

void UalManager::favouritesCallback()
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		(*i)->favouritesChanged();
}

void UalManager::historyCallback()
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		(*i)->historyChanged();
}

void UalManager::addPath( const std::string& path )
{
	if ( !path.length() )
		return;

	std::string pathL = path;
	std::replace( pathL.begin(), pathL.end(), '/', '\\' );
	if ( std::find< std::vector<std::string>::iterator, std::string >(
			paths_.begin(), paths_.end(), pathL ) != paths_.end() )
		return;
	paths_.push_back( pathL );
}

const std::string UalManager::getPath( int i )
{
	if ( i < 0 )
		return "";

	for( std::vector<std::string>::iterator p = paths_.begin(); p != paths_.end(); ++p )
		if ( i-- == 0 )
			return *p;

	return "";
}

int UalManager::getNumPaths()
{
	return paths_.size();
}

void UalManager::setConfigFile( std::string config )
{
	configFile_ = config;
}

void UalManager::fini()
{
	INFO_MSG( "UAL Manager - Waiting for the Thumbnail Manager to stop ...\n" );
	thumbnailManager_->stop();
	INFO_MSG( "UAL Manager - ... Thumbnail Manager stopped\n" );
	UalDialog::fini();
}

// Private
void UalManager::registerDialog( UalDialog* dialog )
{
	dialogs_.push_back( dialog );
}

void UalManager::unregisterDialog( UalDialog* dialog )
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
	{
		if ( *i == dialog )
		{
			dialogs_.erase( i );
			return;
		}
	}
}


/*static*/ void UalManager::onTimer(HWND hwnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime )
{
	instance().thumbnailManager().tick();
}


const std::string UalManager::getConfigFile()
{
	return configFile_;
}

UalDialog* UalManager::getActiveDialog()
{
	// if there's only one, return it
	if ( dialogs_.size() == 1 )
		return *dialogs_.begin();

	// more than one, find the focused control and find the parent dialog,
	HWND fw = GetFocus();
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		if ( (*i)->GetSafeHwnd() == fw || IsChild( (*i)->GetSafeHwnd(), fw ) )
			return (*i);

	// last resort hack: use rectangles, because when the first thing clicked
	// is a toolbar button, then nothing is focused
	CPoint pt;
	GetCursorPos( &pt );
	HWND hwnd = ::WindowFromPoint( pt );
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
	{
		if ( IsChild( (*i)->GetSafeHwnd(), hwnd ) )
			return (*i);
	}
	return 0;
}

void UalManager::updateItem( const std::string& longText )
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		(*i)->updateItem( longText );
}


/**
 *	Force a refresh all of the dialogs.
 */
void UalManager::refreshAllDialogs()
{
	for (DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i)
	{
		(*i)->guiActionRefresh();
	}
}


void UalManager::showItem( const std::string& vfolder, const std::string& longText )
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		(*i)->showItem( vfolder, longText );
}

bool UalManager::guiActionRefresh( GUI::ItemPtr item )
{
	UalDialog* dlg = getActiveDialog();
	if ( dlg )
		dlg->guiActionRefresh();
	return true;
}

bool UalManager::guiActionLayout( GUI::ItemPtr item )
{
	UalDialog* dlg = getActiveDialog();
	if ( dlg )
		dlg->guiActionLayout();
	return true;
}

void UalManager::cancelDrag()
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		(*i)->resetDragDropTargets();
}

UalDialog* UalManager::updateDrag( const UalItemInfo& itemInfo, bool endDrag )
{
	for( DialogsItr i = dialogs_.begin(); i != dialogs_.end(); ++i )
		if ( (*i)->updateDrag( itemInfo, endDrag ) )
			return *i;
	return 0;
}

void UalManager::copyVFolder( UalDialog* srcUal, UalDialog* dstUal, const UalItemInfo& ii )
{
	if ( !srcUal || !dstUal )
		return;

	if ( ii.assetInfo().longText().empty() )
	{
		// It's a VFolder
		int oldCount = dstUal->folderTree_.getLevelCount();
		// it's a VFolder, so try to create it from the custom folders
		dstUal->loadCustomVFolders( srcUal->customVFolders_, ii.assetInfo().text() );
		if ( oldCount < dstUal->folderTree_.getLevelCount() )
		{
			// it was created from a custom vfolder, so add it to the new dialog's custom vfolders
			std::vector<DataSectionPtr> customVFolders;
			srcUal->customVFolders_->openSections( "customVFolder", customVFolders );
			for( std::vector<DataSectionPtr>::iterator s = customVFolders.begin();
				s != customVFolders.end(); ++s )
			{
				if ( ii.assetInfo().text() == (*s)->asString() )
				{
					if ( !dstUal->customVFolders_ )
						dstUal->customVFolders_ = new XMLSection( "customVFolders" );
					DataSectionPtr section = dstUal->customVFolders_->newSection( "customVFolder" );
					section->copy( *s );
					break;
				}
			}
		}

		// if there's a vfolder named the same, load it too. If not, this call
		// is still needed in order to build the excludeVFolders_ vector properly
		DataSectionPtr root = BWResource::openSection( configFile_ );
		if ( root )
			dstUal->loadVFolders( root->openSection( "VFolders" ), ii.assetInfo().text() );
	}
	else if ( ii.folderExtraData_ )
	{
		// it's not a VFolder, so create a custom folder from scratch.
		// For now, only Files VFolder items are clonable, so only managing custom Files-derived VFolders
		if ( !dstUal->customVFolders_ )
			dstUal->customVFolders_ = new XMLSection( "customVFolders" );
		DataSectionPtr section = dstUal->customVFolders_->newSection( "customVFolder" );
		section->setString( ii.assetInfo().text() );

		// find out if it inherits from a customVFolder or a VFolder
		std::string inheritName = ((VFolder*)ii.folderExtraData_)->getName();
		if ( srcUal->customVFolders_ )
		{
			std::vector<DataSectionPtr> customVFolders;
			srcUal->customVFolders_->openSections( "customVFolder", customVFolders );
			for( std::vector<DataSectionPtr>::iterator s = customVFolders.begin();
				s != customVFolders.end(); ++s )
			{
				if ( inheritName == (*s)->asString() )
				{
					inheritName = (*s)->readString( "inheritsFrom" );
					break;
				}
			}
		}
		section->writeString( "inheritsFrom", inheritName );

		section->writeString( "path", ii.assetInfo().longText() );
		dstUal->loadCustomVFolders( dstUal->customVFolders_, ii.assetInfo().text() );
		// build the excludeVFolders_ vector properly (using a special label to exclude all default vfolders)
		DataSectionPtr root = BWResource::openSection( configFile_ );
		if ( root )
			dstUal->loadVFolders( root->openSection( "VFolders" ), "***EXCLUDE_ALL***" );
	}
	// set folder custom info
	VFolderPtr srcVFolder = srcUal->folderTree_.getVFolder( ii.assetInfo().text(), false );
	VFolderPtr dstVFolder = dstUal->folderTree_.getVFolder( ii.assetInfo().text() );
	if ( srcVFolder && dstVFolder )
	{
		UalFolderData* srcData = (UalFolderData*)srcVFolder->getData();
		UalFolderData* dstData = (UalFolderData*)dstVFolder->getData();
		if ( srcData && dstData )
			dstData->thumbSize_ = srcData->thumbSize_;
	}
}
