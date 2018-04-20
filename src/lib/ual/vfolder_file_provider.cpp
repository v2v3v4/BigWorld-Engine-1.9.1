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

#include "pch.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include "thumbnail_manager.hpp"
#include "vfolder_file_provider.hpp"
#include "list_file_provider.hpp"
#include "common/string_utils.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"


// FolderTreeItemData
VFolderFileItemData::VFolderFileItemData(
		VFolderProviderPtr provider,
		ItemDataType type,
		const AssetInfo& assetInfo,
		int group,
		bool expandable ) :
	VFolderItemData( provider, assetInfo, group, expandable ),
	type_( type )
{
}

VFolderFileItemData::~VFolderFileItemData()
{
}

bool VFolderFileItemData::handleDuplicate( VFolderItemDataPtr data )
{
	if ( !data || data->isVFolder() )
		return false;

	VFolderFileItemData* fileData = (VFolderFileItemData*)data.getObject();
	if ( type_ == ITEMDATA_FOLDER && fileData->getItemType() == ITEMDATA_FOLDER )
	{
		std::string newPath = assetInfo().longText();
		newPath += ";";
		newPath += fileData->assetInfo().longText();
		assetInfo().longText( newPath );
		return true;
	}
	else
	{		
		// check if both are files, and if both have the same relative paths
		if ( type_ == fileData->type_ &&
			BWResource::dissolveFilename( assetInfo().longText() ) ==
			BWResource::dissolveFilename( fileData->assetInfo().longText() ) )
			return true; // ignore new data, assume the first one is valid
		else
			return false; // keep it, it is a folder or a file in another path
	}
}


// Virtual Folder File Provider
VFolderFileProvider::VFolderFileProvider()
{
	init( "", "", "", "", "", 0 );
}

VFolderFileProvider::VFolderFileProvider(
	const std::string& thumbnailPostfix,
	const std::string& type,
	const std::string& paths,
	const std::string& extensions,
	const std::string& includeFolders,
	const std::string& excludeFolders,
	int flags ) :
	thumbnailPostfix_( thumbnailPostfix )
{
	init( type, paths , extensions, includeFolders, excludeFolders, flags );
}

VFolderFileProvider::~VFolderFileProvider()
{
	clearFinderStack();
}

void VFolderFileProvider::init(
	const std::string& type,
	const std::string& paths,
	const std::string& extensions,
	const std::string& includeFolders,
	const std::string& excludeFolders,
	int flags )
{
	type_ = type;

	flags_ = flags;
	
	paths_.clear();
	extensions_.clear();
	includeFolders_.clear();
	excludeFolders_.clear();

	std::string pathsL = paths;
	std::replace( pathsL.begin(), pathsL.end(), '/', '\\' );
	StringUtils::vectorFromString( pathsL, paths_ );

	std::string extL = extensions;
	StringUtils::toLowerCase( extL );
	StringUtils::vectorFromString( extL, extensions_ );

	std::string includeFoldersL = includeFolders;
	std::replace( includeFoldersL.begin(), includeFoldersL.end(), '/', '\\' );
	StringUtils::vectorFromString( includeFoldersL, includeFolders_ );

	std::string excludeFoldersL = excludeFolders;
	std::replace( excludeFoldersL.begin(), excludeFoldersL.end(), '/', '\\' );
	StringUtils::vectorFromString( excludeFoldersL, excludeFolders_ );

	StringUtils::filterSpecVector( paths_, excludeFolders_ );
}

void VFolderFileProvider::popFinderStack()
{
	finderStack_.pop();
}

void VFolderFileProvider::clearFinderStack()
{
	while( !finderStack_.empty() )
		popFinderStack();
}

bool VFolderFileProvider::startEnumChildren( const VFolderItemDataPtr parent )
{
	clearFinderStack();
	std::vector<std::string>* paths;
	std::vector<std::string> subPaths;

	if ( !parent || parent->isVFolder() )
	{
		paths = &paths_;
	}
	else
	{
		if ( ((VFolderFileItemData*)parent.getObject())->getItemType() == VFolderFileItemData::ITEMDATA_FILE )
			return false;

		std::string fullPath = parent->assetInfo().longText().c_str();
		StringUtils::vectorFromString( fullPath, subPaths );
		paths = &subPaths;
	}

	if ( paths->empty() )
		return false;

	std::string path = *(paths->begin());
	path += "\\*.*";
	FileFinderPtr finder = new FileFinder();
	if ( !finder->files.FindFile( path.c_str() ) )
		return false;

	finder->paths = *paths;
	finder->path = finder->paths.begin();
	finder->pathEnd = finder->paths.end();
	finder->eof = false;

	finderStack_.push( finder );
	return true;
}

VFolderFileProvider::FileFinderPtr VFolderFileProvider::topFinderStack()
{
	FileFinderPtr finder = finderStack_.top();

	if ( finder->eof )
	{
		// unwind stack to get next path
		if ( ++(finder->path) == finder->pathEnd )
		{
			while ( finder->eof )
			{
				popFinderStack();
				if ( finderStack_.empty() )
					return 0;
				finder = finderStack_.top();
			}
		}
		else
		{
			std::string path = *(finder->path);
			path += "\\*.*";
			if ( finder->files.FindFile( path.c_str() ) )
				finder->eof = false;
		}
	}

	return finder;
}

VFolderItemDataPtr VFolderFileProvider::getNextChild( ThumbnailManager& thumbnailManager, CImage& img )
{
	if ( finderStack_.empty() )
		return 0;

	FileFinderPtr finder = topFinderStack();

	if ( !finder )
		return 0;

	bool found = false;
	int group;
	std::string name;
	VFolderFileItemData::ItemDataType type;
	while( !finder->eof )
	{
		finder->eof = finder->files.FindNextFile()?false:true;

		if ( !finder->files.IsDirectory() )
		{
			// check filters
			filterHolder_->enableSearchText( false );
			if ( ( !filterHolder_ ||
				filterHolder_->filter( (LPCTSTR)finder->files.GetFileName(), (LPCTSTR)finder->files.GetFilePath() ) ) &&
				( includeFolders_.empty() ||
					StringUtils::matchSpec( (LPCTSTR)finder->files.GetRoot(), includeFolders_ ) ) )
			{
				// file
				if ( ( flags_ & FILETREE_SHOWFILES )
					&& StringUtils::matchExtension( (LPCTSTR)finder->files.GetFileName(), extensions_ )
					&& finder->files.GetFileName().Find( thumbnailPostfix_.c_str() ) == -1 
					&& finder->files.GetFileName().Find( ".thumbnail.bmp" ) == -1 )
				{
					std::string filePath = (LPCTSTR)finder->files.GetFilePath();
					// if it's not a DDS, or if its a DDS and a corresponding
					// source image file doesn't exists, return the file.
					if ( BWResource::getExtension( filePath ) != "dds" ||
						(!PathFileExists( BWResource::changeExtension( filePath, ".bmp" ).c_str() ) &&
						!PathFileExists( BWResource::changeExtension( filePath, ".png" ).c_str() ) &&
						!PathFileExists( BWResource::changeExtension( filePath, ".tga" ).c_str() ) ) )
					{
						name = (LPCTSTR)finder->files.GetFileName();
						group = FILEGROUP_FILE;
						type = VFolderFileItemData::ITEMDATA_FILE;
						thumbnailManager.create( filePath, img, 16, 16, folderTree_ );
						found = true;
					}
				}
			}
			filterHolder_->enableSearchText( true );
			if ( found )
				break;
		}
		else if ( !finder->files.IsDots() )
		{
			// dir
			if ( excludeFolders_.empty() ||
					!StringUtils::matchSpec( (LPCTSTR)finder->files.GetFilePath(), excludeFolders_ ) )
			{
				if ( flags_ & FILETREE_SHOWSUBFOLDERS )
				{
					// return the folder's name
					name = finder->files.GetFileName();
					group = FILEGROUP_FOLDER;
					type = VFolderFileItemData::ITEMDATA_FOLDER;
					found = true;
					break;
				}
				else if ( ( flags_ & FILETREE_SHOWFILES )
						&& !( flags_ & FILETREE_DONTRECURSE ) )
				{
					// push the folder in the finder stack to find all files in it
					std::vector<std::string> subPaths;
					std::string path = (LPCTSTR)finder->files.GetFilePath();
					subPaths.push_back( path );
					path += "\\*.*";
					FileFinderPtr newFinder = new FileFinder();
					if ( newFinder->files.FindFile( path.c_str() ) )
					{
						newFinder->paths = subPaths;
						newFinder->path = newFinder->paths.begin();
						newFinder->pathEnd = newFinder->paths.end();
						newFinder->eof = false;

						finderStack_.push( newFinder );
						finder = newFinder;
					}
				}
			}
		}

		if ( finder->eof )
		{
			finder = topFinderStack();

			if ( !finder )
				return 0;
		}
	}

	if ( found )
	{
		VFolderFileItemData* newItem =
			new VFolderFileItemData( this,
				type,
				AssetInfo(
					type==VFolderFileItemData::ITEMDATA_FOLDER?"FOLDER":"FILE",
					name.c_str(),
					(LPCTSTR)finder->files.GetFilePath() ),
				group, (group == FILEGROUP_FILE)?false:true );
		return newItem;
	}
	else
	{
		return 0;
	}
}

void VFolderFileProvider::getThumbnail( ThumbnailManager& thumbnailManager, VFolderItemDataPtr data, CImage& img )
{
	if ( !data )
		return;

	thumbnailManager.create( data->assetInfo().longText(), img, 16, 16, folderTree_ );
}

const std::string VFolderFileProvider::getDescriptiveText( VFolderItemDataPtr data, int numItems, bool finished )
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
		if (finished)
		{
			desc = 
				L
				(
					"UAL/VFOLDER_FILE_PROVIDER/DESCRIPTION", 
					getPathsString(),
					numItems
				);
		}
		else
		{
			desc = 
				L
				(
					"UAL/VFOLDER_FILE_PROVIDER/DESCRIPTION_LOADING", 
					getPathsString(),
					numItems
				);
		}
	}
	else if ( !data->isCustomItem() &&
		((VFolderFileItemData*)data.getObject())->getItemType() == VFolderFileItemData::ITEMDATA_FOLDER )
	{
		if (finished)
		{
			desc = 
				L
				(
					"UAL/VFOLDER_FILE_PROVIDER/DESCRIPTION", 
					data->assetInfo().longText(),
					numItems
				);
		}
		else
		{
			desc = 
				L
				(
					"UAL/VFOLDER_FILE_PROVIDER/DESCRIPTION_LOADING", 
					data->assetInfo().longText(),
					numItems
				);
		}
	}

	return desc;
}

bool VFolderFileProvider::getListProviderInfo(
	VFolderItemDataPtr data,
	std::string& retInitIdString,
	ListProviderPtr& retListProvider,
	bool& retItemClicked )
{
	if ( !data || !listProvider_ )
		return false;

	int flags = ListFileProvider::LISTFILEPROV_DEFAULT;
	std::string fullPath;
	if ( getFlags() & FILETREE_DONTRECURSE ) 
		flags |= ListFileProvider::LISTFILEPROV_DONTRECURSE;
	if ( data->isVFolder() )
		fullPath = getPathsString();
	else if ( !data->isCustomItem() && ((VFolderFileItemData*)data.getObject())->getItemType() == VFolderFileItemData::ITEMDATA_FOLDER )
		fullPath = data->assetInfo().longText();
	else
	{
		// item is a file, so select the item's parent folder to fill up the list
		retItemClicked = true;
		HTREEITEM item = data->getTreeItem();
		VFolderFileItemData* parentData = 0;
		while( item )
		{
			item = folderTree_->GetParentItem( item );
			if ( item )
			{
				parentData = (VFolderFileItemData*)folderTree_->GetItemData( item );
				if ( parentData &&
						( parentData->isVFolder() ||
						parentData->getItemType() == VFolderFileItemData::ITEMDATA_FOLDER ) )
					break;
				parentData = 0;
			}
		}
		if ( parentData )
		{
			if ( parentData->isVFolder() )
				fullPath = getPathsString();
			else if ( parentData->getItemType() == VFolderFileItemData::ITEMDATA_FOLDER )
				fullPath = parentData->assetInfo().longText();
		}
	}

	// construct a list string to detect when the init params are redundant and avoid flicker
	char flagsStr[80];
	bw_snprintf( flagsStr, sizeof(flagsStr), "%d", flags );
	std::string listInit =
		getType() +
		fullPath +
		getExtensionsString() +
		getIncludeFoldersString() +
		getExcludeFoldersString() +
		flagsStr;

	if ( retInitIdString == listInit && retListProvider == listProvider_ )
		return false;

	if ( !fullPath.empty() )
	{
		retListProvider = listProvider_;
		((ListFileProvider*)listProvider_.getObject())->init(
			getType(),
			fullPath,
			getExtensionsString(),
			getIncludeFoldersString(),
			getExcludeFoldersString(),
			flags );
		retInitIdString = listInit;
		return true;
	}

	return false;
}


const std::string VFolderFileProvider::getType()
{
	return type_;
}

int VFolderFileProvider::getFlags()
{
	return flags_;
}

const std::vector<std::string>& VFolderFileProvider::getPaths()
{
	return paths_;
}

const std::vector<std::string>& VFolderFileProvider::getExtensions()
{
	return extensions_;
}

const std::vector<std::string>& VFolderFileProvider::getIncludeFolders()
{
	return includeFolders_;
}

const std::vector<std::string>& VFolderFileProvider::getExcludeFolders()
{
	return excludeFolders_;
}

const std::string VFolderFileProvider::getPathsString()
{
	return StringUtils::vectorToString( paths_ );
}

const std::string VFolderFileProvider::getExtensionsString()
{
	return StringUtils::vectorToString( extensions_ );
}

const std::string VFolderFileProvider::getIncludeFoldersString()
{
	return StringUtils::vectorToString( includeFolders_ );
}

const std::string VFolderFileProvider::getExcludeFoldersString()
{
	return StringUtils::vectorToString( excludeFolders_ );
}
