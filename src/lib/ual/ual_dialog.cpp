/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// UalDialog.cpp : implementation file
//

#include "pch.hpp"
#include "filter_holder.hpp"
#include "ual_dialog.hpp"
#include "ual_manager.hpp"
#include "ual_history.hpp"
#include "ual_favourites.hpp"

#include "ual_name_dlg.hpp"

#include "common/string_utils.hpp"

#include "thumbnail_manager.hpp"

#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/string_provider.hpp"
#include "cstdmf/debug.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_toolbar.hpp"


DECLARE_DEBUG_COMPONENT( 0 );


static const int MAX_SEARCH_TEXT = 50;
static const int MIN_SPLITTER_PANE_SIZE = 16;


// UalDialog dialog

const std::string UalDialog::contentID = "UAL";



UalDialog::UalDialog( const std::string& configFile )
	: CDialog(UalDialog::IDD, 0)
	, configFile_( configFile )
	, fileListProvider_( new ListFileProvider( UalManager::instance().thumbnailManager().postfix() ) )
	, xmlListProvider_( new ListXmlProvider() )
	, historyListProvider_( new ListXmlProvider() )
	, favouritesListProvider_( new ListXmlProvider() )
	, splitterBar_( 0 )
	, dlgShortCaption_( L("UAL/UAL_DIALOG/SHORT_CAPTION") )
	, dlgLongCaption_( L("UAL/UAL_DIALOG/LONG_CAPTION") )
	, preferredWidth_( 290 ) , preferredHeight_( 380 )
	, hicon_( 0 )
	, layoutVertical_( true )
	, layoutLastRowSize_( 0 ) , layoutLastColSize_( 0 )
	, defaultSize_( 100 )
	, folderTree_( &UalManager::instance().thumbnailManager() )
	, smartList_( &UalManager::instance().thumbnailManager() )
	, showFilters_( false )
	, lastFocus_( 0 )
	, customVFolders_( 0 )
{
	lastLanguage_ =
		StringProvider::instance().currentLanguage()->getIsoLangName() + "_" +
		StringProvider::instance().currentLanguage()->getIsoCountryName();

	UalManager::instance().registerDialog( this );
}

UalDialog::~UalDialog()
{
	if( hicon_ )	DeleteObject( hicon_ );
	delete splitterBar_;
	UalManager::instance().unregisterDialog( this );
}

void UalDialog::registerVFolderLoader( UalVFolderLoaderPtr loader )
{
	LoaderRegistry::loaders().push_back( loader );
}

void UalDialog::fini()
{
	LoaderRegistry::loaders().clear();
}

void UalDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_UALTREE, folderTree_);
	DDX_Control(pDX, IDC_UALLIST, smartList_);
	DDX_Control(pDX, IDC_UALSEARCHBK, searchBk_);
	DDX_Control(pDX, IDC_UALSEARCH, search_);
	DDX_Control(pDX, IDC_UALMAGNIFIER, searchFilters_);
	DDX_Control(pDX, IDC_UALSEARCHCLOSE, searchClose_);
	DDX_Control(pDX, IDC_UALSTATUS, statusBar_);
}

// GUITABS::Content right click handler
void UalDialog::handleRightClick( int x, int y )
{
	showContextMenu( 0 );
}

// GUITABS::Content load
bool UalDialog::load( DataSectionPtr section )
{
	if ( !section )
	{
		error( "Problems loading from guitabs layout file." );
		return false;
	}

	// load basic layout info from the guitabs layout Content section

	if ( lastLanguage_ == section->readString( "lastLanguage", lastLanguage_ ) )
	{
		// only read the custom names if the language is the same.
		dlgShortCaption_ = section->readString( "shortCaption", dlgShortCaption_ );
		dlgLongCaption_ = section->readString( "longCaption", dlgLongCaption_ );
	}
	int size = section->readInt( "initialTreeSize", defaultSize_ );
	if ( size < 0 )
		error( "invalid defaultSize. Should be greater or equal to zero." );
	else
		defaultSize_ = size;
	setLayout( section->readBool( "layoutVertical", layoutVertical_ ), true );
	showFilters_ = section->readBool( "filtersVisible", showFilters_ );

	customVFolders_ = new XMLSection( "customVFolders" );
	customVFolders_->copy( section );
	loadCustomVFolders( customVFolders_ );

	loadVFolderExcludeInfo( section );

	std::vector<DataSectionPtr> sections;
	section->openSections( "VFolderData", sections );
	for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
	{
		VFolderPtr vfolder = folderTree_.getVFolder( (*s)->asString() );
		if ( vfolder )
		{
			UalFolderData* data = (UalFolderData*)vfolder->getData();
			if ( data )
				data->thumbSize_ = (*s)->readInt( "thumbSize" );
		}
	}

	folderTree_.setVFolderOrder( section->readString( "vfolderOrder" ) );
	folderTree_.selectVFolder( section->readString( "lastVFolder" ) );

	return true;
}

// GUITABS::Content save
bool UalDialog::save( DataSectionPtr section )
{
	if ( !section )
	{
		error( "Problems saving to guitabs layout file." );
		return false;
	}

	// save basic layout info in the guitabs layout Content section
	section->writeString( "lastLanguage", lastLanguage_ );
	section->writeString( "shortCaption", dlgShortCaption_ );
	section->writeString( "longCaption", dlgLongCaption_ );
	if ( splitterBar_->GetSafeHwnd() )
	{
		int size;
		int min;		
		if ( layoutVertical_ )
			splitterBar_->GetRowInfo( 0, size, min );
		else
			splitterBar_->GetColumnInfo( 0, size, min );
		if ( size < MIN_SPLITTER_PANE_SIZE )
			size = MIN_SPLITTER_PANE_SIZE;
		section->writeInt( "initialTreeSize", size );
	}
	section->writeBool( "layoutVertical", layoutVertical_ );
	section->writeBool( "filtersVisible", showFilters_ );

	// save vfolder extra data, such as thumbSize
	std::vector<HTREEITEM> treeItems;
	folderTree_.getVFolders( treeItems );
	for( std::vector<HTREEITEM>::iterator i = treeItems.begin();
		i != treeItems.end(); ++i )
	{
		VFolderItemData* itemData = (VFolderItemData*)folderTree_.GetItemData( *i );
		if ( !itemData->isVFolder() && !itemData->getVFolder() )
			continue;
		UalFolderData* data = (UalFolderData*)(itemData->getVFolder()->getData());
		if ( data && data->thumbSize_ != data->originalThumbSize_ )
		{
			DataSectionPtr folderSection = section->newSection( "VFolderData" );
			folderSection->setString( (LPCTSTR)folderTree_.GetItemText( *i ) );
			folderSection->writeInt( "thumbSize", data->thumbSize_ );
		}
	}

	// save excludeVFolders data
	std::string excluded;
	for( std::vector<std::string>::iterator i = excludeVFolders_.begin();
		i != excludeVFolders_.end(); ++i )
	{
		if ( !excluded.empty() )
			excluded += ";";
		excluded += (*i);
	}
	if ( !excluded.empty() )
		section->writeString( "excludeVFolder", excluded );

	// save customVFolders
	if ( customVFolders_ )
	{
		std::vector<DataSectionPtr> sections;
		customVFolders_->openSections( "customVFolder", sections );
		for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
		{
			DataSectionPtr customVFolder = section->newSection( "customVFolder" );
			customVFolder->copy( *s );
		}
	}

	// save vfolder order
	section->writeString( "vfolderOrder", folderTree_.getVFolderOrder() );

	// save last selected item
	HTREEITEM item = folderTree_.GetSelectedItem();
	VFolderItemData* data = 0;
	if ( item )
		data = (VFolderItemData*)folderTree_.GetItemData( item );
	if ( data )
	{
		VFolderPtr lastVFolder = folderTree_.getVFolder( data );
		if ( lastVFolder )
			section->writeString( "lastVFolder", lastVFolder->getName() );
	}

	return true;
}

// GUITABS::Content clone
GUITABS::ContentPtr UalDialog::clone()
{
	UalDialogFactory ualFactory;
	UalDialog* newUal = ualFactory.createUal( configFile_ );

	// copy settings to the new UAL
	int min;
	if ( layoutVertical_ )
	{
		splitterBar_->GetRowInfo( 0, newUal->layoutLastRowSize_, min );
		newUal->layoutLastColSize_ = layoutLastColSize_;
	}
	else
	{
		splitterBar_->GetColumnInfo( 0, newUal->layoutLastColSize_, min );
		newUal->layoutLastRowSize_ = layoutLastRowSize_;
	}
	newUal->defaultSize_ = defaultSize_;
	newUal->setLayout( layoutVertical_ );
	newUal->showFilters_ = showFilters_;

	if ( lastItemInfo_.isFolder_ && lastItemInfo_.dialog_ )
	{
		// is the result of dragging and dropping a folder, so clone using that info
		newUal->folderTree_.clear();

		UalManager::instance().copyVFolder( this, newUal, lastItemInfo_ );

		if ( newUal->folderTree_.GetCount() )
			newUal->folderTree_.SelectItem( newUal->folderTree_.GetChildItem( TVI_ROOT ) );
	}
	else
	{
		// it's not being clone because of a drag&drop operation, so do standard stuff
		newUal->customVFolders_ = new XMLSection( "customVFolders" );
		if ( !!customVFolders_ )
			newUal->customVFolders_->copy( customVFolders_ );
		newUal->loadCustomVFolders( newUal->customVFolders_ );

		for( std::vector<std::string>::iterator i = excludeVFolders_.begin();
			i != excludeVFolders_.end(); ++i )
		{
			newUal->folderTree_.removeVFolder( *i );
			newUal->excludeVFolders_.push_back( *i );
		}

		newUal->folderTree_.setVFolderOrder( folderTree_.getVFolderOrder() );

		// set folder custom info
		std::vector<HTREEITEM> treeItems;
		folderTree_.getVFolders( treeItems );
		for( std::vector<HTREEITEM>::iterator i = treeItems.begin();
			i != treeItems.end(); ++i )
		{
			if ( !(*i) )
				continue;
			VFolderPtr srcVFolder = folderTree_.getVFolder(
				(VFolderItemData*)folderTree_.GetItemData( *i ) );
			VFolderPtr dstVFolder = newUal->folderTree_.getVFolder(
				(LPCTSTR)folderTree_.GetItemText( *i ) );
			if ( srcVFolder && dstVFolder )
			{
				UalFolderData* srcData = (UalFolderData*)srcVFolder->getData();
				UalFolderData* dstData = (UalFolderData*)dstVFolder->getData();
				if ( srcData && dstData )
					dstData->thumbSize_ = srcData->thumbSize_;
			}
		}
	}

	// just in case, reset some key values
	lastItemInfo_.dialog_ = 0;
	lastItemInfo_.folderExtraData_ = 0;

	return newUal;
}

void UalDialog::saveConfig()
{
	if ( configFile_.empty() )
	{
		error( "No config file specified." );
		return;
	}

	DataSectionPtr root = BWResource::openSection( configFile_ );
	if ( !root )
	{
		error( "Couldn't save config file." );
		return;
	}
	DataSectionPtr config = root->openSection( "Config" );
	if ( !config )
	{
		error( "Couldn't create Config section. Couldn't save config file." );
		return;
	}

	save( config );

	root->save();
}

bool UalDialog::loadConfig( const std::string fname )
{
	if ( !fname.empty() )
		configFile_ = fname;

	if ( configFile_.empty() )
	{
		error( "No config file specified." );
		return false;
	}

	BWResource::instance().purge( configFile_ );
	DataSectionPtr root = BWResource::openSection( configFile_ );

	if ( !root )
	{
		error( "Couldn't load config file." );
		return false;
	}

	loadMain( root->openSection( "Config" ) );
	loadToolbar( root->openSection( "Toolbar" ) );
	loadFilters( root->openSection( "Filters" ) );
	loadVFolders( root->openSection( "VFolders" ) );
	return true;
}

HICON UalDialog::iconFromXml( DataSectionPtr section, std::string item )
{
	std::string icon = section->readString( item );
	if ( icon.empty() )
		return 0;

	int iconNum = atoi( icon.c_str() );

	HICON ret = 0;
	if ( iconNum != 0 )
	{
		ret = AfxGetApp()->LoadIcon( section->readInt( "icon" ) );
		if ( !ret )
			error( std::string( "Couldn't load icon resource for VFolder " ) + section->asString() );
	}
	else
	{
		icon = BWResource::findFile( icon );
		ret = (HICON)LoadImage( AfxGetInstanceHandle(),
			icon.c_str(), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR | LR_LOADFROMFILE );
		if ( !ret )
			error( std::string( "Couldn't load icon file for VFolder " ) + section->asString() );
	}

	return ret;
}

void UalDialog::loadMain( DataSectionPtr section )
{
	if ( !section )
		return;

	dlgShortCaption_ = L( section->readString( "shortCaption", dlgShortCaption_ ).c_str() );
	dlgLongCaption_ = L( section->readString( "longCaption", dlgLongCaption_ ).c_str() );
	hicon_ = iconFromXml( section, "icon" );
	int width = section->readInt( "preferredWidth", preferredWidth_ );
	if ( width < 1 )
		error( "invalid preferredWidth. Should be greater than zero." );
	else
		preferredWidth_ = width;

	int height = section->readInt( "preferredHeight", preferredHeight_ );
	if ( height < 1 )
		error( "invalid preferredHeight. Should be greater than zero." );
	else
		preferredHeight_ = height;

	int size = section->readInt( "initialTreeSize", defaultSize_ );
	if ( size < 0 )
		error( "invalid defaultSize. Should be greater or equal to zero." );
	else
		defaultSize_ = size;
	
	fileListProvider_->setThreadYieldMsec(
		section->readInt(
			"threadYieldMsec",
			fileListProvider_->getThreadYieldMsec() ) );

	fileListProvider_->setThreadPriority(
		section->readInt(
			"threadPriority",
			fileListProvider_->getThreadPriority() ) );

	setLayout( section->readBool( "layoutVertical", layoutVertical_ ), true );
	showFilters_ = section->readBool( "filtersVisible", showFilters_ );
	folderTree_.setSortVFolders( section->readBool( "sortVFolders", true ) );
	folderTree_.setSortSubFolders( section->readBool( "sortSubFolders", true ) );
	int maxCache = section->readInt( "maxCacheItems", 200 );
	if ( maxCache < 0 )
		error( "invalid maxCacheItems. Should be greater or equal to zero." );
	else
		smartList_.setMaxCache( maxCache );
	smartList_.SetIconSpacing(
			section->readInt( "iconSpacingX", 90 ),
			section->readInt( "iconSpacingY", 100 )
		);
	filtersCtrl_.setPushlike( section->readBool( "pushlikeFilters", false ) );
	searchIdleText_ = 
		section->readString( "searchIdleText", L("UAL/UAL_DIALOG/DEFAULT_SEARCH_IDLE_TEXT") );
}

void UalDialog::loadToolbar( DataSectionPtr section )
{
	if ( !section || !section->countChildren() )
		return;

	for( int i = 0; i < section->countChildren(); ++i )
		GUI::Manager::instance().add( new GUI::Item( section->openChild( i ) ) );

	toolbar_.Create( CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN |
		TBSTYLE_FLAT | WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CBRS_TOOLTIPS,
		CRect(0,0,1,1), this, 0 );
	toolbar_.SetBitmapSize( CSize( 16, 16 ) );
	toolbar_.SetButtonSize( CSize( 24, 22 ) );

	CToolTipCtrl* tc = toolbar_.GetToolTips();
	if ( tc )
		tc->SetWindowPos( &CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

	GUI::Toolbar* guiTB = new GUI::Toolbar( "UalToolbar", toolbar_ );
	GUI::Manager::instance().add( guiTB );

	SIZE tbSize = guiTB->minimumSize();
	toolbar_.SetWindowPos( 0, 0, 0, tbSize.cx, tbSize.cy, SWP_NOMOVE | SWP_NOZORDER );
}

void UalDialog::loadFilters( DataSectionPtr section )
{
	if ( !section )
		return;

	std::vector<DataSectionPtr> filters;
	section->openSections( "Filter", filters );
	for( std::vector<DataSectionPtr>::iterator s = filters.begin(); s != filters.end(); ++s )
	{
		FilterSpecPtr filterSpec;
		if ( (*s)->readBool( "separator", false ) )
		{
			filterSpec = new FilterSpec( "" );
		}
		else
		{
			std::string name = (*s)->asString();
			std::string group = (*s)->readString( "group", "" );

			std::string str[2];
			std::string secstrs[2] = { "include", "exclude" };
			for( int i = 0; i < 2; ++i )
			{
				std::vector<DataSectionPtr> sections;
				(*s)->openSections( secstrs[i], sections );
				for( std::vector<DataSectionPtr>::iterator ss = sections.begin(); ss != sections.end(); ++ss )
				{
					if ( !str[i].empty() )
						str[i] += ";";
					str[i] += (*ss)->asString();
				}
				sections.clear();
			}
			filterSpec = new FilterSpec( name, false, str[0], str[1], group );
			if ( str[0].empty() && str[1].empty() )
				error( std::string( "Filter " ) + name + " has no include nor exclude tags." );
		}
		filterHolder_.addFilter( filterSpec );
	}
}

void UalDialog::loadVFolders( DataSectionPtr section, const std::string& loadOneName, VFolderPtr parent )
{
	if ( !section )
		return;

	for( int i = 0; i < section->countChildren(); ++i )
	{
		DataSectionPtr child = section->openChild( i );
		VFolderPtr vfolder = loadVFolder( child, loadOneName, parent );
		if ( vfolder != NULL )
		{
			if ( vfolder->subVFolders() )
			{
				// look and load nested vfolders
				loadVFolders( child, "", vfolder );
			}
		}
		else if ( !loadOneName.empty() )
		{
			// Check to see if this vfolder has subVFolders.
			UalVFolderLoaderPtr loader = LoaderRegistry::loader( child->sectionName() );
			if ( loader != NULL && loader->subVFolders() )
			{
				// look for loadOneName in the nested folders and load it at the parent's level
				loadVFolders( child, loadOneName, parent );
			}
		}
	}
}

VFolderPtr UalDialog::loadVFolder( DataSectionPtr section, const std::string& loadOneName, VFolderPtr parent, DataSectionPtr customData )
{
	if ( !section )
		return 0;

	if ( section->asString().empty() )
	{
		error( std::string("A VFolder of type '") + section->sectionName() + "' has no name in the XML config file." );
		return 0;
	}

	if ( loadOneName != "***EXCLUDE_ALL***" &&
		( loadOneName.empty() || loadOneName == section->asString() ) )
	{
		UalVFolderLoaderPtr loader = LoaderRegistry::loader( section->sectionName() );

		if ( loader == NULL )
		{
			// it's not a recognized vfolder section, so return.
			// Note: This early error doesn't seem to get caught by WE at the moment,
			// probably because WE registers it's error callback after this.
			error( "VFolder type '" + section->sectionName() + "' could not be loaded" );
			return 0; 
		}

		VFolderPtr vfolder = loader->load( this, section, parent, customData, true/*addToFolderTree*/ );

		if ( !vfolder )
			return 0; // test passed but load failed.

		// remove it from the exclude list, if it's in
		for( std::vector<std::string>::iterator i = excludeVFolders_.begin();
			i != excludeVFolders_.end(); )
			if ( (*i) == section->asString() )
				i = excludeVFolders_.erase( i );
			else
				++i;
		return vfolder;
	}
	else
	{
		// if not created already, exclude it
		if ( !folderTree_.getVFolder( section->asString() )  &&
			std::find(
				excludeVFolders_.begin(),
				excludeVFolders_.end(),
				section->asString() ) == excludeVFolders_.end() )
			excludeVFolders_.push_back( section->asString() );
	}

	return 0;
}

void UalDialog::loadVFolderExcludeInfo( DataSectionPtr section )
{
	excludeVFolders_.clear();
	std::vector<std::string> excluded;
	std::vector<DataSectionPtr> excludeVFolders;
	section->openSections( "excludeVFolder", excludeVFolders );
	for( std::vector<DataSectionPtr>::iterator s = excludeVFolders.begin();
		s != excludeVFolders.end(); ++s )
	{
		excluded.clear();
		StringUtils::vectorFromString( (*s)->asString(), excluded );
		for( std::vector<std::string>::iterator i = excluded.begin();
			i != excluded.end();
			++i )
		{
			if ( !(*i).empty() )
			{
				folderTree_.removeVFolder( (*i) );
				if ( std::find(
								excludeVFolders_.begin(),
								excludeVFolders_.end(),
								*i ) == excludeVFolders_.end() )
					excludeVFolders_.push_back( *i );
			}
		}
	}
}

void UalDialog::loadCustomVFolders( DataSectionPtr section, const std::string& loadOneName )
{
	if ( !section )
		return;

	std::vector<DataSectionPtr> customVFolders;
	section->openSections( "customVFolder", customVFolders );
	if ( customVFolders.empty() )
		return;

	DataSectionPtr root = BWResource::openSection( configFile_ );
	if ( !root )
		return;
	DataSectionPtr vfolders = root->openSection( "VFolders" );
	if ( !vfolders )
		return;

	for( std::vector<DataSectionPtr>::iterator s = customVFolders.begin();
		s != customVFolders.end(); ++s )
	{
		std::string inheritsFrom = (*s)->readString( "inheritsFrom" );
		if ( inheritsFrom.empty() )
			continue;

		if ( loadOneName.empty() || loadOneName == (*s)->asString() )
			VFolderPtr vfolder = loadFromBaseVFolder( vfolders, inheritsFrom, (*s) );
	}
}

VFolderPtr UalDialog::loadFromBaseVFolder( DataSectionPtr section, const std::string& baseName, DataSectionPtr customData, VFolderPtr parent )
{
	if ( !section )
		return 0;

	for( int i = 0; i < section->countChildren(); ++i )
	{
		DataSectionPtr child = section->openChild( i );
		if ( baseName == child->asString() )
		{
			VFolderPtr vfolder = loadVFolder( child, "", parent, customData );
			return vfolder;
		}
		// look for nested vfolders, but gonna load it at the root level
		VFolderPtr vfolder = loadFromBaseVFolder( child, baseName, customData, parent );
		if ( vfolder )
			return vfolder;
	}
	return 0;
}

void UalDialog::updateItem( const std::string& longText )
{
	if ( !GetSafeHwnd() || longText.empty() )
		return;

	std::string longTextTmp = longText;
	std::replace( longTextTmp.begin(), longTextTmp.end(), '/', '\\' );
	std::string textTmp = longTextTmp.c_str() + longTextTmp.find_last_of( '\\' ) + 1;

	if ( folderTree_.GetSafeHwnd() )
		folderTree_.updateItem( AssetInfo( "", textTmp, longTextTmp ) );
	if ( smartList_.GetSafeHwnd() )
		smartList_.updateItem( AssetInfo( "", textTmp, longTextTmp ) );
}

typedef std::pair<UalDialog*,const char*> vfolderTagTestData;

bool UalDialog::vfolderFindByTag( HTREEITEM item, void* testData )
{
	if ( !testData )
		return false;

	vfolderTagTestData dlgInfo = *(vfolderTagTestData*)testData;
	UalDialog* dlg = dlgInfo.first;
	const char* vfolderName = dlgInfo.second;

	VFolderItemDataPtr data = (VFolderItemData*)dlg->folderTree_.GetItemData( item );
	if ( !vfolderName || !data || !data->isVFolder() )
		return false;

	VFolderPtr vfolder = data->getVFolder();
	if ( !vfolder )
		return false;
	
	UalFolderData* folderData = (UalFolderData*)vfolder->getData();
	if ( !folderData )
		return false;

	return folderData->internalTag_ == vfolderName;
}

void UalDialog::showItem( const std::string& vfolder, const std::string& longText )
{
	if ( !GetSafeHwnd() || vfolder.empty() || longText.empty() )
		return;

	std::string longTextTmp = longText;
	std::replace( longTextTmp.begin(), longTextTmp.end(), '/', '\\' );

	if ( folderTree_.GetSafeHwnd() )
		folderTree_.selectVFolderCustom(
		vfolderFindByTag,
		(void*)&vfolderTagTestData( this, vfolder.c_str() ) );

	if ( smartList_.GetSafeHwnd() )
	{
		std::string textTmp = longTextTmp.c_str() + longTextTmp.find_last_of( '\\' ) + 1;
		if ( !smartList_.showItem( AssetInfo( "", textTmp, longTextTmp ) ) )
			delayedListShowItem_ = longTextTmp;
	}
}

void UalDialog::buildFiltersCtrl()
{
	filtersCtrl_.Create(
		AfxRegisterWndClass( 0, 0, GetSysColorBrush( COLOR_BTNFACE ), 0 ),
		"", WS_VISIBLE | WS_CHILD, CRect( 0, 0, 1, 1 ), this, 0 );
	filtersCtrl_.setEventHandler( this );
}

void UalDialog::buildFolderTree()
{
	folderTree_.init();

	folderTree_.setEventHandler( this );
}

void UalDialog::buildSmartList()
{
	xmlListProvider_->setFilterHolder( &filterHolder_ );
	historyListProvider_->setFilterHolder( &filterHolder_ );
	favouritesListProvider_->setFilterHolder( &filterHolder_ );
	fileListProvider_->setFilterHolder( &filterHolder_ );

	smartList_.SetIconSpacing( 90, 90 );

	smartList_.init( 0, 0 );
	smartList_.setEventHandler( this );
}

BOOL UalDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	setLayout( layoutVertical_, true );

	search_.SetLimitText( MAX_SEARCH_TEXT );
	search_.setIdleText( "" );
	HBITMAP img = (HBITMAP)LoadImage( AfxGetInstanceHandle(),
		MAKEINTRESOURCE( IDB_UALSEARCHCLOSE ), IMAGE_BITMAP, 0, 0,
		LR_LOADTRANSPARENT | LR_SHARED );
	searchClose_.SetBitmap( img );
	if( toolTip_.CreateEx( this, 0, WS_EX_TOPMOST ) )
	{
		toolTip_.SetMaxTipWidth( SHRT_MAX );
		toolTip_.AddTool( &search_, L("UAL/UAL_DIALOG/TOOLTIP_SEARCH") );
		toolTip_.AddTool( &searchFilters_, L("UAL/UAL_DIALOG/TOOLTIP_SEARCH_FILTERS") );
		toolTip_.AddTool( &statusBar_, "" );
		toolTip_.SetWindowPos( &CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
		toolTip_.Activate( TRUE );
	}
	setStatusText( "" );

	buildFolderTree();
	buildSmartList();
	buildFiltersCtrl();

	if ( configFile_.length() )
		loadConfig();

	buildSmartListFilters();

	return TRUE;
}

bool UalDialog::guiActionRefresh()
{
	HTREEITEM sel = folderTree_.GetSelectedItem();
	if ( sel )
	{
		VFolderItemData* data = (VFolderItemData*)folderTree_.GetItemData( sel );
		if ( data )
		{
			// save search text in case selection changes
			char txt[MAX_SEARCH_TEXT + 1];
			search_.GetWindowText( txt, MAX_SEARCH_TEXT );

			HTREEITEM oldSel = folderTree_.GetSelectedItem();
			folderTree_.refreshVFolder( folderTree_.getVFolder( data ) );
			HTREEITEM sel = folderTree_.GetSelectedItem();
			if ( oldSel != sel && sel )
			{
				folderTreeSelect( (VFolderItemData*)folderTree_.GetItemData( sel ) );
				search_.SetWindowText( txt );
				return true;
			}
			else
				search_.SetWindowText( txt );
		}
	}
	smartList_.refresh();
	return true;
}

bool UalDialog::guiActionLayout()
{
	setLayout( !layoutVertical_ );
	return true;
}

void UalDialog::adjustSearchSize( int width, int height )
{
	const int xmargin = 4;
	const int ymargin = 6;
	const int xfilter = 20;
	const int xclose = 18;
	const int ysearch = 19;
	const int gap = 2;
	const int minSearchX = 90;

	if ( searchBk_.GetSafeHwnd() )
	{
		CRect trect( 0, 0, 0, 0 );
		if ( toolbar_.GetSafeHwnd() )
			toolbar_.GetWindowRect( &trect );
		CRect rect;
		if ( width - trect.Width() < minSearchX )
		{
			if ( toolbar_.GetSafeHwnd() )
				toolbar_.SetWindowPos( 0, xmargin, ymargin, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
			searchBk_.SetWindowPos( &CWnd::wndBottom,
				xmargin, ymargin + trect.Height() + gap*2,
				width - xmargin*2, ysearch, 0 );
			searchBk_.GetWindowRect( &rect );
			ScreenToClient( &rect );
		}
		else
		{
			searchBk_.SetWindowPos( &CWnd::wndBottom,
				xmargin, ymargin,
				width - trect.Width() - xmargin*2 - gap*2, ysearch, 0 );
			searchBk_.GetWindowRect( &rect );
			ScreenToClient( &rect );
			if ( toolbar_.GetSafeHwnd() )
				toolbar_.SetWindowPos( 0, rect.right + gap*2, rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
		}
		int editRightMargin = 0;
		if ( searchClose_.GetStyle() & WS_VISIBLE )
			editRightMargin = xclose - 2;

		search_.SetWindowPos( 0,
			rect.left + xfilter, rect.top + gap,
			rect.Width() - xfilter - editRightMargin, rect.Height() - gap - 1,
			SWP_NOZORDER );
		searchFilters_.SetWindowPos( 0,
			rect.left + gap, rect.top + gap,
			0, 0, SWP_NOSIZE | SWP_NOZORDER );
		searchClose_.SetWindowPos( 0,
			rect.right - xclose + 1, rect.top + gap,
			0, 0, SWP_NOSIZE | SWP_NOZORDER );

		searchFilters_.RedrawWindow();
		searchClose_.RedrawWindow();
	}
}

void UalDialog::updateFiltersImage()
{
	HBITMAP img = 0;
	int res = 0;
	if ( filtersCtrl_.empty() )
		res = IDB_UALMAGNIFIER;
	else
	{
		if ( filterHolder_.hasActiveFilters() )
			if ( showFilters_ )
				res = IDB_UALHIDEFILTERSA;
			else
				res = IDB_UALSHOWFILTERSA;
		else
			if ( showFilters_ )
				res = IDB_UALHIDEFILTERS;
			else
				res = IDB_UALSHOWFILTERS;
	}
	img = (HBITMAP)LoadImage( AfxGetInstanceHandle(),
		MAKEINTRESOURCE( res ), IMAGE_BITMAP, 0, 0,
		LR_LOADTRANSPARENT | LR_SHARED );
	searchFilters_.SetBitmap( img );
}

void UalDialog::adjustFiltersSize( int width, int height )
{
	if ( filtersCtrl_.GetSafeHwnd() )
	{
		updateFiltersImage();

		if ( !showFilters_ || filtersCtrl_.empty() )
		{
			filtersCtrl_.ShowWindow( SW_HIDE );
		}
		else
		{
			filtersCtrl_.ShowWindow( SW_SHOW );
			filtersCtrl_.recalcWidth( width - 8 );
			int top = 0;
			if ( searchBk_.GetSafeHwnd() )
			{
				CRect rect;
				searchBk_.GetWindowRect( &rect );
				ScreenToClient( &rect );
				top = rect.bottom + 6;
			}
			filtersCtrl_.SetWindowPos( 0, 4, top, width - 8, filtersCtrl_.getHeight(), SWP_NOZORDER );
		}
	}	
}

void UalDialog::adjustSplitterSize( int width, int height )
{
	if ( splitterBar_ && splitterBar_->GetSafeHwnd() )
	{
		int top = 0;
		if ( searchBk_.GetSafeHwnd() )
		{
			CRect rect;
			searchBk_.GetWindowRect( &rect );
			ScreenToClient( &rect );
			top = rect.bottom + 4;
		}
		if ( showFilters_ && !filtersCtrl_.empty() )
			top += filtersCtrl_.getHeight() + 2;
		splitterBar_->SetWindowPos( 0, 3, top, width - 6, height - top - 15, SWP_NOZORDER );
		folderTree_.RedrawWindow();
		smartList_.RedrawWindow();
	}

	if ( statusBar_.GetSafeHwnd() )
	{
		CRect rect;
		splitterBar_->GetWindowRect( &rect );
		ScreenToClient( &rect );
		statusBar_.SetWindowPos( 0, rect.left, rect.bottom, rect.right, 17, SWP_NOZORDER );
		statusBar_.RedrawWindow();
	}
}

void UalDialog::refreshStatusBar()
{
	HTREEITEM item = folderTree_.GetSelectedItem();
	if ( item )
		setFolderTreeStatusBar( (VFolderItemData*)folderTree_.GetItemData( item ) );
}

void UalDialog::setListStyle( SmartListCtrl::ViewStyle style )
{
	smartList_.setStyle( style );
	HTREEITEM sel = folderTree_.GetSelectedItem();
	if ( !sel )
		return;
	VFolderPtr vfolder = folderTree_.getVFolder(
		(VFolderItemData*)folderTree_.GetItemData( sel ) );
	if ( !vfolder )
		return;

	UalFolderData* folderData = (UalFolderData*)vfolder->getData();
	if ( !folderData || !folderData->showInList_ )
		return;

	if ( style == SmartListCtrl::BIGICONS )
		folderData->thumbSize_ = 2;
	else if ( style == SmartListCtrl::SMALLICONS )
		folderData->thumbSize_ = 1;
	else
		folderData->thumbSize_ = 0;
}

void UalDialog::setLayout( bool vertical, bool resetLastSize )
{
	// if a previous splitter exists, save last pane sizes and delete
	if ( splitterBar_->GetSafeHwnd() )
	{
		folderTree_.SetParent( this );
		smartList_.SetParent( this );

		if ( resetLastSize )
		{
			layoutLastRowSize_ = 0;
			layoutLastColSize_ = 0;
		}
		else if ( layoutVertical_ != vertical )
		{
			int min;
			if ( layoutVertical_ )
				splitterBar_->GetRowInfo( 0, layoutLastRowSize_, min );
			else
				splitterBar_->GetColumnInfo( 0, layoutLastColSize_, min );
		}
		splitterBar_->DestroyWindow();
		delete splitterBar_;
		splitterBar_ = 0;
	}

	// update flag and button state
	layoutVertical_ = vertical;

	// create new splitter
	int id2;

	splitterBar_ = new SplitterBarType();
	splitterBar_->setMinRowSize( MIN_SPLITTER_PANE_SIZE );
	splitterBar_->setMinColSize( MIN_SPLITTER_PANE_SIZE );

	if ( layoutVertical_ )
	{
		splitterBar_->CreateStatic( this, 2, 1, WS_CHILD );
		id2 = splitterBar_->IdFromRowCol( 1, 0 );
	}
	else
	{
		splitterBar_->CreateStatic( this, 1, 2, WS_CHILD );
		id2 = splitterBar_->IdFromRowCol( 0, 1 );
	}

	// set parents properly
	folderTree_.SetDlgCtrlID( splitterBar_->IdFromRowCol( 0, 0 ) );
	folderTree_.SetParent( splitterBar_ );

	smartList_.SetDlgCtrlID( id2 );
	smartList_.SetParent( splitterBar_ );

	splitterBar_->ShowWindow( SW_SHOW );

	// restore last saved pane sizes
	int size = defaultSize_;
	if ( layoutVertical_ )
	{
		if ( layoutLastRowSize_ > 0 )
			size = layoutLastRowSize_;
		if ( size < MIN_SPLITTER_PANE_SIZE ) // limit minimum splitter size
			size = MIN_SPLITTER_PANE_SIZE;
		splitterBar_->SetRowInfo( 0, size, 1 );
		splitterBar_->SetRowInfo( 1, 10, 1 );
	}
	else
	{
		if ( layoutLastColSize_ > 0 )
			size = layoutLastColSize_;
		if ( size < MIN_SPLITTER_PANE_SIZE ) // limit minimum splitter size
			size = MIN_SPLITTER_PANE_SIZE;
		splitterBar_->SetColumnInfo( 0, size, 1 );
		splitterBar_->SetColumnInfo( 1, 10, 1 );
	}

	// recalc layout and update
	splitterBar_->RecalcLayout();
	CRect rect;
	GetClientRect( &rect );
	adjustSplitterSize( rect.Width(), rect.Height() );
}

void UalDialog::buildSmartListFilters()
{
	int i = 0;
	int pos = 6;
	FilterSpecPtr filter = 0;

	filtersCtrl_.clear();

	while ( filter = filterHolder_.getFilter( i++ ) )
	{
		if ( filter->getName().length() )
			filtersCtrl_.add( filter->getName().c_str(), filter->getActive(), (void*)filter.getObject() );
		else
			filtersCtrl_.addSeparator();
	} 
	
	CRect rect;
	GetClientRect( &rect );
	adjustFiltersSize( rect.Width(), rect.Height() );
	adjustSplitterSize( rect.Width(), rect.Height() );
}


BEGIN_MESSAGE_MAP(UalDialog, CDialog)
	ON_WM_CTLCOLOR()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_EN_CHANGE( IDC_UALSEARCH, OnSearchChange )
	ON_STN_CLICKED( IDC_UALMAGNIFIER, OnSearchFilters )
	ON_STN_CLICKED( IDC_UALSEARCHCLOSE, OnSearchClose )
	ON_COMMAND_RANGE( GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand )
END_MESSAGE_MAP()

HBRUSH UalDialog::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	if ( pWnd->GetSafeHwnd() == searchBk_.GetSafeHwnd() )
	{
		static CBrush brush( GetSysColor( COLOR_WINDOW ) );
		hbr = brush;
		pDC->SetBkColor( GetSysColor( COLOR_WINDOW ) );
	}

	return hbr;
}

void UalDialog::OnSetFocus( CWnd* pOldWnd )
{
	if ( UalManager::instance().focusCallback() )
		(*UalManager::instance().focusCallback())( this, true );
}

void UalDialog::OnKillFocus( CWnd* pNewWnd )
{
	if ( UalManager::instance().focusCallback() )
		(*UalManager::instance().focusCallback())( this, false );
}

BOOL UalDialog::PreTranslateMessage( MSG* msg )
{
	if ( msg->message == WM_LBUTTONDOWN )
	{
		// Save the las control that had the focus in the UAL
		if ( msg->hwnd == search_.GetSafeHwnd() ||
			msg->hwnd == folderTree_.GetSafeHwnd() ||
			msg->hwnd == smartList_.GetSafeHwnd() )
			lastFocus_ = msg->hwnd;
	}
	else if ( msg->message == WM_MOUSEMOVE )
	{
		// Steal back the focus to the UAL
//		if ( !::IsChild( this->GetSafeHwnd(), ::GetFocus() ) )
//			::SetFocus( lastFocus_ );
	}

    if ( toolTip_.GetSafeHwnd() )
        toolTip_.RelayEvent( msg );

	return 0;
}

void UalDialog::OnDestroy()
{
	CDialog::OnDestroy();
}

void UalDialog::OnSize( UINT nType, int cx, int cy )
{
	CDialog::OnSize( nType, cx, cy );

	adjustSearchSize( cx, cy );
	adjustFiltersSize( cx, cy );
	adjustSplitterSize( cx, cy );	
}

void UalDialog::OnSearchChange()
{
	char txt[MAX_SEARCH_TEXT + 1];
	int size = search_.GetWindowText( txt, MAX_SEARCH_TEXT );
	filterHolder_.setSearchText( txt );
	smartList_.updateFilters();
	bool oldShow = (searchClose_.GetStyle() & WS_VISIBLE)?true:false;
	bool newShow = size?true:false;
	searchClose_.ShowWindow( newShow?SW_SHOW:SW_HIDE );
	if ( newShow != oldShow )
	{
		CRect rect;
		GetClientRect( &rect );
		adjustSearchSize( rect.Width(), rect.Height() );
	}
	refreshStatusBar();
}

void UalDialog::OnSearchFilters()
{
	if ( filtersCtrl_.empty() )
		return;

	showFilters_ = !showFilters_;
	CRect rect;
	GetClientRect( &rect );
	adjustFiltersSize( rect.Width(), rect.Height() );
	adjustSplitterSize( rect.Width(), rect.Height() );
}

void UalDialog::OnSearchClose()
{
	search_.SetWindowText( "" );
}

void UalDialog::OnGUIManagerCommand(UINT nID)
{
	GUI::Manager::instance().act( nID );
}

void UalDialog::setFolderTreeStatusBar( VFolderItemData* data )
{
	if ( data && !!data->getProvider() )
		setStatusText(
			data->getProvider()->getDescriptiveText(
				data, smartList_.GetItemCount(), smartList_.finished() ) );
	else
		setStatusText( "" );
}

void UalDialog::callbackVFolderSelect( VFolderItemData* data )
{
	if ( !data || data->isVFolder() || !UalManager::instance().itemClickCallback() )
		return;

	POINT pt;
	GetCursorPos( &pt );

	UalItemInfo ii( this, data->assetInfo(), pt.x, pt.y );
	(*UalManager::instance().itemClickCallback())( &ii );
}

// UalDialog controls handlers
void UalDialog::favouritesChanged()
{
	folderTree_.refreshVFolders( favouritesFolderProvider_ );
	if ( smartList_.getProvider() == favouritesListProvider_.getObject() )
		smartList_.refresh();
}

void UalDialog::historyChanged()
{
	folderTree_.refreshVFolders( historyFolderProvider_ );
	if ( smartList_.getProvider() == historyListProvider_.getObject() )
		smartList_.refresh();
}

void UalDialog::folderTreeSelect( VFolderItemData* data )
{
	if ( !data )
		return;

	bool showInList = false;

	// get the parent vfolder to get subtree extra info
	VFolderPtr vfolder = folderTree_.getVFolder( data );
	XmlItemVec* customItems = 0;
	if ( !!vfolder )
	{
		UalFolderData* folderData = (UalFolderData*)vfolder->getData();
		customItems = vfolder->getCustomItems();
		if ( folderData )
		{
			search_.setIdleText( folderData->idleText_ );
			if ( folderData->showInList_ )
			{
				// set the thumbnail size / list style
				if ( folderData->thumbSize_ == 2 )
					setListStyle( SmartListCtrl::BIGICONS );
				else if ( folderData->thumbSize_ == 1 )
					setListStyle( SmartListCtrl::SMALLICONS );
				else
					setListStyle( SmartListCtrl::LIST );
				// set filter state disabled/enabled
				filtersCtrl_.enableAll( true );
				filterHolder_.enableAll( true );
				for( std::vector<std::string>::iterator i = folderData->disabledFilters_.begin();
					i != folderData->disabledFilters_.end(); ++i )
				{
					filtersCtrl_.enable( (*i), false );
					filterHolder_.enable( (*i), false );
				}
				showInList = true;
				smartList_.allowMultiSelect( folderData->multiItemDrag_ );
			}
		}
	}

	if ( !!data->getProvider() )
	{
		// see if it's the favourites provider
		CWaitCursor wait;
		ListProviderPtr listProvider;
		bool itemClicked = false;
		if ( showInList &&
			data->getProvider()->getListProviderInfo(
				data, lastListInit_, listProvider, itemClicked ) )
		{
			smartList_.init( listProvider, customItems );
		}
		if ( itemClicked )
			callbackVFolderSelect( data );
		setFolderTreeStatusBar( data );
	}
	else
	{
		// it's a plain vfolder
		smartList_.init( 0, customItems );
		setFolderTreeStatusBar( data );
		lastListInit_ = "";
	}
	updateFiltersImage();
	return;
}

void UalDialog::folderTreeStartDrag( VFolderItemData* data )
{
	if ( !data )
		return;

	// hack: using the getExpandable flag to see if its a folder type,
	// so all expandable items can be cloned (not sure if conceptually correct)
	VFolderPtr vfolder = folderTree_.getVFolder( data );
	std::vector<AssetInfo> assets;
	assets.push_back( data->assetInfo() );
	dragLoop( assets, data->getExpandable(), vfolder.getObject() );
}

void UalDialog::folderTreeItemDelete( VFolderItemData* data )
{
	if ( !data )
		return;

	if ( data->getProvider().getObject() == historyFolderProvider_.getObject() )
	{
		if ( data->isVFolder() )
		{
			if ( MessageBox(
					L("UAL/UAL_DIALOG/CLEAR_HISTORY_TEXT"),
					L("UAL/UAL_DIALOG/CLEAR_HISTORY_TITLE"),
					MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) != IDYES )
			{
				folderTree_.SetFocus();
				return;
			}
			folderTree_.SetFocus();

			UalManager::instance().history().clear();
		}
		else
			UalManager::instance().history().remove( data->assetInfo() );
	}
	else if ( data->getProvider().getObject() == favouritesFolderProvider_.getObject() )
	{
		if ( data->isVFolder() )
		{
			if ( MessageBox(
					L("UAL/UAL_DIALOG/CLEAR_FAVOURITES_TEXT"),
					L("UAL/UAL_DIALOG/CLEAR_FAVOURITES_TITLE"),
					MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) != IDYES )
			{
				folderTree_.SetFocus();
				return;
			}
			folderTree_.SetFocus();

			UalManager::instance().favourites().clear();
		}
		else
			UalManager::instance().favourites().remove( data->assetInfo() );
	}
}

void UalDialog::showItemContextMenu( UalItemInfo* ii )
{
	// build the popup menu
	int openExplorerCmd =		0xFF00;
	int openExplorerCmdRange =	0x0020;	// up to 32 paths
	int copyPathCmd =			0xFF20;
	int copyPathCmdRange =		0x0020;	// up to 32 paths
	int addToFavCmd =			0xFF40;
	int removeFromFavCmd =		0xFF41;
	int removeFromHistCmd =		0xFF42;
	int bigViewCmd =			0xFF43;
	int smallViewCmd =			0xFF44;
	int listViewCmd =			0xFF45;

	PopupMenu menu;

	PopupMenu::Items appItems;
	if ( UalManager::instance().startPopupMenuCallback() )
		(*UalManager::instance().startPopupMenuCallback())( ii, appItems );

	// List Styles submenu
	menu.startSubmenu( L("UAL/UAL_DIALOG/LIST_VIEW_STYLES") );

	std::string check;
	check = smartList_.getStyle() == SmartListCtrl::LIST ? "##" : "";
	menu.addItem( check + L("UAL/UAL_DIALOG/LIST"), listViewCmd );

	check = smartList_.getStyle() == SmartListCtrl::SMALLICONS ? "##" : "";
	menu.addItem( check + L("UAL/UAL_DIALOG/SMALL_ICONS"), smallViewCmd );

	check = smartList_.getStyle() == SmartListCtrl::BIGICONS ? "##" : "";
	menu.addItem( check + L("UAL/UAL_DIALOG/BIG_ICONS"), bigViewCmd );

	menu.endSubmenu();

	// add item paths
	std::vector<std::string> paths;
	if ( ii )
	{
		if ( !ii->isFolder() )
		{
			if ( smartList_.getProvider() == favouritesListProvider_.getObject() )
				menu.addItem( L("UAL/UAL_DIALOG/REMOVE_FROM_FAVOURITES"), removeFromFavCmd );
			else if ( smartList_.getProvider() == historyListProvider_.getObject() )
				menu.addItem( L("UAL/UAL_DIALOG/REMOVE_FROM_HISTORY"), removeFromHistCmd );

			if ( smartList_.getProvider() != favouritesListProvider_.getObject() )
				menu.addItem( L("UAL/UAL_DIALOG/ADD_TO_FAVOURITES"), addToFavCmd );
		}

		if ( !ii->getNext() )
		{
			// allow open in explorer and copy path if only one item is selected
			StringUtils::vectorFromString( ii->longText(), paths );
			if ( paths.size() == 1 )
			{
				menu.addItem( L("UAL/UAL_DIALOG/OPEN_FOLDER_IN_EXPLORER"), openExplorerCmd );
				menu.addItem( L("UAL/UAL_DIALOG/COPY_PATH_TO_CLIPBOARD"), copyPathCmd );
			}
			else
			{
				for( int i = 0; i < (int)paths.size() && i < openExplorerCmdRange; ++i )
				{
					if ( PathFileExists( paths[ i ].c_str() ) )
					{
						menu.addItem( 
								L("UAL/UAL_DIALOG/OPEN_X_IN_EXPLORER", paths[i]),
								openExplorerCmd + i );
					}
				}

				for( int i = 0; i < (int)paths.size() && i < copyPathCmdRange; ++i )
				{
					if ( PathFileExists( paths[ i ].c_str() ) )
					{
						menu.addItem( 
								L("UAL/UAL_DIALOG/COPY_X_TO_CLIPBOARD", paths[i]),
								copyPathCmd + i );
					}
				}
			}
		}
	}

	if ( !appItems.empty() ) 
		menu.addSeparator(); // separator

	menu.addItems( appItems );

	// run the menu
	int result = menu.doModal( GetSafeHwnd() );

	if ( result >= openExplorerCmd && result < openExplorerCmd + openExplorerCmdRange )
	{
		std::string path = paths[ result - openExplorerCmd ];
		std::string cmd = "explorer ";
		if ( !PathIsDirectory( path.c_str() ) )
			cmd += "/select,\"";
		else
			cmd += "\"";
		cmd += path + "\"";

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		GetStartupInfo( &si );

		if( CreateProcess( NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, 0,
			&si, &pi ) )
		{
			CloseHandle( pi.hThread );
			CloseHandle( pi.hProcess );
		}
	}
	else if ( result >= copyPathCmd && result < copyPathCmd + copyPathCmdRange )
	{
		if ( OpenClipboard() )
		{
			std::string path = paths[ result - copyPathCmd ];
			HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, 
				(path.length() + 1) * sizeof(TCHAR)); 
			if ( data && EmptyClipboard() )
			{ 
				LPTSTR str = (LPTSTR)GlobalLock( data );
				memcpy( str, path.c_str(),
					path.length() * sizeof(TCHAR));
				str[ path.length() ] = (TCHAR) 0;
				GlobalUnlock( data );

				SetClipboardData( CF_TEXT, data );
			} 
			CloseClipboard();
		}
	}
	else if ( result == bigViewCmd )
	{
		setListStyle( SmartListCtrl::BIGICONS );
	}
	else if ( result == smallViewCmd )
	{
		setListStyle( SmartListCtrl::SMALLICONS );
	}
	else if ( result == listViewCmd )
	{
		setListStyle( SmartListCtrl::LIST );
	}
	else if ( result == addToFavCmd ||
		result == removeFromFavCmd ||
		result == removeFromHistCmd )
	{
		// multi-items actions
		CWaitCursor wait;
		while ( ii )
		{
			if ( result == addToFavCmd )
			{
				UalManager::instance().favourites().add(
					ii->assetInfo() );
			}
			else if ( result == removeFromFavCmd )
			{
				UalManager::instance().favourites().remove(
					ii->assetInfo() );
			}
			else if ( result == removeFromHistCmd )
			{
				UalManager::instance().history().remove(
					ii->assetInfo() );
			}
			ii = ii->getNext();
		}
	}
	else if ( UalManager::instance().endPopupMenuCallback() ) 
	{
		(*UalManager::instance().endPopupMenuCallback())( ii, result );
	}
}

void UalDialog::showContextMenu( VFolderItemData* data )
{
	if ( !data || data->isVFolder() )
	{
		bool plainVFolder = true;
		if ( data )
			plainVFolder = 
				data->getProvider().getObject() != favouritesFolderProvider_.getObject() &&
				data->getProvider().getObject() != historyFolderProvider_.getObject();

		// build menu items
		int bigViewCmd =			0xFF43;
		int smallViewCmd =			0xFF44;
		int listViewCmd =			0xFF45;
		int renameCmd =				0xFF50;
		int defaultFoldersCmd =		0xFF51;
		int removeFolderCmd =		0xFF52;
		PopupMenu menu;

		// List Styles submenu
		menu.startSubmenu( L("UAL/UAL_DIALOG/LIST_VIEW_STYLES") );

		std::string check;
		check = smartList_.getStyle() == SmartListCtrl::LIST ? "##" : "";
		menu.addItem( check + L("UAL/UAL_DIALOG/LIST"), listViewCmd );

		check = smartList_.getStyle() == SmartListCtrl::SMALLICONS ? "##" : "";
		menu.addItem( check + L("UAL/UAL_DIALOG/SMALL_ICONS"), smallViewCmd );

		check = smartList_.getStyle() == SmartListCtrl::BIGICONS ? "##" : "";
		menu.addItem( check + L("UAL/UAL_DIALOG/BIG_ICONS"), bigViewCmd );

		menu.endSubmenu();

		// common menu items
		menu.addItem( L("UAL/UAL_DIALOG/CHANGE_PANEL_TITLE"), renameCmd );
		menu.addItem( L("UAL/UAL_DIALOG/RELOAD_DEFAULT_FOLDERS"), defaultFoldersCmd );

		if ( data )
		{
			std::string remove = L("UAL/UAL_DIALOG/REMOVE_X", data->assetInfo().text());
			menu.addItem( remove, removeFolderCmd );
		}
		if ( !plainVFolder )
			menu.addItem( L("UAL/UAL_DIALOG/CLEAR_CONTENTS"), 100 );

		// run the menu
		int result = menu.doModal( GetSafeHwnd() );

		if ( result == removeFolderCmd && data )
		{
			excludeVFolders_.push_back( data->assetInfo().text() );
			folderTree_.removeVFolder( data->getTreeItem() );
			if ( customVFolders_ )
			{
				std::vector<DataSectionPtr> sections;
				customVFolders_->openSections( "customVFolder", sections );
				for( std::vector<DataSectionPtr>::iterator s = sections.begin(); s != sections.end(); ++s )
					if ( (*s)->asString() == data->assetInfo().text() )
					{
						customVFolders_->delChild( *s );
						break;
					}
			}
			if ( folderTree_.GetCount() == 0 )
			{
				UalManager::instance().thumbnailManager().resetPendingRequests( &folderTree_ );
				// resetPendingRequests on the SmartList is done in its init
				smartList_.init( 0, 0 );
				setFolderTreeStatusBar( 0 );
				updateFiltersImage();
			}
		}
		else if ( result == defaultFoldersCmd )
		{
			if ( MessageBox( L("UAL/UAL_DIALOG/RELOAD_TEXT"),
				L("UAL/UAL_DIALOG/RELOAD_TITLE"),
				MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION ) == IDYES )
			{
				excludeVFolders_.clear();
				folderTree_.clear();
				UalManager::instance().thumbnailManager().resetPendingRequests( &folderTree_ );
				// resetPendingRequests on the SmartList is done in its init
				smartList_.init( 0, 0 );
				setFolderTreeStatusBar( 0 );
				updateFiltersImage();
				if ( !configFile_.empty() )
				{
					customVFolders_ = 0;
					BWResource::instance().purge( configFile_ );
					DataSectionPtr root = BWResource::openSection( configFile_ );
					if ( root )
						loadVFolders( root->openSection( "VFolders" ) );
				}
			}
		}
		else if ( result == renameCmd )
		{
			UalNameDlg dlg;
			dlg.setNames( dlgShortCaption_, dlgLongCaption_ );
			if ( dlg.DoModal() == IDOK )
			{
				dlg.getNames( dlgShortCaption_, dlgLongCaption_ );
				// Ugly hack: repaint all windows just to get the new panel title repainted :S  Instead, should implement a notification mecanism so the appropriate panel gets the repaint message
				if ( GetDesktopWindow() )
					GetDesktopWindow()->RedrawWindow( 0, 0, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASENOW | RDW_ALLCHILDREN );
			}
		}
		else if ( result == listViewCmd )
		{
			setListStyle( SmartListCtrl::LIST );
		}
		else if ( result == smallViewCmd )
		{
			setListStyle( SmartListCtrl::SMALLICONS );
		}
		else if ( result == bigViewCmd )
		{
			setListStyle( SmartListCtrl::BIGICONS );
		}
		else if ( result == 100 && data )
			folderTreeItemDelete( data );
	}
	else if ( data )
	{
		// create a popup menu for the item and call the app to fill it
		CPoint pt;
		GetCursorPos( &pt );
		UalItemInfo ii( this, data->assetInfo(), pt.x, pt.y );
		ii.isFolder_ = data->getExpandable();

		showItemContextMenu( &ii );
	}
}

void UalDialog::fillAssetsVectorFromList( std::vector<AssetInfo>& assets )
{
	int numSel = smartList_.GetSelectedCount();
	if ( numSel > 500 )
	{
		numSel = 500;
		error( "Dragging too many items, only taking the first 500." );
	}
	assets.reserve( numSel );
	int item = smartList_.GetNextItem( -1, LVNI_SELECTED );
	while( item > -1 && numSel > 0 )
	{
		assets.push_back( smartList_.getAssetInfo( item ) );
		item = smartList_.GetNextItem( item, LVNI_SELECTED );
		numSel--;
	}
}

void UalDialog::folderTreeRightClick( VFolderItemData* data )
{
	showContextMenu( data );
}

void UalDialog::folderTreeDoubleClick( VFolderItemData* data )
{
	if ( !UalManager::instance().itemDblClickCallback() || !data )
		return;

	if ( data->isVFolder() )
		return;

	CPoint pt;
	GetCursorPos( &pt );
	UalItemInfo ii( this, data->assetInfo(), pt.x, pt.y );

	ii.isFolder_ = data->getExpandable();

	(*UalManager::instance().itemDblClickCallback())( &ii );
}

void UalDialog::listLoadingUpdate()
{
	if ( !delayedListShowItem_.empty() )
	{
		std::string textTmp = delayedListShowItem_.c_str() + delayedListShowItem_.find_last_of( '\\' ) + 1;
		if ( smartList_.showItem( AssetInfo( "", textTmp, delayedListShowItem_ ) ) )
			delayedListShowItem_ = "";
	}

	refreshStatusBar();
}

void UalDialog::listLoadingFinished()
{
	delayedListShowItem_ = "";
	refreshStatusBar();
}

void UalDialog::listItemSelect()
{
	// notify
	if ( UalManager::instance().itemClickCallback() )
	{
		int focusItem = smartList_.GetNextItem( -1, LVNI_FOCUSED );
		if ( focusItem >=0 && smartList_.GetItemState( focusItem, LVIS_SELECTED ) == LVIS_SELECTED )
		{
			POINT pt;
			GetCursorPos( &pt );
			AssetInfo assetInfo = smartList_.getAssetInfo( focusItem );
			UalItemInfo ii( this, assetInfo, pt.x, pt.y );
			(*UalManager::instance().itemClickCallback())( &ii );
		}
	}

	int numSel = smartList_.GetSelectedCount();

	if ( !numSel )
		refreshStatusBar();
	else
	{
		// update status bar
		std::string txt;

		txt = 
			L
			(
				"UAL/UAL_DIALOG/SELECTED_ITEMS", 
				numSel, 
				smartList_.GetItemCount()
			);

		if ( numSel > 10 )
			txt += L("UAL/UAL_DIALOG/MANY_ITEMS");
		else
		{
			txt += " : ";
			int item = -1;
			for( int i = 0; i < numSel; ++i )
			{
				item = smartList_.GetNextItem( item, LVNI_SELECTED );
				if ( i != 0 )
					txt += ", ";
				if ( smartList_.getAssetInfo( item ).description().empty() )
					txt += smartList_.getAssetInfo( item ).longText();
				else
					txt += smartList_.getAssetInfo( item ).description();
			}
		}
		setStatusText( txt );
	}
}

void UalDialog::listItemDelete()
{
	if ( smartList_.getProvider() == historyListProvider_.getObject() ||
		smartList_.getProvider() == favouritesListProvider_.getObject() )
	{
		// delete from the history or favourites, depending on the current list provider
		int item = -1;
		int numSel = smartList_.GetSelectedCount();
		if ( numSel > 1 )
		{
			if ( MessageBox(
					L("UAL/UAL_DIALOG/MULTI_DELETE_TEXT"),
					L("UAL/UAL_DIALOG/MULTI_DELETE_TITLE"),
					MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 ) != IDYES )
			{
				smartList_.SetFocus();
				return;
			}
			smartList_.SetFocus();
		}

		for( int i = 0; i < numSel; i++ )
		{
			item = smartList_.GetNextItem( item, LVNI_SELECTED );
			if ( item >= 0 )
			{
				if ( smartList_.getProvider() == historyListProvider_.getObject() )
					UalManager::instance().history().remove(
						smartList_.getAssetInfo( item ),
						i == numSel - 1 );
				else
					UalManager::instance().favourites().remove(
						smartList_.getAssetInfo( item ),
						i == numSel - 1 );
			}
		}
	}
}

void UalDialog::listDoubleClick( int index )
{
	if ( !UalManager::instance().itemDblClickCallback() )
		return;

	CPoint pt;
	GetCursorPos( &pt );
	AssetInfo assetInfo;
	if ( index >= 0 )
		assetInfo = smartList_.getAssetInfo( index );

	UalItemInfo ii( this, assetInfo, pt.x, pt.y );
	(*UalManager::instance().itemDblClickCallback())( &ii );
}

void UalDialog::listStartDrag( int index )
{
	if ( index < 0 || index >= smartList_.GetItemCount() )
		return;

	std::vector<AssetInfo> assets;
	fillAssetsVectorFromList( assets );
	dragLoop( assets );
}

void UalDialog::listItemRightClick( int index )
{
	std::vector<AssetInfo> assets;
	fillAssetsVectorFromList( assets );

	if ( index < 0 || index >= smartList_.GetItemCount() || assets.empty() )
	{
		showItemContextMenu( 0 );
		return;
	}

	CPoint pt;
	GetCursorPos( &pt );

	std::vector<AssetInfo>::iterator a = assets.begin();
	UalItemInfo ii( this, *a++, pt.x, pt.y );
	UalItemInfo* iip = &ii;
	while( a != assets.end() )
	{
		iip->setNext(
			new UalItemInfo( this, *a++, pt.x, pt.y ) );
		iip = iip->getNext();
	}
	
	showItemContextMenu( &ii );
}

void UalDialog::listItemToolTip( int index, std::string& info )
{
	if ( index < 0 )
		return;

	AssetInfo assetInfo = smartList_.getAssetInfo( index );
	info = assetInfo.text();
	if ( !assetInfo.longText().empty() )
	{
		std::string path = BWResource::getFilePath(
			BWResource::dissolveFilename( assetInfo.longText() ) );
		if ( !path.empty() )
		{
			info += L("UAL/UAL_DIALOG/NL_PATH");
			info += path;
		}
	}
	if ( !assetInfo.description().empty() )
	{
		info += "\n";
		info += assetInfo.description();
	}
}

void UalDialog::handleDragMouseMove( UalItemInfo& ii, const CPoint& srcPt, bool isScreenCoords /*= false*/ )
{
	CPoint pt = srcPt;
	if (!isScreenCoords)
	{
		ClientToScreen( &pt );
	}
	if ( smartList_.isDragging() )
	{
		smartList_.updateDrag( pt.x, pt.y );
		smartList_.showDrag( false );
	}
	else if ( folderTree_.isDragging() )
	{
		folderTree_.updateDrag( pt.x, pt.y );
		folderTree_.showDrag( false );
	}
	ii.x_ = pt.x;
	ii.y_ = pt.y;
	if ( !UalManager::instance().updateDrag( ii, false ) &&
		UalManager::instance().updateDragCallback() )
		(*UalManager::instance().updateDragCallback())( &ii );
	if ( smartList_.isDragging() )
		smartList_.showDrag( true );
	else if ( folderTree_.isDragging() )
		folderTree_.showDrag( true );
}

void UalDialog::dragLoop( std::vector<AssetInfo>& assetsInfo, bool isFolder, void* folderExtraData )
{
	if ( assetsInfo.empty() )
		return;

	POINT pt;
	GetCursorPos( &pt );

	std::vector<AssetInfo>::iterator a = assetsInfo.begin();
	UalItemInfo ii( this, *a++, pt.x, pt.y, isFolder, folderExtraData );
	UalItemInfo* iip = &ii;
	while( a != assetsInfo.end() )
	{
		iip->setNext(
			new UalItemInfo( this, *a++, pt.x, pt.y, isFolder, folderExtraData ) );
		iip = iip->getNext();
	}

	if ( ii.isFolder_ )
		lastItemInfo_ = ii; // used when cloneRequired, to know last item dragged to be cloned

	if ( UalManager::instance().startDragCallback() )
		(*UalManager::instance().startDragCallback())( &ii );

	UpdateWindow();
	SetCapture();

	// send at least one update drag message
	handleDragMouseMove( ii, pt, true );

	while ( CWnd::GetCapture() == this )
	{
		MSG msg;
		if ( !::GetMessage( &msg, NULL, 0, 0 ) )
		{
			AfxPostQuitMessage( (int)msg.wParam );
			break;
		}

		if ( msg.message == WM_LBUTTONUP )
		{
			// END DRAG
			POINT pt = { (short)LOWORD( msg.lParam ), (short)HIWORD( msg.lParam ) };
			ClientToScreen( &pt );
			ii.x_ = pt.x;
			ii.y_ = pt.y;
			UalItemInfo* info = 0;
			UalDialog* endDialog = 0;
			if ( !( endDialog = UalManager::instance().updateDrag( ii, true ) ) )
				info = &ii; // if it's not an UAL to UAL drag, call the callback with the item info
			stopDrag();

			if ( UalManager::instance().endDragCallback() )
				(*UalManager::instance().endDragCallback())( info );
			if ( endDialog )
				endDialog->folderTree_.RedrawWindow();
			lastItemInfo_ = UalItemInfo();
			return;
		}
		else if ( msg.message == WM_MOUSEMOVE )
		{
			// UPDATE DRAG
			POINT pt = { (short)LOWORD( msg.lParam ), (short)HIWORD( msg.lParam ) };
			handleDragMouseMove( ii, pt );
		}
		else if ( msg.message == WM_KEYUP || msg.message == WM_KEYDOWN )
		{
			if ( msg.wParam == VK_ESCAPE )
				break; // CANCEL DRAG

			if ( msg.message == WM_KEYUP || !(msg.lParam & 0x40000000) )
			{
				// send update messages, but not if being repeated
				if ( smartList_.isDragging() )
				{
					smartList_.showDrag( false );
				}
				else if ( folderTree_.isDragging() )
				{
					folderTree_.showDrag( false );
				}
				if ( !UalManager::instance().updateDrag( ii, false ) &&
					UalManager::instance().updateDragCallback() )
					(*UalManager::instance().updateDragCallback())( &ii );
				if ( smartList_.isDragging() )
					smartList_.showDrag( true );
				else if ( folderTree_.isDragging() )
					folderTree_.showDrag( true );
			}
		}
		else if ( msg.message == WM_RBUTTONDOWN )
			break; // CANCEL DRAG
		else
			DispatchMessage( &msg );
	}

	cancelDrag();
}

void UalDialog::stopDrag()
{
	if ( smartList_.isDragging() )
		smartList_.endDrag();
	else if ( folderTree_.isDragging() )
		folderTree_.endDrag();
	UalManager::instance().cancelDrag();
	ReleaseCapture();
}

void UalDialog::cancelDrag()
{
	stopDrag();
	if ( UalManager::instance().endDragCallback() )
		(*UalManager::instance().endDragCallback())( 0 );
	lastItemInfo_ = UalItemInfo();
}

void UalDialog::resetDragDropTargets()
{
	folderTree_.SelectDropTarget( 0 );
	folderTree_.SetInsertMark( 0 );
	folderTree_.UpdateWindow();
	smartList_.clearDropTarget();
}

void UalDialog::scrollWindow( CWnd* wnd, CPoint pt )
{
	int scrollZone = 20;

	if ( wnd == &smartList_ )
	{
		CRect rect;
		smartList_.GetClientRect( &rect );
		bool vertical = ((GetWindowLong( smartList_.GetSafeHwnd(), GWL_STYLE ) & LVS_TYPEMASK) == LVS_ICON);
		int size = (vertical?rect.Height():rect.Width());
		int scrollArea = min( scrollZone, size / 4 );
		int coord = vertical?pt.y:pt.x;
		int speedx = vertical?0:1;
		int speedy = vertical?10:0;
		if ( coord < scrollArea ) 
		{
			smartList_.Scroll( CSize( -speedx, -speedy ) );
			smartList_.UpdateWindow();
		}
		else if ( coord >= size - scrollArea && coord < size )
		{
			smartList_.Scroll( CSize( speedx, speedy ) );
			smartList_.UpdateWindow();
		}
	}
	else if ( wnd == &folderTree_ )
	{
		static int speedDamping = 0;
		int speedDampingK = 3;
		CRect rect;
		folderTree_.GetClientRect( &rect );
		int pos = folderTree_.GetScrollPos( SB_VERT );
		int scrollAreaHeight = min( scrollZone, rect.Height() / 4 );
		if ( speedDamping == 0 )
			if ( pt.y < scrollAreaHeight && pos > 0 ) 
				folderTree_.SendMessage( WM_VSCROLL, SB_THUMBPOSITION | ((pos-1)<<16), 0 );
			else if ( pt.y >= rect.Height() - scrollAreaHeight && pt.y < rect.Height() )
				folderTree_.SendMessage( WM_VSCROLL, SB_THUMBPOSITION | ((pos+1)<<16), 0 );
		speedDamping++;
		if ( speedDamping > speedDampingK )
			speedDamping = 0;
	}
}

void UalDialog::updateSmartListDrag( const UalItemInfo& itemInfo, bool endDrag )
{
	CPoint pt( itemInfo.x_, itemInfo.y_ );
	if ( smartList_.getProvider() == favouritesListProvider_.getObject() &&
		!itemInfo.isFolder_ )
	{
		// managing favourites items by drag/drop to the list
		smartList_.ScreenToClient( &pt );
		UINT flags;
		int dropItemL = smartList_.HitTest( pt, &flags );

		if ( !endDrag )
		{
			// update
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
			if ( dropItemL > -1 )
				smartList_.setDropTarget( dropItemL );
			else
				smartList_.clearDropTarget();
			scrollWindow( &smartList_, pt );
		}
		else
		{
			// end drag
			AssetInfo dropAssetInfo;
			if ( dropItemL > -1 )
				dropAssetInfo = smartList_.getAssetInfo( dropItemL );

			bool doAdd = true;
			UalItemInfo* ii = const_cast<UalItemInfo*>( &itemInfo );
			while ( ii )
			{
				if ( ii->assetInfo().equalTo( dropAssetInfo ) )
				{
					// the dragged items are being dropped onto one of it's items,
					// so avoid adding it
					doAdd = false;
					break;
				}
				ii = ii->getNext();
			}

			if ( doAdd ) 
			{
				// only add if dropping over an item not in the dragged set
				CWaitCursor wait;
				ii = const_cast<UalItemInfo*>( &itemInfo );
				while ( ii )
				{
					UalManager::instance().favourites().remove( ii->assetInfo() );
					UalManager::instance().favourites().addAt(
						ii->assetInfo_,
						dropAssetInfo );
					ii = ii->getNext();
				}
			}
		}
	}
	else
	{
		// don't accept dragging of folders to the smartList
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );
		smartList_.clearDropTarget();
	}
}

void UalDialog::updateFolderTreeDrag( const UalItemInfo& itemInfo, bool endDrag )
{
	CPoint pt( itemInfo.x_, itemInfo.y_ );
	folderTree_.ScreenToClient( &pt );
	UINT flags;
	HTREEITEM dropItemT = folderTree_.HitTest( pt, &flags );
	VFolderItemDataPtr data = 0;
	if ( dropItemT )
		data = (VFolderItemData*)folderTree_.GetItemData( dropItemT );
	if ( itemInfo.isFolder_ )
	{
		// dragging a folder, so do folder-related stuff like Drag&Drop cloning or reordering
		if ( !endDrag )
		{
			// update
			folderTree_.SelectDropTarget( 0 );
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
			if ( data && data->isVFolder() )
				folderTree_.SetInsertMark( dropItemT, FALSE );
			else
			{
				// dropping beyond the last item, so find the last item and
				// set the insert mark properly
				HTREEITEM item = folderTree_.GetChildItem( TVI_ROOT );
				while( item && folderTree_.GetNextItem( item, TVGN_NEXT ) )
					item = folderTree_.GetNextItem( item, TVGN_NEXT );

				if ( item )
					folderTree_.SetInsertMark( item, TRUE );
				else
				{
					// should never get here
					SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );
					folderTree_.SetInsertMark( 0 );
				}
			}
			folderTree_.UpdateWindow();
		}
		else
		{
			// end drag
			VFolderPtr vfolder = folderTree_.getVFolder( itemInfo.assetInfo().text() );
			if ( !vfolder )
			{
				// add the dragged folder or vfolder
				UalManager::instance().copyVFolder( itemInfo.dialog_, this, itemInfo );
				vfolder = folderTree_.getVFolder( itemInfo.assetInfo().text() );
				VFolderPtr dropVFolder = folderTree_.getVFolder( data.getObject() );
				if ( vfolder && dropVFolder )
					folderTree_.moveVFolder( vfolder, dropVFolder );
			}
			else if ( itemInfo.dialog_ == this )
			{
				// folder already exists, reorder folders inside the same UAL
				if ( data )
				{
					VFolderPtr dropVFolder = folderTree_.getVFolder( data.getObject() );
					folderTree_.moveVFolder( vfolder, dropVFolder );
				}
				else
					folderTree_.moveVFolder( vfolder, 0 ); // put it last
			}
		}
	}
	else
	{
		// it's not a folder, so treat it like such
		folderTree_.SelectDropTarget( 0 );
		folderTree_.SetInsertMark( 0 );
		if ( data && data->getProvider().getObject() == favouritesFolderProvider_.getObject() )
		{
			// dropping inside the favourites folder, so it's valid
			if ( !endDrag )
			{
				//update
				SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
				if ( dropItemT )
				{
					if ( data->isVFolder() )
						folderTree_.SelectDropTarget( dropItemT ); // dropping on top of the favourites folder
					else
						folderTree_.SetInsertMark( dropItemT, FALSE );
				}
			}
			else
			{
				// end drag
				bool doAdd = true;
				UalItemInfo* ii = const_cast<UalItemInfo*>( &itemInfo );
				while ( ii )
				{
					if ( ii->assetInfo().equalTo( data->assetInfo() ) )
					{
						// the dragged items are being dropped onto one of it's items,
						// so avoid adding it
						doAdd = false;
						break;
					}
					ii = ii->getNext();
				}

				if ( doAdd )
				{
					CWaitCursor wait;
					ii = const_cast<UalItemInfo*>( &itemInfo );
					while ( ii )
					{
						if ( !data->isVFolder() )
						{
							// remove old item, if it exists, in order to add the new one in the proper location
							UalManager::instance().favourites().remove( ii->assetInfo_ );
						}
						// add to favourites
						if ( !UalManager::instance().favourites().getItem( ii->assetInfo_ ) )
							UalManager::instance().favourites().addAt(
								ii->assetInfo_,
								data->assetInfo() );
						else
							UalManager::instance().favourites().add( ii->assetInfo_ );
						ii = ii->getNext();
					}
				}
			}
		}
		else
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );
		folderTree_.UpdateWindow();
	}

	scrollWindow( &folderTree_, pt );
}

bool UalDialog::updateDrag( const UalItemInfo& itemInfo, bool endDrag )
{
	CPoint pt( itemInfo.x_, itemInfo.y_ );
	HWND hwnd = ::WindowFromPoint( pt );
	if ( hwnd == smartList_.GetSafeHwnd() )
	{
		updateSmartListDrag( itemInfo, endDrag );
		return true;
	}
	smartList_.clearDropTarget();

	if ( hwnd == folderTree_.GetSafeHwnd() )
	{
		updateFolderTreeDrag( itemInfo, endDrag );
		return true;
	}
	folderTree_.SelectDropTarget( 0 );
	folderTree_.SetInsertMark( 0 );
	folderTree_.UpdateWindow();

	SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );

	if ( ::IsChild( GetSafeHwnd(), hwnd ) )
		return true;

	return false;
}

void UalDialog::filterClicked( const char* name, bool pushed, void* data )
{
	FilterSpecPtr filter = (FilterSpec*)data;
	filter->setActive( pushed );
	HTREEITEM oldSel = folderTree_.GetSelectedItem();
	folderTree_.refreshVFolders();
	HTREEITEM sel = folderTree_.GetSelectedItem();
	if ( sel && sel != oldSel ) 
		folderTreeSelect( (VFolderItemData*)folderTree_.GetItemData( sel ) );
	smartList_.updateFilters();
	updateFiltersImage();
	refreshStatusBar();
}

void UalDialog::setStatusText( const std::string& text )
{
	statusBar_.SetWindowText( text.c_str() );
	toolTip_.UpdateTipText( text.c_str(), &statusBar_ );
}

void UalDialog::error( const std::string& msg )
{
	if ( UalManager::instance().errorCallback() )
		(*UalManager::instance().errorCallback())( std::string( "Asset Browser: " ) + msg );
}
