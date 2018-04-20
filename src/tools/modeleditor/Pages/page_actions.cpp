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

#include "python_adapter.hpp"

#include "main_frm.h"
#include "me_shell.hpp"
#include "me_app.hpp"

#include "common/string_utils.hpp"
#include "common/user_messages.hpp"

#include "guimanager/gui_manager.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor.hpp"
#include "guimanager/gui_functor_option.hpp"

#include "controls/user_messages.hpp"
#include "controls/edit_commit.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/range_slider_ctrl.hpp"
#include "controls/slider.hpp"

#include "resmgr/xml_section.hpp"

#include "entitydef/constants.hpp"

#include "undo_redo.hpp"
#include "utilities.hpp"
#include "choose_anim.hpp"
#include "trigger_list.hpp"

#include "page_actions.hpp"

DECLARE_DEBUG_COMPONENT( 0 )

// SliderContainer
SliderContainer::SliderContainer(
	controls::RangeSliderCtrl* slider, controls::EditNumeric* minEd, controls::EditNumeric* maxEd, int group /* = 0 */ )
	: slider_( slider )
	, minEd_( minEd )
	, maxEd_( maxEd )
	, group_( group )
{}

struct PageActionsImpl: public SafeReferenceCount
{
	static PageActions* s_currPage;

	DataSectionPtr defaultCapsData;
	
	std::vector< DataSectionPtr > capsList;

	bool inited;
	bool ready;
	bool updating;
	
	int updateCount;

	int lastLockedParents;

	HTREEITEM lastItem;
	HTREEITEM lastMRU;

	std::string modelName;

	std::vector<SliderContainerPtr> sliders;
	
	CTreeCtrl mru;

	CToolBarCtrl toolbar;
	controls::EditCommit name;
	CEdit anim;
	CButton change_anim;

	controls::EditNumeric blendIn;
	controls::Slider blendInSlider;
	controls::EditNumeric blendOut;
	controls::Slider blendOutSlider;

	CButton looped;
	CButton move;
	CButton impact;
	CButton coord;
	controls::EditCommit track;
	CButton ownTrack;

	bool allowEditUpdates;
		
	CStatic match_box;

	CStatic trigger_box;

	CButton		trigger_falling;
	controls::RangeSliderCtrl trigger_speed_sldr;
	controls::EditNumeric trigger_speed_min;
	controls::EditNumeric trigger_speed_max;
	controls::RangeSliderCtrl trigger_aux1_sldr;
	controls::EditNumeric trigger_aux1_min;
	controls::EditNumeric trigger_aux1_max;
	controls::RangeSliderCtrl trigger_yaw_sldr;
	controls::EditNumeric trigger_yaw_min;
	controls::EditNumeric trigger_yaw_max;
	CEdit		trigger_caps_on;
	CButton		trigger_caps_on_sel;
	CEdit		trigger_caps_off;
	CButton		trigger_caps_off_sel;

	CStatic cancel_box;

	CButton		cancel_falling;
	controls::RangeSliderCtrl cancel_speed_sldr;
	controls::EditNumeric cancel_speed_min;
	controls::EditNumeric cancel_speed_max;
	controls::RangeSliderCtrl cancel_aux1_sldr;
	controls::EditNumeric cancel_aux1_min;
	controls::EditNumeric cancel_aux1_max;
	controls::RangeSliderCtrl cancel_yaw_sldr;
	controls::EditNumeric cancel_yaw_min;
	controls::EditNumeric cancel_yaw_max;
	CEdit		cancel_caps_on;
	CButton		cancel_caps_on_sel;
	CEdit		cancel_caps_off;
	CButton		cancel_caps_off_sel;

	CButton one_shot;
	CButton promote_motion;
	CButton scale_playback_speed;
	CButton feet_follow_direction;
};

PageActions* PageActionsImpl::s_currPage = NULL;

// PageActions

//ID string required for the tearoff tab manager
const std::string PageActions::contentID = "PageActionsID";

IMPLEMENT_DYNCREATE(PageActions, TreeList)

// A helper function to setup the match fields
void PageActions::initMatcherField( controls::EditNumeric& field, int dec /* = 0 */, float min /* = 0.f */, float max /* = 100.f */, bool allowNeg /* = true */, bool allowEmpty /* = true */ )
{
	field.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	field.SetNumDecimals ( dec );
	field.SetMinimum( min, true );
	field.SetMaximum( max, true );
	field.SetAllowNegative( allowNeg );
	field.SetAllowEmpty( allowEmpty );
}

PageActions::PageActions():
	TreeList( PageActions::IDD, MeApp::instance().mutant()->actTree(), "actions" )
{
	pImpl_ = new PageActionsImpl;
	
	pImpl_->inited = false;
	pImpl_->ready = false;
	pImpl_->updating = false;

	pImpl_->updateCount = -1;
	pImpl_->lastLockedParents = -1;

	pImpl_->lastItem = NULL;
	pImpl_->lastMRU = NULL;

	pImpl_->modelName = "";

	pImpl_->track.autoSelect( true );

	pImpl_->allowEditUpdates = true;

	pImpl_->blendIn.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	pImpl_->blendIn.SetMinimum( 0.f );
	pImpl_->blendIn.SetMaximum( 1.f );
	pImpl_->blendIn.SetAllowNegative( false );

	pImpl_->blendOut.SetNumericType( controls::EditNumeric::ENT_FLOAT );
	pImpl_->blendOut.SetMinimum( 0.f );
	pImpl_->blendOut.SetMaximum( 1.f );
	pImpl_->blendOut.SetAllowNegative( false );
	
	initMatcherField( pImpl_->trigger_speed_min, 1, 0, 100.f, true );
	initMatcherField( pImpl_->trigger_speed_max, 1, 0, 100.f, true );
	initMatcherField( pImpl_->trigger_aux1_min, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->trigger_aux1_max, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->trigger_yaw_min, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->trigger_yaw_max, 1, -360.f, 360.f, true );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->trigger_speed_sldr,
		&pImpl_->trigger_speed_min,
		&pImpl_->trigger_speed_max,
		0 ) );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->trigger_aux1_sldr,
		&pImpl_->trigger_aux1_min,
		&pImpl_->trigger_aux1_max,
		0 ) );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->trigger_yaw_sldr,
		&pImpl_->trigger_yaw_min,
		&pImpl_->trigger_yaw_max,
		0 ) );

	initMatcherField( pImpl_->cancel_speed_min, 1, 0, 100.f, true );
	initMatcherField( pImpl_->cancel_speed_max, 1, 0, 100.f, true );
	initMatcherField( pImpl_->cancel_aux1_min, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->cancel_aux1_max, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->cancel_yaw_min, 1, -360.f, 360.f, true );
	initMatcherField( pImpl_->cancel_yaw_max, 1, -360.f, 360.f, true );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->cancel_speed_sldr,
		&pImpl_->cancel_speed_min,
		&pImpl_->cancel_speed_max,
		0 ) );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->cancel_aux1_sldr,
		&pImpl_->cancel_aux1_min,
		&pImpl_->cancel_aux1_max,
		0 ) );

	pImpl_->sliders.push_back( new SliderContainer(
		&pImpl_->cancel_yaw_sldr,
		&pImpl_->cancel_yaw_min,
		&pImpl_->cancel_yaw_max,
		0 ) );

	pImpl_->s_currPage = this;

	DataSectionPtr caps = BWResource::openSection(
		EntityDef::Constants::entitiesCapabilitiesFile() );
	if (caps == NULL)
	{
		WARNING_MSG(" Unable to locate the action matcher capabilities file \"%s\". Using capabilities numbers instead.\n",
			EntityDef::Constants::entitiesCapabilitiesFile() );
		pImpl_->defaultCapsData = static_cast<DataSectionPtr>( new XMLSection("capsList") );

		char buf[32];
		for (int i=0; i<256; i++)
		{
			bw_snprintf( buf, sizeof(buf), "%d", i );
			DataSectionPtr data = pImpl_->defaultCapsData->newSection("state");
			data->setInt( i );
			data->writeString( "name", buf );
			data->writeString( "id", buf );
		}
		pImpl_->defaultCapsData->openSections( "state", pImpl_->capsList );
	}
	else
		caps->openSections( "state", pImpl_->capsList );

	std::vector< DataSectionPtr >::iterator it = pImpl_->capsList.begin();
	std::vector< DataSectionPtr >::iterator end = pImpl_->capsList.end();

	for(;it != end; ++it)
	{
		int id = (*it)->asInt();
		std::string name = (*it)->readString( "name", "" );
		capsNames_[id] = name;
	}
}

PageActions::~PageActions()
{
	clearData();
}

/*static*/ PageActions* PageActions::currPage()
{
	return PageActionsImpl::s_currPage;
}

void PageActions::DoDataExchange(CDataExchange* pDX)
{
	TreeList::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_ACT_MRU, pImpl_->mru);

	DDX_Control(pDX, IDC_ACT_NAME, pImpl_->name);
	DDX_Control(pDX, IDC_ACT_ANIM, pImpl_->anim);
	DDX_Control(pDX, IDC_ACT_CHANGE_ANIM, pImpl_->change_anim);

	DDX_Control(pDX, IDC_ACT_BLEND_IN, pImpl_->blendIn);
	DDX_Control(pDX, IDC_ACT_BLEND_IN_SLIDER, pImpl_->blendInSlider);
	DDX_Control(pDX, IDC_ACT_BLEND_OUT, pImpl_->blendOut);
	DDX_Control(pDX, IDC_ACT_BLEND_OUT_SLIDER, pImpl_->blendOutSlider);

	DDX_Control(pDX, IDC_ACT_LOOP, pImpl_->looped);
	DDX_Control(pDX, IDC_ACT_MOVE, pImpl_->move);
	DDX_Control(pDX, IDC_ACT_IMPACT, pImpl_->impact);
	DDX_Control(pDX, IDC_ACT_COORD, pImpl_->coord);
	DDX_Control(pDX, IDC_ACT_TRACK, pImpl_->track);
	DDX_Control(pDX, IDC_ACT_OWN_TRACK, pImpl_->ownTrack);

	DDX_Control(pDX, IDC_ACT_MATCH_BOX, pImpl_->match_box);

	DDX_Control(pDX, IDC_ACT_TRIGGER_BOX, pImpl_->trigger_box);
	
	DDX_Control(pDX, IDC_ACT_TRIGGER_FALLING, pImpl_->trigger_falling);

	DDX_Control(pDX, IDC_ACT_TRIGGER_SPEED_SLDR, pImpl_->trigger_speed_sldr);
	DDX_Control(pDX, IDC_ACT_TRIGGER_SPEED_MIN, pImpl_->trigger_speed_min);
	DDX_Control(pDX, IDC_ACT_TRIGGER_SPEED_MAX, pImpl_->trigger_speed_max);

	DDX_Control(pDX, IDC_ACT_TRIGGER_AUX1_SLDR, pImpl_->trigger_aux1_sldr);
	DDX_Control(pDX, IDC_ACT_TRIGGER_AUX1_MIN, pImpl_->trigger_aux1_min);
	DDX_Control(pDX, IDC_ACT_TRIGGER_AUX1_MAX, pImpl_->trigger_aux1_max);

	DDX_Control(pDX, IDC_ACT_TRIGGER_YAW_SLDR, pImpl_->trigger_yaw_sldr);
	DDX_Control(pDX, IDC_ACT_TRIGGER_YAW_MIN, pImpl_->trigger_yaw_min);
	DDX_Control(pDX, IDC_ACT_TRIGGER_YAW_MAX, pImpl_->trigger_yaw_max);

	DDX_Control(pDX, IDC_ACT_TRIGGER_CAPS_ON, pImpl_->trigger_caps_on);
	DDX_Control(pDX, IDC_ACT_TRIGGER_CAPS_ON_SEL, pImpl_->trigger_caps_on_sel);

	DDX_Control(pDX, IDC_ACT_TRIGGER_CAPS_OFF, pImpl_->trigger_caps_off);
	DDX_Control(pDX, IDC_ACT_TRIGGER_CAPS_OFF_SEL, pImpl_->trigger_caps_off_sel);

	DDX_Control(pDX, IDC_ACT_CANCEL_BOX, pImpl_->cancel_box);
	
	DDX_Control(pDX, IDC_ACT_CANCEL_FALLING, pImpl_->cancel_falling);

	DDX_Control(pDX, IDC_ACT_CANCEL_SPEED_SLDR, pImpl_->cancel_speed_sldr);
	DDX_Control(pDX, IDC_ACT_CANCEL_SPEED_MIN, pImpl_->cancel_speed_min);
	DDX_Control(pDX, IDC_ACT_CANCEL_SPEED_MAX, pImpl_->cancel_speed_max);

	DDX_Control(pDX, IDC_ACT_CANCEL_AUX1_SLDR, pImpl_->cancel_aux1_sldr);
	DDX_Control(pDX, IDC_ACT_CANCEL_AUX1_MIN, pImpl_->cancel_aux1_min);
	DDX_Control(pDX, IDC_ACT_CANCEL_AUX1_MAX, pImpl_->cancel_aux1_max);

	DDX_Control(pDX, IDC_ACT_CANCEL_YAW_SLDR, pImpl_->cancel_yaw_sldr);
	DDX_Control(pDX, IDC_ACT_CANCEL_YAW_MIN, pImpl_->cancel_yaw_min);
	DDX_Control(pDX, IDC_ACT_CANCEL_YAW_MAX, pImpl_->cancel_yaw_max);

	DDX_Control(pDX, IDC_ACT_CANCEL_CAPS_ON, pImpl_->cancel_caps_on);
	DDX_Control(pDX, IDC_ACT_CANCEL_CAPS_ON_SEL, pImpl_->cancel_caps_on_sel);

	DDX_Control(pDX, IDC_ACT_CANCEL_CAPS_OFF, pImpl_->cancel_caps_off);
	DDX_Control(pDX, IDC_ACT_CANCEL_CAPS_OFF_SEL, pImpl_->cancel_caps_off_sel);

	DDX_Control(pDX, IDC_ACT_ONE_SHOT, pImpl_->one_shot);
	DDX_Control(pDX, IDC_ACT_PROMOTE_MOTION, pImpl_->promote_motion);
	DDX_Control(pDX, IDC_ACT_SCALE_PLAYBACK_SPEED, pImpl_->scale_playback_speed);
	DDX_Control(pDX, IDC_ACT_FEET_FOLLOW_DIRECTION, pImpl_->feet_follow_direction);

	pImpl_->toolbar.Create( CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN |
		TBSTYLE_FLAT | WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CBRS_TOOLTIPS,
		CRect(0,0,0,0), this, 0 );

	GUI::Manager::instance().add( new GUI::Toolbar( "ActToolbar", pImpl_->toolbar ) );

	CWnd toolbarPos;
	DDX_Control(pDX, IDC_ACT_TOOLBAR, toolbarPos);
	
	CRect toolbarRect;
    toolbarPos.GetWindowRect (&toolbarRect);
    ScreenToClient (&toolbarRect);

	pImpl_->toolbar.MoveWindow(toolbarRect);

	pImpl_->inited = true;
}

BOOL PageActions::OnInitDialog()
{
	pImpl_->blendIn.SetMinimum(0); 
	pImpl_->blendIn.SetMaximum(100);

	pImpl_->blendOut.SetMinimum(0); 
	pImpl_->blendOut.SetMaximum(100);
	
	pImpl_->trigger_speed_sldr.setRange( 0, 100.f, 1 );
	pImpl_->trigger_aux1_sldr.setRange( -360.f, 360.f, 1 );
	pImpl_->trigger_yaw_sldr.setRange( -360.f, 360.f, 1 );

	pImpl_->cancel_speed_sldr.setRange( 0, 100.f, 1 );
	pImpl_->cancel_aux1_sldr.setRange( -360.f, 360.f, 1 );
	pImpl_->cancel_yaw_sldr.setRange( -360.f, 360.f, 1 );
		
	INIT_AUTO_TOOLTIP();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

std::string PageActions::getNamedCaps( const std::string& capsStr )
{
	std::vector< std::string > temp;
	StringUtils::vectorFromString( capsStr, temp, ";, " );

	std::string caps = "";

	int val = 0;
	for (unsigned i=0; i<temp.size(); i++)
	{	
		sscanf( temp[i].c_str(), "%d", &val );
		if (i != 0)
		{
			caps += ", ";
		}
		caps += capsNames_[ val ];
	}

	return caps;
}

BEGIN_MESSAGE_MAP(PageActions, TreeList)

	ON_WM_CREATE()

	ON_WM_SIZE()

	ON_WM_CTLCOLOR()

	ON_MESSAGE(WM_UPDATE_CONTROLS, OnUpdateControls)

	ON_WM_HSCROLL()

	ON_NOTIFY(NM_CLICK, IDC_ACT_MRU, OnNMClickMRU)
	
	ON_EN_CHANGE(IDC_ACT_NAME, OnEnChangeActName)
	ON_EN_KILLFOCUS(IDC_ACT_NAME, updateName)

	ON_BN_CLICKED(IDC_ACT_CHANGE_ANIM, OnChangeActAnim)

	ON_BN_CLICKED(IDC_ACT_LOOP, updateFlags)
	ON_BN_CLICKED(IDC_ACT_MOVE, updateFlags)
	ON_BN_CLICKED(IDC_ACT_IMPACT, updateFlags)
	ON_BN_CLICKED(IDC_ACT_COORD, updateFlags)
	
	ON_BN_CLICKED(IDC_ACT_OWN_TRACK, OnBnClickedActOwnTrack)

	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_SPEED_MIN, updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_SPEED_MAX, updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_AUX1_MIN,  updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_AUX1_MAX,  updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_YAW_MIN,   updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_YAW_MAX,   updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_CAPS_ON,   updateTriggerFlags)
	ON_EN_KILLFOCUS(IDC_ACT_TRIGGER_CAPS_OFF,  updateTriggerFlags)

	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_SPEED_MIN, updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_SPEED_MAX, updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_AUX1_MIN,  updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_AUX1_MAX,  updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_YAW_MIN,   updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_YAW_MAX,   updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_CAPS_ON,   updateCancelFlags)
	ON_EN_KILLFOCUS(IDC_ACT_CANCEL_CAPS_OFF,  updateCancelFlags)

	ON_BN_CLICKED(IDC_ACT_ONE_SHOT, updateMatchFlags)
	ON_BN_CLICKED(IDC_ACT_PROMOTE_MOTION, updateMatchFlags)
	ON_BN_CLICKED(IDC_ACT_SCALE_PLAYBACK_SPEED, updateMatchFlags)
	ON_BN_CLICKED(IDC_ACT_FEET_FOLLOW_DIRECTION, updateMatchFlags)
	
	ON_COMMAND_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommand)
	ON_UPDATE_COMMAND_UI_RANGE(GUI_COMMAND_START, GUI_COMMAND_END, OnGUIManagerCommandUpdate)

	ON_BN_CLICKED(IDC_ACT_TRIGGER_CAPS_ON_SEL, OnBnClickedActTriggerCapsOnSel)
	ON_BN_CLICKED(IDC_ACT_TRIGGER_CAPS_OFF_SEL, OnBnClickedActTriggerCapsOffSel)
	ON_BN_CLICKED(IDC_ACT_CANCEL_CAPS_ON_SEL, OnBnClickedActCancelCapsOnSel)
	ON_BN_CLICKED(IDC_ACT_CANCEL_CAPS_OFF_SEL, OnBnClickedActCancelCapsOffSel)

	ON_MESSAGE( WM_RANGESLIDER_CHANGED, OnRangeSliderChanged )
	ON_MESSAGE( WM_RANGESLIDER_TRACK, OnRangeSliderTrack )

	ON_MESSAGE(WM_SHOW_TOOLTIP, OnShowTooltip)
	ON_MESSAGE(WM_HIDE_TOOLTIP, OnHideTooltip)

	ON_BN_CLICKED(IDC_ACT_TRIGGER_FALLING, OnBnClickedActTriggerFalling)
	ON_BN_CLICKED(IDC_ACT_CANCEL_FALLING, OnBnClickedActCancelFalling)
END_MESSAGE_MAP()

void PageActions::clearData()
{
	for ( uint32 i = 0; i < data_.size(); i++ )
	{
		std::string* data = data_[i];
		delete data;
	}
	data_.clear();
}

void PageActions::OnGUIManagerCommand(UINT nID)
{
	pImpl_->s_currPage = this;
	GUI::Manager::instance().act( nID );
}

void PageActions::OnGUIManagerCommandUpdate(CCmdUI * cmdUI)
{
	pImpl_->s_currPage = this;
	if( !cmdUI->m_pMenu )                                                   
		GUI::Manager::instance().update( cmdUI->m_nID );
}

afx_msg LRESULT PageActions::OnShowTooltip(WPARAM wParam, LPARAM lParam)
{
	LPSTR* msg = (LPSTR*)wParam;
	CMainFrame::instance().SetMessageText( *msg );
	return 0;
}

afx_msg LRESULT PageActions::OnHideTooltip(WPARAM wParam, LPARAM lParam)
{
	CMainFrame::instance().SetMessageText( "" );
	return 0;
}

SliderContainerPtr PageActions::find( void* ptr )
{
	for ( std::vector<SliderContainerPtr>::iterator i = pImpl_->sliders.begin();
		i != pImpl_->sliders.end(); ++i )
	{
		if ( (*i)->slider_ == (controls::RangeSliderCtrl*)ptr ||
			(*i)->minEd_ == (controls::EditNumeric*)ptr ||
			(*i)->maxEd_ == (controls::EditNumeric*)ptr )
			return (*i);
	}

	return NULL;
}

void PageActions::writeEditNum( controls::EditNumeric* edit, float num, int group /* = 0 */ )
{
	SliderContainerPtr slider = find( edit );
	if ( !slider )
		return;

	edit->SetValue( num );
}

void PageActions::sliderFromEdits( SliderContainerPtr slider )
{
	if ( !pImpl_->allowEditUpdates )
		return;

	CString str;
	
	float min = slider->minEd_->GetValue();
	float max = slider->maxEd_->GetValue();
	
	float oldmin = 0.f;
	float oldmax = 0.f;
	slider->slider_->getThumbValues( oldmin, oldmax );

	if ( (oldmin != min || oldmax != max) && max < min )
	{
		std::swap(min, max);
		pImpl_->allowEditUpdates = false;
		writeEditNum( slider->minEd_, min, slider->group_ );
		writeEditNum( slider->maxEd_, max, slider->group_ );
		pImpl_->allowEditUpdates = true;
	}

	slider->slider_->setThumbValues( min, max );
}

LRESULT PageActions::OnRangeSliderChanged(WPARAM wParam, LPARAM lParam)
{
	LRESULT result = OnRangeSliderTrack( wParam, lParam );

	bool oneUndoOnly = true;
	updateTriggerFlags( oneUndoOnly, false );
	updateCancelFlags( oneUndoOnly, false );

	return result;
}

LRESULT PageActions::OnRangeSliderTrack(WPARAM wParam, LPARAM lParam)
{
	SliderContainerPtr slider = find( (void*)lParam );

	if ( !slider )
		return 0; // should never do this

	pImpl_->allowEditUpdates = false;

	float min = 0;
	float max = 0;
	slider->slider_->getThumbValues( min, max );

	writeEditNum( slider->minEd_, min, slider->group_ );
	writeEditNum( slider->maxEd_, max, slider->group_ );

	pImpl_->allowEditUpdates = true;

	// This will reload the model when the user releases the mouse button
	// when dragging the slider.  This method will be called a final time
	// on mouse release.
	if (!slider->slider_->sliderHasFocus())
	{
		MeApp::instance().mutant()->reloadModel();
	}

	return 0;
}

// PageActions message handlers

int PageActions::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//We might use this later...
	return 1;
}

void PageActions::OnSize(UINT nType, int cx, int cy)
{
	if (!pImpl_->inited) return;
	
	Utilities::stretchToRight( this, pImpl_->mru, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->name, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->anim, cx, 40 );
	Utilities::moveToRight( this, pImpl_->change_anim, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->blendInSlider, cx, 12 );
	Utilities::stretchToRight( this, pImpl_->blendOutSlider, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->match_box, cx, 0 );
	Utilities::stretchToRight( this, pImpl_->trigger_box, cx, 6 );
	
	Utilities::stretchToRight( this, pImpl_->trigger_speed_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->trigger_speed_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->trigger_aux1_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->trigger_aux1_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->trigger_yaw_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->trigger_yaw_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->trigger_caps_on, cx, 40 );
	Utilities::moveToRight( this, pImpl_->trigger_caps_on_sel, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->trigger_caps_off, cx, 40 );
	Utilities::moveToRight( this, pImpl_->trigger_caps_off_sel, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->cancel_box, cx, 6 );
	
	Utilities::stretchToRight( this, pImpl_->cancel_speed_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->cancel_speed_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->cancel_aux1_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->cancel_aux1_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->cancel_yaw_sldr, cx, 58 );
	Utilities::moveToRight( this, pImpl_->cancel_yaw_max, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->cancel_caps_on, cx, 40 );
	Utilities::moveToRight( this, pImpl_->cancel_caps_on_sel, cx, 12 );

	Utilities::stretchToRight( this, pImpl_->cancel_caps_off, cx, 40 );
	Utilities::moveToRight( this, pImpl_->cancel_caps_off_sel, cx, 12 );

	TreeList::OnSize( nType, cx, cy );
}

void PageActions::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	HWND target = pScrollBar->GetSafeHwnd();
	
	int pos, min, max;
	
	if (target == pImpl_->blendInSlider.GetSafeHwnd())
	{
		pos = pImpl_->blendInSlider.GetPos(); 
		min = pImpl_->blendInSlider.GetRangeMin(); 
		max = pImpl_->blendInSlider.GetRangeMax();

		float blendVal = (float)(1.f * (pos-min) / (max-min)); // Scale 0.0 -> 1.0

		pImpl_->blendIn.SetValue( blendVal );
	}

	if (target == pImpl_->blendOutSlider.GetSafeHwnd())
	{
		pos = pImpl_->blendOutSlider.GetPos(); 
		min = pImpl_->blendOutSlider.GetRangeMin(); 
		max = pImpl_->blendOutSlider.GetRangeMax();

		float blendVal = (float)(1.f * (pos-min) / (max-min)); // Scale 0.0 -> 1.0

		pImpl_->blendOut.SetValue( blendVal );
	}

	TreeList::OnHScroll(nSBCode, nPos, pScrollBar);
}

void PageActions::updateCheck( CButton& button, const StringPair& actID, const std::string& flagName )
{
	bool flag = MeApp::instance().mutant()->actFlag( actID, flagName );
	button.SetCheck( flag ? BST_CHECKED : BST_UNCHECKED );
	button.ModifyStyle( WS_DISABLED, 0 );
}

void PageActions::disableCheck( CButton& button )
{
	button.SetCheck( BST_UNCHECKED );
	button.ModifyStyle( 0, WS_DISABLED );
}

void PageActions::updateMatchField( controls::EditNumeric& field, const StringPair& actID, const std::string& typeName, const std::string& flagName, bool lockedParents /* = false */, bool canBeNeg /* = true */ )
{			
	bool valSet;
	float valFloat = MeApp::instance().mutant()->actMatchFloat( actID, typeName, flagName, valSet );
	if (!canBeNeg)
	{
		valFloat = fabsf( valFloat );
	}
	
	if (valSet)
	{
		field.SetValue( valFloat );
	}
	else
	{
		field.Clear();
	}
	field.SetReadOnly( lockedParents );
	field.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
}

void PageActions::disableField( CEdit& field )
{
	field.SetWindowText( "" );
	field.SetReadOnly( true );
	field.ModifyStyle( 0, WS_DISABLED );
}

#define DISABLE_SLIDER_GROUP( name )						\
	pImpl_->##name##_sldr.ModifyStyle( 0, WS_DISABLED );	\
	pImpl_->##name##_sldr.setThumbValues( 0.f, 0.f );		\
	pImpl_->##name##_min.Clear();							\
	pImpl_->##name##_min.SetReadOnly( true );				\
	pImpl_->##name##_min.ModifyStyle( 0, WS_DISABLED );		\
	pImpl_->##name##_max.Clear();							\
	pImpl_->##name##_max.SetReadOnly( true );				\
	pImpl_->##name##_max.ModifyStyle( 0, WS_DISABLED );

void PageActions::addMRU( const StringPair &action )
{
	std::string modelPath = action.second;
	std::string::size_type first = modelPath.rfind("/") + 1;
	std::string::size_type last = modelPath.rfind(".");
	std::string modelName = modelPath.substr( first, last-first );
	std::string actionDisplay = action.first + " - " + modelName;
	
	int items = 0;
	HTREEITEM hItem = pImpl_->mru.GetChildItem( TVI_ROOT );
	while ( hItem != NULL )
	{
		std::string itemDisplay = pImpl_->mru.GetItemText( hItem );
		if ( actionDisplay == itemDisplay )
		{
			std::string* itemModelPath = (std::string*)(pImpl_->mru.GetItemData(hItem));
			pImpl_->mru.DeleteItem( hItem );
			hItem = pImpl_->mru.InsertItem( actionDisplay.c_str() );
			pImpl_->mru.SetItemData( hItem, (DWORD)itemModelPath );
			pImpl_->mru.SelectItem( hItem );
			return;
		}
		items++;
		hItem = pImpl_->mru.GetNextItem( hItem, TVGN_NEXT );
	}
	// Three items is max, if you get more then delete the oldest one
	if ( items == 3 )
	{
		hItem = pImpl_->mru.GetChildItem( TVI_ROOT );
		std::string* itemModelPath = (std::string*)(pImpl_->mru.GetItemData(hItem));
		std::vector<std::string*>::iterator it = data_.begin();
		std::vector<std::string*>::iterator end = data_.end();
		while ( it != end )
		{
			if ( *it == itemModelPath )
			{
				delete itemModelPath;
				data_.erase( it );
				break;
			}
			++it;
		}
		pImpl_->mru.DeleteItem( hItem );
	}
	if ( items >= 0 )
	{
		hItem = pImpl_->mru.InsertItem( actionDisplay.c_str() );
		std::string* newModelPath = new std::string(modelPath);
		pImpl_->mru.SetItemData( hItem, (DWORD)newModelPath );
		data_.push_back( newModelPath );
	}

	pImpl_->mru.RedrawWindow();
}

afx_msg void PageActions::OnNMClickMRU(NMHDR *pNMHDR, LRESULT *pResult)
{
	//Save the item index
	POINT point;
	::GetCursorPos( &point );
	pImpl_->mru.ScreenToClient(&point);
	HTREEITEM hItem = pImpl_->mru.HitTest( point, 0 );

	if ( hItem == NULL )
		return;

	std::string actionDisplay = pImpl_->mru.GetItemText( hItem );
	std::string actionName = actionDisplay.substr( 0, actionDisplay.rfind( " - " ) );
	std::string model = *(std::string*)(pImpl_->mru.GetItemData( hItem ));

	StringPair action( actionName, model );
	MeApp::instance().mutant()->setAct( action );
}

afx_msg LRESULT PageActions::OnUpdateControls(WPARAM wParam, LPARAM lParam)
{
	pImpl_->updating = pImpl_->updateCount != MeApp::instance().mutant()->updateCount("Act");
	pImpl_->updateCount = MeApp::instance().mutant()->updateCount("Act");
	
	//if (CModelEditorApp::instance()->pythonAdapter())
	//{
		// TODO: We will do stuff here...
	//}

	if (!pImpl_->ready)
	{
		OnInitDialog();
		
		pImpl_->ready = true;
	}
	
	if (MeApp::instance().mutant())
	{
		//Has the model changed?
		std::string modelName = MeApp::instance().mutant()->modelName();
		if (pImpl_->updating || (modelName != pImpl_->modelName))
		{
			OnUpdateTreeList();

			if (modelName == "")	
			{
				pImpl_->toolbar.ModifyStyle( 0, WS_DISABLED );
			}
			else
			{
				pImpl_->toolbar.ModifyStyle( WS_DISABLED, 0 );
			}
			GUI::Manager::instance().update();
			pImpl_->toolbar.RedrawWindow();

			// Ensure that the lock state will be updated
			int lockLod = Options::getOptionInt( "settings/lockLodParents", 0 );
			pImpl_->lastLockedParents = !lockLod;

			pImpl_->modelName = modelName;
			
			HTREEITEM hItem = pImpl_->mru.GetChildItem( TVI_ROOT );
			while ( hItem != NULL )
			{
				std::string* itemModelPath = (std::string*)(pImpl_->mru.GetItemData(hItem));
				delete itemModelPath;
				hItem = pImpl_->mru.GetNextItem( hItem, TVGN_NEXT );
			}
			data_.clear();
			pImpl_->mru.DeleteAllItems();
		}
	}

	// Ensure that the parent lock is enabled
	int lockLod = Options::getOptionInt( "settings/lockLodParents", 0 );
	if ( lockLod != pImpl_->lastLockedParents )
	{
		locked( !!lockLod );
		pImpl_->lastLockedParents = lockLod;
	}

	// This flagged determines if the root item in the action tree is selected.
	// If it is, no action should be played.  Note that an empty string is passed
	// as the action name since an action may have the same name as the model.
	static bool rootItemSelected = true;

	// Check whether the action (or model) selection has changed
	if (pImpl_->updating || (pImpl_->lastItem != selItem()) || (pImpl_->lastMRU != pImpl_->mru.GetSelectedItem()))
	{
		bool hasParent = false;
		bool searchTree = true;
		StringPair selection;
		if ( CWnd::GetFocus()->GetSafeHwnd() == pImpl_->mru.GetSafeHwnd() )
		{
			HTREEITEM hItem = pImpl_->mru.GetSelectedItem();
			if ( hItem )
			{
				hasParent = true;
				searchTree = false;
				std::string actionDisplay = pImpl_->mru.GetItemText( hItem );
				selection.first = actionDisplay.substr( 0, actionDisplay.rfind( " - " ) );
				selection.second = *(std::string*)(pImpl_->mru.GetItemData( hItem ));
			}
		}
		else
		{
			hasParent = tree().GetParentItem(selItem()) != NULL;
			selection = selID();
		}
		if (hasParent && (MeApp::instance().mutant()->hasActs( selection.second )))
		{
			// A child item (an action) is selected
			rootItemSelected = false;
			
			bool lockedParents = locked();
			
			char buf[8];
			
			const std::string& actName = MeApp::instance().mutant()->actName( selection );
			pImpl_->name.SetWindowText( actName.c_str() );
			pImpl_->name.SetReadOnly( lockedParents );
			pImpl_->name.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			const std::string& actAnim = MeApp::instance().mutant()->actAnim( selection );
			pImpl_->anim.SetWindowText( actAnim.c_str() );
			pImpl_->anim.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->change_anim.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			float blendInVal = MeApp::instance().mutant()->actBlendTime( selection, "blendInTime" );
			pImpl_->blendIn.SetValue( blendInVal );
			pImpl_->blendIn.SetReadOnly( lockedParents );
			pImpl_->blendIn.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->blendInSlider.SetPos((int)( 100 * blendInVal ));
			pImpl_->blendInSlider.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			float blendOutVal = MeApp::instance().mutant()->actBlendTime( selection, "blendOutTime" );
			pImpl_->blendOut.SetValue( blendOutVal );
			pImpl_->blendOut.SetReadOnly( lockedParents );
			pImpl_->blendOut.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->blendOutSlider.SetPos((int)( 100 * blendOutVal ));
			pImpl_->blendOutSlider.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			updateCheck( pImpl_->looped, selection, "filler" );
			updateCheck( pImpl_->move,   selection, "isMovement" );
			updateCheck( pImpl_->impact, selection, "isImpacting" );
			updateCheck( pImpl_->coord,  selection, "isCoordinated" );

			pImpl_->looped.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->move.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->impact.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->coord.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			bool ownTrack = false;
			int actTrack = MeApp::instance().mutant()->actTrack( selection );
			if (actTrack != -1)
			{
				bw_snprintf( buf, sizeof(buf), "%d", actTrack );
				pImpl_->track.SetWindowText( buf );
				pImpl_->ownTrack.SetCheck( BST_UNCHECKED );
			}
			else
			{
				ownTrack = true;
				pImpl_->track.SetWindowText( "" );
				pImpl_->ownTrack.SetCheck( BST_CHECKED );
			}
			pImpl_->track.SetReadOnly( ownTrack || lockedParents );
			pImpl_->track.ModifyStyle( ownTrack || lockedParents ? 0 : WS_DISABLED, ownTrack || lockedParents ? WS_DISABLED : 0 );

			pImpl_->ownTrack.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			pImpl_->trigger_falling.ModifyStyle( WS_DISABLED, 0 );
			
			pImpl_->trigger_speed_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->trigger_aux1_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->trigger_yaw_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			
			bool valSet;
			bool falling = MeApp::instance().mutant()->actMatchFloat( selection, "trigger", "minEntitySpeed", valSet ) < 0.f;
			pImpl_->trigger_falling.SetCheck( falling ? BST_CHECKED : BST_UNCHECKED );
			pImpl_->trigger_falling.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			
			updateMatchField( pImpl_->trigger_speed_min, selection, "trigger", "minEntitySpeed",	lockedParents, false );
			updateMatchField( pImpl_->trigger_speed_max, selection, "trigger", "maxEntitySpeed",	lockedParents, false );
			updateMatchField( pImpl_->trigger_aux1_min, selection,  "trigger", "minEntityAux1",	lockedParents );
			updateMatchField( pImpl_->trigger_aux1_max, selection,  "trigger", "maxEntityAux1",	lockedParents );
			updateMatchField( pImpl_->trigger_yaw_min, selection,   "trigger", "minModelYaw",		lockedParents );
			updateMatchField( pImpl_->trigger_yaw_max, selection,   "trigger", "maxModelYaw",		lockedParents );

			pImpl_->trigger_caps_on.SetWindowText( getNamedCaps(
				MeApp::instance().mutant()->actMatchCaps( selection, "trigger", "capsOn" )).c_str());
			pImpl_->trigger_caps_off.SetWindowText( getNamedCaps(
				MeApp::instance().mutant()->actMatchCaps( selection, "trigger", "capsOff" )).c_str());

			pImpl_->trigger_caps_on_sel.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->trigger_caps_off_sel.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			pImpl_->cancel_falling.ModifyStyle( WS_DISABLED, 0 );
			
			pImpl_->cancel_speed_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->cancel_aux1_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->cancel_yaw_sldr.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			
			falling = MeApp::instance().mutant()->actMatchFloat( selection, "cancel", "minEntitySpeed", valSet ) < 0.f;
			pImpl_->cancel_falling.SetCheck( falling ? BST_CHECKED : BST_UNCHECKED );
			pImpl_->cancel_falling.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			
			updateMatchField( pImpl_->cancel_speed_min, selection, "cancel", "minEntitySpeed",	lockedParents, false );
			updateMatchField( pImpl_->cancel_speed_max, selection, "cancel", "maxEntitySpeed",	lockedParents, false );
			updateMatchField( pImpl_->cancel_aux1_min, selection,  "cancel", "minEntityAux1",		lockedParents );
			updateMatchField( pImpl_->cancel_aux1_max, selection,  "cancel", "maxEntityAux1",		lockedParents );
			updateMatchField( pImpl_->cancel_yaw_min, selection,   "cancel", "minModelYaw",		lockedParents );
			updateMatchField( pImpl_->cancel_yaw_max, selection,   "cancel", "maxModelYaw",		lockedParents );

			pImpl_->cancel_caps_on.SetWindowText( getNamedCaps(
				MeApp::instance().mutant()->actMatchCaps( selection, "cancel", "capsOn" )).c_str());
			pImpl_->cancel_caps_off.SetWindowText( getNamedCaps(
				MeApp::instance().mutant()->actMatchCaps( selection, "cancel", "capsOff" )).c_str());

			pImpl_->cancel_caps_on_sel.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->cancel_caps_off_sel.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			//Update all the sliders for the matcher fields
			for (unsigned i=0; i<pImpl_->sliders.size(); i++)
			{
				sliderFromEdits( pImpl_->sliders[i] );
			}

			updateCheck( pImpl_->one_shot,              selection, "match/oneShot" );
			updateCheck( pImpl_->promote_motion,        selection, "match/promoteMotion" );
			updateCheck( pImpl_->scale_playback_speed,  selection, "match/scalePlaybackSpeed" );
			updateCheck( pImpl_->feet_follow_direction, selection, "match/feetFollowDirection" );

			pImpl_->one_shot.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->promote_motion.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->scale_playback_speed.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );
			pImpl_->feet_follow_direction.ModifyStyle( lockedParents ? 0 : WS_DISABLED, lockedParents ? WS_DISABLED : 0 );

			if ( searchTree )
				addMRU( selection );
		}
		else // We are a model...
		{
			// The root item (the model) is selected
			rootItemSelected = true;

			HTREEITEM model = tree().GetParentItem( selItem() );
			if (!model) model = selItem();

			disableField( pImpl_->name);
			pImpl_->name.SetWindowText( L("MODELEDITOR/PAGES/PAGE_ANIMATIONS/MODEL", (LPCTSTR)tree().GetItemText( model )) );

			disableField( pImpl_->anim );
			pImpl_->change_anim.ModifyStyle( 0, WS_DISABLED );

			disableField( pImpl_->blendIn );
			pImpl_->blendInSlider.ModifyStyle( 0, WS_DISABLED );

			disableField( pImpl_->blendOut );
			pImpl_->blendOutSlider.ModifyStyle( 0, WS_DISABLED );
			
			disableCheck( pImpl_->looped );
			disableCheck( pImpl_->move );
			disableCheck( pImpl_->impact );
			disableCheck( pImpl_->coord );

			disableField( pImpl_->track );
			pImpl_->ownTrack.ModifyStyle( 0, WS_DISABLED );

			pImpl_->trigger_falling.ModifyStyle( 0, WS_DISABLED );
			
			DISABLE_SLIDER_GROUP( trigger_speed )
			DISABLE_SLIDER_GROUP( trigger_aux1 )
			DISABLE_SLIDER_GROUP( trigger_yaw )

			pImpl_->trigger_caps_on.SetWindowText("");
			pImpl_->trigger_caps_on_sel.ModifyStyle( 0, WS_DISABLED );
			pImpl_->trigger_caps_off.SetWindowText("");
			pImpl_->trigger_caps_off_sel.ModifyStyle( 0, WS_DISABLED );

			pImpl_->cancel_falling.ModifyStyle( 0, WS_DISABLED );
			
			DISABLE_SLIDER_GROUP( cancel_speed )
			DISABLE_SLIDER_GROUP( cancel_aux1 )
			DISABLE_SLIDER_GROUP( cancel_yaw )

			pImpl_->cancel_caps_on.SetWindowText("");
			pImpl_->cancel_caps_on_sel.ModifyStyle( 0, WS_DISABLED );
			pImpl_->cancel_caps_off.SetWindowText("");
			pImpl_->cancel_caps_off_sel.ModifyStyle( 0, WS_DISABLED );

			disableCheck( pImpl_->one_shot );
			disableCheck( pImpl_->promote_motion );
			disableCheck( pImpl_->scale_playback_speed );
			disableCheck( pImpl_->feet_follow_direction );
		}

		if (!pImpl_->updating)
		{
			if (rootItemSelected)
			{
				StringPair emptyAction( "", selection.second );
				MeApp::instance().mutant()->setAct( emptyAction );
			}
			else
			{
				MeApp::instance().mutant()->setAct( selection );
			}
		}

		GUI::Manager::instance().update();
		this->RedrawWindow();

		pImpl_->lastItem = selItem();
		pImpl_->lastMRU = pImpl_->mru.GetSelectedItem();
	}
	else if ( (pImpl_->lastItem == selItem()) && sameClicked() )
	{
		MeApp::instance().mutant()->setAct( selID() );
		sameClicked( false );
	}
	//Now catch any updates from pressing enter in a field or moving a slider

	if (pImpl_->name.doUpdate())
	{
		updateName();
	}

	if ((pImpl_->blendIn.doUpdate()) || (pImpl_->blendInSlider.doUpdate()))
	{
		float val = pImpl_->blendIn.GetValue();
		pImpl_->blendInSlider.SetPos((int)( 100 * val ));
		MeApp::instance().mutant()->actBlendTime( selID(), "blendInTime", val );
	}

	if ((pImpl_->blendOut.doUpdate()) || (pImpl_->blendOutSlider.doUpdate()))
	{
		float val = pImpl_->blendOut.GetValue();
		pImpl_->blendOutSlider.SetPos((int)( 100 * val ));
		MeApp::instance().mutant()->actBlendTime( selID(), "blendOutTime", val );
	}

	if (pImpl_->track.doUpdate())
	{
		updateTrack();
	}

	if ((pImpl_->trigger_speed_min.doUpdate()) ||
		(pImpl_->trigger_speed_max.doUpdate()) ||
		(pImpl_->trigger_aux1_min.doUpdate()) ||
		(pImpl_->trigger_aux1_max.doUpdate()) ||
		(pImpl_->trigger_yaw_min.doUpdate()) ||
		(pImpl_->trigger_yaw_max.doUpdate()))
	{
		updateTriggerFlags();
	}

	if ((pImpl_->cancel_speed_min.doUpdate()) ||
		(pImpl_->cancel_speed_max.doUpdate()) ||
		(pImpl_->cancel_aux1_min.doUpdate()) ||
		(pImpl_->cancel_aux1_max.doUpdate()) ||
		(pImpl_->cancel_yaw_min.doUpdate()) ||
		(pImpl_->cancel_yaw_max.doUpdate()))
	{
		updateCancelFlags();
	}
	
	return 0;
}

bool PageActions::isLockedAct()
{
	return locked();
}

/*~ function ModelEditor.isActLocked
 *	@components{ modeleditor }
 *
 *	This function returns whether the selected action is locked.
 *	If the 'lock LOD parents' option is enabled in the Preferences dialog, then actions
 *	that belong to the LOD parents of the currently visible LOD level will be locked
 *	from modifications.
 *
 *	@return Returns True (1) if the action is locked, False (0) otherwise.
 */
static PyObject * py_isActLocked( PyObject * args )
{
	if (PageActions::currPage())
		return PyInt_FromLong( PageActions::currPage()->isLockedAct() );
	return PyInt_FromLong( 0 );
}
PY_MODULE_FUNCTION( isActLocked, ModelEditor )

void PageActions::actNew()
{
	CChooseAnim chooseAnimDlg( IDD_CREATE_ACT, true );
	if (chooseAnimDlg.DoModal() == IDOK)
	{
		StringPair actID ( chooseAnimDlg.actName() , selID().second );
		
		selID() = MeApp::instance().mutant()->createAct( actID, chooseAnimDlg.animName(), selID() );

		OnUpdateTreeList();
	}
}

/*~ function ModelEditor.newAct
 *	@components{ modeleditor }
 *
 *	This function enables ModelEditor's Create Action dialog.
 */
static PyObject * py_newAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actNew();

	Py_Return;
}
PY_MODULE_FUNCTION( newAct, ModelEditor )

void PageActions::actPromote()
{
	HTREEITEM nextItem = tree().GetPrevSiblingItem( selItem() );

	if (nextItem == NULL) return;

	std::string actionName = std::string( tree().GetItemText( nextItem ) );

	std::string modelName = selID().second;

	StringPair selNext( actionName, modelName );

	MeApp::instance().mutant()->swapActions( "Promoting", selID(), selNext );

	OnUpdateTreeList();
}

/*~ function ModelEditor.promoteAct
 *	@components{ modeleditor }
 *
 *	This function promotes the currently selected action.
 *	Promoting an action moves it higher up the actions list.
 *	When the action matcher chooses an action to trigger, actions
 *	higher on the actions list will take precedence.
 */
static PyObject * py_promoteAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actPromote();

	Py_Return;
}
PY_MODULE_FUNCTION( promoteAct, ModelEditor )

void PageActions::actDemote()
{
	HTREEITEM prevItem = tree().GetNextSiblingItem( selItem() );

	if (prevItem == NULL) return;

	std::string actionName = std::string( tree().GetItemText( prevItem ) );

	std::string modelName = selID().second;

	StringPair selNext( actionName, modelName );

	MeApp::instance().mutant()->swapActions( "Demoting", selID(), selNext );

	OnUpdateTreeList();
}

/*~ function ModelEditor.demoteAct
 *	@components{ modeleditor }
 *
 *	This function demotes the currently selected action.
 *	Demoting an action moves it lower down the actions list.
 *	When the action matcher chooses an action to trigger, actions
 *	higher on the actions list will take precedence.
 */
static PyObject * py_demoteAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actDemote();

	Py_Return;
}
PY_MODULE_FUNCTION( demoteAct, ModelEditor )

void PageActions::actRemove()
{
	StringPair currSel = selID();

	HTREEITEM nextItem = tree().GetPrevSiblingItem( selItem() );
	
	if (nextItem != NULL) 
	{

		std::string actionName = std::string( tree().GetItemText( nextItem ) );

		std::string modelName = selID().second;

		selID() = StringPair( actionName, modelName );
	}
	
	MeApp::instance().mutant()->removeAct( currSel );

	OnUpdateTreeList();
}

/*~ function ModelEditor.removeAct
 *	@components{ modeleditor }
 *
 *	This function removes the currently selected action from the model.
 */
static PyObject * py_removeAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actRemove();

	Py_Return;
}
PY_MODULE_FUNCTION( removeAct, ModelEditor )

void PageActions::actPlay()
{
	MeApp::instance().mutant()->setAct( selID() );
}

/*~ function ModelEditor.playAct
 *	@components{ modeleditor }
 *
 *	This function forces the currently selected action to play.
 */
static PyObject * py_playAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actPlay();

	Py_Return;
}
PY_MODULE_FUNCTION( playAct, ModelEditor )

void PageActions::actStop()
{
	MeApp::instance().mutant()->stopAct();
}

/*~ function ModelEditor.stopAct
 *	@components{ modeleditor }
 *
 *	This function stops all currently playing actions.
 */
static PyObject * py_stopAct( PyObject * args )
{
	if (PageActions::currPage())
		PageActions::currPage()->actStop();

	Py_Return;
}
PY_MODULE_FUNCTION( stopAct, ModelEditor )

void PageActions::OnEnChangeActName()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//Do nothing if there is no parent item (i.e. we are a model, not an action)
	//This is not necessary since the field should be readonly (but lets be safe)
	if (tree().GetParentItem(selItem()) == NULL)
		return;

	int first, last;
	pImpl_->name.GetSel(first,last);
	CString actName_cstr;
	pImpl_->name.GetWindowText( actName_cstr );
	std::string actName( actName_cstr );
	actName = Utilities::pythonSafeName( actName );
	pImpl_->name.SetWindowText( actName.c_str() );
	tree().SetItemText( selItem(), actName.c_str() );
	pImpl_->name.SetSel(first,last);
	
}

void PageActions::updateName()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//Do nothing if there is no parent item (i.e. we are a model, not an action)
	if (tree().GetParentItem(selItem()) == NULL)
		return;
	
	CString new_name_cstr;
	pImpl_->name.GetWindowText(new_name_cstr);
	std::string new_name = std::string( new_name_cstr );

	if (new_name == "")
	{
		pImpl_->name.SetWindowText( selID().first.c_str() );
		tree().SetItemText( selItem(), selID().first.c_str() );
		
		::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			L("MODELEDITOR/PAGES/PAGE_ACTIONS/NO_RENAME_ACTION"),
			L("MODELEDITOR/PAGES/PAGE_ACTIONS/INVALID_ACTION_NAME"), MB_OK | MB_ICONERROR );
	}
	else if (MeApp::instance().mutant()->actName( selID(), new_name ))
	{
		selID().first = new_name;
	}
	else
	{
		pImpl_->name.SetWindowText( selID().first.c_str() );
		tree().SetItemText( selItem(), selID().first.c_str() );
		
		::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
			L("MODELEDITOR/PAGES/PAGE_ACTIONS/ACTION_NAME_USED"),
			L("MODELEDITOR/PAGES/PAGE_ACTIONS/ACTION_NAME_EXISTS"), MB_OK | MB_ICONERROR );
	}
}

void PageActions::OnChangeActAnim()
{
	CChooseAnim chooseAnimDlg( IDD_CHOOSE_ANIM, false );
	if (chooseAnimDlg.DoModal() == IDOK)
	{
		MeApp::instance().mutant()->actAnim( selID(), chooseAnimDlg.animName() );
		pImpl_->anim.SetWindowText( chooseAnimDlg.animName().c_str() );
	}
}

void PageActions::updateFlags()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	MeApp::instance().mutant()->actFlag( selID(), "filler",        pImpl_->looped.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actFlag( selID(), "isMovement",    pImpl_->move.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actFlag( selID(), "isCoordinated", pImpl_->coord.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actFlag( selID(), "isImpacting",   pImpl_->impact.GetCheck() == BST_CHECKED );

	GUI::Manager::instance().update();

	MeApp::instance().mutant()->reloadModel();
}

void PageActions::updateTrack()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	//First lets get the value entered
	CString trackStr;
	pImpl_->track.GetWindowText( trackStr );

	int trackNum = -1;
	if (strcmp(trackStr,"")) // If something was entered
	{
		sscanf( trackStr, "%d", &trackNum );
	}
	if ( trackNum == -1 )
	{
		pImpl_->ownTrack.SetCheck( BST_CHECKED );
		disableField( pImpl_->track );
	}
	MeApp::instance().mutant()->actTrack( selID(), trackNum );
}

std::string PageActions::getFieldText( CEdit& field )
{
	CString fieldStr;
	field.GetWindowText( fieldStr );
	return std::string(fieldStr);
}

void PageActions::updateTriggerFlags()
{
	bool updateUndo = true;
	updateTriggerFlags( updateUndo, true );
}

void PageActions::updateTriggerFlags( bool & shouldUndo, bool reloadModel /*= false*/ )
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;

	bool falling = pImpl_->trigger_falling.GetCheck() == BST_CHECKED;

	bool hasChanged = false;

	// Clear edit field pairs if either is empty
	if (pImpl_->trigger_speed_min.isEmpty() ||
		pImpl_->trigger_speed_max.isEmpty() ||
		( pImpl_->trigger_speed_min.GetValue() == 0.1f &&
		pImpl_->trigger_speed_max.GetValue() == 0.1f ))
	{
		pImpl_->trigger_speed_min.Clear();
		pImpl_->trigger_speed_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "minEntitySpeed", pImpl_->trigger_speed_min.isEmpty(),
		(falling ? -1.f : 1.f) * pImpl_->trigger_speed_min.GetValue(), shouldUndo  );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "maxEntitySpeed", pImpl_->trigger_speed_max.isEmpty(),
		(falling ? -1.f : 1.f) * pImpl_->trigger_speed_max.GetValue(), shouldUndo  );

	// Clear edit field pairs if either is empty
	if (pImpl_->trigger_aux1_min.isEmpty() ||
		pImpl_->trigger_aux1_max.isEmpty() ||
		( pImpl_->trigger_aux1_min.GetValue() == 0.f &&
		pImpl_->trigger_aux1_max.GetValue() == 0.f ))
	{
		pImpl_->trigger_aux1_min.Clear();
		pImpl_->trigger_aux1_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "minEntityAux1",  pImpl_->trigger_aux1_min.isEmpty(),	pImpl_->trigger_aux1_min.GetValue(), shouldUndo  );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "maxEntityAux1",  pImpl_->trigger_aux1_max.isEmpty(),	pImpl_->trigger_aux1_max.GetValue(), shouldUndo  );

	// Clear edit field pairs if either is empty
	if (pImpl_->trigger_yaw_min.isEmpty() ||
		pImpl_->trigger_yaw_max.isEmpty() ||
		( pImpl_->trigger_yaw_min.GetValue() == 0.f &&
		pImpl_->trigger_yaw_max.GetValue() == 0.f ))
	{
		pImpl_->trigger_yaw_min.Clear();
		pImpl_->trigger_yaw_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "minModelYaw",    pImpl_->trigger_yaw_min.isEmpty(),	pImpl_->trigger_yaw_min.GetValue(), shouldUndo  );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "trigger", "maxModelYaw",    pImpl_->trigger_yaw_max.isEmpty(),	pImpl_->trigger_yaw_max.GetValue(), shouldUndo  );

	//Update all the sliders for the matcher fields
	for (unsigned i=0; i<pImpl_->sliders.size(); i++)
	{
		sliderFromEdits( pImpl_->sliders[i] );
	}

	// We only want to reload the model when the reload model flag is set
	// and changes have been made
	if (reloadModel && hasChanged)
	{
		MeApp::instance().mutant()->reloadModel();
	}

	GUI::Manager::instance().update();
}

void PageActions::updateCancelFlags()
{
	bool updateUndo = true;
	updateCancelFlags( updateUndo, true );
}

void PageActions::updateCancelFlags( bool &shouldUndo, bool reloadModel /*= false*/ )
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;

	bool falling = pImpl_->cancel_falling.GetCheck() == BST_CHECKED;

	bool hasChanged = false;

	// Clear edit field pairs if either is empty
	if (pImpl_->cancel_speed_min.isEmpty() ||
		pImpl_->cancel_speed_max.isEmpty() ||
		( pImpl_->cancel_speed_min.GetValue() == 0.1f &&
		pImpl_->cancel_speed_max.GetValue() == 0.1f ))
	{
		pImpl_->cancel_speed_min.Clear();
		pImpl_->cancel_speed_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "minEntitySpeed", pImpl_->cancel_speed_min.isEmpty(),
		(falling ? -1.f : 1.f) * pImpl_->cancel_speed_min.GetValue(), shouldUndo );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "maxEntitySpeed", pImpl_->cancel_speed_max.isEmpty(),
		(falling ? -1.f : 1.f) * pImpl_->cancel_speed_max.GetValue(), shouldUndo  );

	// Clear edit field pairs if either is empty
	if (pImpl_->cancel_aux1_min.isEmpty() ||
		pImpl_->cancel_aux1_max.isEmpty() ||
		( pImpl_->cancel_aux1_min.GetValue() == 0.f &&
		pImpl_->cancel_aux1_max.GetValue() == 0.f ))
	{
		pImpl_->cancel_aux1_min.Clear();
		pImpl_->cancel_aux1_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "minEntityAux1",  pImpl_->cancel_aux1_min.isEmpty(),
		pImpl_->cancel_aux1_min.GetValue(), shouldUndo  );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "maxEntityAux1",  pImpl_->cancel_aux1_max.isEmpty(),
		pImpl_->cancel_aux1_max.GetValue(), shouldUndo  );

	// Clear edit field pairs if either is empty
	if (pImpl_->cancel_yaw_min.isEmpty() ||
		pImpl_->cancel_yaw_max.isEmpty() ||
		( pImpl_->cancel_yaw_min.GetValue() == 0.f &&
		pImpl_->cancel_yaw_max.GetValue() == 0.f ))
	{
		pImpl_->cancel_yaw_min.Clear();
		pImpl_->cancel_yaw_max.Clear();
	}
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "minModelYaw",    pImpl_->cancel_yaw_min.isEmpty(),
		pImpl_->cancel_yaw_min.GetValue(), shouldUndo  );
	hasChanged |= MeApp::instance().mutant()->actMatchVal( selID(), "cancel", "maxModelYaw",    pImpl_->cancel_yaw_max.isEmpty(),
		pImpl_->cancel_yaw_max.GetValue(), shouldUndo  );

	//Update all the sliders for the matcher fields
	for (unsigned i=0; i<pImpl_->sliders.size(); i++)
	{
		sliderFromEdits( pImpl_->sliders[i] );
	}

	// We only want to reload the model when the reload model flag is set
	// and changes have been made
	if (reloadModel && hasChanged)
	{
		MeApp::instance().mutant()->reloadModel();
	}

	GUI::Manager::instance().update();
}

void PageActions::updateMatchFlags()
{
	//Do nothing if we are currrently updating
	if (pImpl_->updating) return;
	
	MeApp::instance().mutant()->actMatchFlag( selID(), "oneShot",             pImpl_->one_shot.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actMatchFlag( selID(), "promoteMotion",       pImpl_->promote_motion.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actMatchFlag( selID(), "scalePlaybackSpeed",  pImpl_->scale_playback_speed.GetCheck() == BST_CHECKED );
	MeApp::instance().mutant()->actMatchFlag( selID(), "feetFollowDirection", pImpl_->feet_follow_direction.GetCheck() == BST_CHECKED );

	GUI::Manager::instance().update();

	MeApp::instance().mutant()->reloadModel();
}

void PageActions::OnBnClickedActOwnTrack()
{
	bool ownTrack = pImpl_->ownTrack.GetCheck() == BST_CHECKED;
	if ( ownTrack )
	{
		disableField( pImpl_->track );
		MeApp::instance().mutant()->actTrack( selID(), -1 );
	}
	else
	{
		pImpl_->track.ModifyStyle( WS_DISABLED, 0 );
		pImpl_->track.SetReadOnly( false );
		pImpl_->track.SetWindowText( "0" );
		MeApp::instance().mutant()->actTrack( selID(), 0 );
	}
}

void PageActions::OnBnClickedActTriggerCapsOnSel()
{
	std::string capsStr = MeApp::instance().mutant()->actMatchCaps( selID(), "trigger", "capsOn" );
	CTriggerList triggerListDlg( "Action Triggers (On)", pImpl_->capsList, capsStr );
	if (triggerListDlg.DoModal() == IDOK)
	{
		std::string caps = triggerListDlg.caps();
		
		MeApp::instance().mutant()->actMatchCaps( selID(), "trigger", "capsOn", caps );

		pImpl_->trigger_caps_on.SetWindowText( getNamedCaps( caps ).c_str() );
	}
}

void PageActions::OnBnClickedActTriggerCapsOffSel()
{
	std::string capsStr = MeApp::instance().mutant()->actMatchCaps( selID(), "trigger", "capsOff" );
	CTriggerList triggerListDlg( "Action Triggers (Off)", pImpl_->capsList, capsStr );
	if (triggerListDlg.DoModal() == IDOK)
	{
		std::string caps = triggerListDlg.caps();
		
		MeApp::instance().mutant()->actMatchCaps( selID(), "trigger", "capsOff", caps );

		pImpl_->trigger_caps_off.SetWindowText( getNamedCaps( caps ).c_str() );
	}
}

void PageActions::OnBnClickedActCancelCapsOnSel()
{
	std::string capsStr = MeApp::instance().mutant()->actMatchCaps( selID(), "cancel", "capsOn" );
	CTriggerList triggerListDlg( "Action Cancel (On)", pImpl_->capsList, capsStr );
	if (triggerListDlg.DoModal() == IDOK)
	{
		std::string caps = triggerListDlg.caps();
		
		MeApp::instance().mutant()->actMatchCaps( selID(), "cancel", "capsOn", caps );

		pImpl_->cancel_caps_on.SetWindowText( getNamedCaps( caps ).c_str() );
	}
}

void PageActions::OnBnClickedActCancelCapsOffSel()
{
	std::string capsStr = MeApp::instance().mutant()->actMatchCaps( selID(), "cancel", "capsOff" );
	CTriggerList triggerListDlg( "Action Cancel (Off)", pImpl_->capsList, capsStr );
	if (triggerListDlg.DoModal() == IDOK)
	{
		std::string caps = triggerListDlg.caps();
		
		MeApp::instance().mutant()->actMatchCaps( selID(), "cancel", "capsOff", caps );

		pImpl_->cancel_caps_off.SetWindowText( getNamedCaps( caps ).c_str() );
	}
}

void PageActions::OnBnClickedActTriggerFalling()
{
	bool falling = pImpl_->trigger_falling.GetCheck() == BST_CHECKED;

	initMatcherField( pImpl_->trigger_speed_min, 1, 
		falling ? 0.1f : 0.f,
		100.f, false, !falling );

	initMatcherField( pImpl_->trigger_speed_max, 1, 
		falling ? 0.1f : 0.f,
		100.f, false, !falling );

	initMatcherField( pImpl_->trigger_aux1_min, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->trigger_aux1_max, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->trigger_yaw_min, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->trigger_yaw_max, 0, 
		-360.f, 360.f, true, !falling );

	pImpl_->trigger_speed_sldr.setRange(
		falling ? 0.1f : 0.f,
		100.f, 1 );

	pImpl_->trigger_aux1_sldr.setRange(
		-360.f, 360.f, 0 );

	pImpl_->trigger_yaw_sldr.setRange(
		-360.f, 360.f, 0 );

	if (falling) 
	{
		if ((pImpl_->trigger_speed_min.isEmpty()) || (pImpl_->trigger_speed_min.GetValue() == 0.f))
		{
			pImpl_->trigger_speed_min.SetValue( 0.1f );
		}
		if ((pImpl_->trigger_speed_max.isEmpty()) || (pImpl_->trigger_speed_min.GetValue() == 0.f))
		{
			pImpl_->trigger_speed_max.SetValue( 0.1f );
		}
		if ((pImpl_->trigger_aux1_min.isEmpty()) || (pImpl_->trigger_aux1_min.GetValue() == 0.f))
		{
			pImpl_->trigger_aux1_min.SetValue( 0.0f );
		}
		if ((pImpl_->trigger_aux1_max.isEmpty()) || (pImpl_->trigger_aux1_max.GetValue() == 0.f))
		{
			pImpl_->trigger_aux1_max.SetValue( 0.0f );
		}
		if ((pImpl_->trigger_yaw_min.isEmpty()) || (pImpl_->trigger_yaw_min.GetValue() == 0.f))
		{
			pImpl_->trigger_yaw_min.SetValue( 0.0f );
		}
		if ((pImpl_->trigger_yaw_max.isEmpty()) || (pImpl_->trigger_yaw_max.GetValue() == 0.f))
		{
			pImpl_->trigger_yaw_max.SetValue( 0.0f );
		}
	}

	updateTriggerFlags();
}

void PageActions::OnBnClickedActCancelFalling()
{
	bool falling = pImpl_->cancel_falling.GetCheck() == BST_CHECKED;

	initMatcherField( pImpl_->cancel_speed_min, 1, 
		falling ? 0.1f : 0.f,
		100.f, false, !falling );

	initMatcherField( pImpl_->cancel_speed_max, 1, 
		falling ? 0.1f : 0.f,
		100.f, false, !falling );

	initMatcherField( pImpl_->cancel_aux1_min, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->cancel_aux1_max, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->cancel_yaw_min, 0, 
		-360.f, 360.f, true, !falling );

	initMatcherField( pImpl_->cancel_yaw_max, 0, 
		-360.f, 360.f, true, !falling );

	pImpl_->cancel_speed_sldr.setRange(
		falling ? 0.1f : 0.f,
		100.f, 1 );

	pImpl_->cancel_aux1_sldr.setRange(
		-360.f, 360.f, 0 );

	pImpl_->cancel_yaw_sldr.setRange(
		-360.f, 360.f, 0 );

	if (falling) 
	{
		if ((pImpl_->cancel_speed_min.isEmpty()) || (pImpl_->cancel_speed_min.GetValue() == 0.f))
		{
			pImpl_->cancel_speed_min.SetValue( 0.1f );
		}
		if ((pImpl_->cancel_speed_max.isEmpty()) || (pImpl_->cancel_speed_min.GetValue() == 0.f))
		{
			pImpl_->cancel_speed_max.SetValue( 0.1f );
		}
		if ((pImpl_->cancel_aux1_min.isEmpty()) || (pImpl_->cancel_aux1_min.GetValue() == 0.f))
		{
			pImpl_->cancel_aux1_min.SetValue( 0.0f );
		}
		if ((pImpl_->cancel_aux1_max.isEmpty()) || (pImpl_->cancel_aux1_max.GetValue() == 0.f))
		{
			pImpl_->cancel_aux1_max.SetValue( 0.0f );
		}
		if ((pImpl_->cancel_yaw_min.isEmpty()) || (pImpl_->cancel_yaw_min.GetValue() == 0.f))
		{
			pImpl_->cancel_yaw_min.SetValue( 0.0f );
		}
		if ((pImpl_->cancel_yaw_max.isEmpty()) || (pImpl_->cancel_yaw_max.GetValue() == 0.f))
		{
			pImpl_->cancel_yaw_max.SetValue( 0.0f );
		}
	}

	updateCancelFlags();
}

/*afx_msg*/ HBRUSH PageActions::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor ) 
{    
	HBRUSH brush = CFormView::OnCtlColor( pDC, pWnd, nCtlColor ); 

	for ( std::vector<SliderContainerPtr>::iterator i = pImpl_->sliders.begin();
		i != pImpl_->sliders.end(); ++i )
	{
		(*i)->minEd_->SetBoundsColour( pDC, pWnd,
			(*i)->minEd_->GetMinimum(), (*i)->maxEd_->GetMaximum());

		(*i)->maxEd_->SetBoundsColour( pDC, pWnd, 
				(*i)->minEd_->GetMinimum(), (*i)->maxEd_->GetMaximum() );
	}

	return brush; 
} 
