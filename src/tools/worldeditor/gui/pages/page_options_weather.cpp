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
#include "worldeditor/gui/pages/page_options_weather.hpp"
#include "worldeditor/framework/world_editor_app.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "appmgr/options.hpp"
#include "romp/time_of_day.hpp"
#include "common/user_messages.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/Watcher.hpp"


DECLARE_DEBUG_COMPONENT2( "WorldEditor", 0 )


class ReturnNotifyEdit : public controls::EditNumeric
{
	DECLARE_DYNAMIC(ReturnNotifyEdit)

protected:
	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL PreTranslateMessage( MSG* pMsg )
	{
		if( pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN )
		{
			( (PageOptionsWeather*)GetParent() )->OnWeatherSettingsKillFocus( GetDlgCtrlID() );
		}
		return 0;
	}
};


IMPLEMENT_DYNAMIC(ReturnNotifyEdit, controls::EditNumeric)


BEGIN_MESSAGE_MAP(ReturnNotifyEdit, controls::EditNumeric)
		ON_WM_CHAR()
END_MESSAGE_MAP()


// PageOptionsWeather


// GUITABS content ID ( declared by the IMPLEMENT_BASIC_CONTENT macro )
const std::string PageOptionsWeather::contentID = "PageOptionsWeather";



PageOptionsWeather::PageOptionsWeather()
	: CFormView(PageOptionsWeather::IDD),
	pageReady_( false )
{
	changingWeatherSettings_ = false;
	separatorNum_ = 0;

	LOGBRUSH brushLog = { BS_SOLID, RGB( 0xd0, 0xd0, 0xbf ), 0, };
	bevelBrush_ = CreateBrushIndirect( &brushLog );
}

PageOptionsWeather::~PageOptionsWeather()
{
	cleanWatch();
}

void PageOptionsWeather::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
}


BOOL PageOptionsWeather::InitPage()
{
	addWatch( "Client Settings/Clouds/wind y", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_VELOCITY",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_VELOCITY_DESC", -320.f, 0.f, 1.f, 1.0, true );

	addWatch( "Client Settings/Weather/CLEAR/propensity", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLEAR_WEIGHT", 
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLEAR_WEIGHT_DESC", 0.0f, 10.f, 0.1f, 0.1f, true );

	addWatch( "Client Settings/Weather/CLOUD/propensity", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_WEIGHTING",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_WEIGHTING_DESC", 0.0f, 10.f, 0.1f, 0.1f, false );
	addWatch( "Client Settings/Weather/CLOUD/arg0", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COVER",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COVER_DESC", 0.0f, 1.f, 0.01f, 0.01f, false );
	addWatch( "Client Settings/Weather/CLOUD/arg1", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COHENSION",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COHENSION_DESC", 0.0f, 1.f, 0.01f, 0.01f, true );

	addWatch( "Client Settings/Weather/RAIN/propensity", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/RAIN_WEIGHTING",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/RAIN_WEIGHTING_DESC", 0.0f, 10.f, 0.1f, 0.1f, false );
	addWatch( "Client Settings/Weather/RAIN/arg1", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COHENSION",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/CLOUD_COHENSION_DESC", 0.0f, 1.f, 0.01f, 0.01f, false );
	addWatch( "Client Settings/Weather/RAIN/arg0", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/DARKNESS",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/DARKNESS_DESC", 0.0f, 1.f, 0.01f, 0.01f, false );
	addWatch( "Client Settings/Rain/area", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/EFFECT_DROP_SIZE",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/EFFECT_DROP_SIZE_DESC", 0.5f, 10.f, 0.1f, 0.1f, true );

	addWatch( "Client Settings/Weather/STORM/propensity", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/STORM_WEIGHTING",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/STORM_WEIGHTING_DESC", 0.0f, 10.f, 0.1f, 0.1f, false );
	addWatch( "Client Settings/Rain/area", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/EFFECT_DROP_SIZE",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/EFFECT_DROP_SIZE_DESC", 0.5f, 10.f, 0.1f, 0.1f, true );

	addWatch( "Client Settings/Weather/windVelX", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/WIND_VELOCITY_X",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/WIND_VELOCITY_X_DESC", -25.f, 25.f, 0.01f, 0.01f, false );
	addWatch( "Client Settings/Weather/windVelY", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/WIND_VELOCITY_Z",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/WIND_VELOCITY_Z_DESC", -25.f, 25.f, 0.01f, 0.01f, false );

	addWatch( "Client Settings/Weather/temperature", "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/TEMPERATURE",
		"`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/TEMPERATURE_DESC", -40.0f, 40.0f, 0.1f, 0.1f, false );

	descText_.Create( "`WORLDEDITOR/GUI/PAGE_OPTIONS_WEATHER/PANEL_DESC",
		WS_CHILD | WS_VISIBLE | SS_NOTIFY, CRect(0,0,10,10), this );
	descText_.SetFont( GetFont() );

	EnableToolTips();
	return true;
}


BEGIN_MESSAGE_MAP(PageOptionsWeather, CFormView)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_WEATHERSETTINGSSTATIC + 1, IDC_WEATHERSETTINGSSTATIC + 1 + MAX_WEATHERSETTING_ITEM * 4, OnWeatherSettingsChange)
	ON_CONTROL_RANGE(EN_KILLFOCUS, IDC_WEATHERSETTINGSSTATIC + 1, IDC_WEATHERSETTINGSSTATIC + 1 + MAX_WEATHERSETTING_ITEM * 4, OnWeatherSettingsKillFocus)
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_WEATHER_RESETALL, OnBnClickedWeatherResetall)
	ON_BN_CLICKED(IDC_WEATHER_SETDEFAULTS, OnBnClickedWeatherSetdefaults)
END_MESSAGE_MAP()


// PageOptionsWeather message handlers

void PageOptionsWeather::addWatch( const CString& name, const CString& displayName, const CString& toolTip, float min, float max, float sliderStep,
	float granularity, bool separaterFollow )
{
	ASSERT( WatchControls.size() < MAX_WEATHERSETTING_ITEM );
	if( WatchControls.size() >= MAX_WEATHERSETTING_ITEM )
		return;

	int id = WatchControls.size()	?	WatchControls.back().slider_->GetDlgCtrlID() + 1
									:	IDC_WEATHERSETTINGSSTATIC + 1;
	CFont* font = GetFont();
	WatchControl control;
	control.static_ = NULL;
	control.edit_ = NULL;
	control.slider_ = NULL;
	control.separater_ = NULL;

	RECT rect = { 0, 0, 10, 10 };

	if( min > max )
	{
		float temp = min;
		min = max;
		max = temp;
	}

	control.cur_ = getValue( name );
	control.default_ = control.cur_;

	if( control.cur_ < min )
		min = control.cur_;
	if( control.cur_ > max )
		max = control.cur_;

	control.name_ = name;
	control.displayName_ = displayName;
	control.toolTip_ = toolTip;
	control.minValue_ = min;
	control.maxValue_ = max;
	control.sliderStep_ = sliderStep;
	control.granularity_ = granularity;

	control.static_ = new CStatic;
	control.static_->Create( displayName, WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY, rect, this );
	control.static_->SetFont( font );

	CString text;
	text.Format( "%g", control.cur_ );
	control.edit_ = new ReturnNotifyEdit;
	control.edit_->Create(
		ES_LEFT | WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER, rect,
		this, id );
	control.edit_->SetFont( font );
	control.edit_->SetWindowText( text );
	++id;

	control.slider_ = new CSliderCtrl;
	control.slider_->Create(
		TBS_HORZ | TBS_NOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect,
		this, id );
	control.slider_->SetRange( BW_ROUND_TO_INT( min / granularity ), BW_ROUND_TO_INT( max / granularity ) );
	control.slider_->SetPos( BW_ROUND_TO_INT( control.cur_ / control.granularity_ ) );

	if( separaterFollow )
	{
		control.separater_ = new CStatic;
		control.separater_->Create( "", WS_CHILD | WS_VISIBLE | SS_SUNKEN, rect, this );
		++separatorNum_;
	}
	else
	{
		control.separater_ = NULL;
	}
	WatchControls.push_back( control );
}

float PageOptionsWeather::getValue( const CString& name )
{
	std::string result;
	Watcher::Mode mode;

	if( !Watcher::rootWatcher().getAsString( NULL, name, result, mode ) )
		return 0.f;// impossible, shall I throw?
	return float( atof( result.c_str() ) );
}

void PageOptionsWeather::setValue( const CString& name, float value )
{
	CString s;
	s.Format( "%g", value );
	Watcher::rootWatcher().setFromString( NULL, name, s );
}

void PageOptionsWeather::resizeWatch()
{
	static const int CONTROL_HEIGHT = 20;
	static const int CONTROL_SPACE = 2;
	static const int STATIC_VMARGIN = 16;
	static const int STATIC_HMARGIN = 6;
	static const int DISPLAYNAME_SIZE = 120;
	static const int EDIT_SIZE = 40;
	static const int SEPARATER_HEIGHT = 16;
	static const int SEPARATER_VMARGIN = 4;

	if ( !pageReady_ )
		return;

	int hmin;
	int hmax;
	GetScrollRange( SB_HORZ, &hmin, &hmax );
	int vmin;
	int vmax;
	GetScrollRange( SB_VERT, &vmin, &vmax );

	CRect rect;
	GetClientRect( &rect );
	rect.OffsetRect( -( GetScrollPos( SB_HORZ ) - hmin ),
		-( GetScrollPos( SB_VERT ) - vmin ) );

	rect.bottom = rect.top + WatchControls.size() * ( CONTROL_HEIGHT + CONTROL_SPACE )
		+ STATIC_VMARGIN * 2 + separatorNum_ * ( SEPARATER_HEIGHT + SEPARATER_VMARGIN * 2 );

	int y = rect.top + CONTROL_SPACE + STATIC_VMARGIN;
	int right = rect.right - CONTROL_SPACE - STATIC_HMARGIN;
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		int x = rect.left + CONTROL_SPACE + STATIC_HMARGIN;
		iter->static_->MoveWindow( x, y, DISPLAYNAME_SIZE, CONTROL_HEIGHT );
		x += DISPLAYNAME_SIZE;
		iter->edit_->MoveWindow( x, y, EDIT_SIZE, CONTROL_HEIGHT );
		x += EDIT_SIZE;
		iter->slider_->MoveWindow( x, y, right - x > 0 ? right - x : 0, CONTROL_HEIGHT );
		if( iter->separater_ )
		{
			y += CONTROL_SPACE / 2 + CONTROL_HEIGHT;
			y += SEPARATER_VMARGIN;
			x = rect.left + CONTROL_SPACE;
			iter->separater_->MoveWindow( x, y, right - x + STATIC_HMARGIN> 0 ? right - x + STATIC_HMARGIN: 0,
				SEPARATER_HEIGHT );
			y += SEPARATER_HEIGHT + SEPARATER_VMARGIN;
			y += CONTROL_SPACE / 2;
		}
		else
			y += CONTROL_SPACE + CONTROL_HEIGHT;
	}

	CWnd* setDefaults = GetDlgItem( IDC_WEATHER_SETDEFAULTS );
	RECT buttonRect;
	setDefaults->GetClientRect( &buttonRect );
	int buttonWidth = buttonRect.right - buttonRect.left;
	int buttonHeight = buttonRect.bottom - buttonRect.top;
	buttonRect.top = y + STATIC_VMARGIN;
	buttonRect.bottom = buttonRect.top + buttonHeight;
	buttonRect.left = ( rect.right - buttonRect.right * 2 ) / 3;
	buttonRect.right = buttonRect.left + buttonWidth;
	setDefaults->MoveWindow( &buttonRect );

	CWnd* resetAll = GetDlgItem( IDC_WEATHER_RESETALL );
	buttonRect.left = buttonRect.right + buttonRect.left;
	buttonRect.right = ( rect.right - buttonRect.left ) * 2;
	resetAll->MoveWindow( &buttonRect );

	buttonRect.left = rect.left + CONTROL_SPACE + STATIC_HMARGIN;
	buttonRect.right = rect.right - CONTROL_SPACE - STATIC_HMARGIN;
	buttonRect.top = buttonRect.bottom + STATIC_VMARGIN;
	buttonRect.bottom = buttonRect.top + 4*CONTROL_HEIGHT;

	descText_.MoveWindow( &buttonRect );
	descText_.RedrawWindow();

}

void PageOptionsWeather::cleanWatch()
{
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		delete iter->static_;
		delete iter->edit_;
		delete iter->slider_;
		delete iter->separater_;
	}
	WatchControls.clear();
	DeleteObject( bevelBrush_ );
}

afx_msg void PageOptionsWeather::OnWeatherSettingsChange( UINT nID )
{
	if( changingWeatherSettings_ )
		return;
	changingWeatherSettings_ = true;
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		if( iter->edit_->GetDlgCtrlID() == nID )
		{
			CString text;
			iter->edit_->GetWindowText( text );
			if( atof( text ) <= iter->maxValue_ && atof( text ) >= iter->minValue_ )
			{
				iter->cur_ = float( atof( text ) );
				iter->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / iter->granularity_ ) );

				setValue( iter->name_, iter->cur_ );

				for( std::vector<WatchControl>::iterator jet = WatchControls.begin();
					jet != WatchControls.end(); ++jet )
				{
					if( jet.operator->() == iter.operator->() )
						continue;
					if( jet->name_ == iter->name_ )
					{
						jet->edit_->SetWindowText( text );
						jet->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / jet->granularity_ ) );
					}
				}
			}
			break;
		}
	}
	changingWeatherSettings_ = false;
}

afx_msg void PageOptionsWeather::OnWeatherSettingsKillFocus( UINT nID )
{
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		if( iter->edit_->GetDlgCtrlID() == nID )
		{
			CString text;
			iter->edit_->GetWindowText( text );
			if( atof( text ) > iter->maxValue_ || atof( text ) < iter->minValue_ )
			{
				iter->cur_ = float( atof( text ) );

				if( iter->cur_ > iter->maxValue_ )
					iter->cur_ = iter->maxValue_;
				if( iter->cur_ < iter->minValue_ )
					iter->cur_ = iter->minValue_;

				text.Format( "%g", iter->cur_ );

				iter->edit_->SetWindowText( text );
				iter->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / iter->granularity_ ) );

				setValue( iter->name_, iter->cur_ );

				for( std::vector<WatchControl>::iterator jet = WatchControls.begin();
					jet != WatchControls.end(); ++jet )
				{
					if( jet.operator->() == iter.operator->() )
						continue;
					if( jet->name_ == iter->name_ )
					{
						jet->edit_->SetWindowText( text );
						jet->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / jet->granularity_ ) );
					}
				}
			}
			else
			{
				text.Format( "%g", float( atof( text ) ) );
				iter->edit_->SetWindowText( text );
			}
		}
	}
}

/**
 *  This is called when each item is about to be drawn.  We want limit slider edit
 *	to be highlighted is they are out of bounds.
 *
 *	@param pDC	Contains a pointer to the display context for the child window.
 *	@param pWnd	Contains a pointer to the control asking for the color.
 *	@param nCtlColor	Specifies the type of control.
 *	@return	A handle to the brush that is to be used for painting the control
 *			background.
 */
afx_msg HBRUSH PageOptionsWeather::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	HBRUSH brush = CFormView::OnCtlColor( pDC, pWnd, nCtlColor );
	
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		if (iter->edit_->SetBoundsColour(
					pDC, pWnd, iter->minValue_, iter->maxValue_ ))
		{
			break;
		}
		if( iter->separater_ && iter->separater_->m_hWnd == pWnd->m_hWnd )
		{
			brush = bevelBrush_;
			pDC->SetBkMode( OPAQUE );
			pDC->SetBkColor( RGB( 0xd0, 0xd0, 0xbf ) );
			break;
		}
	}
	
	return brush;
}

int PageOptionsWeather::OnToolHitTest( CPoint point, TOOLINFO* pTI ) const
{
	for( std::vector<WatchControl>::const_iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		CRect rect;
		iter->static_->GetWindowRect( &rect );
		ScreenToClient( &rect );
		if( rect.PtInRect( point ) )
		{
			pTI->hwnd = m_hWnd;
			pTI->uId = (UINT)iter->static_->m_hWnd;
			pTI->uFlags |= TTF_IDISHWND;
			pTI->lpszText = strdup( iter->toolTip_ );
			return iter->static_->GetDlgCtrlID();
		}
		iter->edit_->GetWindowRect( &rect );
		ScreenToClient( &rect );
		if( rect.PtInRect( point ) )
		{
			pTI->hwnd = m_hWnd;
			pTI->uId = (UINT)iter->edit_->m_hWnd;
			pTI->uFlags |= TTF_IDISHWND;
			pTI->lpszText = strdup( iter->toolTip_ );
			return iter->edit_->GetDlgCtrlID();
		}
		iter->slider_->GetWindowRect( &rect );
		ScreenToClient( &rect );
		if( rect.PtInRect( point ) )
		{
			pTI->hwnd = m_hWnd;
			pTI->uId = (UINT)iter->slider_->m_hWnd;
			pTI->uFlags |= TTF_IDISHWND;
			pTI->lpszText = strdup( iter->toolTip_ );
			return iter->slider_->GetDlgCtrlID();
		}
	}
	return CFormView::OnToolHitTest( point, pTI );
}

afx_msg void PageOptionsWeather::OnSize( UINT nType, int cx, int cy )
{
	CFormView::OnSize( nType, cx, cy );

	resizeWatch();

	SetRedraw();
}

afx_msg LRESULT PageOptionsWeather::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	if ( !IsWindowVisible() )
		return 0;

	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
		float cur = getValue( iter->name_ );
		CString text;
		text.Format( "%g", cur );
		if( iter->cur_ == cur )
			continue;
		iter->cur_ = cur;
		iter->edit_->SetWindowText( text );
		iter->slider_->SetPos( BW_ROUND_TO_INT( cur / iter->granularity_ ) );
	}

	if ( !pageReady_ )
	{
		InitPage();
		pageReady_ = true;
		resizeWatch();
		SetRedraw();
	}

	return 0;
}

void PageOptionsWeather::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if( !changingWeatherSettings_)
	{
		changingWeatherSettings_ = true;
		for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
			iter != WatchControls.end(); ++iter )
		{
			if( pScrollBar && iter->slider_->m_hWnd != pScrollBar->m_hWnd )
				continue;
			iter->cur_ = iter->slider_->GetPos() * iter->granularity_;
			CString text;
			text.Format( "%g", iter->cur_ );
			iter->edit_->SetWindowText( text );
			setValue( iter->name_, iter->cur_ );

			for( std::vector<WatchControl>::iterator jet = WatchControls.begin();
				jet != WatchControls.end(); ++jet )
			{
				if( jet.operator->() == iter.operator->() )
					continue;
				if( jet->name_ == iter->name_ )
				{
					jet->edit_->SetWindowText( text );
					jet->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / jet->granularity_ ) );
				}
			}
		}
		changingWeatherSettings_ = false;
	}

	CFormView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageOptionsWeather::setAllWeatherSettings( bool useDefaults )
{
	for( std::vector<WatchControl>::iterator iter = WatchControls.begin();
		iter != WatchControls.end(); ++iter )
	{
			if (useDefaults)
				iter->cur_ = iter->default_;
			else if (iter->minValue_ >= 0)
				iter->cur_ = iter->minValue_;
			else
				iter->cur_ = iter->maxValue_;

		CString text;
		text.Format( "%g", iter->cur_ );

		iter->edit_->SetWindowText( text );
		iter->slider_->SetPos( BW_ROUND_TO_INT( iter->cur_ / iter->granularity_ ) );
	}
}

void PageOptionsWeather::OnBnClickedWeatherResetall()
{
	//This will reset all the settings to zero.
	setAllWeatherSettings( false );
	setValue( "Client Settings/Clouds/wind y", -20000.f );
	WorldManager::instance().refreshWeather();
	setValue( "Client Settings/Clouds/wind y", 0.f );
}

void PageOptionsWeather::OnBnClickedWeatherSetdefaults()
{
	//This will reset all the settings to the defaults.
	setAllWeatherSettings( true );
}
