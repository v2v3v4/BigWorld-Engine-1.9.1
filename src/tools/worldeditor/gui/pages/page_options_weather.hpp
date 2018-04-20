/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PAGE_OPTIONS_WEATHER_HPP
#define PAGE_OPTIONS_WEATHER_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "worldeditor/resource.h"
#include "controls/edit_numeric.hpp"
#include "guitabs/guitabs_content.hpp"
#include "resmgr/string_provider.hpp"
#include <afxwin.h>
#include <afxcmn.h>
#include <vector>


class PageOptionsWeather : public CFormView, public GUITABS::Content
{
	IMPLEMENT_BASIC_CONTENT( L("WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/SHORT_NAME"),
		L("WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/LONG_NAME"), 290, 500, NULL )

	struct WatchControl
	{
		CString name_;
		CString displayName_;
		CString toolTip_;
		float minValue_;
		float cur_;
		float default_;
		float maxValue_;
		float sliderStep_;
		float granularity_;
		CStatic* static_;
		controls::EditNumeric* edit_;
//		CSpinButtonCtrl* spin_;
		CSliderCtrl* slider_;
		CStatic* separater_;
	};
	// it is not very efficient to use vector here, but what's efficiency?
	std::vector<WatchControl> WatchControls;
	// this function should be called in OnInitDialog or after
	void addWatch( const CString& name, const CString& displayName, const CString& toolTip, float min, float max, float sliderStep,
		float granularity, bool separaterFollow );
	void addSeparater();
	static float getValue( const CString& name );
	static void setValue( const CString& name, float value );
	void resizeWatch();
	void cleanWatch();
	static const int MAX_WEATHERSETTING_ITEM = 100;
	bool changingWeatherSettings_;
	int separatorNum_;
	HBRUSH bevelBrush_;
	virtual int OnToolHitTest( CPoint point, TOOLINFO* pTI ) const;
public:
	PageOptionsWeather();
	virtual ~PageOptionsWeather();

// Dialog Data
	enum { IDD = IDD_PAGE_OPTIONS_WEATHER };

private:
	bool pageReady_;
	CStatic descText_;
	void updateSliderEdits();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL InitPage();

	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg LRESULT OnUpdateControls(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnWeatherSettingsChange( UINT nID );
	afx_msg void OnWeatherSettingsKillFocus( UINT nID );
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	void setAllWeatherSettings( bool useDefaults );
	afx_msg void OnBnClickedWeatherResetall();
	afx_msg void OnBnClickedWeatherSetdefaults();
};


IMPLEMENT_BASIC_CONTENT_FACTORY( PageOptionsWeather )


#endif // PAGE_OPTIONS_WEATHER_HPP
