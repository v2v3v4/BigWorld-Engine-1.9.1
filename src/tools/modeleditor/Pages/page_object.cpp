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

#include "shlwapi.h"
#include "atlimage.h"

#include "python_adapter.hpp"

#include "main_frm.h"
#include "me_shell.hpp"
#include "me_app.hpp"
#include "mru.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "utilities.hpp"

#include "string_utils.hpp"
#include "ual/ual_manager.hpp"

#include "common/material_utility.hpp"
#include "common/material_properties.hpp"
#include "common/material_editor.hpp"
#include "common/editor_views.hpp"
#include "common/user_messages.hpp"
#include "common/file_dialog.hpp"

#include "physics2/material_kinds.hpp"

#include "me_material_proxies.hpp"

#include "page_object.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

struct PageObjectImpl: public SafeReferenceCount
{
	static PageObject* s_currPage;

	DataSectionPtr materialKinds;
	std::vector< int* > kindData;
	std::map< int, int > kindItem;

	bool inited;
	bool ready;
	bool updating;

	std::string lastDefaultText;

	CImage* thumbnail;

	std::string modelName;

	CStatic name;

	CStatic thumbnailRect;
	
	CButton batch;

	CButton occluder;

	std::string editorProxyName;
	
	CEdit editorProxy;

	CButton editorProxySel;
	
	CComboBox kind;

	GeneralEditorPtr editor;

	int pageWidth;
};

PageObject* PageObjectImpl::s_currPage = NULL;

// PageObject

//ID string required for the tearoff tab manager
const std::string PageObject::contentID = "PageObjectID";

IMPLEMENT_DYNCREATE(PageObject, CFormView)

PageObject::PageObject():
	PropertyTable( PageObject::IDD )
{
	pImpl_ = new PageObjectImpl;

	pImpl_->inited = false;
	pImpl_->ready = false;
	pImpl_->updating = false;

	pImpl_->modelName = "";
	pImpl_->editorProxyName = "";

	pImpl_->thumbnail = new CImage();
	
	DataSectionPtr flagsFile = BWResource::openSection( "resources/flags.xml" );
	pImpl_->materialKinds = flagsFile->newSection( "materialKinds" );
	pImpl_->lastDefaultText = "None";
	pImpl_->materialKinds->writeInt( pImpl_->lastDefaultText, 0 );
	MaterialKinds::instance().populateDataSection( pImpl_->materialKinds );

	pImpl_->pageWidth = 0;
	pImpl_->s_currPage = this;
}

PageObject::~PageObject()
{
	delete pImpl_->thumbnail;
	for (unsigned i=0; i<pImpl_->kindData.size(); i++)
		delete pImpl_->kindData[i];

	if (pImpl_->editor)
	{
		pImpl_->editor->expel();
	}
}

/*static*/ PageObject* PageObject::currPage()
{
	return PageObjectImpl::s_currPage;
}

void PageObject::DoDataExchange(CDataExchange* pDX)
{
	PropertyTable::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_OBJECT_NAME, pImpl_->name);

	DDX_Control(pDX, IDC_OBJECT_THUMBNAIL, pImpl_->thumbnailRect);
	
	DDX_Control(pDX, IDC_OBJECT_BATCH, pImpl_->batch);

	DDX_Control(pDX, IDC_OBJECT_OCCLUDER, pImpl_->occluder);

	DDX_Control(pDX, IDC_OBJECT_PROXY, pImpl_->editorProxy);
	DDX_Control(pDX, IDC_OBJECT_PROXY_SEL, pImpl_->editorProxySel);
	
	DDX_Control(pDX, IDC_OBJECT_KIND, pImpl_->kind);

	CRect setupsRect;
	pImpl_->kind.GetWindowRect(setupsRect);
    ScreenToClient (&setupsRect);
	setupsRect.bottom += 256;	// Extend the dropdown box to show a reasonable selection
	pImpl_->kind.MoveWindow(setupsRect);
		
	int item = 0;

	DataSectionIterator it = pImpl_->materialKinds->begin();
	for (; it != pImpl_->materialKinds->end(); ++it)
	{
		int* materialID = new int( (*it)->asInt() );
		pImpl_->kindData.push_back( materialID );
		pImpl_->kindItem[ *materialID ] = 
			pImpl_->kind.InsertString( item,
				(*it)->sectionName().c_str() );
		pImpl_->kind.SetItemData( item, (DWORD)materialID );
		item++;
	}

	pImpl_->inited = true;
}

BOOL PageObject::OnInitDialog()
{
	bool canBatch = MeApp::instance().mutant()->canBatch();
	pImpl_->batch.EnableWindow( canBatch ? TRUE : FALSE );
	pImpl_->batch.SetCheck( MeApp::instance().mutant()->batched() ? BST_CHECKED : BST_UNCHECKED );

	pImpl_->occluder.SetCheck( MeApp::instance().mutant()->dpvsOccluder() ? BST_CHECKED : BST_UNCHECKED );

	UalManager::instance().dropManager().add(
		new UalDropFunctor< PageObject >( &(pImpl_->editorProxy), "model", this, &PageObject::changeEditorProxyDrop ) );
	
	INIT_AUTO_TOOLTIP();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(PageObject, CFormView)

	ON_WM_CREATE()

	ON_WM_SIZE()

	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)

	ON_MESSAGE(WM_CHANGE_PROPERTYITEM, OnChangePropertyItem)

	ON_WM_HSCROLL()

	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_UPDATE_COMMAND_UI_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommandUpdate)

	ON_CBN_SELCHANGE(IDC_OBJECT_KIND, OnCbnSelchangeObjectKind)
	ON_BN_CLICKED(IDC_OBJECT_BATCH, OnBnClickedObjectBatch)
	ON_BN_CLICKED(IDC_OBJECT_OCCLUDER, OnBnClickedObjectOccluder)

	ON_MESSAGE(WM_SHOW_TOOLTIP, OnShowTooltip)
	ON_MESSAGE(WM_HIDE_TOOLTIP, OnHideTooltip)

	ON_BN_CLICKED(IDC_OBJECT_PROXY_SEL, OnBnClickedObjectProxySel)
	ON_BN_CLICKED(IDC_OBJECT_PROXY_REMOVE, OnBnClickedObjectProxyRemove)
END_MESSAGE_MAP()

void PageObject::OnGUIManagerCommand(UINT nID)
{
	pImpl_->s_currPage = this;
	GUI::Manager::instance().act( nID );
}

void PageObject::OnGUIManagerCommandUpdate(CCmdUI * cmdUI)
{
	pImpl_->s_currPage = this;
	if( !cmdUI->m_pMenu )                                                   
		GUI::Manager::instance().update( cmdUI->m_nID );
}

afx_msg LRESULT PageObject::OnShowTooltip(WPARAM wParam, LPARAM lParam)
{
	LPSTR* msg = (LPSTR*)wParam;
	CMainFrame::instance().SetMessageText( *msg );
	return 0;
}

afx_msg LRESULT PageObject::OnHideTooltip(WPARAM wParam, LPARAM lParam)
{
	CMainFrame::instance().SetMessageText( "" );
	return 0;
}

// PageObject message handlers

int PageObject::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//We might use this later...
	return 1;
}

void PageObject::OnSize(UINT nType, int cx, int cy)
{
	if (!pImpl_->inited) return;
	
	Utilities::stretchToRight( this, pImpl_->name, cx, 12 );
	PathSetDlgItemPath( this->GetSafeHwnd(), IDC_OBJECT_NAME, pImpl_->modelName.c_str() );

	Utilities::centre( this, pImpl_->thumbnailRect, cx );

	Utilities::stretchToRight( this, pImpl_->editorProxy, cx, 40 );
	PathSetDlgItemPath( this->GetSafeHwnd(), IDC_OBJECT_PROXY, pImpl_->editorProxyName.c_str() );

	Utilities::moveToRight( this, pImpl_->editorProxySel, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->kind, cx, 12 );

	PropertyTable::OnSize( nType, cx, cy );

	pImpl_->pageWidth = cx;
}

afx_msg LRESULT PageObject::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	static int s_updateCount = 0;
	pImpl_->updating = s_updateCount != MeApp::instance().mutant()->updateCount("Object");
	s_updateCount = MeApp::instance().mutant()->updateCount("Object");
	
	if (!pImpl_->ready)
	{
		OnInitDialog();

		pImpl_->updating = true; // Do an update first time through...

		pImpl_->ready = true;
	}

	// Check whether the LOD selection has changed
	if (pImpl_->updating)
	{
		pImpl_->s_currPage = this;

		pImpl_->modelName = MeApp::instance().mutant()->modelName();
		PathSetDlgItemPath( this->GetSafeHwnd(), IDC_OBJECT_NAME, pImpl_->modelName.c_str() );

		OnUpdateThumbnail();

		bool canBatch = MeApp::instance().mutant()->canBatch();
		pImpl_->batch.EnableWindow( canBatch ? TRUE : FALSE );
		pImpl_->batch.SetCheck( MeApp::instance().mutant()->batched() ? BST_CHECKED : BST_UNCHECKED );

		pImpl_->occluder.SetCheck( MeApp::instance().mutant()->dpvsOccluder() ? BST_CHECKED : BST_UNCHECKED );

		pImpl_->editorProxyName = MeApp::instance().mutant()->editorProxyName();
		PathSetDlgItemPath( this->GetSafeHwnd(), IDC_OBJECT_PROXY, pImpl_->editorProxyName.c_str() );

		int item = pImpl_->kindItem[ MeApp::instance().mutant()->modelMaterial() ];
		
		pImpl_->kind.SetCurSel( item );

		char text[256];
		pImpl_->kind.GetLBText( item, text );
		bw_snprintf( text, sizeof(text), "%s (Default)", text );
			
		pImpl_->materialKinds->delChild( pImpl_->lastDefaultText );
		pImpl_->lastDefaultText = text;
		pImpl_->materialKinds->writeInt( pImpl_->lastDefaultText, 0 );

		OnUpdateList();
	}

	PropertyTable::update();

	return 0;
}

void PageObject::OnUpdateThumbnail()
{
	std::string::size_type last = pImpl_->modelName.rfind(".");
	std::string thumbName = pImpl_->modelName.substr( 0, last );
	thumbName += ".thumbnail.jpg";
	thumbName = BWResource::resolveFilename( thumbName );

	pImpl_->thumbnail->Destroy();
	pImpl_->thumbnail->Load( thumbName.c_str() );
	UalManager::instance().thumbnailManager().stretchImage( *(pImpl_->thumbnail), 128, 128, true );
	pImpl_->thumbnailRect.SetBitmap( (HBITMAP)(*(pImpl_->thumbnail)) );

	//Do the centering here to make sure that it is done with the correct size
	Utilities::centre( this, pImpl_->thumbnailRect, pImpl_->pageWidth );
}

void PageObject::OnUpdateList()
{
	TreeRoot* treeRoot = MeApp::instance().mutant()->materialTree();

	PropTable::table( this );
	
	if (pImpl_->editor)
	{
		pImpl_->editor->expel();
	}

	pImpl_->editor = GeneralEditorPtr(new GeneralEditor(), true);

	ChoiceProperty* pProp;
	
	for (unsigned m=0; m<treeRoot->size(); m++)
	{
		std::string materialName = (*treeRoot)[m].first.first;
		std::string displayName = MeApp::instance().mutant()->materialDisplayName( materialName );
		
		SmartPointer< MeMaterialFlagProxy > mkp =
			new MeMaterialFlagProxy(
				"materialKind",
				materialName);
		
		pProp = new ChoiceProperty( displayName, mkp, pImpl_->materialKinds );
		pImpl_->editor->addProperty( pProp );

	}

	pImpl_->editor->elect();
}

afx_msg LRESULT PageObject::OnChangePropertyItem(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		BaseView* relevantView = (BaseView*)lParam;
		bool transient = !!wParam;
		relevantView->onChange( transient );
	}

	return 0;
}

void PageObject::OnCbnSelchangeObjectKind()
{
	int id = *(int*)(pImpl_->kind.GetItemData( pImpl_->kind.GetCurSel() ));

	MeApp::instance().mutant()->modelMaterial( id );
}

void PageObject::OnBnClickedObjectBatch()
{
	MeApp::instance().mutant()->batched( pImpl_->batch.GetCheck() == BST_CHECKED );
}

void PageObject::OnBnClickedObjectOccluder()
{
	MeApp::instance().mutant()->dpvsOccluder( pImpl_->occluder.GetCheck() == BST_CHECKED );
}

bool PageObject::changeEditorProxy( const std::string& editorProxyFile )
{
	if (editorProxyFile != pImpl_->modelName)
	{
		MeApp::instance().mutant()->editorProxyName( editorProxyFile );
		PathSetDlgItemPath( this->GetSafeHwnd(), IDC_OBJECT_PROXY, editorProxyFile.c_str() );
		pImpl_->editorProxyName = editorProxyFile;
		return true;
	}

	::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
		L("MODELEDITOR/PAGES/PAGE_OBJECT/BAD_DIR_EP"),
		L("MODELEDITOR/PAGES/PAGE_OBJECT/UNABLE_RESOLVE_EP"),
		MB_OK | MB_ICONWARNING );
	return false;
}

bool PageObject::changeEditorProxyDrop( UalItemInfo* ii ) 
{
	return changeEditorProxy( BWResource::dissolveFilename( ii->longText() ));
}

void PageObject::OnBnClickedObjectProxySel()
{
	static char BASED_CODE szFilter[] =	"Model (*.model)|*.model||";
	BWFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter);

	std::string modelDir = pImpl_->editorProxyName;
	if (modelDir == "")
	{
		MRU::instance().getDir("models", modelDir );
	}
	fileDlg.m_ofn.lpstrInitialDir = modelDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		std::string editorProxyFile = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));

		if (BWResource::validPath( editorProxyFile ))
		{
			changeEditorProxy( editorProxyFile );
		}
		else
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				L("MODELEDITOR/APP/ME_APP/BAD_DIR"),
				L("MODELEDITOR/APP/ME_APP/UNABLE_RESOLVE"),
				MB_OK | MB_ICONWARNING );
		}
	}
}

void PageObject::OnBnClickedObjectProxyRemove()
{
	MeApp::instance().mutant()->removeEditorProxy();
	pImpl_->editorProxyName = "";
	pImpl_->editorProxy.SetWindowText( "" );
}
