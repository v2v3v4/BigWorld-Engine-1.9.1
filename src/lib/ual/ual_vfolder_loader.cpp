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
 *	UalVFolderLoader: manages VFolder parsing from the config file
 */



#include "pch.hpp"
#include "ual_dialog.hpp"
#include "vfolder_multi_provider.hpp"
#include "list_multi_provider.hpp"
#include "ual_vfolder_loader.hpp"
#include "resmgr/bwresource.hpp"
#include "common/string_utils.hpp"



///////////////////////////////////////////////////////////////////////////////
//	LoaderRegistry: VFolder loaders vector singleton class
///////////////////////////////////////////////////////////////////////////////

UalVFolderLoaderPtr LoaderRegistry::loader( const std::string& sectionName )
{
	VFolderLoaders::iterator loader = LoaderRegistry::loaders().end();

	for( VFolderLoaders::iterator i = LoaderRegistry::loaders().begin();
		i != LoaderRegistry::loaders().end(); ++i )
	{
		if ( (*i)->test( sectionName ) )
		{
			loader = i;
			break;
		}
	}

	if ( loader == LoaderRegistry::loaders().end() )
	{
		return NULL; 
	}

	return *loader;
}


///////////////////////////////////////////////////////////////////////////////
//	UalVFolderLoader
///////////////////////////////////////////////////////////////////////////////

void UalVFolderLoader::error( UalDialog* dlg, const std::string& msg )
{
	if ( dlg )
		dlg->error( msg );
}

void UalVFolderLoader::beginLoad( UalDialog* dlg, DataSectionPtr section, DataSectionPtr customData, int defaultThumbSize )
{
	if( icon_ )	
	{
		DestroyIcon( icon_ );
		icon_ = NULL;
	}
	if( iconSel_ )	
	{
		DestroyIcon( iconSel_ );
		iconSel_ = NULL;
	}

	displayName_ = !customData?section->asString():customData->asString();
	icon_ = dlg->iconFromXml( section, "icon" );
	iconSel_ = dlg->iconFromXml( section, "iconSel" );
	show_ = section->readBool( "show", true );
	folderData_ = new UalFolderData();
	folderData_->internalTag_ = section->readString( "internalTag" );
	folderData_->thumbSize_ = section->readInt( "thumbnailSize", defaultThumbSize );
	folderData_->originalThumbSize_ = folderData_->thumbSize_;
	if ( folderData_->thumbSize_ < 0 || folderData_->thumbSize_ > 2 )
	{
		folderData_->thumbSize_ = defaultThumbSize;
		dlg->error( "Wrong thumbnailSize. Valid values are 0, 1 or 2" );
	}
	folderData_->showInList_ = section->readBool( "showInList", true );
	folderData_->multiItemDrag_ = section->readBool( "multiItemDrag", false );

	std::vector<DataSectionPtr> sections;
	section->openSections( "disableFilter", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
		StringUtils::vectorFromString( (*s)->asString(), folderData_->disabledFilters_ );
	sections.clear();

	XmlItemList customItemList;
	customItemList.setDataSection( section->openSection( "customItems" ) );
	customItemList.getItems( folderData_->customItems_ );

	folderData_->idleText_ = section->readString( "searchIdleText", dlg->searchIdleText_ );

	dlg->folderData_.push_back( folderData_ );
}

VFolderPtr UalVFolderLoader::endLoad( UalDialog* dlg, VFolderProviderPtr provider,
							VFolderPtr parent, bool expandable,
							bool addToFolderTree /*=true*/, bool subVFolders /*=false*/ )
{
	provider->setFolderTree( &dlg->folderTree_ );
	provider->setFilterHolder( &dlg->filterHolder_ );

	VFolderPtr ret;
	if ( addToFolderTree )
	{
		ret = dlg->folderTree_.addVFolder(
			displayName_, provider, parent,
			icon_, iconSel_, show_,
			expandable,
			&folderData_->customItems_,
			(void*)folderData_.getObject(),
			subVFolders );
	}
	else
	{
		// don't add it to the folderTree_ tree control, just return it
		ret = new VFolder( NULL, 
			displayName_, NULL, provider, expandable,
			true, &folderData_->customItems_,
			(void*)folderData_.getObject(),
			subVFolders );
	}

	return ret;
}


///////////////////////////////////////////////////////////////////////////////
//	UalVFolderLoaderFactory
///////////////////////////////////////////////////////////////////////////////

UalVFolderLoaderFactory::UalVFolderLoaderFactory( UalVFolderLoaderPtr loader )
{
	UalDialog::registerVFolderLoader( loader );
}


///////////////////////////////////////////////////////////////////////////////
//	UalFilesVFolderLoader
///////////////////////////////////////////////////////////////////////////////

bool UalFilesVFolderLoader::pathIsGood( const std::string& path )
{
	return path.length() > 0 && path != "."
		&& path != "./" && path != "/"
		&& path != ".\\" && path != "\\";
}

VFolderPtr UalFilesVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	beginLoad( dlg, section, customData, 2 );

	std::string type = section->readString( "type", "FILE" );

	int flags = FILETREE_SHOWSUBFOLDERS;
	if ( section->readBool( "showSubfolders", true ) )
		flags |= FILETREE_SHOWSUBFOLDERS;
	else
		flags &= ~FILETREE_SHOWSUBFOLDERS;
	if ( section->readBool( "showFiles", false ) )
		flags |= FILETREE_SHOWFILES;
	else
		flags &= ~FILETREE_SHOWFILES;
	if ( !section->readBool( "recurseFiles", true ) )
		flags |= FILETREE_DONTRECURSE;
	else
		flags &= ~FILETREE_DONTRECURSE;

	bool expandable = (flags & FILETREE_SHOWFILES) || (flags & FILETREE_SHOWSUBFOLDERS);

	std::string paths;
	std::vector<DataSectionPtr> sections;
	if ( !customData )
		section->openSections( "path", sections );
	else
		customData->openSections( "path", sections );

	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		std::string xmlPath = (*s)->asString();
		std::vector<std::string> pathVec;
		StringUtils::vectorFromString( xmlPath, pathVec );

		for ( int i = 0; i < UalManager::instance().getNumPaths(); i++ )
		{
			for( std::vector<std::string>::iterator p = pathVec.begin();
				p != pathVec.end(); ++p )
			{
				std::string path = UalManager::instance().getPath( i );
				bool xmlPathGood = pathIsGood( *p );

				if ( xmlPathGood )
				{
					path += "/";
					path += *p;
				}
				if ( PathIsDirectory( path.c_str() ) )
				{
					if ( !paths.empty() )
						paths += ";";
					paths += path;
				}
			}
		}
		for( std::vector<std::string>::iterator p = pathVec.begin();
			p != pathVec.end(); ++p )
		{
			if ( pathIsGood( *p ) && PathIsDirectory( (*p).c_str() ) )
			{
				if ( !paths.empty() )
					paths += ";";
				paths += *p;
			}
		}
	}
	sections.clear();

	std::string extensions;
	section->openSections( "extension", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		if ( extensions.length() > 0 )
			extensions += ";";
		extensions += (*s)->asString();
	}
	sections.clear();

	std::string excludeFolders;
	section->openSections( "folderExclude", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		if ( excludeFolders.length() > 0 )
			excludeFolders += ";";
		excludeFolders += (*s)->asString();
	}
	sections.clear();

	std::string includeFolders;
	section->openSections( "folderInclude", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		if ( includeFolders.length() > 0 )
			includeFolders += ";";
		includeFolders += (*s)->asString();
	}
	sections.clear();

	VFolderFileProvider* prov = new VFolderFileProvider(
		UalManager::instance().thumbnailManager().postfix(),
		type, paths, extensions, includeFolders, excludeFolders, flags );
	prov->setListProvider( dlg->fileListProvider() );

	return endLoad( dlg,
		prov,
		parent, expandable, addToFolderTree );
}
static UalVFolderLoaderFactory filesFactory( new UalFilesVFolderLoader() );


///////////////////////////////////////////////////////////////////////////////
//	UalXmlVFolderLoader
///////////////////////////////////////////////////////////////////////////////

VFolderPtr UalXmlVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	beginLoad( dlg, section, customData, 2 );

	bool showItems = section->readBool( "showItems", false );
	std::string path = section->readString( "path" );

	if ( !BWResource::fileExists( path ) )
		error( dlg, std::string( "XML file not found: " ) + path + "." );

	VFolderXmlProvider* prov = new VFolderXmlProvider( path );
	prov->setListProvider( dlg->xmlListProvider() );
	VFolderPtr ret = endLoad( dlg,
		prov,
		parent, showItems, addToFolderTree );
	ret->setSortSubFolders( prov->getSort() );
	return ret;
}
static UalVFolderLoaderFactory xmlFactory( new UalXmlVFolderLoader() );


///////////////////////////////////////////////////////////////////////////////
//	UalHistoryVFolderLoader
///////////////////////////////////////////////////////////////////////////////

VFolderPtr UalHistoryVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	beginLoad( dlg, section, customData, 2 );

	bool showItems = section->readBool( "showItems", false );
	std::string path = section->readString( "path" );
	UalManager::instance().history().setPath( path );
	int maxItems = section->readInt( "maxItems", UalManager::instance().history().getMaxItems() );
	if ( maxItems < 0 )
		error( dlg, "Wrong History/maxItems. Must be greater or equal to zero." );
	else
		UalManager::instance().history().setMaxItems( maxItems );

	if ( section->readBool( "clearOnLoad", false ) )
		UalManager::instance().history().clear();

	if ( !BWResource::fileExists( path ) )
		error( dlg, std::string( "History file not found: " ) + path + "." );

	VFolderXmlProvider* prov = new VFolderXmlProvider( path );
	prov->setListProvider( dlg->historyListProvider() );
	VFolderPtr ret = endLoad( dlg,
		prov,
		parent, showItems, addToFolderTree );
	ret->setSortSubFolders( prov->getSort() );
	dlg->historyFolderProvider( prov );
	return ret;
}
static UalVFolderLoaderFactory historyFactory( new UalHistoryVFolderLoader() );


///////////////////////////////////////////////////////////////////////////////
//	UalFavouritesVFolderLoader
///////////////////////////////////////////////////////////////////////////////

VFolderPtr UalFavouritesVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	beginLoad( dlg, section, customData, 2 );

	bool showItems = section->readBool( "showItems", false );
	std::string path = section->readString( "path" );
	UalManager::instance().favourites().setPath( path );

	if ( !BWResource::fileExists( path ) )
		error( dlg, std::string( "Favourites file not found: " ) + path + "." );

	VFolderXmlProvider* prov = new VFolderXmlProvider( path );
	prov->setListProvider( dlg->favouritesListProvider() );
	VFolderPtr ret = endLoad( dlg,
		prov,
		parent, showItems, addToFolderTree );
	ret->setSortSubFolders( prov->getSort() );
	dlg->favouritesFolderProvider( prov );
	return ret;
}
static UalVFolderLoaderFactory favouritesFactory( new UalFavouritesVFolderLoader() );


///////////////////////////////////////////////////////////////////////////////
//	UalMultiVFolderLoader
///////////////////////////////////////////////////////////////////////////////

VFolderPtr UalMultiVFolderLoader::load( 
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	DataSectionPtr vfolders = section->openSection( "Providers" );
	if ( vfolders == NULL )
		return NULL;

	beginLoad( dlg, section, customData, 2 );

	bool showItems = section->readBool( "showItems", true );

	// create the MultiVFolder providers and link them
	VFolderMultiProvider* prov = new VFolderMultiProvider();
	ListMultiProvider* listProv = new ListMultiProvider();
	prov->setListProvider( listProv );
	
	VFolderPtr ret = endLoad( dlg,
		prov,
		parent, showItems, addToFolderTree );

	for ( int i = 0; i < vfolders->countChildren(); ++i )
	{
		DataSectionPtr cs = vfolders->openChild( i );
		if ( !cs )
		{
			char str[256];
			_snprintf( str, 255,
				"Failed opening section %d in the 'Providers' section of MultiVFolder '%s'.",
				i, section->asString().c_str() );
			str[255] = '\0';
			error( dlg, str );
			continue;
		}

		UalVFolderLoaderPtr loader = LoaderRegistry::loader( cs->sectionName() );
		if ( loader == NULL )
		{
			char str[256];
			_snprintf( str, 255,
				"Cannot load unknown sub-folder type '%s' in MultiVFolder '%s'.",
				cs->sectionName().c_str(), section->asString().c_str() );
			str[255] = '\0';
			error( dlg, str );
			continue;
		}

		// load the sub-provider through the LoaderRegistry into a 'vfolder'
		// VFolder object. Note the use of false for the addToFolderTree param
		// to avoid getting the sub-vfolder added to the tree control.
		// **NOTE**: At the moment, only one 'Files' provider is supported in a
		// MultiVFolder folder.
		VFolderPtr vfolder = loader->load( dlg, cs, parent, customData, false/*addToFolderTree*/ );
		
		if ( vfolder == NULL )
		{
			char str[256];
			_snprintf( str, 255,
				"Failed loading sub-folder of type '%s' of MultiVFolder '%s'.",
				cs->sectionName().c_str(), section->asString().c_str() );
			str[255] = '\0';
			error( dlg, str );
			continue;
		}

		// add the sub-providers to the respective Multi providers
		// (VFolder/List)
		prov->addProvider( vfolder->getProvider() );
		listProv->addProvider( vfolder->getProvider()->getListProvider() );
	}

	return ret;
}
static UalVFolderLoaderFactory multiFactory( new UalMultiVFolderLoader() );


///////////////////////////////////////////////////////////////////////////////
//	UalPlainVFolderLoader
///////////////////////////////////////////////////////////////////////////////

VFolderPtr UalPlainVFolderLoader::load(
	UalDialog* dlg, DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
	bool addToFolderTree )
{
	if ( !dlg || !section || !test( section->sectionName() ) )
		return 0;

	beginLoad( dlg, section, customData, 2 );

	return endLoad( dlg,
		0, // NULL provider
		parent,
		true/*expandable*/, addToFolderTree, true/*subVFolders*/ );
}
static UalVFolderLoaderFactory plainFactory( new UalPlainVFolderLoader() );
