/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


#ifndef SEARCH_EDIT_HPP
#define SEARCH_EDIT_HPP

class SearchEdit : public CEdit
{
public:
	SearchEdit();
	void setIdleText( const std::string& idleText );

private:
	std::string idleText_;

	bool idle();

	afx_msg void OnPaint();
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	afx_msg void OnKillFocus( CWnd* pNewWnd );
	DECLARE_MESSAGE_MAP()
};

#endif // SEARCH_EDIT_HPP