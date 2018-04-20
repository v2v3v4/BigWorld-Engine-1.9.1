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
 *	VFolderMultiProvider: Inherits from VFolderProvider to implement a VFolder
 *	provider that manages one or more sub-providers, allowing multiple asset
 *	sources to be shown under one UAL folder
 */


#include "pch.hpp"
#include <vector>
#include <string>
#include "thumbnail_manager.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"
#include "common/string_utils.hpp"
#include "vfolder_multi_provider.hpp"
#include "list_multi_provider.hpp"


// Virtual Folder Xml VFolderMultiProvider
VFolderMultiProvider::VFolderMultiProvider()
{
}

VFolderMultiProvider::~VFolderMultiProvider()
{
}

bool VFolderMultiProvider::startEnumChildren( const VFolderItemDataPtr parent )
{
	iter_ = providers_.begin();
	if ( iter_ == providers_.end() )
		return false;

	parent_ = parent.getObject();
	(*iter_)->startEnumChildren( parent_ );
	return true;
}

VFolderItemDataPtr VFolderMultiProvider::getNextChild( ThumbnailManager& thumbnailManager, CImage& img )
{
	if ( iter_ == providers_.end() )
		return NULL;

	// loop until we find another item in a provider, or until there are
	// no items left in any of the providers.
	VFolderItemDataPtr data;
	while ( (data = (*iter_)->getNextChild( thumbnailManager, img )) == NULL )
	{
		// the current provider ran out of items, so go to the next provider.
		++iter_;
		if ( iter_ == providers_.end() )
			return NULL; // no more providers, break.
		// moved to the next provider. Start enumerating it's items
		(*iter_)->startEnumChildren( parent_ );
	}

	if ( data != NULL )
	{
		// an item was found, so try to load it's thumbnail
		(*iter_)->getThumbnail( thumbnailManager, data, img );
	}

	return data;
}

void VFolderMultiProvider::getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img )
{
	if ( !data || !data->getProvider() )
		return;

	// load the item's thumbnail from it's provider
	data->getProvider()->getThumbnail( thumbnailManager, data, img );
}


const std::string VFolderMultiProvider::getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished )
{
	if ( !data )
		return "";

	std::string desc;
	if ( data->isVFolder() || !data->getExpandable() )
	{
		// if it's a folder or a vfolder, build summary info
		if ( data->assetInfo().description().empty() )
			desc = data->assetInfo().longText();
		else
			desc = data->assetInfo().description();

		if (finished)
		{
			desc = 
				L
				(
					"UAL/VFOLDER_MULTI_PROVIDER/DESCRIPTION", 
					desc,
					numItems
				);
		}
		else
		{
			desc = 
				L
				(
					"UAL/VFOLDER_MULTI_PROVIDER/DESCRIPTION_LOADING", 
					desc,
					numItems
				);
		}
	}
	else if ( data->getProvider() && data->getProvider()->getListProvider() )
	{
		// simple get the item's descriptive text directly from it's provider
		desc = data->getProvider()->getDescriptiveText( data, numItems, finished );
	}

	return desc;
}

bool VFolderMultiProvider::getListProviderInfo(
	VFolderItemDataPtr data,
	std::string& retInitIdString,
	ListProviderPtr& retListProvider,
	bool& retItemClicked )
{
	if ( !data || !listProvider_ )
		return false;

	if ( data->isVFolder() || !data->getExpandable() )
	{
		// if it's a folder or a vfolder, build summary info
		retItemClicked = false;
		retListProvider = listProvider_;
		// call this method in all the sub-providers, so they prepare their
		// list providers properly. Ignore their return values by using dummy
		// variables
		std::string dummyInitId;
		ListProviderPtr dummyList;
		bool dummyClick;
		bool ret = false;
		for( ProvVec::iterator i = providers_.begin();
			i != providers_.end(); ++i )
		{
			ret |= (*i)->getListProviderInfo(
				data, dummyInitId, dummyList, dummyClick );
		}
		return ret;
	}
	else
	{
		// it's an item, so simply return the item's info from its provider
		retItemClicked = true;
		if ( data->getProvider() )
		{
			return data->getProvider()->getListProviderInfo(
				data, retInitIdString, retListProvider, retItemClicked );
		}
	}

	return false;
}


void VFolderMultiProvider::addProvider( VFolderProviderPtr provider )
{
	if ( !provider )
		return;

	providers_.push_back( provider );
}
