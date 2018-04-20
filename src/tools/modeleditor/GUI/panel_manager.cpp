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

#include "appmgr/options.hpp"
#include "common/user_messages.hpp"
#include "guitabs/guitabs.hpp"
#include "python_adapter.hpp"
#include "resmgr/string_provider.hpp"

#include "model_editor.h"
#include "main_frm.h"
#include "about_box.hpp"
#include "me_error_macros.hpp"

#include "page_display.hpp"
#include "page_object.hpp"
#include "page_animations.hpp"
#include "page_actions.hpp"
#include "page_lod.hpp"
#include "page_lights.hpp"
#include "page_materials.hpp"
#include "page_messages.hpp"

#include "ual/ual_history.hpp"
#include "ual/ual_manager.hpp"
#include "ual/ual_dialog.hpp"

#include "controls/user_messages.hpp"
#include "controls/message_box.hpp"
#include "cstdmf/restart.hpp"
#include "panel_manager.hpp"

#include <afxdhtml.h>



namespace
{
    class ShortcutsDlg: public CDHtmlDialog
    {
    public:
	    ShortcutsDlg( int ID ): CDHtmlDialog( ID ) {}

	    BOOL ShortcutsDlg::OnInitDialog() 
	    {
		    std::string shortcutsHtml = Options::getOptionString(
			    "help/shortcutsHtml",
			    "resources/html/shortcuts.html");
		    std::string shortcutsUrl = BWResource::resolveFilename( shortcutsHtml );
		    CDHtmlDialog::OnInitDialog();
		    Navigate( shortcutsUrl.c_str() );
		    return TRUE; 
	    }

        /*virtual*/ void OnCancel()
        {
            DestroyWindow();
            s_instance = NULL;
        }

        static ShortcutsDlg *instance()
        {
            if (s_instance == NULL)
            {
                s_instance = new ShortcutsDlg(IDD_SHORTCUTS);
                s_instance->Create(IDD_SHORTCUTS);
            }
            return s_instance;
        }

        static void cleanup()
        {
            if (s_instance != NULL)
                s_instance->OnCancel();
        }

    private:
        static ShortcutsDlg    *s_instance;
    };

    ShortcutsDlg *ShortcutsDlg::s_instance = NULL;
}


BW_SINGLETON_STORAGE( PanelManager )


PanelManager::PanelManager() :
	GUI::ActionMaker<PanelManager>( "doDefaultPanelLayout", &PanelManager::loadDefaultPanels ),
	GUI::ActionMaker<PanelManager, 1>( "doShowSidePanel", &PanelManager::showSidePanel ),
	GUI::ActionMaker<PanelManager, 2>( "doHideSidePanel", &PanelManager::hideSidePanel ),
	GUI::ActionMaker<PanelManager, 3>( "doLoadPanelLayout", &PanelManager::loadLastPanels ),
	GUI::ActionMaker<PanelManager, 4>( "recent_models", &PanelManager::recent_models ),
	GUI::ActionMaker<PanelManager, 5>( "recent_lights", &PanelManager::recent_lights ),
	GUI::ActionMaker<PanelManager, 6>( "doAboutApp", &PanelManager::OnAppAbout ),
	GUI::ActionMaker<PanelManager, 7>( "doToolsReferenceGuide", &PanelManager::OnToolsReferenceGuide ),
	GUI::ActionMaker<PanelManager, 8>( "doContentCreation", &PanelManager::OnContentCreation ),
	GUI::ActionMaker<PanelManager, 9>( "doShortcuts", &PanelManager::OnShortcuts ),
	GUI::ActionMaker<PanelManager, 10>( "setLanguage", &PanelManager::setLanguage ),
	GUI::UpdaterMaker<PanelManager>( "updateSidePanel", &PanelManager::updateSidePanel ),
	GUI::UpdaterMaker<PanelManager, 1>( "updateLanguage", &PanelManager::updateLanguage ),
	mainFrame_( NULL ),
	ready_( false )
{
}


/*static*/ void PanelManager::fini()
{
	ShortcutsDlg::cleanup();

	instance().ready_ = false;

	delete pInstance();
}


/*static*/ bool PanelManager::init( CFrameWnd* mainFrame, CWnd* mainView )
{
	PanelManager* manager = new PanelManager();

	instance().mainFrame_ = mainFrame;
	instance().panels().insertDock( mainFrame, mainView );

	if ( !instance().initPanels() )
		return false;

	return true;
}


void PanelManager::finishLoad()
{
	// show the default panels
	this->panels().showPanel( UalDialog::contentID, true );
	
	PageMessages* msgs = (PageMessages*)(this->panels().getContent(PageMessages::contentID ));
	if (msgs)
	{
		msgs->mainFrame( mainFrame_ );
		msgs->pythonAdapter( CModelEditorApp::instance().pythonAdapter() );
	}

	ready_ = true;
}

bool PanelManager::initPanels()
{
	if ( ready_ )
		return false;

	CWaitCursor wait;

	// UAL Setup
	for ( int i = 0; i < BWResource::getPathNum(); i++ )
	{
		std::string path = BWResource::getPath( i );
		if ( path.find("modeleditor") != -1 )
			continue;
		UalManager::instance().addPath( path );
	}
	UalManager::instance().setConfigFile(
		Options::getOptionString(
			"ualConfigPath",
			"resources/ual/ual_config.xml" ) );

	UalManager::instance().setItemDblClickCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( pInstance(), &PanelManager::ualItemDblClick ) );
	UalManager::instance().setStartDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( pInstance(), &PanelManager::ualStartDrag ) );
	UalManager::instance().setUpdateDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( pInstance(), &PanelManager::ualUpdateDrag ) );
	UalManager::instance().setEndDragCallback(
		new UalFunctor1<PanelManager, UalItemInfo*>( pInstance(), &PanelManager::ualEndDrag ) );
	UalManager::instance().setPopupMenuCallbacks(
		new UalFunctor2<PanelManager, UalItemInfo*, UalPopupMenuItems&>( pInstance(), &PanelManager::ualStartPopupMenu ),
		new UalFunctor2<PanelManager, UalItemInfo*, int>( pInstance(), &PanelManager::ualEndPopupMenu ));
	
	this->panels().registerFactory( new UalDialogFactory() );

	// Setup the map which is used for python
	contentID_["UAL"] = UalDialog::contentID;
	contentID_["Display"] = PageDisplay::contentID;
	contentID_["Object"] = PageObject::contentID;
	contentID_["Animations"] = PageAnimations::contentID;
	contentID_["Actions"] = PageActions::contentID;
	contentID_["LOD"] = PageLOD::contentID;
	contentID_["Lights"] = PageLights::contentID;
	contentID_["Materials"] = PageMaterials::contentID;
	contentID_["Messages"] = PageMessages::contentID;

	// other panels setup
	this->panels().registerFactory( new PageDisplayFactory );
	this->panels().registerFactory( new PageObjectFactory );
	this->panels().registerFactory( new PageAnimationsFactory );
	this->panels().registerFactory( new PageActionsFactory );
	this->panels().registerFactory( new PageLODFactory );
	this->panels().registerFactory( new PageLightsFactory );
	this->panels().registerFactory( new PageMaterialsFactory );
	this->panels().registerFactory( new PageMessagesFactory );
	
	if ( ( (CMainFrame*)mainFrame_ )->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !this->panels().load() )
	{
		loadDefaultPanels( NULL );
	}

	finishLoad();

	return true;
}

bool PanelManager::loadDefaultPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	bool isFirstCall = true;
	if ( ready_ )
	{
		if ( MessageBox( mainFrame_->GetSafeHwnd(),
			L("MODELEDITOR/GUI/PANEL_MANAGER/LOAD_DEFAULT_Q"),
			L("MODELEDITOR/GUI/PANEL_MANAGER/LOAD_DEFAULT"),
			MB_YESNO | MB_ICONQUESTION ) != IDYES )
			return false;

		ready_ = false;
		isFirstCall = false;
		// already has something in it, so clean up first
		this->panels().removePanels();
	}

	if ( item != 0 )
	{
		// not first panel load, so rearrange the toolbars
		((CMainFrame*)mainFrame_)->defaultToolbarLayout();
	}

	GUITABS::PanelHandle basePanel = panels().insertPanel( UalDialog::contentID, GUITABS::RIGHT );
	this->panels().insertPanel( PageObject::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageDisplay::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageAnimations::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageActions::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageLOD::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageLights::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageMaterials::contentID, GUITABS::TAB, basePanel );
	this->panels().insertPanel( PageMessages::contentID, GUITABS::TAB, basePanel );

	if ( !isFirstCall )
		finishLoad();

	return true;
}

bool PanelManager::loadLastPanels( GUI::ItemPtr item )
{
	CWaitCursor wait;
	if ( MessageBox( mainFrame_->GetSafeHwnd(),
		L("MODELEDITOR/GUI/PANEL_MANAGER/LOAD_RECENT_Q"),
		L("MODELEDITOR/GUI/PANEL_MANAGER/LOAD_RECENT"),
		MB_YESNO | MB_ICONQUESTION ) != IDYES )
		return false;

	ready_ = false;

	if ( ( (CMainFrame*)mainFrame_ )->verifyBarState( "TBState" ) )
		mainFrame_->LoadBarState( "TBState" );

	if ( !this->panels().load() )
		loadDefaultPanels( NULL );

	finishLoad();

	return true;
}

bool PanelManager::recent_models( GUI::ItemPtr item )
{
	if (!MeApp::instance().canExit( false ))
		return false;
	
	CModelEditorApp::instance().modelToLoad( (*item)[ "fileName" ] );

	return true;
}

bool PanelManager::recent_lights( GUI::ItemPtr item )
{
	PageLights* lightPage = (PageLights*)PanelManager::instance().panels().getContent( PageLights::contentID );
	
	bool loaded = lightPage->openLightFile( (*item)[ "fileName" ] );

	CModelEditorApp::instance().updateRecentList( "lights" );

	return loaded;
}

bool PanelManager::setLanguage( GUI::ItemPtr item )
{
	std::string languageName = (*item)[ "LanguageName" ];
	std::string countryName = (*item)[ "CountryName" ];

	// Do nothing if we are not changing language
	if (currentLanguageName_ == languageName && currentCountryName_ == countryName)
	{
		return true;
	}

	unsigned int result;
	if (MeApp::instance().isDirty())
	{
		result = MsgBox( L("RESMGR/CHANGING_LANGUAGE_TITLE"), L("RESMGR/CHANGING_LANGUAGE"),
			L("RESMGR/SAVE_AND_RESTART"), L("RESMGR/DISCARD_AND_RESTART"),
			L("RESMGR/RESTART_LATER"), L("RESMGR/CANCEL") ).doModal();
	}
	else
	{
		result = MsgBox( L("RESMGR/CHANGING_LANGUAGE_TITLE"), L("RESMGR/CHANGING_LANGUAGE"),
			L("RESMGR/RESTART_NOW"), L("RESMGR/RESTART_LATER"), L("RESMGR/CANCEL") ).doModal() + 1;
	}
	switch (result)
	{
	case 0:
		Options::setOptionString( "currentLanguage", languageName );
		Options::setOptionString( "currentCountry", countryName );
		MeApp::instance().saveModel();
		startNewInstance();
		AfxGetApp()->GetMainWnd()->PostMessage( WM_COMMAND, ID_APP_EXIT );
		break;
	case 1:	
		Options::setOptionString( "currentLanguage", languageName );
		Options::setOptionString( "currentCountry", countryName );
		MeApp::instance().forceClean();
		startNewInstance();
		AfxGetApp()->GetMainWnd()->PostMessage( WM_COMMAND, ID_APP_EXIT );
		break;
	case 2:
		Options::setOptionString( "currentLanguage", languageName );
		Options::setOptionString( "currentCountry", countryName );
		currentLanguageName_ = languageName;
		currentCountryName_ = countryName;
		break;
	case 3:
		break;
	}
	return true;
}

unsigned int PanelManager::updateLanguage( GUI::ItemPtr item )
{
	if (currentLanguageName_.empty())
	{
		currentLanguageName_ = StringProvider::instance().currentLanguage()->getIsoLangName();
		currentCountryName_ = StringProvider::instance().currentLanguage()->getIsoCountryName();
	}
	return currentLanguageName_ == (*item)[ "LanguageName" ] && currentCountryName_ == (*item)[ "CountryName" ];
}

// App command to run the about dialog
bool PanelManager::OnAppAbout( GUI::ItemPtr item )
{
	CAboutDlg().DoModal();
	return true;
}

bool PanelManager::openHelpFile( const std::string& name, const std::string& defaultFile )
{
	CWaitCursor wait;
	
	std::string helpFile = Options::getOptionString(
		"help/" + name,
		"..\\..\\doc\\" + defaultFile );

	int result = (int)ShellExecute( AfxGetMainWnd()->GetSafeHwnd(), "open", helpFile.c_str() , NULL, NULL, SW_SHOWNORMAL );
	if ( result < 32 )
	{
		ME_WARNING_MSG( L("MODELEDITOR/GUI/MODEL_EDITOR/NO_HELP_FILE", helpFile, name ) );
	}

	return result >= 32;
}

// Open the Tools Reference Guide
bool PanelManager::OnToolsReferenceGuide( GUI::ItemPtr item )
{
	return openHelpFile( "toolsReferenceGuide" , "content_tools_reference_guide.pdf" );
}

// Open the Content Creation Manual (CCM)
bool PanelManager::OnContentCreation( GUI::ItemPtr item )
{
	return openHelpFile( "contentCreationManual" , "content_creation.chm" );
}

// App command to show the keyboard shortcuts
bool PanelManager::OnShortcuts( GUI::ItemPtr item )
{
    ShortcutsDlg::instance()->ShowWindow(SW_SHOW);
	return true;
}

bool PanelManager::ready()
{
	return ready_;
}

void PanelManager::showPanel( std::string& pyID, int show /* = 1 */ )
{
	std::string contentID = contentID_[pyID];

	if ( contentID.length()>0 )
	{
		this->panels().showPanel( contentID, show != 0 );
	}
}

int PanelManager::isPanelVisible( std::string& pyID )
{
	std::string contentID = contentID_[pyID];

	if ( contentID.length()>0 )
	{
		return this->panels().isContentVisible( contentID );
	}
	return 0;
}

void PanelManager::ualItemDblClick( UalItemInfo* ii )
{
	if ( !ii ) return;

	if (CModelEditorApp::instance().pythonAdapter())
	{
		CModelEditorApp::instance().pythonAdapter()->callString( "openFile", 
			BWResource::dissolveFilename( ii->longText() ) );
	}
}

void PanelManager::ualStartDrag( UalItemInfo* ii )
{
	if ( !ii ) return;

	UalManager::instance().dropManager().start( 
		BWResource::getExtension( ii->longText() ));
}

void PanelManager::ualUpdateDrag( UalItemInfo* ii )
{
	if ( !ii ) return;

	SmartPointer< UalDropCallback > dropable = UalManager::instance().dropManager().test( ii );
		
	if ((ii->isFolder()) || (dropable))
	{
		if ((ii->isFolder()) ||
			((dropable && dropable->canAdd() &&
				((GetAsyncKeyState(VK_LCONTROL) < 0) ||
				(GetAsyncKeyState(VK_RCONTROL) < 0) ||
				(GetAsyncKeyState(VK_LMENU) < 0) ||
				(GetAsyncKeyState(VK_RMENU) < 0)))))
		{
			SetCursor( AfxGetApp()->LoadCursor( IDC_ADD_CURSOR ));
		}
		else
		{
			SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );
		}
	}
	else
		SetCursor( AfxGetApp()->LoadStandardCursor( IDC_NO ) );
}

void PanelManager::ualEndDrag( UalItemInfo* ii )
{
	SetCursor( AfxGetApp()->LoadStandardCursor( IDC_ARROW ) );

	if ( !ii ) return;

	if ( ii->isFolder() )
	{
		// folder drag
		CPoint pt;
		GetCursorPos( &pt );
		AfxGetMainWnd()->ScreenToClient( &pt );
		this->panels().clone( (GUITABS::Content*)( ii->dialog() ),
			pt.x - 5, pt.y - 5 );
	}
	else
	{
		UalManager::instance().dropManager().end(ii);
	}
}

void PanelManager::ualStartPopupMenu( UalItemInfo* ii, UalPopupMenuItems& menuItems )
{
	if (!ii) return;
	
	if (!CModelEditorApp::instance().pythonAdapter())
	{
		return;
	}

	std::map<int,std::string> pyMenuItems;
	CModelEditorApp::instance().pythonAdapter()->contextMenuGetItems(
		ii->type(),
		BWResource::dissolveFilename( ii->longText() ).c_str(),
		pyMenuItems );

	if ( !pyMenuItems.size() ) return;

	for( std::map<int,std::string>::iterator i = pyMenuItems.begin();
		i != pyMenuItems.end(); ++i )
	{
		menuItems.push_back( UalPopupMenuItem( (*i).second, (*i).first ) );
	}
}

void PanelManager::ualEndPopupMenu( UalItemInfo* ii, int result )
{
	if (!ii) return;
	
	CModelEditorApp::instance().pythonAdapter()->contextMenuHandleResult(
		ii->type(),
		BWResource::dissolveFilename( ii->longText() ).c_str(),
		result );
}

void PanelManager::ualAddItemToHistory( std::string filePath )
{
	// called from python
	std::string fname = BWResource::getFilename( filePath );
	std::string longText = BWResource::resolveFilename( filePath );
	std::replace( longText.begin(), longText.end(), '/', '\\' );
	UalManager::instance().history().add( AssetInfo( "FILE", fname, longText ));
}


bool PanelManager::showSidePanel( GUI::ItemPtr item )
{
	bool isDockVisible = this->panels().isDockVisible();

	if ( !isDockVisible )
	{
		this->panels().showDock( !isDockVisible );
		this->panels().showFloaters( !isDockVisible );
	}
	return true;
}

bool PanelManager::hideSidePanel( GUI::ItemPtr item )
{
	bool isDockVisible = this->panels().isDockVisible();

	if ( isDockVisible )
	{
		this->panels().showDock( !isDockVisible );
		this->panels().showFloaters( !isDockVisible );
	}
	return true;
}

unsigned int PanelManager::updateSidePanel( GUI::ItemPtr item )
{
	if ( this->panels().isDockVisible() )
		return 0;
	else
		return 1;
}

void PanelManager::updateControls()
{
	this->panels().broadcastMessage( WM_UPDATE_CONTROLS, 0, 0 );
}

void PanelManager::onClose()
{
	if ( Options::getOptionBool( "panels/saveLayoutOnExit", true ) )
	{
		this->panels().save();
		mainFrame_->SaveBarState( "TBState" );
	}
	this->panels().showDock( false );
	UalManager::instance().fini();
}