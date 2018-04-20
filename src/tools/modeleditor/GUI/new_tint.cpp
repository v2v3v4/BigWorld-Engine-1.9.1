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

#include <algorithm>

#include "resmgr/auto_config.hpp"
#include "resmgr/string_provider.hpp"
#include "appmgr/options.hpp"
#include "common/file_dialog.hpp"

#include "mru.hpp"

#include "new_tint.hpp"

static AutoConfigString s_default_fx( "system/defaultShaderPath" );
static AutoConfigString s_default_mfm( "system/defaultMfmPath" );


CNewTint::CNewTint( std::vector< std::string >& tintNames ):
	CDialog( CNewTint::IDD ),
	tintNames_(tintNames),
	fxFile_(""),
	mfmFile_("")
{}

void CNewTint::DoDataExchange(CDataExchange* pDX)
{
	CRect listRect;
	
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_NEW_TINT_NAME, name_);

	DDX_Control(pDX, IDC_NEW_TINT_FX_CHECK, fxCheck_ );

	DDX_Control(pDX, IDC_NEW_TINT_FX_LIST, fxList_ );

	fxList_.GetWindowRect(listRect);
    ScreenToClient (&listRect);
	listRect.bottom += 256;
	fxList_.MoveWindow(listRect);
	fxList_.SelectString(-1, "");

	DDX_Control(pDX, IDC_NEW_TINT_FX_SEL, fxSel_ );

	DDX_Control(pDX, IDC_NEW_TINT_MFM_CHECK, mfmCheck_ );
	
	DDX_Control(pDX, IDC_NEW_TINT_MFM_LIST, mfmList_ );

	mfmList_.GetWindowRect(listRect);
    ScreenToClient (&listRect);
	listRect.bottom += 256;
	mfmList_.MoveWindow(listRect);
	mfmList_.SelectString(-1, "");

	DDX_Control(pDX, IDC_NEW_TINT_MFM_SEL, mfmSel_ );

	DDX_Control(pDX, IDOK, ok_);
}

BOOL CNewTint::OnInitDialog()
{
	CDialog::OnInitDialog();

	redrawList( fxList_, "fx" );
	redrawList( mfmList_, "mfm" );

	if (Options::getOptionInt( "settings/lastNewTintFX", 1 ))
	{
		fxCheck_.SetCheck( BST_CHECKED );
		OnBnClickedNewTintFxCheck();
	}
	else
	{
		mfmCheck_.SetCheck( BST_CHECKED );
		OnBnClickedNewTintMfmCheck();
	}

	ok_.ModifyStyle( 0, WS_DISABLED );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BEGIN_MESSAGE_MAP(CNewTint, CDialog)
	ON_EN_CHANGE(IDC_NEW_TINT_NAME, OnEnChangeNewTintName)
	ON_BN_CLICKED(IDC_NEW_TINT_MFM_CHECK, OnBnClickedNewTintMfmCheck)
	ON_BN_CLICKED(IDC_NEW_TINT_FX_CHECK, OnBnClickedNewTintFxCheck)
	ON_CBN_SELCHANGE(IDC_NEW_TINT_FX_LIST, OnCbnSelchangeNewTintFxList)
	ON_CBN_SELCHANGE(IDC_NEW_TINT_MFM_LIST, OnCbnSelchangeNewTintMfmList)
	ON_BN_CLICKED(IDC_NEW_TINT_FX_SEL, OnBnClickedNewTintFxSel)
	ON_BN_CLICKED(IDC_NEW_TINT_MFM_SEL, OnBnClickedNewTintMfmSel)
END_MESSAGE_MAP()

void CNewTint::checkComplete()
{
	if (tintName_.length() && (fxFile_.length() || mfmFile_.length()))
	{
		ok_.ModifyStyle( WS_DISABLED, 0 );
	}
	else
	{
		ok_.ModifyStyle( 0, WS_DISABLED );
	}

	ok_.RedrawWindow();
}

void CNewTint::OnEnChangeNewTintName()
{
	CString tintName_cstr;
	name_.GetWindowText( tintName_cstr );
 	tintName_ = std::string( tintName_cstr );

	std::string::size_type first = tintName_.find_first_not_of(" ");
	std::string::size_type last = tintName_.find_last_not_of(" ") + 1;
	if (first != std::string::npos)
	{
		tintName_ = tintName_.substr( first, last-first );
	}
	else
	{
		tintName_ = "";
	}
	
	checkComplete();
}

void CNewTint::OnBnClickedNewTintFxCheck()
{
	fxList_.ModifyStyle( WS_DISABLED, 0 );
	fxSel_.ModifyStyle( WS_DISABLED, 0 );
	fxSel_.RedrawWindow();

	mfmList_.ModifyStyle( 0, WS_DISABLED );
	mfmSel_.ModifyStyle( 0, WS_DISABLED );
	mfmSel_.RedrawWindow();

	Options::setOptionInt( "settings/lastNewTintFX", 1 );

	std::vector<std::string> fx;
	MRU::instance().read( "fx", fx );
	if (fx.size() != 0)
		fxFile_ = fx[0];
	else
		fxFile_ = "";
	mfmFile_ = "";

	checkComplete();
}

void CNewTint::OnBnClickedNewTintMfmCheck()
{
	fxList_.ModifyStyle( 0, WS_DISABLED );
	fxSel_.ModifyStyle( 0, WS_DISABLED );
	fxSel_.RedrawWindow();

	mfmList_.ModifyStyle( WS_DISABLED, 0 );
	mfmSel_.ModifyStyle( WS_DISABLED, 0 );
	mfmSel_.RedrawWindow();

	Options::setOptionInt( "settings/lastNewTintFX", 0 );

	std::vector<std::string> mfm;
	MRU::instance().read( "mfm", mfm );
	if (mfm.size() != 0)
		mfmFile_ = mfm[0];
	else
		mfmFile_ = "";
	fxFile_ = "";

	checkComplete();
}

void CNewTint::redrawList( CComboBox& list, const std::string& name )
{
	std::vector<std::string> data;
	MRU::instance().read( name, data );
	list.ResetContent();
	for (unsigned i=0; i<data.size(); i++)
	{
		std::string::size_type first = data[i].rfind("/") + 1;
		std::string::size_type last = data[i].rfind(".");
		std::string dataName = data[i].substr( first, last-first );
		list.InsertString( i, dataName.c_str() );
	}
	list.InsertString( data.size(), L("MODELEDITOR/OTHER") );
	list.SetCurSel( data.size() ? 0 : -1 );

	checkComplete();
}

void CNewTint::OnCbnSelchangeNewTintFxList()
{
	if (fxList_.GetCurSel() == fxList_.GetCount()-1)
	{
		redrawList( fxList_, "fx" );
		OnBnClickedNewTintFxSel();
		return;
	}
	
	std::vector<std::string> fx;
	MRU::instance().read( "fx", fx );
	fxFile_ = fx[ fxList_.GetCurSel() ];
	MRU::instance().update( "fx", fxFile_, true );

	redrawList( fxList_, "fx" );
}

void CNewTint::OnCbnSelchangeNewTintMfmList()
{
	if (mfmList_.GetCurSel() == mfmList_.GetCount()-1)
	{
		redrawList( mfmList_, "mfm" );
		OnBnClickedNewTintMfmSel();
		return;
	}
	
	std::vector<std::string> mfm;
	MRU::instance().read( "mfm", mfm );
	mfmFile_ = mfm[ mfmList_.GetCurSel() ];
	MRU::instance().update( "mfm", mfmFile_, true );

	redrawList( mfmList_, "mfm" );
}

void CNewTint::OnBnClickedNewTintFxSel()
{	
	static char BASED_CODE szFilter[] =	"Effect (*.fx)|*.fx||";
	BWFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter);

	std::string fxDir;
	MRU::instance().getDir("fx", fxDir, s_default_fx );
	fileDlg.m_ofn.lpstrInitialDir = fxDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		fxFile_ = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));

		if (BWResource::validPath( fxFile_ ))
		{
			MRU::instance().update( "fx", fxFile_, true );

			redrawList( fxList_, "fx" );
		}
		else
		{
			MessageBox( L("MODELEDITOR/GUI/NEW_TINT/BAD_DIR_EFFECT"),
				L("MODELEDITOR/GUI/NEW_TINT/UNABLE_RESOLVE_EFFECT"),
				MB_OK | MB_ICONWARNING );
		}
	}
}

void CNewTint::OnBnClickedNewTintMfmSel()
{
	static char BASED_CODE szFilter[] =	"MFM (*.mfm)|*.mfm||";
	BWFileDialog fileDlg (TRUE, "", "", OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter);

	std::string mfmDir;
	MRU::instance().getDir("mfm", mfmDir, s_default_mfm );
	fileDlg.m_ofn.lpstrInitialDir = mfmDir.c_str();

	if ( fileDlg.DoModal() == IDOK )
	{
		mfmFile_ = BWResource::dissolveFilename( std::string( fileDlg.GetPathName() ));

		if (BWResource::validPath( mfmFile_ ))
		{
			MRU::instance().update( "mfm", mfmFile_, true );

			redrawList( mfmList_, "mfm" );
		}
		else
		{
			MessageBox( L("MODELEDITOR/GUI/NEW_TINT/BAD_DIR_MFM"),
				L("MODELEDITOR/GUI/NEW_TINT/UNABLE_RESOLVE_MFM"),
				MB_OK | MB_ICONWARNING );
		}
	}
}

void CNewTint::OnOK()
{
	std::string::size_type first = tintName_.find_first_not_of(" ");
	std::string::size_type last = tintName_.find_last_not_of(" ") + 1;
	if (first != std::string::npos)
	{
		tintName_ = tintName_.substr( first, last-first );
	}
	else
	{
		MessageBox( L("MODELEDITOR/GUI/NEW_TINT/BAD_TINT_NAME"),
			L("MODELEDITOR/GUI/NEW_TINT/INVALID_TINT_NAME"), 
			MB_OK | MB_ICONERROR );

		tintName_ = "";
		name_.SetWindowText( "" );
		checkComplete();
		name_.SetFocus();

		return;
	}
	
	name_.SetWindowText( tintName_.c_str() );
	name_.SetSel( 0, -1 );
		
	if ( std::find( tintNames_.begin(), tintNames_.end(), tintName_ ) != tintNames_.end() )
	{
		MessageBox( L("MODELEDITOR/GUI/NEW_TINT/TINT_ALREADY_EXISTS"),
			L("MODELEDITOR/GUI/NEW_TINT/TINT_EXISTS"), 
			MB_OK | MB_ICONERROR );
			
		name_.SetFocus();

		return;
	}

	if (tintName_ == "Default")
	{
		MessageBox( L("MODELEDITOR/GUI/NEW_TINT/DEFAULT_TINT_RESERVED"), 
			L("MODELEDITOR/GUI/NEW_TINT/DEFAULT_TINT"), 
			MB_OK | MB_ICONERROR );
			
		name_.SetFocus();

		return;
	}

	CDialog::OnOK();
}
