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
#include "worldeditor/framework/world_editor_view.hpp"
#include "worldeditor/framework/world_editor_app.hpp"
#include "worldeditor/framework/world_editor_doc.hpp"
#include "worldeditor/framework/mainframe.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "appmgr/app.hpp"
#include "appmgr/module_manager.hpp"
#include "common/cooperative_moo.hpp"
#include "input/input.hpp"


IMPLEMENT_DYNCREATE(WorldEditorView, CView)


BEGIN_MESSAGE_MAP(WorldEditorView, CView)
	ON_WM_PAINT()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()


WorldEditorView::WorldEditorView() :
	lastRect_( 0, 0, 0, 0 )
{
}


WorldEditorView::~WorldEditorView()
{
}


BOOL WorldEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	// Change style to no background to avoid flicker in the 3D view:
	cs.lpszClass = AfxRegisterWndClass(
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW), 0 );
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	return CView::PreCreateWindow(cs);
}


void WorldEditorView::OnDraw(CDC* /*pDC*/)
{
}


WorldEditorDoc* WorldEditorView::GetDocument() const 
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(WorldEditorDoc)));
	return (WorldEditorDoc*)m_pDocument;
}


void WorldEditorView::OnPaint()
{
	CView::OnPaint();

	CRect rect;
	GetClientRect( &rect );

	if 
	( 
		WorldEditorApp::instance().mfApp() 
		&&
		ModuleManager::instance().currentModule() 
	)
	{
		if ( CooperativeMoo::beginOnPaint() )
		{		
			// Change mode when a paint message is received and the size of the
			// window is different than last stored size:
			if 
			( 
				lastRect_ != rect 
				&&
				Moo::rc().device() 
				&& 
				Moo::rc().windowed() 
				&&
				rect.Width() && rect.Height() 
				&&
				!((MainFrame*)WorldEditorApp::instance().mainWnd())->resizing() 
			)
			{
				CWaitCursor wait;
				Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed(), true);
				lastRect_ = rect;
			}
			WorldEditorApp::instance().mfApp()->updateFrame( false );
			CooperativeMoo::endOnPaint();
		}
	}
	else
	{
		CWindowDC dc( this );
		dc.FillSolidRect( rect, ::GetSysColor( COLOR_BTNFACE ) );
	}
}


void WorldEditorView::OnActivateView
(
	BOOL		bActivate, 
	CView		*pActivateView, 
	CView		*pDeactiveView
)
{
	CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}


BOOL WorldEditorView::OnSetCursor(CWnd *wnd, UINT, UINT)
{
	::SetCursor( WorldManager::instance().cursor() );
	return TRUE;
}
