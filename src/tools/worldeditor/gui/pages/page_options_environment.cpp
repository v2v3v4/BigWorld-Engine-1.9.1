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
#include "worldeditor/gui/pages/page_options_environment.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/framework/world_editor_app.hpp"
#include "worldeditor/scripting/we_python_adapter.hpp"
#include "controls/user_messages.hpp"
#include "appmgr/app.hpp"
#include "appmgr/options.hpp"
#include "gizmo/undoredo.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/auto_config.hpp"
#include "romp/enviro_minder.hpp"
#include "romp/sky_gradient_dome.hpp"
#include "romp/time_of_day.hpp"
#include "moo/visual_manager.hpp"
#include "terrain/terrain_settings.hpp"
#include "common/format.hpp"
#include "common/string_utils.hpp"
#include "common/user_messages.hpp"
#include "common/file_dialog.hpp"
#include <afxpriv.h>


using namespace std;
using namespace controls;


DECLARE_DEBUG_COMPONENT2("WorldEditor", 0)


BEGIN_MESSAGE_MAP(PageOptionsEnvironment, CFormView)
    ON_WM_HSCROLL()
	ON_WM_CTLCOLOR()
    ON_MESSAGE  (WM_UPDATE_CONTROLS     , OnUpdateControls  )
    ON_MESSAGE  (WM_NEW_SPACE           , OnNewSpace        )
    ON_MESSAGE  (WM_BEGIN_SAVE          , OnBeginSave       )
    ON_MESSAGE  (WM_END_SAVE            , OnEndSave         )
    ON_COMMAND  (IDC_SKYFILE_BTN        , OnBrowseSkyFile   )
    ON_COMMAND  (IDC_NEWSKYFILE_BTN     , OnCopySkyFile     )
    ON_COMMAND  (IDC_TODFILE_BTN        , OnBrowseTODFile   )
    ON_COMMAND  (IDC_NEWTODFILE_BTN     , OnCopyTODFile     )
    ON_COMMAND  (IDC_ADDSKYDOME_BTN     , OnAddSkyDome      )
    ON_COMMAND  (IDC_CLEARSKYDOME_BTN   , OnClearSkyDomes   )
    ON_COMMAND  (IDC_SB_GRAD_BTN        , OnBrowseSkyGradBtn)
    ON_EN_CHANGE(IDC_HOURLENGTH         , OnHourLengthEdit  )
    ON_EN_CHANGE(IDC_STARTTIME          , OnStartTimeEdit   )
    ON_MESSAGE  (WM_CT_SEL_TIME         , OnCTSelTime       )
    ON_EN_CHANGE(IDC_SUNANGLE_EDIT      , OnSunAngleEdit    )
    ON_EN_CHANGE(IDC_MOONANGLE_EDIT     , OnMoonAngleEdit   )
    ON_MESSAGE  (WM_CT_UPDATE_BEGIN     , OnTimelineBegin   )
    ON_MESSAGE  (WM_CT_UPDATE_MIDDLE    , OnTimelineMiddle  )
    ON_MESSAGE  (WM_CT_UPDATE_DONE      , OnTimelineDone    )
    ON_MESSAGE  (WM_CT_ADDED_COLOR      , OnTimelineAdd     )
    ON_MESSAGE  (WM_CT_NEW_SELECTION    , OnTimelineNewSel  )
    ON_MESSAGE  (WM_CP_LBUTTONDOWN      , OnPickerDown      )
    ON_MESSAGE  (WM_CP_LBUTTONMOVE      , OnPickerMove      )
    ON_MESSAGE  (WM_CP_LBUTTONUP        , OnPickerUp        )
    ON_COMMAND  (IDC_SUNANIM_BTN        , OnSunAnimBtn      )
    ON_COMMAND  (IDC_AMBANIM_BTN        , OnAmbAnimBtn      )
    ON_COMMAND  (IDC_CREATEANIM_BTN     , OnResetBtn        )
    ON_COMMAND  (IDC_ADDCOLOR_BTN       , OnAddClrBtn       )
    ON_COMMAND  (IDC_DELCOLOR_BTN       , OnDelClrBtn       )
    ON_EN_CHANGE(IDC_R_EDIT             , OnEditClrText     )
    ON_EN_CHANGE(IDC_G_EDIT             , OnEditClrText     )
    ON_EN_CHANGE(IDC_B_EDIT             , OnEditClrText     )
    ON_EN_CHANGE(IDC_MIEAMOUNT          , OnEditEnvironText )  
    ON_EN_CHANGE(IDC_TURBOFFS           , OnEditEnvironText )
    ON_EN_CHANGE(IDC_TURBFACTOR         , OnEditEnvironText )
    ON_EN_CHANGE(IDC_VERTHEIGHTEFFECT   , OnEditEnvironText )
    ON_EN_CHANGE(IDC_SUNHEIGHTEFFECT    , OnEditEnvironText )
    ON_EN_CHANGE(IDC_POWER              , OnEditEnvironText )
	ON_EN_CHANGE(IDC_TEXLOD_START_EDIT  , OnTexLodEdit      )
	ON_EN_CHANGE(IDC_TEXLOD_DIST_EDIT   , OnTexLodEdit      )
	ON_EN_CHANGE(IDC_TEXLOD_PRELOAD_EDIT, OnTexLodEdit      )
    ON_EN_KILLFOCUS(IDC_SUNANGLE_EDIT      , OnSunAngleEditKillFocus		)
    ON_EN_KILLFOCUS(IDC_MOONANGLE_EDIT     , OnMoonAngleEditKillFocus		)
    ON_EN_KILLFOCUS(IDC_MIEAMOUNT          , OnMieAmountEditKillFocus		)  
    ON_EN_KILLFOCUS(IDC_TURBOFFS           , OnTurbOffEditKillFocus			)
    ON_EN_KILLFOCUS(IDC_TURBFACTOR         , OnTurbFactEditKillFocus		)
    ON_EN_KILLFOCUS(IDC_VERTHEIGHTEFFECT   , OnVertEffEditKillFocus			)
    ON_EN_KILLFOCUS(IDC_SUNHEIGHTEFFECT    , OnSunEffEditKillFocus			)
    ON_EN_KILLFOCUS(IDC_POWER              , OnPowerEditKillFocus			)
	ON_EN_KILLFOCUS(IDC_TEXLOD_START_EDIT  , OnTexLodStartEditKillFocus		)
	ON_EN_KILLFOCUS(IDC_TEXLOD_DIST_EDIT   , OnTexLodBlendEditKillFocus		)
	ON_EN_KILLFOCUS(IDC_TEXLOD_PRELOAD_EDIT, OnTexLodPreloadEditKillFocus	)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
    ON_UPDATE_COMMAND_UI(IDC_SKYDOME_UP  , OnSkyboxUpEnable  )
    ON_UPDATE_COMMAND_UI(IDC_SKYDOME_DOWN, OnSkyboxDownEnable)
    ON_UPDATE_COMMAND_UI(IDC_SKYDOME_DEL , OnSkyboxDelEnable )
    ON_COMMAND(IDC_SKYDOME_UP  , OnSkyboxUp  )
    ON_COMMAND(IDC_SKYDOME_DOWN, OnSkyboxDown)
    ON_COMMAND(IDC_SKYDOME_DEL , OnSkyboxDel )
END_MESSAGE_MAP()


const std::string PageOptionsEnvironment::contentID = "PageOptionsEnvironment";


/*static*/ PageOptionsEnvironment *PageOptionsEnvironment::s_instance_ = NULL;


namespace
{
    const float SLIDER_PREC = 100.0f;   // conversion from slider to float
	const float LOD_EPSILON	=   0.1f;	// minimum precision required for a texture lod change

    // This class is for environment undo/redo operations.
    class EnvironmentUndo : public UndoRedo::Operation
    {
    public:
        EnvironmentUndo();

        /*virtual*/ void undo();

        /*virtual*/ bool iseq(UndoRedo::Operation const &other) const;

    private:
        XMLSectionPtr       ds_;
    };

    EnvironmentUndo::EnvironmentUndo():
		UndoRedo::Operation((int)(typeid(EnvironmentUndo).name())),
		ds_(new XMLSection("Environment undo/redo"))
    {
        PageOptionsEnvironment *poe = PageOptionsEnvironment::instance();

        EnviroMinder &em = PageOptionsEnvironment::getEnviroMinder();
        em.save(ds_, false); // save everything internally

        // Explicitly save and restore the game time:
        float gameTime = em.timeOfDay()->gameTime();
        ds_->writeFloat("gametime", gameTime);

        // Explicity save the game speed:
        float gameSpeed = WorldManager::instance().secondsPerHour();
        ds_->writeFloat("gamespeed", gameSpeed);

        // Explicitly store any selection in the colour timeline and game 
        // seconds per hour:
        if (poe != NULL)
        {
            float selTime = poe->getSelTime();
            ds_->writeFloat("seltime", selTime);            
        }
    }


    /*virtual*/ void EnvironmentUndo::undo()
    {
        // Save the current state to the undo/redo stack:
        UndoRedo::instance().add(new EnvironmentUndo());

        // Do the actual undo:
        EnviroMinder &em = PageOptionsEnvironment::getEnviroMinder();
        em.load(ds_, false); // load everything internally

        // Restore the game time:
        float gameTime = ds_->readFloat("gametime", -1.0f);
        if (gameTime != -1.0f)
            em.timeOfDay()->gameTime(gameTime);

        // Restore the game speed:
        float gameSpeed = ds_->readFloat("gamespeed", 0.0f);
        WorldManager::instance().secondsPerHour(gameSpeed);

        // Update the PageOptionsEnvironment page:
        PageOptionsEnvironment *poe = PageOptionsEnvironment::instance();
        if (poe != NULL)
        {
            poe->reinitialise();
            float selTime = ds_->readFloat("seltime", -1.0f);
            poe->setSelTime(selTime);
        }
    }

    /*virtual*/ 
    bool 
    EnvironmentUndo::iseq
    (
        UndoRedo::Operation         const &other
    ) const
    {
        return false;
    }


	bool wasSlider(CWnd const &testScrollBar, CWnd const &scrollBar)
	{
		return 
			testScrollBar.GetSafeHwnd() == scrollBar.GetSafeHwnd()
			&&
			::IsWindow(scrollBar.GetSafeHwnd()) != FALSE;
	}

    // This class is for terrain lod undo/redo operations.
    class TerrainLodUndo : public UndoRedo::Operation
	{
    public:
        TerrainLodUndo();

        /*virtual*/ void undo();

        /*virtual*/ bool iseq(UndoRedo::Operation const &other) const;

    private:
        float			lodTextureStart_;
		float			lodTextureDistance_;
		float			blendPreloadDistance_;
	};

	TerrainLodUndo::TerrainLodUndo():
		UndoRedo::Operation((int)(typeid(TerrainLodUndo).name())),
		lodTextureStart_     (WorldManager::instance().pTerrainSettings()->lodTextureStart     ()),
		lodTextureDistance_  (WorldManager::instance().pTerrainSettings()->lodTextureDistance  ()),
		blendPreloadDistance_(WorldManager::instance().pTerrainSettings()->blendPreloadDistance())
	{
	}

	void TerrainLodUndo::undo()
	{
        // Save the current state to the undo/redo stack:
        UndoRedo::instance().add(new TerrainLodUndo());

		// Restore the saved texture lod values:
		Terrain::TerrainSettingsPtr pTerrainSettings = WorldManager::instance().pTerrainSettings();

		pTerrainSettings->lodTextureStart     (lodTextureStart_     );
		pTerrainSettings->lodTextureDistance  (lodTextureDistance_  );
		pTerrainSettings->blendPreloadDistance(blendPreloadDistance_);

		// Force an update of the page if it exists:
        PageOptionsEnvironment *poe = PageOptionsEnvironment::instance();
        if (poe != NULL)
        {
            poe->reinitialise();
		}
	}

    /*virtual*/ bool 
    TerrainLodUndo::iseq
    (
        UndoRedo::Operation         const &other
    ) const
    {
        return false;
    }
}


PageOptionsEnvironment::PageOptionsEnvironment() :
    CFormView(IDD),
    colourTimeline_(NULL),
    colourPicker_(NULL),
    initialised_(false),
    filterChange_(0),
    mode_(MODE_SUN),
    initialValue_(0.0f),
    sliding_(false)
{
    s_instance_ = this;
}


/*virtual*/ PageOptionsEnvironment::~PageOptionsEnvironment()
{
    s_instance_ = NULL;
	
	delete colourTimeline_;
	colourTimeline_ = NULL;

	delete colourPicker_;
	colourPicker_ = NULL;
}


/*static*/ PageOptionsEnvironment *PageOptionsEnvironment::instance()
{
    return s_instance_;
}


void PageOptionsEnvironment::reinitialise()
{
    ++filterChange_;

    CDataExchange ddx(this, FALSE);
    DoDataExchange(&ddx);

    EnviroMinder    &enviroMinder   = getEnviroMinder();
    TimeOfDay       &tod            = getTimeOfDay();
    SkyGradientDome *skd            = enviroMinder.skyGradientDome();

    skyFileEdit_.SetWindowText(enviroMinder.skyGradientDomeFile().c_str());
    todFileEdit_.SetWindowText(enviroMinder.timeOfDayFile().c_str());

    rebuildSkydomeList();

    if (skd != NULL)
        skyBoxGradEdit_.SetWindowText(skd->textureName().c_str());

    hourLength_.SetValue(WorldManager::instance().secondsPerHour());

    float stime = tod.startTime();
    startTime_.SetValue(stime);

    float sunAngle = tod.sunAngle();
    sunAngleSlider_.setDigits( 2 );
    sunAngleSlider_.setRangeLimit( 0, 90.0f );
    sunAngleSlider_.setRange( 0, 90.0f );
    sunAngleSlider_.setValue( sunAngle );
    sunAngleEdit_.SetNumDecimals( 2 );
    sunAngleEdit_.SetValue( sunAngle );

    float moonAngle = tod.moonAngle();
    moonAngleSlider_.setDigits( 2 );
    moonAngleSlider_.setRangeLimit( 0, 90.0f );
    moonAngleSlider_.setRange( 0, 90.0f );
    moonAngleSlider_.setValue( moonAngle );
    moonAngleEdit_.SetNumDecimals( 2 );
    moonAngleEdit_.SetValue( moonAngle );

    float gameTime = tod.gameTime();
    timeOfDaySlider_.SetRange(0, BW_ROUND_TO_INT(SLIDER_PREC*24.0f));
    timeOfDaySlider_.SetPos(BW_ROUND_TO_INT(SLIDER_PREC*gameTime));
    string gameTimeStr = tod.getTimeOfDayAsString();
    timeOfDayEdit_.SetWindowText(gameTimeStr.c_str());

    if (skd != NULL)
    {
		mieSlider_.setDigits( 2 );
		mieSlider_.setRangeLimit( 0, 1.0f );
		mieSlider_.setRange( 0, 1.0f );
		mieSlider_.setValue( skd->mieEffect() );
		mieEdit_.SetNumDecimals( 2 );
		mieEdit_.SetValue( skd->mieEffect() );

		turbOffsSlider_.setDigits( 2 );
		turbOffsSlider_.setRangeLimit( 0, 1.0f );
		turbOffsSlider_.setRange( 0, 1.0f );
		turbOffsSlider_.setValue( skd->turbidityOffset() );
		turbOffsEdit_.SetNumDecimals( 2 );
		turbOffsEdit_.SetValue( skd->turbidityOffset() );

		turbFactorSlider_.setDigits( 2 );
		turbFactorSlider_.setRangeLimit( 0.1f, 1.0f );
		turbFactorSlider_.setRange( 0.1f, 1.0f );
		turbFactorSlider_.setValue( skd->turbidityFactor() );
		turbFactorEdit_.SetNumDecimals( 2 );
		turbFactorEdit_.SetValue( skd->turbidityFactor() );

		vertexHeightEffectSlider_.setDigits( 2 );
		vertexHeightEffectSlider_.setRangeLimit( 0, 2.0f );
		vertexHeightEffectSlider_.setRange( 0, 2.0f );
		vertexHeightEffectSlider_.setValue( skd->vertexHeightEffect() );
		vertexHeightEffectEdit_.SetNumDecimals( 2 );
		vertexHeightEffectEdit_.SetValue( skd->vertexHeightEffect() );

		sunHeightEffectSlider_.setDigits( 2 );
		sunHeightEffectSlider_.setRangeLimit( 0, 2.0f );
		sunHeightEffectSlider_.setRange( 0, 2.0f );
		sunHeightEffectSlider_.setValue( skd->sunHeightEffect() );
		sunHeightEffectEdit_.SetNumDecimals( 2 );
		sunHeightEffectEdit_.SetValue( skd->sunHeightEffect() );

		powerSlider_.setDigits( 2 );
		powerSlider_.setRangeLimit( 1.0f, 32.0f );
		powerSlider_.setRange( 1.0f, 32.0f );
		powerSlider_.setValue( skd->power() );
		powerEdit_.SetNumDecimals( 2 );
		powerEdit_.SetValue( skd->power() );
    }

	texLodStartEdit_  .SetAllowNegative(false);
	texLodDistEdit_   .SetAllowNegative(false);
	texLodPreloadEdit_.SetAllowNegative(false);

	texLodStartEdit_  .SetNumDecimals(1);
	texLodDistEdit_   .SetNumDecimals(1);
	texLodPreloadEdit_.SetNumDecimals(1);

	texLodStartSlider_.setDigits( 1 );
	texLodStartSlider_.setRangeLimit( 0.0f, 5000.0f );
	texLodStartSlider_.setRange( 0.0f, 5000.0f );

	texLodDistSlider_.setDigits( 1 );
	texLodDistSlider_.setRangeLimit( 0.0f, 5000.0f );
	texLodDistSlider_.setRange( 0.0f, 5000.0f );
	
	texLodPreloadSlider_.setDigits( 1 );
	texLodPreloadSlider_.setRangeLimit( 0.0f, 5000.0f );
	texLodPreloadSlider_.setRange( 0.0f, 5000.0f );

	Terrain::TerrainSettingsPtr pTerrainSettings = WorldManager::instance().pTerrainSettings();

	if (pTerrainSettings->version() >= 200)
	{
		texLodStartEdit_    .EnableWindow(TRUE);
		texLodStartSlider_  .EnableWindow(TRUE);
		texLodDistEdit_     .EnableWindow(TRUE);
		texLodDistSlider_   .EnableWindow(TRUE);
		texLodPreloadEdit_  .EnableWindow(TRUE);
		texLodPreloadSlider_.EnableWindow(TRUE);

		float texLodStart	= pTerrainSettings->lodTextureStart();
		float texLodDist	= pTerrainSettings->lodTextureDistance();
		float texLodPreload	= pTerrainSettings->blendPreloadDistance();

		texLodStartEdit_    .SetValue(texLodStart);
		texLodStartSlider_  .setValue(texLodStart);
		texLodDistEdit_     .SetValue(texLodDist);
		texLodDistSlider_   .setValue(texLodDist);
		texLodPreloadEdit_  .SetValue(texLodPreload);
		texLodPreloadSlider_.setValue(texLodPreload);
	}
	else
	{
		texLodStartEdit_    .EnableWindow(FALSE);
		texLodStartSlider_  .EnableWindow(FALSE);
		texLodDistEdit_     .EnableWindow(FALSE);
		texLodDistSlider_   .EnableWindow(FALSE);
		texLodPreloadEdit_  .EnableWindow(FALSE);
		texLodPreloadSlider_.EnableWindow(FALSE);
	}

    onModeChanged();

    --filterChange_;
}


float PageOptionsEnvironment::getSelTime() const
{
    if (colourTimeline_ == NULL)
        return -1.0f;
    ColorScheduleItem *selItem = 
        colourTimeline_->getColorScheduleItemSelected();
    if (selItem == NULL)
        return -1.0f;
    return selItem->normalisedTime_;
}


void PageOptionsEnvironment::setSelTime(float time)
{
    if (colourTimeline_ == NULL)
        return;
    colourTimeline_->setColorScheduleItemSelected(time);
    timelineChanged();
}


/*static*/ EnviroMinder &PageOptionsEnvironment::getEnviroMinder()
{
    return WorldManager::instance().enviroMinder();
}


/*static*/ TimeOfDay &PageOptionsEnvironment::getTimeOfDay()
{
    return *WorldManager::instance().timeOfDay();
}


void PageOptionsEnvironment::InitPage()
{
    ++filterChange_;

    reinitialise();

    skyBrowseFileBtn_.setBitmapID(IDB_OPEN     , IDB_OPEND     , RGB(255, 255, 255));
    skyCopyFileBtn_  .setBitmapID(IDB_DUPLICATE, IDB_DUPLICATED, RGB(  0, 128, 128));
    todBrowseFileBtn_.setBitmapID(IDB_OPEN     , IDB_OPEND     , RGB(255, 255, 255));
    todCopyFileBtn_  .setBitmapID(IDB_DUPLICATE, IDB_DUPLICATED, RGB(  0, 128, 128));

    mode_ = MODE_SUN;
    sunAnimBtn_.SetCheck(BST_CHECKED);
    ambAnimBtn_.SetCheck(BST_UNCHECKED);
    onModeChanged();

    if (!initialised_)
        INIT_AUTO_TOOLTIP();

    // Initialise the toolbar with flip, rotate etc: 
    if (skyDomesTB_.GetSafeHwnd() == NULL)
    {
        skyDomesTB_.CreateEx
        (
            this, 
            TBSTYLE_FLAT, 
            WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP
        );
        skyDomesTB_.LoadToolBarEx(IDR_SKYDOME_TB, IDR_SKYDOME_DIS_TB);
        skyDomesTB_.SetBarStyle(CBRS_ALIGN_TOP | CBRS_TOOLTIPS | CBRS_FLYBY);              
        skyDomesTB_.Subclass(IDC_SKYDOME_TB);     
        skyDomesTB_.ShowWindow(SW_SHOW);
    }

    --filterChange_;

    initialised_ = true;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnUpdateControls
(
    WPARAM      /*wParam*/, 
    LPARAM      /*lParam*/
)
{
    if (!initialised_)
        InitPage();

    SendMessageToDescendants
    (
        WM_IDLEUPDATECMDUI,
        (WPARAM)TRUE, 
        0, 
        TRUE, 
        TRUE
    );

    return 0;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnNewSpace
(
    WPARAM      /*wParam*/, 
    LPARAM      /*lParam*/
)
{
    InitPage(); // reinitialise due to the new space
    return 0;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnBeginSave
(
    WPARAM      /*wParam*/, 
    LPARAM      /*lParam*/
)
{
    TimeOfDay &tod = getTimeOfDay();
    tod.secondsPerGameHour(WorldManager::instance().secondsPerHour());
    return 0;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnEndSave
(
    WPARAM      /*wParam*/, 
    LPARAM      /*lParam*/
)
{
    TimeOfDay &tod = getTimeOfDay();
    tod.secondsPerGameHour(0.0f);
    return 0;
}


/*virtual*/ void PageOptionsEnvironment::DoDataExchange(CDataExchange *dx)
{
    CFormView::DoDataExchange(dx);

    DDX_Control(dx, IDC_SKYFILE_EDIT           , skyFileEdit_             );
    DDX_Control(dx, IDC_SKYFILE_BTN            , skyBrowseFileBtn_        );
    DDX_Control(dx, IDC_NEWSKYFILE_BTN         , skyCopyFileBtn_          );
    DDX_Control(dx, IDC_TODFILE_EDIT           , todFileEdit_             );
    DDX_Control(dx, IDC_TODFILE_BTN            , todBrowseFileBtn_        );
    DDX_Control(dx, IDC_NEWTODFILE_BTN         , todCopyFileBtn_          );
                                                                        
    DDX_Control(dx, IDC_SKYDOME_LIST           , skyDomesList_            );
    DDX_Control(dx, IDC_ADDSKYDOME_BTN         , skyDomesAddBtn_          );
    DDX_Control(dx, IDC_CLEARSKYDOME_BTN       , skyDomesClearBtn_        );
    DDX_Control(dx, IDC_SB_GRAD_EDIT           , skyBoxGradEdit_          );
    DDX_Control(dx, IDC_SB_GRAD_BTN            , skyBoxGradBtn_           );
                                                                        
    DDX_Control(dx, IDC_HOURLENGTH             , hourLength_              );
    DDX_Control(dx, IDC_STARTTIME              , startTime_               );
                                                                          
    DDX_Control(dx, IDC_SUNANGLE_EDIT          , sunAngleEdit_            );
    DDX_Control(dx, IDC_SUNANGLE_SLIDER        , sunAngleSlider_          );
    DDX_Control(dx, IDC_MOONANGLE_EDIT         , moonAngleEdit_           );
    DDX_Control(dx, IDC_MOONANGLE_SLIDER       , moonAngleSlider_         );
                                                                          
    DDX_Control(dx, IDC_TIMEOFDAY_SLIDER       , timeOfDaySlider_         );
    DDX_Control(dx, IDC_TIMEOFDAY_EDIT         , timeOfDayEdit_           );
                                                                          
    DDX_Control(dx, IDC_SUNANIM_BTN            , sunAnimBtn_              );
    DDX_Control(dx, IDC_AMBANIM_BTN            , ambAnimBtn_              );
    DDX_Control(dx, IDC_CREATEANIM_BTN         , resetBtn_                );
    DDX_Control(dx, IDC_R_EDIT                 , rEdit_                   );
    DDX_Control(dx, IDC_G_EDIT                 , gEdit_                   );
    DDX_Control(dx, IDC_B_EDIT                 , bEdit_                   );
    DDX_Control(dx, IDC_ADDCOLOR_BTN           , addClrBtn_               );
    DDX_Control(dx, IDC_DELCOLOR_BTN           , delClrBtn_               );
                                                                          
    DDX_Control(dx, IDC_MIEAMOUNT              , mieEdit_                 );
    DDX_Control(dx, IDC_MIEAMOUNT_SLIDER       , mieSlider_               );
    DDX_Control(dx, IDC_TURBOFFS               , turbOffsEdit_            );
    DDX_Control(dx, IDC_TURBOFFS_SLIDER        , turbOffsSlider_          );
    DDX_Control(dx, IDC_TURBFACTOR             , turbFactorEdit_          );
    DDX_Control(dx, IDC_TURBFACTOR_SLIDER      , turbFactorSlider_        );
    DDX_Control(dx, IDC_VERTHEIGHTEFFECT       , vertexHeightEffectEdit_  );
    DDX_Control(dx, IDC_VERTHEIGHTEFFECT_SLIDER, vertexHeightEffectSlider_);
    DDX_Control(dx, IDC_SUNHEIGHTEFFECT        , sunHeightEffectEdit_     );
    DDX_Control(dx, IDC_SUNHEIGHTEFFECT_SLIDER , sunHeightEffectSlider_   );
    DDX_Control(dx, IDC_POWER                  , powerEdit_               );
    DDX_Control(dx, IDC_POWER_SLIDER           , powerSlider_             );

	DDX_Control(dx, IDC_TEXLOD_START_EDIT      , texLodStartEdit_         );
	DDX_Control(dx, IDC_TEXLOD_START_SLIDER    , texLodStartSlider_       );
	DDX_Control(dx, IDC_TEXLOD_DIST_EDIT       , texLodDistEdit_          );
	DDX_Control(dx, IDC_TEXLOD_DIST_SLIDER     , texLodDistSlider_        );
	DDX_Control(dx, IDC_TEXLOD_PRELOAD_EDIT    , texLodPreloadEdit_       );
	DDX_Control(dx, IDC_TEXLOD_PRELOAD_SLIDER  , texLodPreloadSlider_     );
}


/*afx_msg*/ void PageOptionsEnvironment::OnBrowseSkyFile()
{
    char *filter = "Sky files (*.xml)|*.xml|All Files (*.*)|*.*||";

    BWFileDialog 
        openDlg
        (
            TRUE,
            "XML",
            NULL,
            OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            filter,
            AfxGetMainWnd()
        );
    if (openDlg.DoModal() == IDOK)
    { 
        string filename = openDlg.GetPathName().GetBuffer();
        StringUtils::replace(filename, '\\', '/');
        string dissolvedFilename = 
            BWResource::dissolveFilename(filename);
        if (strcmpi(dissolvedFilename.c_str(), filename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {
            CWaitCursor waitCursor;
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_SKY_FILE"));
            EnviroMinder &enviroMinder = getEnviroMinder();
            enviroMinder.skyGradientDomeFile(dissolvedFilename);
            reinitialise();
            WorldManager::instance().environmentChanged();
        }
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnCopySkyFile()
{
    char *filter = "Sky files (*.xml)|*.xml|All Files (*.*)|*.*||";

    EnviroMinder &enviroMinder = getEnviroMinder();
    std::string currentSkyFile = enviroMinder.skyGradientDomeFile();
    currentSkyFile = BWResource::resolveFilename(currentSkyFile);
    StringUtils::replace(currentSkyFile, '/', '\\');

    BWFileDialog 
        saveDlg
        (
            FALSE,
            "XML",
            currentSkyFile.c_str(),
            OFN_OVERWRITEPROMPT,
            filter,
            AfxGetMainWnd()
        );
    if (saveDlg.DoModal() == IDOK)
    { 
        string newFilename = saveDlg.GetPathName().GetBuffer();
        StringUtils::replace(newFilename, '\\', '/');
        string dissolvedFilename = 
            BWResource::dissolveFilename(newFilename);
        // The saved to file must be in BW_RESPATH:
        if (strcmpi(dissolvedFilename.c_str(), newFilename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {   
            // The saved file name must be different to the old file name:
            std::string source  = currentSkyFile;
            std::string dest    = newFilename;

            StringUtils::replace(source, '/', '\\');
            StringUtils::replace(dest  , '/', '\\');

            if (strcmpi(source.c_str(), dest.c_str()) == 0)
            {
                std::string msg = sformat(IDS_FILESMUSTDIFFER, newFilename);
                AfxMessageBox(msg.c_str());
            }
            else
            {
                CWaitCursor waitCursor;
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/COPY_SKY_FILE"));
                SkyGradientDome *skd = enviroMinder.skyGradientDome();
                DataSectionPtr section = BWResource::openSection(newFilename, true);
                skd->save(section);
                section->save();
                enviroMinder.skyGradientDomeFile(dissolvedFilename);
                reinitialise();
                WorldManager::instance().environmentChanged();
            }
        }
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnBrowseTODFile()
{
    char *filter = "Time of day files (*.xml)|*.xml|All Files (*.*)|*.*||";

    BWFileDialog 
        openDlg
        (
            TRUE,
            "XML",
            NULL,
            OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            filter,
            AfxGetMainWnd()
        );
    if (openDlg.DoModal() == IDOK)
    { 
        string filename = openDlg.GetPathName().GetBuffer();
        StringUtils::replace(filename, '\\', '/');
        string dissolvedFilename = 
            BWResource::dissolveFilename(filename);
        if (strcmpi(dissolvedFilename.c_str(), filename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {
            CWaitCursor waitCursor;
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_TIME_OF_DAY_FILE"));
            EnviroMinder &enviroMinder = getEnviroMinder();
            enviroMinder.timeOfDayFile(dissolvedFilename);
            reinitialise();
            WorldManager::instance().environmentChanged();
        }
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnCopyTODFile()
{
    char *filter = "Sky files (*.xml)|*.xml|All Files (*.*)|*.*||";

    EnviroMinder &enviroMinder = getEnviroMinder();
    std::string currentTODFile = enviroMinder.timeOfDayFile();
    currentTODFile = BWResource::resolveFilename(currentTODFile);
    StringUtils::replace(currentTODFile, '/', '\\');

    BWFileDialog 
        saveDlg
        (
            FALSE,
            "XML",
            currentTODFile.c_str(),
            OFN_OVERWRITEPROMPT,
            filter,
            AfxGetMainWnd()
        );
    if (saveDlg.DoModal() == IDOK)
    { 
        string newFilename = saveDlg.GetPathName().GetBuffer();
        StringUtils::replace(newFilename, '\\', '/');
        string dissolvedFilename = 
            BWResource::dissolveFilename(newFilename);
        if (strcmpi(dissolvedFilename.c_str(), newFilename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {
            // The saved file name must be different to the old file name:
            std::string source  = currentTODFile;
            std::string dest    = newFilename;

            StringUtils::replace(source, '/', '\\');
            StringUtils::replace(dest  , '/', '\\');

            if (strcmpi(source.c_str(), dest.c_str()) == 0)
            {
                std::string msg = sformat(IDS_FILESMUSTDIFFER, newFilename);
                AfxMessageBox(msg.c_str());
            }
            else
            {
                CWaitCursor waitCursor;
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/COPY_TIME_OF_DAY_FILE"));
                TimeOfDay &tod = getTimeOfDay();
                tod.save(newFilename);
                enviroMinder.timeOfDayFile(dissolvedFilename);
                reinitialise();
                WorldManager::instance().environmentChanged();
            }
        }
    }
}


/*afx_msg*/  void PageOptionsEnvironment::OnAddSkyDome()
{
    char *filter = "Visuals (*.visual)|*.visual|All Files (*.*)|*.*||";

    BWFileDialog 
        openDlg
        (
            TRUE,
            "TGA",
            NULL,
            OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            filter,
            AfxGetMainWnd()
        );
    if (openDlg.DoModal() == IDOK)
    {        
        string filename = openDlg.GetPathName().GetBuffer();
        StringUtils::replace(filename, '\\', '/');
        string dissolvedFilename = 
            BWResource::dissolveFilename(filename);
        if (strcmpi(dissolvedFilename.c_str(), filename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {
            CWaitCursor waitCursor;
            Moo::VisualPtr skyDome = 
                Moo::VisualManager::instance()->get(dissolvedFilename);
            if (skyDome == NULL)
            {
                waitCursor.Restore();
				std::string msg = L("RCST_IDS_NOLOADSKYDOME"); 
                AfxMessageBox(msg.c_str());
            }
            else
            {
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/ADD_SKY_DOME"));
                EnviroMinder &enviroMinder = getEnviroMinder();
                enviroMinder.skyDomes().push_back(skyDome);
                dissolvedFilename = 
                    BWResource::getFilename(dissolvedFilename);
                skyDomesList_.AddString(dissolvedFilename.c_str());
                WorldManager::instance().environmentChanged();
            }
        }
    }
}


/*afx_msg*/  void PageOptionsEnvironment::OnClearSkyDomes()
{    
    EnviroMinder &enviroMinder = getEnviroMinder();
    if (!enviroMinder.skyDomes().empty())
    {
        saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/CLEAR_SKY_DOME"));
        enviroMinder.skyDomes().clear();
        skyDomesList_.ResetContent();
        WorldManager::instance().environmentChanged();
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnBrowseSkyGradBtn()
{
    EnviroMinder    &enviroMinder   = getEnviroMinder();
    TimeOfDay       &tod            = getTimeOfDay();
    SkyGradientDome *skd            = enviroMinder.skyGradientDome();

    if (skd == NULL)
        return;

    string origFilename = skd->textureName();
    string filename     = BWResource::resolveFilename(origFilename);
    char   *filter      = "TGA Images (*.tga)|*.tga|All Files (*.*)|*.*||";

    StringUtils::replace(filename, '/', '\\');

    BWFileDialog 
        openDlg
        (
            TRUE,
            "TGA",
            filename.c_str(),
            OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            filter,
            AfxGetMainWnd()
        );

    if (openDlg.DoModal() == IDOK)
    {
        string newFilename = openDlg.GetPathName().GetBuffer();
        StringUtils::replace(newFilename, '\\', '/');
        string dissolvedFilename = BWResource::dissolveFilename(newFilename);
        if (strcmpi(dissolvedFilename.c_str(), newFilename.c_str()) == 0)
        {
			std::string msg = L("RCST_IDS_NOTRESPATH");
            AfxMessageBox(msg.c_str());
        }
        else
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/LOAD_SKY_BOX_TEXTURE"));
            bool ok = skd->loadTexture(dissolvedFilename);
            if (ok)
            {
                skyBoxGradEdit_.SetWindowText(dissolvedFilename.c_str());
            }
            else
            {
                UndoRedo::instance().undo();
                skd->loadTexture(origFilename);
				std::string msg = L("RCST_IDS_NOLOADSKYGRADIENT");
                AfxMessageBox(msg.c_str());
            }
        }
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxUpEnable(CCmdUI *cmdui)
{
    int sel = skyDomesList_.GetCurSel();
    if (sel != 0 && sel != CB_ERR)
        cmdui->Enable(TRUE);
    else
        cmdui->Enable(FALSE);
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxDownEnable(CCmdUI *cmdui)
{
    int sel = skyDomesList_.GetCurSel();
    if (sel != skyDomesList_.GetCount() - 1 && sel != CB_ERR)
        cmdui->Enable(TRUE);
    else
        cmdui->Enable(FALSE);
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxDelEnable(CCmdUI *cmdui)
{
    int sel = skyDomesList_.GetCurSel();
    if (sel != CB_ERR)
        cmdui->Enable(TRUE);
    else
        cmdui->Enable(FALSE);
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxUp()
{
    // Get the selection:
    int sel = skyDomesList_.GetCurSel();
    if (sel == CB_ERR || sel == 0)
        return;

    // Save current state:
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/MOVE_SKY_DOME_UP"));
    
    // Remove the skydome and add it back in at it's new position:
    EnviroMinder &enviroMinder = getEnviroMinder();
    std::vector<Moo::VisualPtr> &skyDomes = enviroMinder.skyDomes();
    Moo::VisualPtr selSkyDome = skyDomes[sel];
    skyDomes.erase(skyDomes.begin() + sel);
    skyDomes.insert(skyDomes.begin() + sel - 1, selSkyDome);

    // Update the control:
    rebuildSkydomeList();

    // Set the selection:
    skyDomesList_.SetCurSel(sel - 1);
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxDown()
{
    // Get the selection:
    int sel = skyDomesList_.GetCurSel();
    if (sel == CB_ERR)
        return;

    // Save current state:
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/MOVE_SKY_DOME_DOWN"));
    
    // Remove the skydome and add it back in at it's new position:
    EnviroMinder &enviroMinder = getEnviroMinder();
    std::vector<Moo::VisualPtr> &skyDomes = enviroMinder.skyDomes();
    Moo::VisualPtr selSkyDome = skyDomes[sel];
    skyDomes.erase(skyDomes.begin() + sel);
    skyDomes.insert(skyDomes.begin() + sel + 1, selSkyDome);

    // Update the control:
    rebuildSkydomeList();

    // Set the selection:
    skyDomesList_.SetCurSel(sel + 1);
}


/*afx_msg*/ void PageOptionsEnvironment::OnSkyboxDel()
{
    // Get the selection:
    int sel = skyDomesList_.GetCurSel();
    if (sel == CB_ERR)
        return;

    // Save current state:
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/DELETE_SKY_DOME"));
    
    // Remove the skydome:
    EnviroMinder &enviroMinder = getEnviroMinder();
    std::vector<Moo::VisualPtr> &skyDomes = enviroMinder.skyDomes();
    skyDomes.erase(skyDomes.begin() + sel);

    // Update the control:
    rebuildSkydomeList();

    // Set the selection:
    if (sel < skyDomesList_.GetCount())
        skyDomesList_.SetCurSel(sel);
    else if (sel - 1 >= 0)
        skyDomesList_.SetCurSel(sel - 1);
    else
        skyDomesList_.SetCurSel(0);
}


/*afx_msg*/ void PageOptionsEnvironment::OnHourLengthEdit()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;
        
        TimeOfDay &tod = getTimeOfDay();

        if (WorldManager::instance().secondsPerHour() != hourLength_.GetValue())
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_HOUR_LENGTH"));
            WorldManager::instance().secondsPerHour(hourLength_.GetValue());
            WorldManager::instance().environmentChanged();
        }

        --filterChange_;
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnStartTimeEdit()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        TimeOfDay &tod = getTimeOfDay();

        if (tod.startTime() != startTime_.GetValue())
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/ON_EDIT_START_TIME"));
            tod.startTime(startTime_.GetValue());
            WorldManager::instance().environmentChanged();
        }

        --filterChange_;
    }
}


/*afx_msg*/ 
void 
PageOptionsEnvironment::OnHScroll
(   
    UINT        sbcode, 
    UINT        pos, 
    CScrollBar  *scrollBar
)
{
    CFormView::OnHScroll(sbcode, pos, scrollBar);

    CWnd *wnd = (CWnd *)scrollBar;

    // Work out whether this is the first part of a slider movement,
    // in the middle of a slider movement or the end of a slider movement:
    SliderMovementState sms = SMS_MIDDLE;
    if (!sliding_)
    {
        sms = SMS_STARTED;
        sliding_ = true;
    }
    if (sbcode == TB_ENDTRACK)
    {
        sms = SMS_DONE;
        sliding_ = false;
    }

    if (wasSlider(*wnd, sunAngleSlider_))
    {
        OnSunAngleSlider(sms);
    }
    else if (wasSlider(*wnd, moonAngleSlider_))
    {
        OnMoonAngleSlider(sms);
    }
    else if (wasSlider(*wnd, timeOfDaySlider_))
    {
        OnTimeOfDaySlider(sms);
    }
    else if (wasSlider(*wnd, mieSlider_))
    {
        OnMIESlider(sms);
    }
    else if (wasSlider(*wnd, turbOffsSlider_))
    {
        OnTurbOffsSlider(sms);
    }
    else if (wasSlider(*wnd, turbFactorSlider_))
    {
        OnTurbFactSlider(sms);
    }
    else if (wasSlider(*wnd, vertexHeightEffectSlider_))
    {
        OnVertEffSlider(sms);
    }
    else if (wasSlider(*wnd, sunHeightEffectSlider_))
    {
        OnSunHeightEffSlider(sms);
    }
    else if (wasSlider(*wnd, powerSlider_))
    {
        OnPowerSlider(sms);
    }
	else if
	(
		wasSlider(*wnd, texLodStartSlider_)
		||
		wasSlider(*wnd, texLodDistSlider_)
		||
		wasSlider(*wnd, texLodPreloadSlider_)
	)
	{
		OnTexLodSlider(sms);
	}
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnCTSelTime(WPARAM /*wParam*/, LPARAM lParam)
{
    if (filterChange_ == NULL)
    {
        ++filterChange_;
        float gameTime = 0.0001f*lParam;
        TimeOfDay &tod = getTimeOfDay();
        tod.gameTime(gameTime);
        string gameTimeStr = tod.getTimeOfDayAsString();
        timeOfDayEdit_.SetWindowText(gameTimeStr.c_str());
        timeOfDaySlider_.SetPos(BW_ROUND_TO_INT(gameTime*SLIDER_PREC));
        colourTimeline_->showLineAtTime(gameTime);

		MF_ASSERT( WorldEditorApp::instance().pythonAdapter() != NULL &&
			"PageOptionsEnvironment::OnCTSelTime: PythonAdapter is NULL" );
		WorldEditorApp::instance().pythonAdapter()->onSliderAdjust(
            "slrProjectCurrentTime", 
            timeOfDaySlider_.GetPos(), 
            timeOfDaySlider_.GetRangeMin(), 
            timeOfDaySlider_.GetRangeMax() );

        Options::setOptionInt( 
            "graphics/timeofday", 
			int( timeOfDaySlider_.GetPos() * WorldManager::TIME_OF_DAY_MULTIPLIER / SLIDER_PREC ) );

        WorldEditorApp::instance().mfApp()->updateFrame(false);
        --filterChange_;
    }
    return TRUE;
}


void PageOptionsEnvironment::OnSunAngleSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        TimeOfDay &tod = getTimeOfDay();
		float sunAngle = sunAngleSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = tod.sunAngle();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != sunAngle)
            {
                // Save an undo state at the initial value:
                tod.sunAngle(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_SUN_ANGLE"));
                WorldManager::instance().environmentChanged();                
            }
        }

        tod.sunAngle(sunAngle);
        sunAngleEdit_.SetValue(sunAngle);

        --filterChange_;        
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnSunAngleEdit()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        TimeOfDay &tod = getTimeOfDay();
        float sunAngle = sunAngleEdit_.GetValue();
        if (tod.sunAngle() != sunAngle)
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_SUN_ANGLE"));  
            sunAngleSlider_.setValue( sunAngle );            
			tod.sunAngle( sunAngleSlider_.getValue() );
            WorldManager::instance().environmentChanged();
        }

        --filterChange_;        
    }
}


void PageOptionsEnvironment::OnMoonAngleSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        TimeOfDay &tod = getTimeOfDay();
        float moonAngle = moonAngleSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = tod.moonAngle();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != moonAngle)
            {
                // Save an undo state at the initial value:
                tod.moonAngle(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_MOON_ANGLE"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        tod.moonAngle(moonAngle);
        moonAngleEdit_.SetValue(moonAngle);

        --filterChange_;
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnMoonAngleEdit()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        TimeOfDay &tod = getTimeOfDay();

        float moonAngle = moonAngleEdit_.GetValue();
        if (tod.moonAngle() != moonAngle)
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_MOON_ANGLE"));
            moonAngleSlider_.setValue( moonAngle );
            tod.moonAngle( moonAngleSlider_.getValue() );
            WorldManager::instance().environmentChanged();
        }

        --filterChange_;        
    }
}


void PageOptionsEnvironment::OnTimeOfDaySlider(SliderMovementState /*sms*/)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        TimeOfDay &tod = getTimeOfDay();
        float gameTime = timeOfDaySlider_.GetPos()/SLIDER_PREC;
        tod.gameTime(gameTime);
        string gameTimeStr = tod.getTimeOfDayAsString();
        timeOfDayEdit_.SetWindowText(gameTimeStr.c_str());

		MF_ASSERT( WorldEditorApp::instance().pythonAdapter() != NULL &&
			"PageOptionsEnvironment::OnTimeOfDaySlider: PythonAdapter is NULL" );
		WorldEditorApp::instance().pythonAdapter()->onSliderAdjust(
            "slrProjectCurrentTime", 
            timeOfDaySlider_.GetPos(), 
            timeOfDaySlider_.GetRangeMin(), 
            timeOfDaySlider_.GetRangeMax() );

        Options::setOptionInt( 
            "graphics/timeofday", 
            int( timeOfDaySlider_.GetPos() * WorldManager::TIME_OF_DAY_MULTIPLIER / SLIDER_PREC ) );

        if (colourTimeline_ != NULL)
        {
            colourTimeline_->showLineAtTime(gameTime);
        }

        --filterChange_;
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnSunAnimBtn()
{
    mode_ = MODE_SUN;
    onModeChanged();
}


/*afx_msg*/ void PageOptionsEnvironment::OnAmbAnimBtn()
{
    mode_ = MODE_AMB;
    onModeChanged();
}


/*afx_msg*/ void PageOptionsEnvironment::OnResetBtn()
{
    TimeOfDay &tod = getTimeOfDay();
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/RESET_ENVIRONMENT_ANIMATION"));
    switch (mode_)
    {
    case MODE_SUN:
        tod.clearSunAnimations();
        tod.addSunAnimation( 0.00f, Vector3( 43.0f, 112.0f, 168.0f));
        tod.addSunAnimation( 4.02f, Vector3( 28.0f,  81.0f, 110.0f));
        tod.addSunAnimation( 5.07f, Vector3( 57.0f,  81.0f, 110.0f));
        tod.addSunAnimation( 6.40f, Vector3(207.0f, 157.0f,  90.0f));
        tod.addSunAnimation( 9.30f, Vector3(246.0f, 195.0f, 157.0f));
        tod.addSunAnimation(16.95f, Vector3(246.0f, 195.0f, 157.0f));
        tod.addSunAnimation(17.87f, Vector3(244.0f, 124.0f,   4.0f));
        break;
    case MODE_AMB:
        tod.clearAmbientAnimations();
        tod.addAmbientAnimation( 0.00f, Vector3( 21.0f,  81.0f, 130.0f));
        tod.addAmbientAnimation( 5.67f, Vector3( 21.0f,  81.0f, 130.0f));
        tod.addAmbientAnimation( 8.47f, Vector3( 64.0f,  64.0f,  32.0f));
        tod.addAmbientAnimation(11.37f, Vector3( 64.0f,  64.0f,  32.0f));
        tod.addAmbientAnimation(18.00f, Vector3( 69.0f,  77.0f,  82.0f));
        break;
    default:
        ASSERT(0);
    }
    onModeChanged();
    WorldManager::instance().environmentChanged();
}


/*afx_msg*/ void PageOptionsEnvironment::OnAddClrBtn()
{
    if (colourTimeline_ != NULL)
    {
        colourTimeline_->addColorAtLButton();
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnDelClrBtn()
{
    if (colourTimeline_ != NULL)
    {
        bool ok = colourTimeline_->removeSelectedColor();
        if (ok)
        {
            saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/DELETE_ENVIRONMENT_ANIMATION_COLOR"));
            rebuildAnimation();            
        }
    }
}


/*afx_msg*/ void PageOptionsEnvironment::OnEditClrText()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        int r = (int)rEdit_.GetValue();
        int g = (int)gEdit_.GetValue();
        int b = (int)bEdit_.GetValue();

        COLORREF curColor = colourPicker_->getRGB();
        COLORREF newColor = RGB(r, g, b);

        if (newColor != curColor)
        {
            if (colourPicker_ != NULL)
            {
                colourPicker_->setRGB(newColor);
            }   
            if (colourTimeline_ != NULL)
            {
                if (colourTimeline_->itemSelected())
                {
                    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ENVIRONMENT_COLOR"));
                    colourTimeline_->setColorScheduleItemSelectedColor
                    (
                        Vector4(r/255.0f, g/255.0f, b/255.0f, 1.0f)
                    );
                    rebuildAnimation();
                }
            }   
        }

        --filterChange_;
    }
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnTimelineBegin
(
    WPARAM      wparam, 
    LPARAM      lparam
)
{
    ColorScheduleItem *item = colourTimeline_->getColorScheduleItemSelected();
    initialValue_ = item->normalisedTime_;
    timelineChanged();
    return TRUE;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnTimelineMiddle
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    timelineChanged();
    return TRUE;
}


/*afx_msg*/ 
LRESULT 
PageOptionsEnvironment::OnTimelineDone
(
    WPARAM wparam, 
    LPARAM lparam
)
{
    // Create an undo position by going back to the original state:
    ColorScheduleItem *item = colourTimeline_->getColorScheduleItemSelected();
    float thisTime = item->normalisedTime_;
    item->normalisedTime_ = initialValue_;
    rebuildAnimation();
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ENVIRONMENT_TIMELINE"));

    // Now do the actual update:
    item->normalisedTime_ = thisTime;
    timelineChanged();

    return TRUE;
}


/*afx_msg*/ 
LRESULT
PageOptionsEnvironment::OnTimelineAdd
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/ADD_ENVIRONMENT_COLOR"));
    timelineChanged();
    return TRUE;
}


/*afx_msg*/ 
LRESULT
PageOptionsEnvironment::OnTimelineNewSel
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    timelineChanged();
    return TRUE;
}


/*afx_msg*/ 
LRESULT
PageOptionsEnvironment::OnPickerDown
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    // Remember the initial color:
    if (colourTimeline_->itemSelected())
    {
        ColorScheduleItem *item = 
            colourTimeline_->getColorScheduleItemSelected();
        initialColor_ = item->color_;
    }

    pickerChanged();

    return TRUE;
}


/*afx_msg*/ 
LRESULT
PageOptionsEnvironment::OnPickerMove
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    pickerChanged();

    return TRUE;
}


/*afx_msg*/ 
LRESULT
PageOptionsEnvironment::OnPickerUp
(
    WPARAM      /*wparam*/, 
    LPARAM      /*lparam*/
)
{
    pickerChanged();

    if (colourTimeline_->itemSelected())
    {
        // Create an undo position by going back to the original state:
        ColorScheduleItem *item = 
            colourTimeline_->getColorScheduleItemSelected();
        Vector4 thisColor = item->color_;
        item->color_ = initialColor_;
        rebuildAnimation();
        saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ENVIRONMENT_COLOR"));

        // Now change the model back again:
        item->color_ = thisColor;
        rebuildAnimation();
    }

    return TRUE;
}


/*afx_msg*/ void PageOptionsEnvironment::OnEditEnvironText()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        if (skd != NULL)
        {
            bool changed = 
                skd->mieEffect         () != mieEdit_               .GetValue()
                ||
                skd->turbidityOffset   () != turbOffsEdit_          .GetValue()
                ||
                skd->turbidityFactor   () != turbFactorEdit_        .GetValue()
                ||
                skd->vertexHeightEffect() != vertexHeightEffectEdit_.GetValue()
                ||
                skd->sunHeightEffect   () != sunHeightEffectEdit_   .GetValue()
                ||
                skd->power             () != powerEdit_             .GetValue();

            if (changed)
            {
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));

                mieSlider_               .setValue(mieEdit_               .GetValue());
                turbOffsSlider_          .setValue(turbOffsEdit_          .GetValue());
                turbFactorSlider_        .setValue(turbFactorEdit_        .GetValue());
                vertexHeightEffectSlider_.setValue(vertexHeightEffectEdit_.GetValue());
                sunHeightEffectSlider_   .setValue(sunHeightEffectEdit_   .GetValue());
                powerSlider_             .setValue(powerEdit_             .GetValue());

                skd->mieEffect         (mieSlider_               .getValue());
                skd->turbidityOffset   (turbOffsSlider_          .getValue());
                skd->turbidityFactor   (turbFactorSlider_        .getValue());
                skd->vertexHeightEffect(vertexHeightEffectSlider_.getValue());
                skd->sunHeightEffect   (sunHeightEffectSlider_   .getValue());
                skd->power             (powerSlider_             .getValue());

                WorldManager::instance().environmentChanged();
            }
        }

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnMIESlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           mieValue      = mieSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->mieEffect();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != mieValue)
            {
                // Save an undo state at the initial value:
                skd->mieEffect(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->mieEffect(mieValue);
        mieEdit_.SetValue(mieValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnTurbOffsSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           turbValue     = turbOffsSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->turbidityOffset();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != turbValue)
            {
                // Save an undo state at the initial value:
                skd->turbidityOffset(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->turbidityOffset(turbValue);
        turbOffsEdit_.SetValue(turbValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnTurbFactSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           turbValue     = turbFactorSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->turbidityFactor();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != turbValue)
            {
                // Save an undo state at the initial value:
                skd->turbidityFactor(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->turbidityFactor(turbValue);
        turbFactorEdit_.SetValue(turbValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnVertEffSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           vertValue     = vertexHeightEffectSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->vertexHeightEffect();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != vertValue)
            {
                // Save an undo state at the initial value:
                skd->vertexHeightEffect(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->vertexHeightEffect(vertValue);
        vertexHeightEffectEdit_.SetValue(vertValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnSunHeightEffSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           sunValue      = sunHeightEffectSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->sunHeightEffect();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != sunValue)
            {
                // Save an undo state at the initial value:
                skd->sunHeightEffect(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->sunHeightEffect(sunValue);
        sunHeightEffectEdit_.SetValue(sunValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnPowerSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

        EnviroMinder    &enviroMinder = getEnviroMinder();
        SkyGradientDome *skd          = enviroMinder.skyGradientDome();
        float           powerValue    = powerSlider_.getValue();

        if (sms == SMS_STARTED)
        {
            initialValue_ = skd->power();
        }
        else if (sms == SMS_DONE)
        {
            if (initialValue_ != powerValue)
            {
                // Save an undo state at the initial value:
                skd->power(initialValue_);
                saveUndoState(L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/EDIT_ATMOSPHERIC_COLOR"));
                WorldManager::instance().environmentChanged();                
            }
        }                 
        skd->power(powerValue);
        powerEdit_.SetValue(powerValue);

        --filterChange_;
    }
}


void PageOptionsEnvironment::OnTexLodSlider(SliderMovementState sms)
{
    if (filterChange_ == 0)
    {
        ++filterChange_;   

		// If started sliding then create and undo/redo block and tell WE
		// that the enviroment has changed.
		if (sms == SMS_STARTED)
		{
			// Add a new undo/redo state:
			UndoRedo::instance().add(new TerrainLodUndo());
			UndoRedo::instance().barrier
			(
				L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_TERRAINLOD"), 
				false
			);

			// Tell WorldEditor that the environment has changed, and so saving 
			// should be done.
			WorldManager::instance().environmentChanged();
		}

		float texLodStart	= texLodStartSlider_  .getValue();
		float texLodDist	= texLodDistSlider_   .getValue();
		float texLodPreload	= texLodPreloadSlider_.getValue();

		Terrain::TerrainSettingsPtr pTerrainSettings = 
			WorldManager::instance().pTerrainSettings();
		float oldTexLodStart   = pTerrainSettings->lodTextureStart     ();
		float oldTexLodDist    = pTerrainSettings->lodTextureDistance  ();
		float oldTexLodPreload = pTerrainSettings->blendPreloadDistance();

		// Update the edits:
		texLodStartEdit_  .SetValue(texLodStart	 );
		texLodDistEdit_   .SetValue(texLodDist	 );
		texLodPreloadEdit_.SetValue(texLodPreload);

		// Let the settings know about the changes:
		pTerrainSettings->lodTextureStart     (texLodStart  );
		pTerrainSettings->lodTextureDistance  (texLodDist   );
		pTerrainSettings->blendPreloadDistance(texLodPreload);
		
		--filterChange_;
    }
}


void PageOptionsEnvironment::OnTexLodEdit()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

		float texLodStart	= texLodStartEdit_  .GetValue();
		float texLodDist	= texLodDistEdit_   .GetValue();
		float texLodPreload	= texLodPreloadEdit_.GetValue();

		Terrain::TerrainSettingsPtr pTerrainSettings = WorldManager::instance().pTerrainSettings();
		float oldTexLodStart   = pTerrainSettings->lodTextureStart     ();
		float oldTexLodDist    = pTerrainSettings->lodTextureDistance  ();
		float oldTexLodPreload = pTerrainSettings->blendPreloadDistance();

		// Filter out non-changes:
		if 
		(
			!almostEqual(texLodStart   ,oldTexLodStart  , LOD_EPSILON)
			|| 
			!almostEqual(texLodDist    ,oldTexLodDist   , LOD_EPSILON)
			||
			!almostEqual(texLodPreload ,oldTexLodPreload, LOD_EPSILON)
		)
		{
			// Add a new undo/redo state:
			UndoRedo::instance().add(new TerrainLodUndo());
			UndoRedo::instance().barrier
			(
				L("WORLDEDITOR/GUI/PAGE_OPTIONS_ENVIRONMENT/SET_TERRAINLOD"), 
				false
			);

			// Update the sliders:
			texLodStartSlider_  .setValue(texLodStart);
			texLodDistSlider_   .setValue(texLodDist);
			texLodPreloadSlider_.setValue(texLodPreload);

			// Let the settings know about the changes:
			pTerrainSettings->lodTextureStart     (texLodStartSlider_  .getValue());
			pTerrainSettings->lodTextureDistance  (texLodDistSlider_   .getValue());
			pTerrainSettings->blendPreloadDistance(texLodPreloadSlider_.getValue());

			// Tell WorldEditor that the environment has changed, and so saving 
			// should be done.
			WorldManager::instance().environmentChanged();
		}

		--filterChange_;
    }
}


BOOL PageOptionsEnvironment::OnToolTipText(UINT, NMHDR* pNMHDR, LRESULT *result)
{
    // Allow top level routing frame to handle the message
    if (GetRoutingFrame() != NULL)
        return FALSE;

    // Need to handle both ANSI and UNICODE versions of the message
    TOOLTIPTEXTA *pTTTA = (TOOLTIPTEXTA*)pNMHDR;
    TOOLTIPTEXTW *pTTTW = (TOOLTIPTEXTW*)pNMHDR;
    CString cstTipText;
    CString cstStatusText;

    UINT nID = pNMHDR->idFrom;
    if 
    (
        pNMHDR->code == TTN_NEEDTEXTA 
        && 
        (pTTTA->uFlags & TTF_IDISHWND) 
        ||
        pNMHDR->code == TTN_NEEDTEXTW 
        && 
        (pTTTW->uFlags & TTF_IDISHWND)
    )
    {
        // idFrom is actually the HWND of the tool
        nID = ((UINT)(WORD)::GetDlgCtrlID((HWND)nID));
    }

    if (nID != 0) // will be zero on a separator
    {
        cstTipText.LoadString(nID);
        cstStatusText.LoadString(nID);
    }

    // Non-UNICODE Strings only are shown in the tooltip window...
    if (pNMHDR->code == TTN_NEEDTEXTA)
    {
        lstrcpyn
        (
            pTTTA->szText, 
            cstTipText,
            (sizeof(pTTTA->szText)/sizeof(pTTTA->szText[0]))            
        );
    }
    else
    {
        _mbstowcsz
        (
            pTTTW->szText, 
            cstTipText,
            (sizeof(pTTTW->szText)/sizeof(pTTTW->szText[0]))
        );
    }
    *result = 0;

    // bring the tooltip window above other popup windows
    ::SetWindowPos
    (
        pNMHDR->hwndFrom, 
        HWND_TOP, 
        0, 
        0, 
        0, 
        0,
        SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE
    );

    return TRUE;    // message was handled
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
/*afx_msg*/ HBRUSH PageOptionsEnvironment::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	HBRUSH brush = CFormView::OnCtlColor( pDC, pWnd, nCtlColor );

	sunAngleEdit_.SetBoundsColour( pDC, pWnd,
		sunAngleSlider_.getMinRangeLimit(), sunAngleSlider_.getMaxRangeLimit() );
	moonAngleEdit_.SetBoundsColour( pDC, pWnd,
		moonAngleSlider_.getMinRangeLimit(), moonAngleSlider_.getMaxRangeLimit() );
	mieEdit_.SetBoundsColour( pDC, pWnd,
		mieSlider_.getMinRangeLimit(), mieSlider_.getMaxRangeLimit() );
	turbOffsEdit_.SetBoundsColour( pDC, pWnd,
		turbOffsSlider_.getMinRangeLimit(), turbOffsSlider_.getMaxRangeLimit() );
	turbFactorEdit_.SetBoundsColour( pDC, pWnd,
		turbFactorSlider_.getMinRangeLimit(), turbFactorSlider_.getMaxRangeLimit() );
	vertexHeightEffectEdit_.SetBoundsColour( pDC, pWnd,
		vertexHeightEffectSlider_.getMinRangeLimit(), vertexHeightEffectSlider_.getMaxRangeLimit() );
	sunHeightEffectEdit_.SetBoundsColour( pDC, pWnd,
		sunHeightEffectSlider_.getMinRangeLimit(), sunHeightEffectSlider_.getMaxRangeLimit() );
	powerEdit_.SetBoundsColour( pDC, pWnd,
		powerSlider_.getMinRangeLimit(), powerSlider_.getMaxRangeLimit() );
	texLodStartEdit_.SetBoundsColour( pDC, pWnd,
		texLodStartSlider_.getMinRangeLimit(), texLodStartSlider_.getMaxRangeLimit() );
	texLodDistEdit_.SetBoundsColour( pDC, pWnd,
		texLodDistSlider_.getMinRangeLimit(), texLodDistSlider_.getMaxRangeLimit() );
	texLodPreloadEdit_.SetBoundsColour( pDC, pWnd,
		texLodPreloadSlider_.getMinRangeLimit(), texLodPreloadSlider_.getMaxRangeLimit() );

	return brush;
}


/**
 *	Called when the sun angle edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnSunAngleEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        sunAngleEdit_.SetValue( sunAngleSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the moon angle edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnMoonAngleEditKillFocus()
{
	if (filterChange_ == 0)
    {
        ++filterChange_;

        moonAngleEdit_.SetValue( moonAngleSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the Mie amount edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnMieAmountEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        mieEdit_.SetValue( mieSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the turbulance edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnTurbOffEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        turbOffsEdit_.SetValue( turbOffsSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the turbulance factor edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnTurbFactEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        turbFactorEdit_.SetValue( turbFactorSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the vertex effect edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnVertEffEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        vertexHeightEffectEdit_.SetValue( vertexHeightEffectSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the sun effect edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnSunEffEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        sunHeightEffectEdit_.SetValue( sunHeightEffectSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the power edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnPowerEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        powerEdit_.SetValue( powerSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the texture LOD start edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnTexLodStartEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        texLodStartEdit_.SetValue( texLodStartSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the texture LOD blend edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnTexLodBlendEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        texLodDistEdit_.SetValue( texLodDistSlider_.getValue() );

        --filterChange_;        
    }
}


/**
 *	Called when the texture LOD preload edit field loses focus.
 */
/*afx_msg*/ void PageOptionsEnvironment::OnTexLodPreloadEditKillFocus()
{
    if (filterChange_ == 0)
    {
        ++filterChange_;

        texLodPreloadEdit_.SetValue( texLodPreloadSlider_.getValue() );

        --filterChange_;        
    }
}


void PageOptionsEnvironment::onModeChanged()
{
    if (colourTimeline_ != NULL)
    {
        colourTimeline_->DestroyWindow();
		delete colourTimeline_;
        colourTimeline_ = NULL;
    }

    if (colourPicker_ != NULL)
    {
        colourPicker_->DestroyWindow();
		delete colourPicker_;
        colourPicker_ = NULL;
    }

    TimeOfDay &tod = getTimeOfDay();
    
    LinearAnimation<Vector3> const *animation = NULL;
    switch (mode_)
    {
    case MODE_SUN:
        animation = &tod.sunAnimation();
        break;
    case MODE_AMB:
        animation = &tod.ambientAnimation();
        break;
    default:
        ASSERT(0);
    }

    if (!animation->empty())
    {
        ColorScheduleItems schedule;
        for
        (
            LinearAnimation<Vector3>::const_iterator it = animation->begin();
            it != animation->end();
            ++it
        )
        {
            Vector3 const &clr = it->second;
            ColorScheduleItem item;
            item.normalisedTime_ = it->first/24.0f;
            item.color_ = 
                Vector4(clr.x/255.0f, clr.y/255.0f, clr.z/255.0f, 1.0f);
            schedule.push_back(item);
        }
        CWnd *timelineFrame = GetDlgItem(IDC_COLORTIMELINE);
        CRect timelineRect;
        timelineFrame->GetWindowRect(&timelineRect);
        ScreenToClient(&timelineRect);
        colourTimeline_ = new ColorTimeline();
        colourTimeline_->Create
        (
            WS_CHILD | WS_VISIBLE, 
            timelineRect, 
            this, 
            schedule,
            false,                          // no alpha
            ColorTimeline::TS_HOURS_MINS,   // time in hrs.mins format
            true                            // wrap
        );
        colourTimeline_->totalScheduleTime(24.0f);
        colourTimeline_->Invalidate();
        colourTimeline_->showLineAtTime(tod.gameTime());
    
        CWnd *pickerFrame = GetDlgItem(IDC_COLORPICKER);
        CRect pickerRect;
        pickerFrame->GetWindowRect(&pickerRect);
        ScreenToClient(&pickerRect);
        colourPicker_ = new ColorPicker();
        colourPicker_->Create(WS_CHILD | WS_VISIBLE, pickerRect, this, false);
        colourPicker_->Invalidate();

        ++filterChange_;

        addClrBtn_.EnableWindow(TRUE);
        delClrBtn_.EnableWindow(TRUE);
        rEdit_.EnableWindow(TRUE);         
        gEdit_.EnableWindow(TRUE);         
        bEdit_.EnableWindow(TRUE); 
        Vector4 colour = colourPicker_->getRGBA();
        rEdit_.SetValue(255.0f*colour.x);
        gEdit_.SetValue(255.0f*colour.y);
        bEdit_.SetValue(255.0f*colour.z);

        --filterChange_;
    }
    else
    {
        ++filterChange_;

        addClrBtn_.EnableWindow(FALSE);
        delClrBtn_.EnableWindow(FALSE);
        rEdit_.EnableWindow(FALSE); 
        rEdit_.SetWindowText("");
        gEdit_.EnableWindow(FALSE); 
        gEdit_.SetWindowText("");
        bEdit_.EnableWindow(FALSE); 
        bEdit_.SetWindowText("");

        --filterChange_;
    }
}


void PageOptionsEnvironment::rebuildAnimation()
{
    TimeOfDay &tod = getTimeOfDay();
    ColorScheduleItems schedule = colourTimeline_->colourScheduleItems();
    std::sort(schedule.begin(), schedule.end());

    if (colourTimeline_ == NULL)
        return;

    switch (mode_)
    {
    case MODE_SUN:
        {
        tod.clearSunAnimations();
        for 
        (
            ColorScheduleItems::const_iterator it = schedule.begin();
            it != schedule.end();
            ++it
        )
        {
            ColorScheduleItem const &item = *it;
            Vector3 color = 
                Vector3
                (
                    item.color_.x*255.0f,
                    item.color_.y*255.0f,
                    item.color_.z*255.0f
                );
            tod.addSunAnimation
            (
                item.normalisedTime_*24.0f,
                color
            );
        }
        }
        break;
    case MODE_AMB:
        {
        tod.clearAmbientAnimations();
        for 
        (
            ColorScheduleItems::const_iterator it = schedule.begin();
            it != schedule.end();
            ++it
        )
        {
            ColorScheduleItem const &item = *it;
            Vector3 color = 
                Vector3
                (
                    item.color_.x*255.0f,
                    item.color_.y*255.0f,
                    item.color_.z*255.0f
                );
            tod.addAmbientAnimation
            (
                item.normalisedTime_*24.0f,
                color
            );
        }
        }
        break;
    }
    WorldManager::instance().environmentChanged();
}


void PageOptionsEnvironment::saveUndoState(string const &description)
{
    // Add a new undo/redo state:
    UndoRedo::instance().add(new EnvironmentUndo());
    UndoRedo::instance().barrier(description, false);
}


void PageOptionsEnvironment::timelineChanged()
{
    rebuildAnimation();
    if (colourTimeline_ != NULL && colourPicker_ != NULL)
    {
        ++filterChange_;

        Vector4 colour = 
            colourTimeline_->getColorScheduleItemSelectedColor();
        colourPicker_->setRGBA(colour);
        rEdit_.SetValue(255.0f*colour.x);
        gEdit_.SetValue(255.0f*colour.y);
        bEdit_.SetValue(255.0f*colour.z);
        ColorScheduleItem *item = 
            colourTimeline_->getColorScheduleItemSelected();
        if (item != NULL)
        {
            float gameTime = 
                item->normalisedTime_*colourTimeline_->totalScheduleTime();
            TimeOfDay &tod = getTimeOfDay();
            tod.gameTime(gameTime);
            string gameTimeStr = tod.getTimeOfDayAsString();
            timeOfDayEdit_.SetWindowText(gameTimeStr.c_str());
            colourTimeline_->showLineAtTime(gameTime);
            timeOfDaySlider_.SetPos(BW_ROUND_TO_INT(gameTime*SLIDER_PREC));
        }

		MF_ASSERT( WorldEditorApp::instance().pythonAdapter() != NULL &&
			"PageOptionsEnvironment::timelineChanged: PythonAdapter is NULL" );
		WorldEditorApp::instance().pythonAdapter()->onSliderAdjust(
            "slrProjectCurrentTime", 
            timeOfDaySlider_.GetPos(), 
            timeOfDaySlider_.GetRangeMin(), 
            timeOfDaySlider_.GetRangeMax() );

        Options::setOptionInt( 
            "graphics/timeofday", 
            int( timeOfDaySlider_.GetPos() * WorldManager::TIME_OF_DAY_MULTIPLIER / SLIDER_PREC ) );

        --filterChange_;        
    }
}


void PageOptionsEnvironment::pickerChanged()
{
    if (colourTimeline_ != NULL && colourPicker_ != NULL)
    {
        ++filterChange_;

        Vector4 colour = colourPicker_->getRGBA();
        colourTimeline_->setColorScheduleItemSelectedColor(colour);
        rEdit_.SetValue(255.0f*colour.x);
        gEdit_.SetValue(255.0f*colour.y);
        bEdit_.SetValue(255.0f*colour.z);


        if (colourTimeline_->itemSelected())
        {
            rebuildAnimation();
        }

        --filterChange_;
    }  
}


void PageOptionsEnvironment::rebuildSkydomeList()
{
    EnviroMinder &enviroMinder = getEnviroMinder();
    skyDomesList_.ResetContent();
    std::vector<Moo::VisualPtr> &skyDomes = enviroMinder.skyDomes();
    for
    (
        std::vector<Moo::VisualPtr>::const_iterator it = skyDomes.begin();
        it != skyDomes.end();
        ++it
    )
    {
        Moo::VisualPtr skyDome = *it;
        string file = BWResource::getFilename(skyDome->resourceID());
        skyDomesList_.AddString(file.c_str());
    }
}
