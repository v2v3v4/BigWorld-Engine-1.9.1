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
 *	VFolderFileItemData: Inherits from VFolderItemData
 */

#ifndef VFOLDER_FILE_PROVIDER_HPP
#define VFOLDER_FILE_PROVIDER_HPP

#include <stack>
#include "folder_tree.hpp"

enum FILETREE_FLAGS {
	FILETREE_SHOWSUBFOLDERS = 1,
	FILETREE_SHOWFILES = 2,
	FILETREE_DONTRECURSE = 4
};


// VFolderFileItemData
class VFolderFileItemData : public VFolderItemData
{
public:
	enum ItemDataType {
		ITEMDATA_FOLDER,
		ITEMDATA_FILE
	};

	VFolderFileItemData(
		VFolderProviderPtr provider,
		ItemDataType type,
		const AssetInfo& assetInfo,
		int group,
		bool expandable );
	virtual ~VFolderFileItemData();

	virtual bool isVFolder() const { return false; };

	virtual bool handleDuplicate( VFolderItemDataPtr data );

	virtual ItemDataType VFolderFileItemData::getItemType() { return type_; };

private:
	ItemDataType type_;
};

// VFolderFileProvider
class VFolderFileProvider : public VFolderProvider
{
public:
	enum FileGroup {
		FILEGROUP_FOLDER = VFolderProvider::GROUP_FOLDER,
		FILEGROUP_FILE = VFolderProvider::GROUP_ITEM
	};
	VFolderFileProvider();
	VFolderFileProvider(
		const std::string& thumbnailPostfix,
		const std::string& type,
		const std::string& paths,
		const std::string& extensions,
		const std::string& includeFolders,
		const std::string& excludeFolders,
		int flags );
	virtual ~VFolderFileProvider();

	virtual void init(
		const std::string& type,
		const std::string& paths,
		const std::string& extensions,
		const std::string& includeFolders,
		const std::string& excludeFolders,
		int flags );

	virtual bool startEnumChildren( const VFolderItemDataPtr parent );
	virtual VFolderItemDataPtr getNextChild( ThumbnailManager& thumbnailManager, CImage& img );
	virtual void getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img );
	virtual const std::string getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished );
	virtual bool getListProviderInfo(
		VFolderItemDataPtr data,
		std::string& retInitIdString,
		ListProviderPtr& retListProvider,
		bool& retItemClicked );

	virtual const std::string getType();
	virtual int getFlags();
	virtual const std::vector<std::string>& getPaths();
	virtual const std::vector<std::string>& getExtensions();
	virtual const std::vector<std::string>& getIncludeFolders();
	virtual const std::vector<std::string>& getExcludeFolders();
	virtual const std::string getPathsString();
	virtual const std::string getExtensionsString();
	virtual const std::string getIncludeFoldersString();
	virtual const std::string getExcludeFoldersString();

private:
	class FileFinder : public ReferenceCount
	{
	public:
		CFileFind files;
		std::vector<std::string> paths;
		std::vector<std::string>::iterator path;
		std::vector<std::string>::iterator pathEnd;
		bool eof;
	};
	std::string type_;
	int flags_;
	typedef SmartPointer<FileFinder> FileFinderPtr;
	std::stack<FileFinderPtr> finderStack_;
	std::string thumbnailPostfix_;
	std::vector<std::string> paths_;
	std::vector<std::string> extensions_;
	std::vector<std::string> includeFolders_;
	std::vector<std::string> excludeFolders_;

	void popFinderStack();
	void clearFinderStack();
	FileFinderPtr topFinderStack();
};


#endif // VFOLDER_FILE_PROVIDER_HPP