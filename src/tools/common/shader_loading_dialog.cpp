/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

// LoadingDialog.cpp : implementation file
//
#include "pch.hpp"

#include "shader_loading_dialog.hpp"


// Default constructor.
CShaderLoadingDialog::CShaderLoadingDialog()
	: CDialog(CShaderLoadingDialog::IDD)
{
	Create( IDD_SHADER_LOADING );
}

// Destructor.
CShaderLoadingDialog::~CShaderLoadingDialog()
{
	DestroyWindow();
}

// Setup data exchange with progress bar.
void CShaderLoadingDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SHADER_PROGRESS_BAR, bar_);
}

// On initialisation centre the progress bar.
BOOL CShaderLoadingDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	this->CenterWindow();

	return TRUE;
}

// Set the range of the progress bar.
void CShaderLoadingDialog::setRange( int num )
{
	bar_.SetRange( 0, num );
	bar_.SetStep( 1 );
}

// Step the progress bar on a unit.
void CShaderLoadingDialog::step()
{
	bar_.StepIt();
}


BEGIN_MESSAGE_MAP(CShaderLoadingDialog, CDialog)
END_MESSAGE_MAP()
