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

#include "windowsx.h"

#include "common/string_utils.hpp"

#include "trigger_list.hpp"


BEGIN_MESSAGE_MAP(CheckList, CTreeCtrl)
END_MESSAGE_MAP()

/*
void CheckList::OnClick(NMHDR *pNMHDR, LRESULT *pResult)
{
	// retrieve mouse cursor position when msg was sent
	DWORD dw = GetMessagePos();							
	CPoint point(GET_X_LPARAM(dw), GET_Y_LPARAM(dw));
	ScreenToClient(&point);

	UINT htFlags = 0;
	HTREEITEM item = HitTest(point, &htFlags);

	bool checked = this->GetCheck( item ) == BST_UNCHECKED;
	
	if (item && ( htFlags & TVHT_ONITEMSTATEICON ) )
	{
		Select(item, TVGN_CARET);
	}
	else
	{
		return;
	}

	int id = *(int*)(this->GetItemData( item ));

	if (checked)
	{
		capsSet_.insert( id );
	}
	else
	{
		std::set< int >::iterator entry = std::find( capsSet_.begin(), capsSet_.end(), id );
		if (entry != capsSet_.end())
		{
			capsSet_.erase( entry );
		}
	}
}
*/

void CheckList::capsStr( const std::string& capsStr )
{
	std::vector< std::string > temp;
	StringUtils::vectorFromString( capsStr, temp, ";, " );

	int val = 0;
	for (unsigned i=0; i<temp.size(); i++)
	{
		sscanf( temp[i].c_str(), "%d", &val );
		capsSet_.insert( val );
	}
}

std::string CheckList::caps()
{
	char buf[256] = "";

	std::set<int>::iterator it = capsSet_.begin();
	std::set<int>::iterator end = capsSet_.end();
	
	if (capsSet_.size() > 0)
		bw_snprintf(buf, sizeof(buf), "%d", *it);
	for (++it; it != end; ++it)
		bw_snprintf(buf, sizeof(buf), "%s %d", buf, *it);

	return std::string( buf );
}

void CheckList::redrawList()
{
	HTREEITEM item = GetFirstVisibleItem();
	do
	{
		int id = *(int*)(GetItemData( item ));

		SetCheck( item, BST_UNCHECKED);

		std::set<int>::iterator it = capsSet_.begin();
		std::set<int>::iterator end = capsSet_.end();

		for (; it != end; ++it)
		{
			if (*it == id)
			{
				SetCheck( item, BST_CHECKED);
				break;
			}
		}
	}
	while ( item = GetNextVisibleItem( item ) );
}

void CheckList::updateList()
{
	capsSet_.clear();
	
	HTREEITEM item = GetFirstVisibleItem();
	do
	{
		if (GetCheck( item ) == BST_CHECKED)
		{
			capsSet_.insert( *(int*)(GetItemData( item )));
		}
	}
	while ( item = GetNextVisibleItem( item ) );
}

CTriggerList::CTriggerList( const std::string& capsName, std::vector< DataSectionPtr >& capsList, const std::string& capsStr /* = "" */ ):
	CDialog( CTriggerList::IDD ),
	capsName_(capsName),
	capsList_(capsList),
	capsStr_(capsStr)
{
	checkList_.capsStr( capsStr_ );
}

CTriggerList::~CTriggerList()
{
	std::vector<int*>::iterator capsDataIt = capsData_.begin();
	std::vector<int*>::iterator capsDataEnd = capsData_.end();
	for (;capsDataIt != capsDataEnd; ++capsDataIt)
	{
		delete (*capsDataIt);
	}
	capsData_.clear();
}

void CTriggerList::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_TRIGGER_LIST, checkList_);
}

BOOL CTriggerList::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText( capsName_.c_str() );

	std::vector< DataSectionPtr >::iterator it = capsList_.begin();
	std::vector< DataSectionPtr >::iterator end = capsList_.end();

	HTREEITEM item;
		
	for(;it != end; ++it)
	{
		int id = (*it)->asInt();
		std::string name = (*it)->readString( "name", "" );
		item = checkList_.InsertItem( name.c_str(), NULL );

		int* capNum = new int( id );
		checkList_.SetItemData( item, (DWORD)capNum );
		capsData_.push_back( capNum );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

std::string CTriggerList::caps()
{
	return checkList_.caps();
}

BEGIN_MESSAGE_MAP(CTriggerList, CDialog)
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CTriggerList::OnPaint()
{
	CDialog::OnPaint();

	checkList_.redrawList();
}

void CTriggerList::OnOK()
{
	checkList_.updateList();

	CDialog::OnOK();
}
