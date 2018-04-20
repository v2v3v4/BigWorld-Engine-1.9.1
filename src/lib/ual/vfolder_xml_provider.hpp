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

#ifndef VFOLDER_XML_PROVIDER_HPP
#define VFOLDER_XML_PROVIDER_HPP

#include "folder_tree.hpp"
#include "resmgr/datasection.hpp"


// VFolderXmlItemData
class VFolderXmlItemData : public VFolderItemData
{
public:
	VFolderXmlItemData(
		VFolderProviderPtr provider,
		const AssetInfo& assetInfo,
		int group,
		bool expandable,
		const std::string& thumb ) :
		VFolderItemData( provider, assetInfo, group, expandable ),
		thumb_( thumb )
	{};

	virtual bool isVFolder() const { return false; };
	virtual std::string thumb() { return thumb_; };

private:
	std::string thumb_;
};

// VFolderXmlProvider
class VFolderXmlProvider : public VFolderProvider
{
public:
	enum FileGroup {
		XMLGROUP_ITEM = VFolderProvider::GROUP_ITEM
	};
	VFolderXmlProvider();
	VFolderXmlProvider( const std::string& path );
	virtual ~VFolderXmlProvider();

	virtual void init( const std::string& path );

	virtual bool startEnumChildren( const VFolderItemDataPtr parent );
	virtual VFolderItemDataPtr getNextChild( ThumbnailManager& thumbnailManager, CImage& img );
	virtual void getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img );

	virtual const std::string getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished );
	virtual bool getListProviderInfo(
		VFolderItemDataPtr data,
		std::string& retInitIdString,
		ListProviderPtr& retListProvider,
		bool& retItemClicked );

	// additional interface
	virtual std::string getPath();
	virtual bool getSort();
private:
	std::string path_;
	bool sort_;
	typedef SmartPointer<AssetInfo> ItemPtr;
	std::vector<ItemPtr> items_;
	std::vector<ItemPtr>::iterator itemsItr_;
};


#endif // VFOLDER_XML_PROVIDER_HPP