/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PAGE_OPTIONS_HISTOGRAM_HPP
#define PAGE_OPTIONS_HISTOGRAM_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/resource.h"
#include "controls/auto_tooltip.hpp"
#include "controls/slider.hpp"
#include "guitabs/guitabs_content.hpp"
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>


class PageOptionsHistogram : public CDialog, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( L("WORLDEDITOR/GUI/PAGE_OPTIONS_HISTOGRAM/SHORT_NAME"),
		L("WORLDEDITOR/GUI/PAGE_OPTIONS_HISTOGRAM/LONG_NAME"), 290, 250, NULL )

public:
	PageOptionsHistogram();
	virtual ~PageOptionsHistogram();

// Dialog Data
	enum { IDD = IDD_PAGE_OPTIONS_HISTOGRAM };

private:
	bool userAdded_;
	int width_;
	int height_;
	int lumHeight_;
	int rgbLabelPos_;
	int rgbPos_;
	int rgbHeight_;
	int buttonsPos_;
	int redBtnPos_;
	int greenBtnPos_;
	int blueBtnPos_;
	int sliderPos_;
	int sliderWidth_;

protected:
	DECLARE_AUTO_TOOLTIP(PageOptionsHistogram,CDialog);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();

	afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );
	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnDrawItem( int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct );
	CButton mRed;
	CButton mGreen;
	CButton mBlue;
	CStatic mRangeRatio;
	controls::Slider mRangeRatioSlider;
	afx_msg void OnBnClickedRed();
	afx_msg void OnBnClickedGreen();
	afx_msg void OnBnClickedBlue();
	afx_msg void OnNMCustomdrawRangeratioslider(NMHDR *pNMHDR, LRESULT *pResult);
};


IMPLEMENT_CDIALOG_CONTENT_FACTORY( PageOptionsHistogram, PageOptionsHistogram::IDD )


#endif // PAGE_OPTIONS_HISTOGRAM_HPP
