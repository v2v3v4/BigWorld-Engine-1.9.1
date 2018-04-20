/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once


// UalNameDlg dialog

class UalNameDlg : public CDialog
{
public:
	UalNameDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~UalNameDlg();

// Dialog Data
	enum { IDD = IDD_UALNAME };

	void getNames( std::string& shortName, std::string& longName );
	void setNames( const std::string& shortName, const std::string& longName );

protected:
	CString longName_;
	CString shortName_;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	void OnOK();

	DECLARE_MESSAGE_MAP()
};
