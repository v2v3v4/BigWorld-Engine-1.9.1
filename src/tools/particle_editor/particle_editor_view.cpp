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
#include "particle_editor_view.hpp"

#include "appmgr/app.hpp"
#include "appmgr/module_manager.hpp"
#include "common/cooperative_moo.hpp"
#include "main_frame.hpp"
#include "moo/render_context.hpp"
#include "particle_editor.hpp"
#include "particle_editor_doc.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

DECLARE_DEBUG_COMPONENT2( "ParticleEditor", 0 )

IMPLEMENT_DYNCREATE(ParticleEditorView, CView)

BEGIN_MESSAGE_MAP(ParticleEditorView, CView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
    ON_WM_SIZE()
	ON_WM_PAINT()
END_MESSAGE_MAP()

/*static*/ ParticleEditorView *ParticleEditorView::s_instance_ = NULL;

ParticleEditorView::ParticleEditorView() :
	lastRect_( 0, 0, 0, 0 )
{
    s_instance_ = this;
}

ParticleEditorView::~ParticleEditorView()
{
    s_instance_ = NULL;
}

BOOL ParticleEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	// changing style to no-background to avoid flicker in the 3D view.
	cs.lpszClass = AfxRegisterWndClass(
		CS_OWNDC | CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW), 0 );
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	return CView::PreCreateWindow(cs);
}

/*static*/ ParticleEditorView *ParticleEditorView::instance()
{
    return s_instance_;
}

ParticleEditorDoc* ParticleEditorView::GetDocument() const 
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(ParticleEditorDoc)));
	return (ParticleEditorDoc*)m_pDocument;
}

/*virtual*/ void ParticleEditorView::OnDraw(CDC *dc)
{
}

void 
ParticleEditorView::OnActivateView
(
    BOOL        activate, 
    CView       *activateView, 
    CView       *deactivateView
)
{
	if (ParticleEditorApp::instance().mfApp())
	{
		ParticleEditorApp::instance().mfApp()->handleSetFocus(activate != FALSE);
	}

	CView::OnActivateView(activate, activateView, deactivateView);
}

void ParticleEditorView::OnSize(UINT type, int cx, int cy)
{
    CView::OnSize(type, cx, cy);

//	No longer changing Moo mode here, since it's too slow
	//if (cx > 0 && cy > 0 && Moo::rc().device() && Moo::rc().windowed())
	//	Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed(), true);
}

void ParticleEditorView::OnPaint()
{
	CView::OnPaint();

	CRect rect;
	GetClientRect( &rect );

	if ( ParticleEditorApp::instance().mfApp() &&
		ModuleManager::instance().currentModule() )
	{
		if ( CooperativeMoo::beginOnPaint() )
		{		
			// changing mode when a paint message is received and the size of the
			// window is different than last stored size.
			if ( lastRect_ != rect &&
				Moo::rc().device() && Moo::rc().windowed() &&
				rect.Width() && rect.Height() &&
				!((MainFrame*)ParticleEditorApp::instance().mainWnd())->resizing() )
			{
				CWaitCursor wait;
				Moo::rc().changeMode(Moo::rc().modeIndex(), Moo::rc().windowed(), true);
				lastRect_ = rect;
			}

			ParticleEditorApp::instance().mfApp()->updateFrame( false );

			CooperativeMoo::endOnPaint();
		}
	}
	else
	{
		CWindowDC dc( this );

		dc.FillSolidRect( rect, ::GetSysColor( COLOR_BTNFACE ) );
	}
}
