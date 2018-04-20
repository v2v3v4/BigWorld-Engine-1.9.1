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
#include "worldeditor/gui/pages/page_properties.hpp"
#include "worldeditor/framework/world_editor_app.hpp"
#include "worldeditor/scripting/we_python_adapter.hpp"
#include "gizmo/gizmo_manager.hpp"
#include "gizmo/general_editor.hpp"
#include "gizmo/general_properties.hpp"
#include "common/user_messages.hpp"
#include "common/utilities.hpp"


DECLARE_DEBUG_COMPONENT( 0 )

// PageProperties
PageProperties * PageProperties::instance_ = NULL;


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageProperties::contentID = "PageProperties";



PageProperties::PageProperties()
	: PropertyTable(PageProperties::IDD)
	, rclickItem_( NULL )
	, inited_( false )
{
	ASSERT(!instance_);
	instance_ = this;
	PropTable::table( this );
}

PageProperties::~PageProperties()
{
	ASSERT(instance_);
	instance_ = NULL;
}

PageProperties& PageProperties::instance()
{
	ASSERT(instance_);
	return *instance_;
}


void PageProperties::DoDataExchange(CDataExchange* pDX)
{
	PropertyTable::DoDataExchange(pDX);
}

void PageProperties::addItems()
{
	pImpl_->propertyList.SetRedraw(FALSE);

	pImpl_->propertyList.clear();

	for (std::list<BaseView*>::iterator vi = pImpl_->viewList.begin();
		vi != pImpl_->viewList.end();
		vi++)
	{
		addItemsForView(*vi);
	}

	pImpl_->propertyList.SetRedraw(TRUE);
}


BEGIN_MESSAGE_MAP(PageProperties, CFormView)
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_MESSAGE(WM_DEFAULT_PANELS , OnDefaultPanels ) 
	ON_MESSAGE(WM_LAST_PANELS    , OnDefaultPanels ) 
	ON_MESSAGE(WM_SELECT_PROPERTYITEM, OnSelectPropertyItem)
	ON_MESSAGE(WM_CHANGE_PROPERTYITEM, OnChangePropertyItem)
	ON_MESSAGE(WM_DBLCLK_PROPERTYITEM, OnDblClkPropertyItem)
	ON_MESSAGE(WM_RCLK_PROPERTYITEM, OnRClkPropertyItem)
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()


// PageProperties message handlers
void PageProperties::OnSize( UINT nType, int cx, int cy )
{
	Utilities::stretchToBottomRight( this, pImpl_->propertyList, cx, 12, cy, 12 );

	PropertyTable::OnSize( nType, cx, cy );
}

afx_msg void PageProperties::OnClose()
{
	clear();
	CFormView::OnClose();
}

afx_msg LRESULT PageProperties::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if (!inited_)
	{	
		// pre allocate 1000 strings of about 16 char per string
		pImpl_->propertyList.InitStorage(1000, 16);

		CString name;
		GetWindowText(name);
		if (WorldEditorApp::instance().pythonAdapter())
		{
			WorldEditorApp::instance().pythonAdapter()->onPageControlTabSelect("pgc", name.GetBuffer());
		}

		// add any current properties to the list
		addItems();

		inited_ = true;
	}
	
	if ( !IsWindowVisible() )
		return 0;

	update();

	return 0;
}


afx_msg LRESULT PageProperties::OnDefaultPanels(WPARAM wParam, LPARAM lParam)
{
	if (!inited_)
		return 0;

	// Make sure the property list gets cleared.
	GeneralEditor::Editors newEds;
	GeneralEditor::currentEditors(newEds);

	return 0;
}


afx_msg LRESULT PageProperties::OnSelectPropertyItem(WPARAM wParam, LPARAM lParam)
{
	GizmoManager::instance().forceGizmoSet( NULL );

	if (lParam)
	{
		BaseView* relevantView = (BaseView*)lParam;
		relevantView->onSelect();
	}

	return 0;
}

afx_msg LRESULT PageProperties::OnChangePropertyItem(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		BaseView* relevantView = (BaseView*)lParam;
		relevantView->onChange(wParam != 0);
	}

	return 0;
}

afx_msg LRESULT PageProperties::OnDblClkPropertyItem(WPARAM wParam, LPARAM lParam)
{
	if (lParam)
	{
		PropertyItem* relevantView = (PropertyItem*)lParam;
		relevantView->onBrowse();
	}
	
	return 0;
}

afx_msg LRESULT PageProperties::OnRClkPropertyItem(WPARAM wParam, LPARAM lParam)
{
	PropertyItem * item = (PropertyItem*)(lParam);
	if (!item)
		return 0;

	rclickItem_ = item;

	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	if (!view)
		return 0;

	PropertyManagerPtr propManager = view->getPropertyManger();
	if (!propManager)
		return 0;

	if (propManager->canAddItem())
	{
		CMenu menu;
		menu.LoadMenu( IDR_PROPERTIES_LIST_POPUP );
		CMenu* pPopup = menu.GetSubMenu(0);

		POINT pt;
		::GetCursorPos( &pt );
		pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y,
			AfxGetMainWnd() );
	}
	else if (propManager->canRemoveItem())
	{
		CMenu menu;
		menu.LoadMenu( IDR_PROPERTIES_LISTITEM_POPUP );
		CMenu* pPopup = menu.GetSubMenu(0);

		POINT pt;
		::GetCursorPos( &pt );
		pPopup->TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y,
			AfxGetMainWnd() );
	}

	return 0;
}

void PageProperties::OnListAddItem()
{
	// add a new item to the selected list
	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	PropertyManagerPtr propManager = view->getPropertyManger();
	if (propManager)
	{
		propManager->addItem();
	}
}

void PageProperties::OnListItemRemoveItem()
{
	// remove the item from the list
	BaseView * view = (BaseView *)rclickItem_->getChangeBuddy();
	PropertyManagerPtr propManager = view->getPropertyManger();
	if (propManager)
	{
		propManager->removeItem();
	}
}

BOOL PageProperties::PreTranslateMessage(MSG* pMsg)
{
    if(pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_TAB)
		{
			if( GetAsyncKeyState( VK_SHIFT ) )
				pImpl_->propertyList.selectPrevItem();
			else
				pImpl_->propertyList.selectNextItem();
			return true;
		}
        if (pMsg->wParam == VK_RETURN)
		{
			pImpl_->propertyList.deselectCurrentItem();
			return true;
		}
    }

	return CFormView::PreTranslateMessage(pMsg);
}


void PageProperties::adviseSelectedId( std::string id )
{
	if (!GetSafeHwnd())     // see if this page has been created
		return;

/*	CPropertySheet* parentSheet = (CPropertySheet *)GetParent();

	if (parentSheet->GetActivePage() != this)
		return; */

	PropertyItem * hItem = pImpl_->propertyList.getHighlightedItem();
	if ((hItem == NULL) || (hItem->getType() != PropertyItem::Type_ID))
		return;

	StaticTextView * view = (StaticTextView*)(hItem->getChangeBuddy());
	view->setCurrentValue( id );
}
