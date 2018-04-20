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
 *	VFolderXmlProvider: Inherits from VFolderProvider
 */

#include "pch.hpp"
#include <vector>
#include <string>
#include "thumbnail_manager.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"
#include "common/string_utils.hpp"
#include "vfolder_xml_provider.hpp"
#include "list_xml_provider.hpp"


// Virtual Folder Xml Provider
VFolderXmlProvider::VFolderXmlProvider()
{
	init( "" );
}

VFolderXmlProvider::VFolderXmlProvider(	const std::string& path )
{
	init( path );
}

VFolderXmlProvider::~VFolderXmlProvider()
{
}

void VFolderXmlProvider::init( const std::string& path )
{
	path_ = path;
	StringUtils::toLowerCase( path_ );
	std::replace( path_.begin(), path_.end(), '/', '\\' );

	items_.clear();

	sort_ = false;

	DataSectionPtr dataSection;

	dataSection = BWResource::openSection( path_ );

	if ( !dataSection )
		return;
	
	sort_ = dataSection->readBool( "sort", sort_ );
}

bool VFolderXmlProvider::startEnumChildren( const VFolderItemDataPtr parent )
{
	DataSectionPtr dataSection;

	items_.clear();

	BWResource::instance().purge( path_ );
	dataSection = BWResource::openSection( path_ );

	if ( !dataSection )
		return false;

	std::vector<DataSectionPtr> sections;
	dataSection->openSections( "item", sections );
	filterHolder_->enableSearchText( false );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		AssetInfoPtr item = new AssetInfo( *s );
		if ( !filterHolder_ || filterHolder_->filter( item->text(), item->longText() ) )
			items_.push_back( item );
	}
	filterHolder_->enableSearchText( true );

	itemsItr_ = items_.begin();
	return true;
}

VFolderItemDataPtr VFolderXmlProvider::getNextChild( ThumbnailManager& thumbnailManager, CImage& img )
{
	if ( itemsItr_ == items_.end() )
		return 0;

	if ( !(*itemsItr_)->thumbnail().empty() )
		thumbnailManager.create( (*itemsItr_)->thumbnail(), img, 16, 16, folderTree_ );
	else
		thumbnailManager.create( (*itemsItr_)->longText(), img, 16, 16, folderTree_ );

	VFolderXmlItemData* newItem = new VFolderXmlItemData( this,
		*(*itemsItr_), XMLGROUP_ITEM, false, (*itemsItr_)->thumbnail() );

	itemsItr_++;

	return newItem;
}

void VFolderXmlProvider::getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img )
{
	if ( !data )
		return;

	VFolderXmlItemData* xmlData = (VFolderXmlItemData*)data.getObject();

	if ( !xmlData->thumb().empty() )
		thumbnailManager.create( xmlData->thumb(), img, 16, 16, folderTree_ );
	else
		thumbnailManager.create( xmlData->assetInfo().longText(), img, 16, 16, folderTree_ );
}


const std::string VFolderXmlProvider::getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished )
{
	if ( !data )
		return "";

	std::string desc;
	if ( data->assetInfo().description().empty() )
		desc = data->assetInfo().longText();
	else
		desc = data->assetInfo().description();

	if ( data->isVFolder() )
	{
		desc =
			L
			(
				"UAL/VFOLDER_XML_PROVIDER/NUM_ITEMS",
				getPath(),
				numItems
			);
	}

	return desc;
}

bool VFolderXmlProvider::getListProviderInfo(
	VFolderItemDataPtr data,
	std::string& retInitIdString,
	ListProviderPtr& retListProvider,
	bool& retItemClicked )
{
	if ( !data || !listProvider_ )
		return false;

	retItemClicked = !data->isVFolder();

	if ( retInitIdString == getPath() && retListProvider == listProvider_ )
		return false;

	retListProvider = listProvider_;
	((ListXmlProvider*)listProvider_.getObject())->init( getPath() );
	retInitIdString = getPath();

	return true;
}


std::string VFolderXmlProvider::getPath()
{
	return path_;
}

bool VFolderXmlProvider::getSort()
{
	return sort_;
}
