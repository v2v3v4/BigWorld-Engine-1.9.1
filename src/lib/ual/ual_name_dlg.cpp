/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// UalNameDlg.cpp : implementation file
//

#include "pch.hpp"
#include <string>
#include "ual_resource.h"
#include "ual_name_dlg.hpp"
#include "resmgr/string_provider.hpp"


// UalNameDlg dialog

UalNameDlg::UalNameDlg(CWnd* pParent /*=NULL*/)
	: CDialog(UalNameDlg::IDD, pParent)
{
}

UalNameDlg::~UalNameDlg()
{
}

void UalNameDlg::DoDataExchange(CDataExchange* pDX)
{
	DDX_Text(pDX, IDC_UALNAMELONG, longName_);
	DDV_MaxChars(pDX, longName_, 80);
	DDX_Text(pDX, IDC_UALNAMESHORT, shortName_);
	DDV_MaxChars(pDX, shortName_, 20);
	CDialog::DoDataExchange(pDX);
}

void UalNameDlg::getNames( std::string& shortName, std::string& longName )
{
	longName = (LPCTSTR)longName_;
	shortName = (LPCTSTR)shortName_;
}

void UalNameDlg::setNames( const std::string& shortName, const std::string& longName )
{
	longName_ = longName.c_str();
	shortName_ = shortName.c_str();
}

BEGIN_MESSAGE_MAP(UalNameDlg, CDialog)
END_MESSAGE_MAP()

void UalNameDlg::OnOK()
{
	UpdateData( TRUE );
	if ( longName_.Trim().IsEmpty() || shortName_.Trim().IsEmpty() )
	{
		MessageBox
		( 
			L("UAL/UAL_NAME_DLG/TYPE_BOTH_TEXT"),
			L("UAL/UAL_NAME_DLG/TYPE_BOTH_TITLE"),
			MB_ICONERROR 
		);
		return;
	}
	CDialog::OnOK();
}


// UalNameDlg message handlers
