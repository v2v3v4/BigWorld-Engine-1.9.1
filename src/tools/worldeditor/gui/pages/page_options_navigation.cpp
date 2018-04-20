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
#include "worldeditor/gui/pages/page_options_navigation.hpp"
#include "worldeditor/gui/pages/panel_manager.hpp"
#include "worldeditor/framework/world_editor_app.hpp"
#include "worldeditor/scripting/world_editor_script.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/misc/world_editor_camera.hpp"
#include "appmgr/options.hpp"
#include "common/user_messages.hpp"
#include "resmgr/sanitise_helper.hpp"
#include "romp/time_of_day.hpp"
#include "controls/user_messages.hpp"
#include "chunk/chunk_manager.hpp"
#include "chunk/chunk_space.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/watcher.hpp"


DECLARE_DEBUG_COMPONENT2( "WorldEditor", 0 )


namespace
{
	// The number used to convert heights to the height slider fixed point
	// value:
	const unsigned int	HEIGHT_SLIDER_PREC	= 10;
}


// PageOptionsNavigation

// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageOptionsNavigation::contentID = "PageOptionsNavigation";


PageOptionsNavigation::PageOptionsNavigation()
	: CFormView(PageOptionsNavigation::IDD),
	pageReady_( false ),
	dontUpdateHeightEdit_(false),
	cameraHeightEdited_(false)
{
}

PageOptionsNavigation::~PageOptionsNavigation()
{
}

void PageOptionsNavigation::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_SEARCH_TEXT, search_);
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_SEARCH_CANCEL, search_cancel_);

	DDX_Control(pDX, IDC_OPTIONS_LOCATION_LIST, locationList_);
	
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_RENAME, locationRename_);
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_UPDATE, locationUpdate_);
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_REMOVE, locationRemove_);
	DDX_Control(pDX, IDC_OPTIONS_LOCATION_MOVE, locationMoveTo_);
	
	DDX_Control(pDX, IDC_OPTIONS_POS_X, posXEdit_);
	DDX_Control(pDX, IDC_OPTIONS_POS_Y, posYEdit_);
	DDX_Control(pDX, IDC_OPTIONS_POS_Z, posZEdit_);

	DDX_Control(pDX, IDC_OPTIONS_CHUNK, posChunkEdit_);
		
	DDX_Control(pDX, IDC_OPTIONS_CAMERAHEIGHT_SLIDER, cameraHeightSlider_);
	DDX_Control(pDX, IDC_OPTIONS_CAMERAHEIGHT_EDIT, cameraHeightEdit_);
	
	DDX_Control(pDX, IDC_PLAYER_PREVIEW_MODE, isPlayerPreviewModeEnabled_);
}

BOOL PageOptionsNavigation::PreTranslateMessage( MSG* pMsg )
{
	//Handle tooltips first...
	CALL_TOOLTIPS( pMsg );
	
	// If edit control is visible in tree view control, when you send a
	// WM_KEYDOWN message to the edit control it will dismiss the edit
	// control. When the ENTER key was sent to the edit control, the
	// parent window of the tree view control is responsible for updating
	// the item's label in TVN_ENDLABELEDIT notification code.
	if (pMsg->message == WM_KEYDOWN &&
		(pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE))
	{
		CEdit* edit = locationList_.GetEditControl();
		if (edit)
		{
			edit->SendMessage( WM_KEYDOWN, pMsg->wParam, pMsg->lParam );
			return TRUE; // Handled
		}
	}
	return CFormView::PreTranslateMessage( pMsg );
}

void PageOptionsNavigation::InitPage()
{
	INIT_AUTO_TOOLTIP();

	posXEdit_.SetNumericType(controls::EditNumeric::ENT_FLOAT);
	posXEdit_.SetNumDecimals(1);
	posXEdit_.commitOnFocusLoss(false);

	posYEdit_.SetNumericType(controls::EditNumeric::ENT_FLOAT);
	posYEdit_.SetNumDecimals(1);
	posYEdit_.commitOnFocusLoss(false);

	posZEdit_.SetNumericType(controls::EditNumeric::ENT_FLOAT);
	posZEdit_.SetNumDecimals(1);
	posZEdit_.commitOnFocusLoss(false);

	posChunkEdit_.commitOnFocusLoss(false);

	Matrix view = WorldEditorCamera::instance().currentCamera().view();
	view.invert();
	Vector3 pos = view.applyToUnitAxisVector(3);
	lastPos_ = pos;
	
	posXEdit_.SetValue( pos.x );
	posYEdit_.SetValue( pos.y );
	posZEdit_.SetValue( pos.z );

	Chunk* chunk = ChunkManager::instance().cameraSpace()->findChunkFromPoint( pos );
	if (chunk)
	{
		posChunkEdit_.SetWindowText( chunk->identifier().c_str() );
	}

	locationList_.updateLocationsList( WorldManager::instance().getCurrentSpace() );

	cameraHeightSlider_.SetRangeMin( 1 * HEIGHT_SLIDER_PREC );
	cameraHeightSlider_.SetRangeMax( 200 * HEIGHT_SLIDER_PREC );
	cameraHeightSlider_.SetPageSize(0);

	cameraHeightEdit_.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	cameraHeightEdit_.SetAllowNegative( false );
	cameraHeightEdit_.SetMinimum( 1 );
	cameraHeightEdit_.SetMaximum( 200 );
	cameraHeightEdit_.SetValue( Options::getOptionFloat( "graphics/cameraHeight", 2.f ) );
	cameraHeightSlider_.SetPos( (int)(HEIGHT_SLIDER_PREC*Options::getOptionFloat( "graphics/cameraHeight", 2.f )) );

	updateSliderEdits();
}


BEGIN_MESSAGE_MAP(PageOptionsNavigation, CFormView)
	ON_WM_SHOWWINDOW()
	ON_WM_HSCROLL()
	ON_WM_CTLCOLOR()
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_EN_CHANGE(IDC_OPTIONS_CAMERAHEIGHT_EDIT, OnEnChangeOptionsCameraHeightEdit)
	ON_BN_CLICKED(IDC_PLAYER_PREVIEW_MODE, OnBnClickedPlayerPreviewMode)
	ON_BN_CLICKED(IDC_OPTIONS_POS_MOVE, OnBnClickedOptionsPosMove)
	ON_BN_CLICKED(IDC_OPTIONS_CHUNK_MOVE, OnBnClickedOptionsChunkMove)
	ON_BN_CLICKED(IDC_OPTIONS_LOCATION_ADD, OnBnClickedOptionsLocationAdd)
	ON_BN_CLICKED(IDC_OPTIONS_LOCATION_REMOVE, OnBnClickedOptionsLocationRemove)
	ON_BN_CLICKED(IDC_OPTIONS_LOCATION_MOVE, OnBnClickedOptionsLocationMove)
	ON_STN_CLICKED(IDC_OPTIONS_LOCATION_SEARCH_BUTTON, OnStnClickedOptionsLocationSearchButton)
	ON_EN_CHANGE(IDC_OPTIONS_LOCATION_SEARCH_TEXT, OnEnChangeOptionsLocationSearchText)
	ON_STN_CLICKED(IDC_OPTIONS_LOCATION_SEARCH_CANCEL, OnStnClickedOptionsLocationSearchCancel)
	ON_BN_CLICKED(IDC_OPTIONS_LOCATION_RENAME, OnBnClickedOptionsLocationRename)
	ON_BN_CLICKED(IDC_OPTIONS_LOCATION_UPDATE, &PageOptionsNavigation::OnBnClickedOptionsLocationUpdate)
END_MESSAGE_MAP()


// PageOptionsNavigation message handlers
afx_msg void PageOptionsNavigation::OnShowWindow( BOOL bShow, UINT nStatus )
{
	CFormView::OnShowWindow( bShow, nStatus );

	if ( bShow == FALSE )
	{
	}
	else
	{
		OnUpdateControls( 0, 0 );
	}
}

afx_msg LRESULT PageOptionsNavigation::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !pageReady_ )
	{
		InitPage();
		pageReady_ = true;
	}

	if ( !IsWindowVisible() )
		return 0;

	int enabled = 0;
	int checked = 0;

	isPlayerPreviewModeEnabled_.SetCheck( WorldManager::instance().isInPlayerPreviewMode() );

	static float s_lastCameraHeight = -1.f;
	float cameraHeight = Options::getOptionFloat( "graphics/cameraHeight", 2.f );
	if ( cameraHeightEdited_ )
	{
		s_lastCameraHeight = cameraHeight;
		cameraHeightEdited_ = false;
	}
	if (cameraHeight != s_lastCameraHeight)
	{
		cameraHeightSlider_.SetPos( (int)(HEIGHT_SLIDER_PREC*cameraHeight ));
		cameraHeightEdit_.SetValue( cameraHeight );
		s_lastCameraHeight = cameraHeight;
	}

	if ((posXEdit_.doUpdate()) || (posYEdit_.doUpdate()) || (posZEdit_.doUpdate()))
	{
		OnBnClickedOptionsPosMove();
	}

	if (posChunkEdit_.doUpdate())
	{
		OnBnClickedOptionsChunkMove();
	}

	Matrix view = WorldEditorCamera::instance().currentCamera().view();
	view.invert();
	Vector3 pos = view.applyToUnitAxisVector(3);
	if ((fabsf( pos.x - lastPos_.x ) > 0.1f) ||
		(fabsf( pos.y - lastPos_.y ) > 0.1f) ||
		(fabsf( pos.z - lastPos_.z ) > 0.1f))
	{
		posXEdit_.SetValue( pos.x );
		posYEdit_.SetValue( pos.y );
		posZEdit_.SetValue( pos.z );

		Chunk* chunk = ChunkManager::instance().cameraSpace()->findChunkFromPoint( pos );
		if (chunk)
		{
			posChunkEdit_.SetWindowText( chunk->identifier().c_str() );
		}

		lastPos_ = pos;
	}

	static std::string s_lastSpaceName = "";
	std::string spaceName = WorldManager::instance().getCurrentSpace();
	if (spaceName != s_lastSpaceName)
	{
		locationList_.updateLocationsList( spaceName );

		s_lastSpaceName = spaceName;
	}

	HTREEITEM item = locationList_.GetSelectedItem();
	static HTREEITEM s_lastItem = (HTREEITEM)(-1);
	if (item != s_lastItem)
	{
		BOOL enabled = item ? TRUE : FALSE;
		locationRename_.EnableWindow( enabled );
		locationUpdate_.EnableWindow( enabled );
		locationRemove_.EnableWindow( enabled );
		locationMoveTo_.EnableWindow( enabled );
		RedrawWindow();
		s_lastItem = item;
	}

	return 0;
}

void PageOptionsNavigation::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// this captures all the scroll events for the page
	// upadate all the slider buddy windows
	updateSliderEdits();

	Options::setOptionFloat
	( 
		"graphics/cameraHeight", 
		(float)cameraHeightSlider_.GetPos()/HEIGHT_SLIDER_PREC 
	);

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageOptionsNavigation::updateSliderEdits()
{
	if (!dontUpdateHeightEdit_)
	{
		cameraHeightEdit_.SetValue( (float)cameraHeightSlider_.GetPos()/HEIGHT_SLIDER_PREC );
	}
}

void PageOptionsNavigation::OnEnChangeOptionsCameraHeightEdit()
{
	cameraHeightEdited_ = true;
	
	dontUpdateHeightEdit_ = true;
	cameraHeightSlider_.SetPos( (int)(HEIGHT_SLIDER_PREC*cameraHeightEdit_.GetValue()) );
	dontUpdateHeightEdit_ = false;

	Options::setOptionFloat( "graphics/cameraHeight", cameraHeightEdit_.GetValue() );
}

void PageOptionsNavigation::OnBnClickedPlayerPreviewMode()
{
	WorldManager::instance().setPlayerPreviewMode( isPlayerPreviewModeEnabled_.GetCheck() == TRUE );
}

void PageOptionsNavigation::OnBnClickedOptionsPosMove()
{
	Vector3 pos;
	pos.x = posXEdit_.GetValue();
	pos.y = posYEdit_.GetValue();
	pos.z = posZEdit_.GetValue();
	if (!almostEqual( pos, lastPos_))
	{
		Matrix view = WorldEditorCamera::instance().currentCamera().view();
		view.invert();
		view.translation( pos );
		view.invert();
		WorldEditorCamera::instance().currentCamera().view( view );
		lastPos_ = pos;
	}
}

void PageOptionsNavigation::OnBnClickedOptionsChunkMove()
{
	CString chunkName_cstr;
	posChunkEdit_.GetWindowText( chunkName_cstr );
	std::string chunkName = std::string( chunkName_cstr );
	ChunkDirMapping* mapping = WorldManager::instance().chunkDirMapping();

	if (*chunkName.rbegin() != 'o' && *chunkName.rbegin() != 'i')
	{
		std::string insideChunkName = chunkName + "i";
		if( BWResource::fileExists( mapping->path() + insideChunkName + ".chunk" ) )
			chunkName = insideChunkName;
		else
			chunkName += 'o';
	}
	if( *chunkName.rbegin() != 'i')
	{
		int16 x = -32768, z = -32768;
		mapping->gridFromChunkName( chunkName, x, z );
		chunkName = mapping->outsideChunkIdentifier( x, z );
	}

	Chunk *chunk = NULL;
	if( BWResource::fileExists( mapping->path() + chunkName + ".chunk" ) )
		chunk = ChunkManager::instance().findChunkByName( chunkName, mapping );
	if( !chunk )
	{
		ERROR_MSG("\"%s\" is not a valid chunk\n", (LPCTSTR)chunkName_cstr );
		return;
	}

	if (chunk->loaded())
	{
		ChunkSpacePtr space = ChunkManager::instance().cameraSpace();
		if ( space )
		{
			Matrix view( Matrix::identity );

			Vector3 camPos( chunk->centre() );
	
			if (chunk->isOutsideChunk())
			{
				const float EXTENT_RANGE = 5000.0f;

				camPos.y = EXTENT_RANGE;
				
				Vector3 extent = camPos + ( Vector3( 0.f, -2.f*EXTENT_RANGE, 0.f ) );

				float dist = space->collide( camPos, extent);

				camPos = camPos + Vector3( 0.f, 2.f - dist, 0.f );
			}

			view.translation( camPos );
			view.invert();
			WorldEditorCamera::instance().currentCamera().view( view );
		}
	}
	else
	{
		bool got = false;
		Vector3 world;
		if( chunk->isOutsideChunk() )
		{
			BoundingBox bb = chunk->boundingBox();
			world = bb.centre();
			world.y = 0.f;
			got = true;
		}
		else
		{
			DataSectionPtr pBBSect = BWResource::openSection(
				mapping->path() + chunkName + ".chunk/boundingBox" );
			if( pBBSect )
			{
				BoundingBox bb( pBBSect->readVector3( "min" ),
					pBBSect->readVector3( "max" ) );
				world = bb.centre();
				if( bb.minBounds().y + 0.5 < world.y )
					world.y = bb.minBounds().y + 0.5f;
				got = true;
			}
		}
		if( got )
		{
			Matrix view = WorldEditorCamera::instance().currentCamera().view();
			view.setTranslate( world );
			view.preRotateX( DEG_TO_RAD( 90.f ) );
			view.invert();
			WorldEditorCamera::instance().currentCamera().view( view );

			if( Options::getOptionInt( "camera/ortho" ) == WorldEditorCamera::CT_Orthographic )
			{
				WorldEditorCamera::instance().changeToCamera( WorldEditorCamera::CT_MouseLook );
				WorldEditorCamera::instance().changeToCamera( WorldEditorCamera::CT_Orthographic );
			}

			PanelManager::instance().setDefaultToolMode();
		}
		else
		{
			ERROR_MSG("\"%s\" is not a valid chunk\n", (LPCTSTR)chunkName_cstr );
		}
	}
}

void PageOptionsNavigation::OnStnClickedOptionsLocationSearchButton()
{
	search_.SetFocus();
}

void PageOptionsNavigation::OnEnChangeOptionsLocationSearchText()
{
	//First lets get the value entered
	CString search_cstr;
	search_.GetWindowText( search_cstr );
	std::string search_str = std::string( search_cstr );

	//convert it to lowercase
	std::transform (search_str.begin(), search_str.end(), search_str.begin(), tolower);

	search_cancel_.ShowWindow( search_str != "" );

	locationList_.setSearchString( search_str );

	locationList_.redrawLocationsList();
}

void PageOptionsNavigation::OnStnClickedOptionsLocationSearchCancel()
{
	search_.SetWindowText("");
}

void PageOptionsNavigation::OnBnClickedOptionsLocationAdd()
{
	locationList_.doAdd();
}

void PageOptionsNavigation::OnBnClickedOptionsLocationRename()
{
	locationList_.doRename();
}

void PageOptionsNavigation::OnBnClickedOptionsLocationUpdate()
{
	locationList_.doUpdate();
}

/*afx_msg*/ HBRUSH PageOptionsNavigation::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	HBRUSH brush = CFormView::OnCtlColor( pDC, pWnd, nCtlColor );
	
	cameraHeightEdit_.SetBoundsColour( pDC, pWnd,
		cameraHeightEdit_.GetMinimum(), cameraHeightEdit_.GetMaximum() );

	return brush;
}

void PageOptionsNavigation::OnBnClickedOptionsLocationRemove()
{
	locationList_.doRemove();
}

void PageOptionsNavigation::OnBnClickedOptionsLocationMove()
{
	locationList_.doMove();
}

IMPLEMENT_DYNAMIC(OptionsLocationsTree, CTreeCtrl)
OptionsLocationsTree::OptionsLocationsTree():
	search_str_("")
{
}

void OptionsLocationsTree::setSearchString( const std::string& searchString )
{
	search_str_ = searchString;
}

BEGIN_MESSAGE_MAP(OptionsLocationsTree, CTreeCtrl)
	ON_NOTIFY_REFLECT(TVN_ENDLABELEDIT, &OptionsLocationsTree::OnTvnEndlabeleditOptionsLocationList)
	ON_NOTIFY_REFLECT(NM_DBLCLK, &OptionsLocationsTree::OnNMDblclkOptionsLocationList)
	ON_NOTIFY_REFLECT(TVN_KEYDOWN, &OptionsLocationsTree::OnTvnKeydownOptionsLocationList)
END_MESSAGE_MAP()

void OptionsLocationsTree::OnTvnEndlabeleditOptionsLocationList(NMHDR *pNMHDR, LRESULT *pResult)
{
	bool needsRedraw = false;
	
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);

	HTREEITEM item = pTVDispInfo->item.hItem;

	if (!item) return;

	std::string oldLocationName = GetItemText( item );
	
	if (!pTVDispInfo->item.pszText) return;
	
	std::string newLocationName = pTVDispInfo->item.pszText;

	if (locations_.find(newLocationName) != locations_.end())
	{
		if (::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			L("WORLDEDITOR/GUI/PAGE_OPTIONS_NAVIGATION/OVERWRITTEN_LOCATION_MARK_TEXT", newLocationName),
			L("WORLDEDITOR/GUI/PAGE_OPTIONS_NAVIGATION/OVERWRITTEN_LOCATION_MARK_TITLE"),
			MB_YESNO) == IDNO)
			return;

		needsRedraw = true;
	}

	SetItemText( item, newLocationName.c_str() );

	std::vector<DataSectionPtr> bookmarks;
	locationData_->openSections( "bookmark", bookmarks );
	for ( size_t i = 0; i < bookmarks.size(); i++ )
	{
		if ( bookmarks[i]->readString( "name", "" ) == oldLocationName )
		{
			bookmarks[i]->writeString( "name", newLocationName );
			break;
		}
	}
	locations_.erase( oldLocationName );
	locations_.insert( newLocationName );

	locationData_->save();

	if ( !needsRedraw )
	{
		SortChildren( NULL );
	}
	else
	{
		redrawLocationsList();
	}

	item = GetRootItem();
	while ((item) && (strcmp(GetItemText(item),newLocationName.c_str())))
	{
		item = GetNextSiblingItem( item );
	}
	if (item)
	{
		SelectItem(item);
	}

	*pResult = 0;

	return;
}

void OptionsLocationsTree::OnNMDblclkOptionsLocationList(NMHDR *pNMHDR, LRESULT *pResult)
{
	doMove();

	*pResult = 0;
}

void OptionsLocationsTree::OnTvnKeydownOptionsLocationList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);
	
	if (pTVKeyDown->wVKey == VK_F2)
	{
		SetFocus();
		EditLabel( GetSelectedItem() );
	}

	*pResult = 0;
}

void OptionsLocationsTree::updateLocationsList( const std::string& spaceName )
{
	locations_.clear();
	
	locationData_ = BWResource::openSection(
		spaceName + "/locations.xml", true );

	int numChildren = locationData_->countChildren();
	for( int i = 0; i < numChildren; ++i )
	{
		std::string locationName = locationData_->openChild(i)->sectionName();
		if ( locationName == "bookmark" )
			locations_.insert( locationData_->openChild(i)->readString( "name", "" ) );
		else
		{
			DataSectionPtr newBookmark = locationData_->newSection( "bookmark" );
			newBookmark->writeString( "name", locationName );
			newBookmark->writeMatrix34( "view", locationData_->readMatrix34( locationName, Matrix::identity ) );
			locations_.insert( locationName );
		}
	}

	for ( int i = 0; i < locationData_->countChildren(); ++i )
	{
		std::string locationName = locationData_->openChild(i)->sectionName();
		if ( locationName != "bookmark" )
		{
			--i;
			locationData_->deleteSection( locationName );
		}
	}

	redrawLocationsList();
}

void OptionsLocationsTree::redrawLocationsList()
{
	HTREEITEM oldItem = GetSelectedItem();
	CString oldItemText = "";
	if (oldItem)
	{
		oldItemText = GetItemText( GetSelectedItem() );
	}
	
	DeleteAllItems();

	std::set< std::string >::iterator it = locations_.begin();
	std::set< std::string >::iterator end = locations_.end();
	for(; it != end; ++it)
	{
		std::string locationLower = *it;
		
		//convert it to lowercase
		std::transform (
			locationLower.begin(),
			locationLower.end(),
			locationLower.begin(),
			tolower);

		if (locationLower.find( search_str_, 0 ) != std::string::npos)
		{
			InsertItem( (*it).c_str() );
		}
	}

	SortChildren( NULL );

	if (!oldItem) return;
		
	//Select the previously selected item
	HTREEITEM item = GetRootItem();
	while ((item) && (strcmp( GetItemText( item ), oldItemText )))
	{
		item = GetNextSiblingItem( item );
	}
	if (item)
	{
		SelectItem( item );
	}
}

void OptionsLocationsTree::doAdd()
{
	std::string newName = L("WORLDEDITOR/GUI/PAGE_OPTIONS_NAVIGATION/UNTITLED");
	int i = 2;
	while (locations_.find( newName ) != locations_.end())
	{
		newName = L("WORLDEDITOR/GUI/PAGE_OPTIONS_NAVIGATION/UNTITLED_D", i);
		++i;
	}
	
	HTREEITEM item = InsertItem( newName.c_str() );

	DataSectionPtr newBookmark = locationData_->newSection( "bookmark" );
	newBookmark->writeString( "name", newName );
	newBookmark->writeMatrix34( "view", WorldEditorCamera::instance().currentCamera().view() );

	locations_.insert( newName );

	locationData_->save();

	EditLabel( item );
}

void OptionsLocationsTree::doRename()
{
	SetFocus();
	EditLabel( GetSelectedItem() );
}

void OptionsLocationsTree::doUpdate()
{
	std::string locationName = GetItemText( GetSelectedItem() );

	std::vector<DataSectionPtr> bookmarks;
	locationData_->openSections( "bookmark", bookmarks );
	for ( size_t i = 0; i < bookmarks.size(); i++ )
	{
		if ( bookmarks[i]->readString( "name", "" ) == locationName )
		{
			bookmarks[i]->writeMatrix34( "view", WorldEditorCamera::instance().currentCamera().view() );
			break;
		}
	}
	
	locationData_->save();
}

void OptionsLocationsTree::doRemove()
{
	HTREEITEM item = GetSelectedItem();

	std::string locationName = GetItemText( item );

	std::vector<DataSectionPtr> bookmarks;
	locationData_->openSections( "bookmark", bookmarks );
	for ( size_t i = 0; i < bookmarks.size(); i++ )
	{
		if ( bookmarks[i]->readString( "name", "" ) == locationName )
		{
			locationData_->delChild( bookmarks[i] );
			break;
		}
	}

	DeleteItem( item );

	locations_.erase( locationName );

	locationData_->save();
}

void OptionsLocationsTree::doMove()
{
	HTREEITEM item = GetSelectedItem();

	std::string locationName = GetItemText( item );

	std::vector<DataSectionPtr> bookmarks;
	locationData_->openSections( "bookmark", bookmarks );
	for ( size_t i = 0; i < bookmarks.size(); i++ )
	{
		if ( bookmarks[i]->readString( "name", "" ) == locationName )
		{
			WorldEditorCamera::instance().currentCamera().view(
				bookmarks[i]->readMatrix34( "view", WorldEditorCamera::instance().currentCamera().view() ) );
			break;
		}
	}
}
