/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// MainFrm.cpp : implementation of the CMainFrame class
//

#include "pch.hpp"

#include "model_editor_doc.h"

#include "me_app.hpp"

#include "appmgr/options.hpp"
#include "resmgr/string_provider.hpp"

#include "model_editor.h"
#include "splash_dialog.hpp"

#include "guitabs/guitabs.hpp"

#include "guimanager/gui_manager.hpp"

#include "common/property_list.hpp"
#include "common/user_messages.hpp"

#include "page_materials.hpp"

#include "panel_manager.hpp"

#include "main_frm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_CREATE()

	ON_WM_CLOSE()

	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_UPDATE_COMMAND_UI_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommandUpdate)

	ON_WM_MENUSELECT()
	ON_WM_EXITMENULOOP()
	ON_NOTIFY_RANGE( TBN_HOTITEMCHANGE, 0, 0xffffffff, OnToolbarHotItemChange  ) 

	ON_MESSAGE( WM_ENTERSIZEMOVE, OnEnterSizeMove)
	ON_MESSAGE( WM_EXITSIZEMOVE, OnExitSizeMove)

END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_TRIANGLES,
	ID_INDICATOR_NODES,
	ID_INDICATOR_BLENDBONE_NODES,
	ID_INDICATOR_TETXURE_MEM,
	ID_INDICATOR_FRAMERATE,
};

// CMainFrame construction/destruction

CMainFrame::CMainFrame() :
	GUI::ActionMaker<CMainFrame>( "doShowToolbar", &CMainFrame::showToolbar ),
	GUI::ActionMaker<CMainFrame, 1>( "doHideToolbar", &CMainFrame::hideToolbar ),
	GUI::UpdaterMaker<CMainFrame>( "updateToolbar", &CMainFrame::updateToolbar ),
	resizing_( false )
{
	m_bAutoMenuEnable = FALSE;
	s_instance = this;
	PropertyList::mainFrame( this );
}

CMainFrame::~CMainFrame()
{
}

bool CMainFrame::showToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::showToolbar( item );
}

bool CMainFrame::hideToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::hideToolbar( item );
}

unsigned int CMainFrame::updateToolbar( GUI::ItemPtr item )
{
	return BaseMainFrame::updateToolbar( item );
}

POINT CMainFrame::currentCursorPosition() const
{
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( MeShell::instance().hWndGraphics(), &pt );
	return pt;
}

Vector3 CMainFrame::getWorldRay(int x, int y) const
{
	Vector3 v = Moo::rc().invView().applyVector(
		Moo::rc().camera().nearPlanePoint(
			(float(x) / Moo::rc().screenWidth()) * 2.f - 1.f,
			1.f - (float(y) / Moo::rc().screenHeight()) * 2.f ) );
	v.normalise();
	return v;
}

bool CMainFrame::cursorOverGraphicsWnd() const
{
	HWND fore = ::GetForegroundWindow();
	if ( fore != MeShell::instance().hWndApp() && ::GetParent( fore ) != MeShell::instance().hWndApp() )
		return false; // foreground window is not the main window nor a floating panel.

	RECT rt;
	::GetWindowRect( MeShell::instance().hWndGraphics(), &rt );
	POINT pt;
	::GetCursorPos( &pt );

	if ( pt.x < rt.left ||
		pt.x > rt.right ||
		pt.y < rt.top ||
		pt.y > rt.bottom )
	{
		return false;
	}

	HWND hwnd = ::WindowFromPoint( pt );
	if ( hwnd != MeShell::instance().hWndGraphics() )
		return false; // it's a floating panel, return.
	HWND parent = hwnd;
	while( ::GetParent( parent ) != NULL )
		parent = ::GetParent( parent );
	::SendMessage( hwnd, WM_MOUSEACTIVATE, (WPARAM)parent, WM_LBUTTONDOWN * 65536 + HTCLIENT );

	CMainFrame::instance().SetMessageText( "" );

	return true;
}

/*virtual*/ BOOL CMainFrame::PreTranslateMessage(MSG *pmsg)
{
   UINT msg = pmsg->message;

   if (msg == WM_SYSKEYDOWN)
   {
       bool  alt     = (HIWORD(pmsg->lParam) & KF_ALTDOWN) == KF_ALTDOWN;
	   bool  tab     = (GetKeyState(VK_TAB    ) & 0x8000) != 0;
       bool  control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
       bool  f4      = (GetKeyState(VK_F4     ) & 0x8000) != 0;

       if (alt && !tab && !control && !f4)
           return TRUE;
   }

   return CFrameWnd::PreTranslateMessage(pmsg);
} 


int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	SetWindowLong( m_hWnd, GWL_STYLE, GetStyle() & ~FWS_ADDTOTITLE );

	SetWindowText( L( "MODELEDITOR/DASH_MODELEDITOR", L("MODELEDITOR/UNTITLED") ) );

	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	//Show the splashscreen
	if (!!Options::getOptionInt( "startup/showSplashScreen", 1 ) && !IsDebuggerPresent() )
		CSplashDlg::ShowSplashScreen( NULL );
	
	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
			sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TRIANGLES),
							ID_INDICATOR_TRIANGLES, SBPS_NORMAL, 128);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_NODES),
							ID_INDICATOR_NODES, SBPS_NORMAL, 128);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_BLENDBONE_NODES),
							ID_INDICATOR_BLENDBONE_NODES, SBPS_NORMAL, 128);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_TETXURE_MEM),
							ID_INDICATOR_TETXURE_MEM, SBPS_NORMAL, 128);
	m_wndStatusBar.SetPaneInfo(m_wndStatusBar.CommandToIndex(ID_INDICATOR_FRAMERATE),
							ID_INDICATOR_FRAMERATE, SBPS_NORMAL, 128);

	setStatusText( ID_INDICATOR_TRIANGLES, "" );
	setStatusText( ID_INDICATOR_NODES, "" );
	setStatusText( ID_INDICATOR_BLENDBONE_NODES, "" );
	setStatusText( ID_INDICATOR_TETXURE_MEM, "" );
	setStatusText( ID_INDICATOR_FRAMERATE, "" );

	EnableDocking(CBRS_ALIGN_ANY);

	return 0;
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT cs, CCreateContext* pContext)
{
	CFrameWnd::OnCreateClient(cs, pContext);
	
	return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	return CFrameWnd::PreCreateWindow(cs);
}

void CMainFrame::OnGUIManagerCommand(UINT nID)
{
	GUI::Manager::instance().act( nID );
}

void CMainFrame::OnGUIManagerCommandUpdate(CCmdUI * cmdUI)
{
	if( !cmdUI->m_pMenu )
		GUI::Manager::instance().update( cmdUI->m_nID );
}

void CMainFrame::updateGUI( bool force /* = false */)
{
	//Update the Title bar name if the model is modified
	bool dirty = MeApp::instance().mutant()->dirty();
	static bool s_wasDirty = !dirty;
	if ((force) || (s_wasDirty != dirty))
	{
		std::string titleText = MeApp::instance().mutant()->modelName();

		std::string::size_type first = titleText.rfind("/") + 1;
		std::string::size_type last = titleText.rfind(".");
		titleText = titleText.substr( first, last-first );

		if (titleText == "")
			titleText = L("MODELEDITOR/UNTITLED");

		if (dirty)
			titleText = titleText + " *";

		titleText = L("MODELEDITOR/DASH_MODELEDITOR", titleText);

		AfxGetApp()->m_pMainWnd->SetWindowText( titleText.c_str() ); 

		GUI::Manager::instance().update();

		s_wasDirty = dirty;
	}
		
	//Update the undo/redo buttons if needed
	static bool firstTime = true;
	static bool canUndo, canRedo;
	if( force || firstTime ||
		(canUndo != UndoRedo::instance().canUndo()) ||
		(canRedo != UndoRedo::instance().canRedo()) )
	{
		firstTime = false;
		canUndo = UndoRedo::instance().canUndo();
		canRedo = UndoRedo::instance().canRedo();
		GUI::Manager::instance().update();
	}
	
	//Update all the Panels
	PanelManager::instance().updateControls();
}

void CMainFrame::OnMenuSelect( UINT nItemID, UINT nFlags, HMENU hSysMenu )
{
   std::string s;
   if( ! ( nFlags & ( MF_DISABLED | MF_GRAYED | MF_SEPARATOR ) ) )
   {
       GUI::Manager::instance().update();
       GUI::ItemPtr item = GUI::Manager::instance().findByCommandID( nItemID );
       if( item )
       {
           s = item->description();
           while( s.find( '&' ) != s.npos )
           {
               s.erase( s.begin() + s.find( '&' ) );
           }
       }
   }
   SetMessageText( s.c_str() );
}

void CMainFrame::OnExitMenuLoop( BOOL bIsTrackPopupMenu )
{
   SetMessageText( "" );
}

void CMainFrame::OnToolbarHotItemChange( UINT id, NMHDR* pNotifyStruct, LRESULT* result )
{
   *result = 0;
   std::string s;
   LPNMTBHOTITEM hotItem = (LPNMTBHOTITEM)pNotifyStruct;
   GUI::ItemPtr item = GUI::Manager::instance().findByCommandID( hotItem->idNew );
   if( item )
   {
       s = item->description();
       while( s.find( '&' ) != s.npos )
       {
           s.erase( s.begin() + s.find( '&' ) );
       }
   }
   SetMessageText( s.c_str() );
} 

LRESULT CMainFrame::OnEnterSizeMove (WPARAM, LPARAM)
{
	// Set the resizing_ flag to true, so the view knows that we are resizing
	// and that it shouldn't change the Moo mode.
	resizing_ = true;
	return 0;
}

LRESULT CMainFrame::OnExitSizeMove (WPARAM, LPARAM)
{
	// Set the resizing_ flag back to false, so the view knows that it has to
	// change the Moo mode on the next repaint.
	resizing_ = false;
	// And send the repaint message to the view.
	::InvalidateRect( MeShell::instance().hWndGraphics(), NULL, TRUE );
	::UpdateWindow( MeShell::instance().hWndGraphics() );
	return 0;
}

void CMainFrame::OnClose()
{
	if (MeApp::instance().canExit( true ))
	{
		PageMaterials::currPage()->restoreView();
		MeApp::instance().mutant()->unregisterModelChangeCallback( PageMaterials::currPage() );

		//Make sure the current model is at the head of the MRU list
		std::string modelName = MeApp::instance().mutant()->modelName();
		if (modelName != "")
		{
			MRU::instance().update( "models", MeApp::instance().mutant()->modelName(), true );
		}

		MeApp::instance().camera()->save();
			
		Options::save();

		PanelManager::instance().onClose();

		CFrameWnd::OnClose();
	}
}

// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/*
 * stuff that model editor doesn't use, but is needed for linking purposes
 */

#include "../worldeditor/editor/editor_group.hpp"
void EditorGroup::enterGroup( ChunkItem* item ) { MF_ASSERT(1); }
void EditorGroup::leaveGroup( ChunkItem* item ) { MF_ASSERT(1); }
EditorGroup* EditorGroup::findOrCreateChild( const std::string& name ) { return NULL; }
EditorGroup* EditorGroup::findOrCreateGroup( const std::string& fullName ) { return NULL; }
std::string EditorGroup::fullName() const { return ""; }
void changedChunk(class Chunk *) { MF_ASSERT(1); }
bool chunkWritable( Chunk * pChunk, bool bCheckSurroundings /*= true*/ )	{	return true;	}

#include "chunk/chunk_vlo.hpp"
void VeryLargeObject::edDelete( ChunkVLO* instigator ) { MF_ASSERT(1); }

#include "gizmo/item_functor.hpp"
#include "gizmo/combination_gizmos.hpp"
PyObject * DynamicFloatDevice::pyNew(PyObject * object) { return NULL; }
PyObject * MatrixRotator::pyNew(PyObject * object) { return NULL; }
PyObject * MatrixScaler::pyNew(PyObject * object) { return NULL; }
