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

#include "editor_views.hpp"

#include "property_table.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

PropertyTable::PropertyTable( UINT dialogID ):
	CFormView( dialogID )
{
	pImpl_ = new PropertyTableImpl;
}

PropertyTable::~PropertyTable() {}

void PropertyTable::update()
{
	for (std::list< BaseView* >::iterator it = pImpl_->viewList.begin();
		it != pImpl_->viewList.end();
		it++)
	{
		(*it)->updateGUI();
	}
}
	
void PropertyTable::clear()
{
	pImpl_->propertyList.clear();
	pImpl_->viewList.clear();
}

PropertyList* PropertyTable::propertyList()
{
	return &(pImpl_->propertyList);
}

void PropertyTable::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_PROPERTIES_LIST, pImpl_->propertyList);
}

BEGIN_MESSAGE_MAP(PropertyTable, CFormView)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL PropertyTable::PreTranslateMessage(MSG* pMsg)
{
    if(pMsg->message == WM_KEYDOWN)
    {
        if (pMsg->wParam == VK_TAB)
		{
			if (GetAsyncKeyState(VK_SHIFT) < 0)
			{
				if (pImpl_->propertyList.changeSelectItem( -1 )) return true;
			}
			else
			{
				if (pImpl_->propertyList.changeSelectItem( +1 )) return true;
			}
		}
        if (pMsg->wParam == VK_RETURN)
		{
			pImpl_->propertyList.deselectCurrentItem();
			//return true;
		}
    }

	return CFormView::PreTranslateMessage(pMsg);
}

void PropertyTable::stretchToRight( CWnd& widget, int pageWidth, int border )
{
	CRect rect;
	widget.GetWindowRect( &rect );
    ScreenToClient( &rect );
	widget.SetWindowPos( 0, 0, 0, pageWidth - rect.left - border, rect.Height(), SWP_NOMOVE | SWP_NOZORDER );
}

void PropertyTable::OnSize( UINT nType, int cx, int cy )
{
	stretchToRight( pImpl_->propertyList, cx, 12 );
	pImpl_->propertyList.setDividerPos( (cx - 12) / 2 );
	pImpl_->propertyList.RedrawWindow();
	RedrawWindow();
	CFormView::OnSize( nType, cx, cy );
}

int PropertyTable::addView( BaseView* view )
{
	pImpl_->viewList.push_back( view );

	return addItemsForView( view );
}

int PropertyTable::addItemsForView( BaseView* view )
{
	int firstIndex = -1;
	for (BaseView::PropertyItems::iterator it = view->propertyItems().begin();
		it != view->propertyItems().end();
		it++)
	{
		int newIndex = pImpl_->propertyList.AddPropItem(&*(*it));
		if (firstIndex == -1)
			firstIndex = newIndex;
	}
	return firstIndex;
}
