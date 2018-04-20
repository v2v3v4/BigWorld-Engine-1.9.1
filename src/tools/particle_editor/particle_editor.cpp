/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/*~ module ParticleEditor
 *	@components{ particleeditor }
 *
 *	The ParticleEditor Module is a Python module that provides an interface to
 *	the various information about the particles(s) loaded into ParticleEditor.
 *	It also provides an interface to change and edit particle-specific information
 *	and the various ParticleEditor preferences.
 */
#include "pch.hpp"
#include "particle_editor.hpp"
#include "main_frame.hpp"
#include "pe_app.hpp"
#include "particle_editor_doc.hpp"
#include "particle_editor_view.hpp"
#include "about_dlg.hpp"
#include "common/compile_time.hpp"
#include "common/tools_camera.hpp"
#include "shell/pe_module.hpp"
#include "shell/pe_shell.hpp"
#include "gui/panel_manager.hpp"
#include "controls/dir_dialog.hpp"
#include "gui/action_selection.hpp"
#include "appmgr/app.hpp"
#include "appmgr/options.hpp"
#include "guimanager/gui_menu.hpp"
#include "guimanager/gui_toolbar.hpp"
#include "guimanager/gui_functor_python.hpp"
#include "moo/render_context.hpp"
#include "common/tools_common.hpp"
#include "common/format.hpp"
#include "common/cooperative_moo.hpp"
#include "common/command_line.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/timestamp.hpp"
#include "cstdmf/bgtask_manager.hpp"
#include "chunk/chunk_manager.hpp"
#if UMBRA_ENABLE
#include "chunk/chunk_umbra.hpp"
#endif
#include "particle/meta_particle_system.hpp"
#include "particle/py_meta_particle_system.hpp"
#include "chunk/chunk_loader.hpp"
#include "gizmo/gizmo_manager.hpp"
#include "gizmo/tool_manager.hpp"
#include "ual/ual_dialog.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "romp/lens_effect_manager.hpp"
#include "resmgr/string_provider.hpp"
#include "resmgr/auto_config.hpp"
#include "common/directory_check.hpp"
#include "common/string_utils.hpp"
#include "cstdmf/restart.hpp"

#include <windows.h>
#include <afxdhtml.h>

using namespace std;

DECLARE_DEBUG_COMPONENT2( "ParticleEditor", 0 )

static AutoConfigString s_LanguageFile( "system/language" );

namespace
{
    // A helper class that allows display of a html dialog for the shortcut
    // information.
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
                s_instance = new ShortcutsDlg(IDD_KEY_CUTS);
                s_instance->Create(IDD_KEY_CUTS);
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

// Make sure that these items are linked in:
extern int PyModel_token;
static int s_chunkTokenSet = PyModel_token;

// Update via python script:
static const bool c_useScripting = false;

// The one and only ParticleEditorApp object:
ParticleEditorApp theApp;

BEGIN_MESSAGE_MAP(ParticleEditorApp, CWinApp)
END_MESSAGE_MAP()

/*static*/ ParticleEditorApp *ParticleEditorApp::s_instance = NULL;

ParticleEditorApp::ParticleEditorApp() :
	m_appShell(NULL),
	m_peApp(NULL),
	mfApp_( NULL ),
	m_desiredFrameRate(60.0f),
	m_state(PE_PLAYING)
{
    ASSERT(s_instance == NULL);
    s_instance = this;
}

ParticleEditorApp::~ParticleEditorApp()
{
    ASSERT(s_instance != NULL);
	s_instance = NULL;
}

namespace
{
	char * s_lpCmdLine = NULL;

}

bool ParticleEditorApp::InitialiseMF( std::string &openFile )
{
	DirectoryCheck( "ParticleEditor" );
	
	// parse command line
	const int MAX_ARGS = 20;
	char * argv[ MAX_ARGS ];
	int argc = 0;

	char * str = s_lpCmdLine;
	while (char * token = StringUtils::retrieveCmdToken( str ))
	{
		if (argc >= MAX_ARGS)
		{
			ERROR_MSG( "ModelEditor::parseCommandLineMF: Too many arguments!!\n" );
			return FALSE;
		}
		if (argc && (!strcmp( argv[ argc-1 ], "-o" ) || !strcmp( argv[ argc-1 ], "-O" )))
		{
			openFile = std::string( token );
		}
		argv[argc++] = token;
	}
	
	return Options::init( argc, argv, false ) && BWResource::init( argc, (const char **)argv );
}


// ParticleEditorApp initialisation
BOOL ParticleEditorApp::InitInstance()
{
	waitForRestarting();

    // InitCommonControls() is required on Windows XP if an application
    // manifest specifies use of ComCtl32.dll version 6 or later to enable
    // visual styles.  Otherwise, any window creation will fail.
    InitCommonControls();

    CWinApp::InitInstance();

	s_lpCmdLine = new char[ strlen( m_lpCmdLine ) + 1];
	strcpy( s_lpCmdLine, m_lpCmdLine );

    // Get the command line before ParseCommandLine has a go at it.
    std::string commandLine = m_lpCmdLine;

    // Initialise OLE libraries
    if (!AfxOleInit())
    {
        AfxMessageBox(IDP_OLE_INIT_FAILED);
        return FALSE;
    }
    AfxEnableControlContainer();
    // Standard initialisation
    // If you are not using these features and wish to reduce the size
    // of your final executable, you should remove from the following
    // the specific initialisation routines you do not need
    // Change the registry key under which our settings are stored
    // TODO: You should modify this string to be something appropriate
    // such as the name of your company or organization
    SetRegistryKey(_T("Local AppWizard-Generated Applications"));
    LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)
    // Register the application's document templates.  Document templates
    //  serve as the connection between documents, frame windows and views
    CSingleDocTemplate* pDocTemplate;
    pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(ParticleEditorDoc),
        RUNTIME_CLASS(MainFrame),       // main SDI frame window
        RUNTIME_CLASS(ParticleEditorView));
    AddDocTemplate(pDocTemplate);

    // Initialise MF things
	std::string openFile;
    if (!InitialiseMF(openFile))
        return 0;

	// initialise language provider
	if( !s_LanguageFile.value().empty() )
		StringProvider::instance().load( BWResource::openSection( s_LanguageFile ) );
	std::vector<DataSectionPtr> languages;
	Options::pRoot()->openSections( "language", languages );
	if (!languages.empty())
	{
		for( std::vector<DataSectionPtr>::iterator iter = languages.begin(); iter != languages.end(); ++iter )
			if( !(*iter)->asString().empty() )
				StringProvider::instance().load( BWResource::openSection( (*iter)->asString() ) );
	}
	else
	{
		// Force English:
		StringProvider::instance().load( BWResource::openSection("helpers/languages/particleeditor_gui_en.xml"));
		StringProvider::instance().load( BWResource::openSection("helpers/languages/particleeditor_rc_en.xml" ));
		StringProvider::instance().load( BWResource::openSection("helpers/languages/files_en.xml"             ));
	}

	std::string currentLanguage = Options::getOptionString( "currentLanguage", "" );
	std::string currentCountry  = Options::getOptionString( "currentCountry", "" );
	if ( currentLanguage != "" )
		StringProvider::instance().setLanguages( currentLanguage, currentCountry );
	else
		StringProvider::instance().setLanguage();
	
    // Check the use-by date
    if (!ToolsCommon::canRun())
    {
        ToolsCommon::outOfDateMessage( "ParticleEditor" );
        return FALSE;
    }
	
	WindowTextNotifier::instance();
    
    // Parse command line for standard shell commands, DDE, file open
	MFCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    // Dispatch commands specified on the command line.  Will return FALSE if
    // app was launcAhed with /RegServer, /Register, /Unregserver or /Unregister.
    if (!ProcessShellCommand(cmdInfo))
    {
        ERROR_MSG("ParticleEditorApp::InitInstance - ProcessShellCommand failed\n");
        return FALSE;
    }

    // The one and only window has been initialised, so show and update it
    m_pMainWnd->ShowWindow(SW_SHOWMAXIMIZED);
    m_pMainWnd->UpdateWindow();
    // call DragAcceptFiles only if there's a suffix
    //  In an SDI app, this should occur after ProcessShellCommand

    MainFrame *mainFrame = (MainFrame *)(m_pMainWnd);
    CView     *mainView  = mainFrame->GetActiveView();

	mainFrame->UpdateTitle();

    // create the app and module
    ASSERT(!mfApp_);
    mfApp_ = new App;

    ASSERT(!m_appShell);
    m_appShell = new PeShell;    

    HINSTANCE hInst = AfxGetInstanceHandle();
    
    if 
    (
        !mfApp_->init
        ( 
            hInst, 
            mainFrame->m_hWnd, 
            mainView->m_hWnd, 
            PeShell::initApp 
        )
    )
    {
        ERROR_MSG( "ParticleEditorApp::InitInstance - init failed\n" );
        return FALSE;
    }

    m_peApp = new PeApp();

	CooperativeMoo::init();

	GUI::Manager::init();

	// Must do this after the panels are inited, they init GUI::Manager.
	GUI::Manager::instance().pythonFunctor().defaultModule( "MenuUIAdapter" );
	GUI::Manager::instance().optionFunctor().setOption(this);
	DataSectionPtr section = BWResource::openSection("resources/data/gui.xml");
	for (int i = 0; i < section->countChildren(); ++i)
		GUI::Manager::instance().add(new GUI::Item(section->openChild(i)));

	// Setup the main menu:
	GUI::Manager::instance().add
    (
        new GUI::Menu("MainMenu", AfxGetMainWnd()->GetSafeHwnd()) 
    );

	updateLanguageList();

	AfxGetMainWnd()->DrawMenuBar();

	// Add the toolbar(s) using the BaseMainFrame helper method
	mainFrame->createToolbars( "AppToolbars" );

    // GUITABS Tearoff tabs system init and setup
	PanelManager::init(mainFrame, mainView);

    // kick off the chunk loading
	BgTaskManager::instance().startThreads( 1 );
    
	if ( !openFile.empty() )
	{
		openDirectory( BWResource::getFilePath( openFile ) );
		update();
		std::string psName = BWResource::getFilename( openFile );
		psName = BWResource::removeExtension( psName );
        bool ok =
			((MainFrame*)AfxGetMainWnd())->SelectParticleSystem( psName );
        if (!ok)
        {
            AfxMessageBox(L("RCS_IDS_COULDNTOPENFILE", openFile));
        }
    }
	
	/* Disable Umbra if it is enabled
	 * This fixes mouse lag issues caused by the present thread allowing the cpu to 
	 * get a few frames ahead of the GPU and then stalling for it to catch up
	 * Note identical code is set in the Model Editor InitInstance code, please
	 * update it if you update the code below.
	 */
	#ifdef UMBRA_ENABLE
	if (Options::getOptionInt( "render/useUmbra", 1) == 1)
	{
		WARNING_MSG( "Umbra is enabled in Particle Editor, It will now be disabled\n" );
	}
	Options::setOptionInt( "render/useUmbra", 0 );
	UmbraHelper::instance().umbraEnabled( false );
	#endif

    return TRUE;
}


bool ParticleEditorApp::InitPyScript()
{
    // make a python dictionary here
    PyObject *pScript     = PyImport_ImportModule("pe_shell");
    PyObject *pScriptDict = PyModule_GetDict(pScript);
    PyObject *pInit       = PyDict_GetItemString(pScriptDict, "init");
    if (pInit != NULL)
    {
        PyObject * pResult = PyObject_CallFunction(pInit, "");

        if (pResult != NULL)
        {
            Py_DECREF(pResult);
        }
        else
        {
            PyErr_Print();
            return false;
        }
    }
    else
    {
        PyErr_Print();
        return false;
    }
    return true;
}

int ParticleEditorApp::ExitInstance() 
{
	if ( mfApp_ )
	{
		ShortcutsDlg::cleanup();

		GizmoManager::instance().removeAllGizmo();
		while ( ToolManager::instance().tool() )
		{
			ToolManager::instance().popTool();
		}

		PanelManager::fini();

		//GeneralEditor::Editors none;
		//GeneralEditor::currentEditors(none);

		mfApp_->fini();
		delete mfApp_;
		mfApp_ = NULL;

		delete m_peApp; 
		m_peApp = NULL;

		m_appShell->fini();
		delete m_appShell; 
		m_appShell = NULL;

		GUI::Manager::fini();
	
		WindowTextNotifier::fini();	
		Options::fini();
	}

	delete [] s_lpCmdLine;
	return CWinApp::ExitInstance();
}

void ParticleEditorApp::updateLanguageList()
{
	GUI::ItemPtr languageList = GUI::Manager::instance()( "/MainMenu/Languages/LanguageList" );
	if( languageList )
	{
		while( languageList->num() )
			languageList->remove( 0 );
		for( unsigned int i = 0; i < StringProvider::instance().languageNum(); ++i )
		{
			LanguagePtr l = StringProvider::instance().getLanguage( i );
			std::stringstream name, displayName;
			name << "language" << i;
			displayName << '&' << l->getLanguageName();
			GUI::ItemPtr item = new GUI::Item( "CHILD", name.str(), displayName.str(),
				"",	"", "", "setLanguage", "updateLanguage", "" );
			item->set( "LanguageName", l->getIsoLangName() );
			item->set( "CountryName", l->getIsoCountryName() );
			languageList->add( item );
		}
	}
}

BOOL ParticleEditorApp::OnIdle(LONG lCount)
{    
	// The following two lines need to be uncommented for toolbar docking to
	// work properly, and the application's toolbars have to inherit from
	// GUI::CGUIToolBar
	if ( CWinApp::OnIdle( lCount ) )
		return TRUE; // give priority to windows GUI as MS says it should be.

    HWND foreWindow = GetForegroundWindow();
    CWnd *mainFrame = MainFrame::instance();

	bool isWindowActive =
		foreWindow == mainFrame->m_hWnd || GetParent( foreWindow ) == mainFrame->m_hWnd;

	if (!CooperativeMoo::canUseMoo( isWindowActive ) || !isWindowActive)
    {
		// If activate failed, because the app is minimised, there's not enough
		// videomem to restore, or the app is in the background and other apps
		// we need to cooperate with are running, we just try again later.
		mfApp_->calculateFrameTime(); // Do this to effectively freeze time
    }
    else
    {
		// measure the update time
		uint64 beforeTime = timestamp();

		update();

		uint64 afterTime = timestamp();
		float lastUpdateMilliseconds = (float) (((int64)(afterTime - beforeTime)) / stampsPerSecondD()) * 1000.f;

        if (m_desiredFrameRate > 0)
        {
            // limit the frame rate
            const float desiredFrameTime = 1000.f/m_desiredFrameRate; // milliseconds
            float lastFrameTime = PeModule::instance().lastTimeStep();

            if (desiredFrameTime > lastUpdateMilliseconds)
            {
                float compensation = desiredFrameTime - lastUpdateMilliseconds;
                compensation = std::min(compensation, 2000.f);
                Sleep((int)compensation);
            }
            MainFrame::instance()->UpdateGUI();
        }
    }

    return TRUE;
}


void ParticleEditorApp::update()
{
    if (!c_useScripting)
    {
        static bool firstUpdate = true;
        if (firstUpdate)
        {
            MainFrame::instance()->InitialiseMetaSystemRegister();
            firstUpdate = false;
        }
        mfApp_->updateFrame();        
    }
    else
    {
        PyObject *pScript     = PyImport_ImportModule("pe_shell");
        PyObject *pScriptDict = PyModule_GetDict(pScript);
        PyObject *pUpdate     = PyDict_GetItemString(pScriptDict, "update");
        if (pUpdate != NULL)
        {
            PyObject * pResult = PyObject_CallFunction(pUpdate, "");
            if (pResult != NULL)
            {
                Py_DECREF( pResult );
            }
            else
            {
                PyErr_Print();
            }
        }
        else
        {
            PyErr_Print();
        }
    }
}

/*~ function ParticleEditor.update
 *	@components{ particleeditor }
 *
 *	This function forces ParticleEditor to update each of its modules.
 */
PY_MODULE_STATIC_METHOD( ParticleEditorApp, update, ParticleEditor )

PyObject * ParticleEditorApp::py_update( PyObject * args )
{
	if (ParticleEditorApp::instance().mfApp())
    {
        // update all of the modules
        ParticleEditorApp::instance().mfApp()->updateFrame();
    }
    Py_Return;
}

/*~ function ParticleEditor.particleSystem
 *	@components{ particleeditor }
 *
 *	This function returns the currently selected MetaParticleSystem.
 *
 *	@return Returns the currently selected MetaParticleSystem object.
 */
PY_MODULE_STATIC_METHOD(ParticleEditorApp, particleSystem, ParticleEditor)

PyObject *ParticleEditorApp::py_particleSystem(PyObject * args)
{
    if (MainFrame::instance()->IsMetaParticleSystem())
	{
        return Script::getData( new PyMetaParticleSystem(MainFrame::instance()->GetMetaParticleSystem()));
	}

    if (MainFrame::instance()->IsCurrentParticleSystem())
	{
        return Script::getData(MainFrame::instance()->GetCurrentParticleSystem());
	}

    Py_Return;
}

void ParticleEditorApp::OnDirectoryOpen()
{
    DirDialog dlg; 

    dlg.windowTitle_ = L("PARTICLEEDITOR/OPEN");
    dlg.promptText_  = L("PARTICLEEDITOR/CHOOSE_PS_DIR");
    dlg.fakeRootDirectory_ = dlg.basePath();

    // Set the start directory, check if exists:
    dlg.startDirectory_ = dlg.basePath();
    string fullDirectory =
        BWResource::resolveFilename
        (
            MainFrame::instance()->ParticlesDirectory().GetBuffer()
        );
    WIN32_FIND_DATA data;
    HANDLE hFind;
    char lastChar = fullDirectory.at(fullDirectory.length()-1);
    if ( (lastChar != '/') && (lastChar != '\\') )
        fullDirectory += "/";
    
    hFind = FindFirstFile((fullDirectory + "*").c_str(), &data);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        dlg.startDirectory_ = 
            BWResource::resolveFilename
            (
                MainFrame::instance()->ParticlesDirectory().GetBuffer()
            ).c_str();
        FindClose(hFind);
    }

    if (dlg.doBrowse(AfxGetApp()->m_pMainWnd)) 
    {
        openDirectory((LPCTSTR)dlg.userSelectedDirectory_);
    }
}

void
ParticleEditorApp::setState(State state)
{    
    MainFrame *mainFrame = MainFrame::instance();
    switch (state)
    {
    case PE_PLAYING:
		mfApp_->pause( false );
        if (m_state != PE_PAUSED)
        {
            if (mainFrame->IsMetaParticleSystem())
            {
				mainFrame->GetMetaParticleSystem()->clear(); 
				mainFrame->GetMetaParticleSystem()->spawn(); 
            }
        }
		else 
		{
			mainFrame->appendOneShotPS();
		}
        break;
    case PE_STOPPED:
        mfApp_->pause( true );
        if (mainFrame->IsMetaParticleSystem())
		{
			mainFrame->GetMetaParticleSystem()->clear();
			// remove flares when stopping particle editor
			LensEffectManager::instance().clear();
			mainFrame->clearAppendedPS();
		}
        break;
    case PE_PAUSED:
        mfApp_->pause( true );
        break;
    }
    m_state = state;
}

ParticleEditorApp::State ParticleEditorApp::getState() const
{
    return m_state;
}




void ParticleEditorApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

bool ParticleEditorApp::openHelpFile( const std::string& name, const std::string& defaultFile )
{
	CWaitCursor wait;
	
	std::string helpFile = Options::getOptionString(
		"help/" + name,
		"..\\..\\doc\\" + defaultFile );

	int result = (int)ShellExecute( AfxGetMainWnd()->GetSafeHwnd(), "open", helpFile.c_str() , NULL, NULL, SW_SHOWNORMAL );
	if ( result < 32 )
	{
		::MessageBox
        ( 
            AfxGetMainWnd()->GetSafeHwnd(), 
            L("PARTICLEEDITOR/MAIN/PARTICLE_EDITOR/UNABLE_LOCATE_BODY", name, helpFile), 
            L("PARTICLEEDITOR/MAIN/PARTICLE_EDITOR/UNABLE_LOCATE_TITLE", name),  
            MB_OK 
        );
	}

	return result >= 32;
}

// Open the Tools Reference Guide
void ParticleEditorApp::OnToolsReferenceGuide()
{
	openHelpFile( "toolsReferenceGuide" , "content_tools_reference_guide.pdf" );
}

// Open the Content Creation Manual (CCM)
void ParticleEditorApp::OnContentCreation()
{
	openHelpFile( "contentCreationManual" , "content_creation.chm" );
}

// App command to show the keyboard shortcuts
void ParticleEditorApp::OnAppShortcuts()
{
    ShortcutsDlg::instance()->ShowWindow(SW_SHOW);
}

void 
ParticleEditorApp::openDirectory
(
    string      const &dir_, 
    bool        forceRefresh    /*= false*/
)
{
    string dir = BWResource::formatPath(dir_);

    string relativeDirectory = BWResource::dissolveFilename(dir);

    // check if directory changed
    if 
    (
        MainFrame::instance()->ParticlesDirectory() 
        != 
        CString(relativeDirectory.c_str())
        ||
        forceRefresh
    )
    {
        MainFrame::instance()->PromptSave(MB_YESNO, true);
        // record the change
        MainFrame::instance()->ParticlesDirectory(relativeDirectory.c_str());
        ParticleEditorDoc::instance().SetTitle(relativeDirectory.c_str());
        MainFrame::instance()->PotentiallyDirty( false );       
        // tell the window
        MainFrame::instance()->InitialiseMetaSystemRegister();
    }
}

void ParticleEditorApp::OnFileSaveParticleSystem()
{
    MainFrame::instance()->ForceSave();
}

void ParticleEditorApp::OnViewShowSide()
{
    bool vis = PanelManager::instance().isPanelVisible( ActionSelection::contentID );

	PanelManager::instance().showPanel( ActionSelection::contentID, !vis );
}

void ParticleEditorApp::OnViewShowUAL()
{
	bool vis = PanelManager::instance().isPanelVisible( UalDialog::contentID );

	PanelManager::instance().showPanel( UalDialog::contentID, !vis );
}

/*virtual*/ string ParticleEditorApp::get(string const &key) const
{
    return Options::getOptionString(key);
}

/*virtual*/ bool ParticleEditorApp::exist(string const &key) const
{
    return Options::optionExists(key);
}

/*virtual*/ void ParticleEditorApp::set(string const &key, string const &value)
{
    return Options::setOptionString(key, value);
}

/*~ function ParticleEditor.openFile
 *	@components{ particleeditor }
 *
 *	This function enables the Open ParticleSystem dialog, which allows a ParticleSystem to be loaded.
 */
static PyObject *py_openFile(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnDirectoryOpen();

	Py_Return;
}
PY_MODULE_FUNCTION(openFile, ParticleEditor)

/*~ function ParticleEditor.savePS
 *	@components{ particleeditor }
 *
 *	This function saves any changes made to the currently selected ParticleSystem.
 */
static PyObject *py_savePS(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnFileSaveParticleSystem();

	Py_Return;
}
PY_MODULE_FUNCTION(savePS, ParticleEditor)

/*~ function ParticleEditor.reloadTextures
 *	@components{ particleeditor }
 *
 *	This function forces ParticleEditor to reload all its textures.
 */
static PyObject * py_reloadTextures( PyObject * args )
{
	CWaitCursor wait;
	
	Moo::ManagedTexture::accErrs(true);
	
	Moo::TextureManager::instance()->reloadAllTextures();

	std::string errStr = Moo::ManagedTexture::accErrStr();
	if (errStr != "")
	{
		ERROR_MSG
        ( 
            "Moo:ManagedTexture::load, unable to load the following textures:\n"
			"%s\n\nPlease ensure these textures exist.", 
            errStr.c_str() 
        );
	}

	Moo::ManagedTexture::accErrs(false);

	Py_Return;
}
PY_MODULE_FUNCTION(reloadTextures, ParticleEditor)

/*~ function ParticleEditor.exit
 *	@components{ particleeditor }
 *
 *	This function closes ParticleEditor.
 */
static PyObject *py_exit(PyObject * /*args*/)
{
    AfxGetApp()->GetMainWnd()->PostMessage( WM_COMMAND, ID_APP_EXIT );

	Py_Return;
}
PY_MODULE_FUNCTION(exit, ParticleEditor)

/*~ function ParticleEditor.showToolbar
 *	@components{ particleeditor }
 *
 *	This function shows the specified toolbar.
 *
 *	@param str	The name of the toolbar to show.
 */
static PyObject *py_showToolbar(PyObject* args)
{
	if ( !MainFrame::instance() )
		return PyInt_FromLong( 0 );

	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_showToolbar: Argument parsing error." );
		return NULL;
	}
	MainFrame::instance()->showToolbarIndex( atoi( str ) );

    Py_Return;
}
PY_MODULE_FUNCTION(showToolbar, ParticleEditor)

/*~ function ParticleEditor.hideToolbar
 *	@components{ particleeditor }
 *
 *	This function hides the specified toolbar.
 *
 *	@param str	The name of the toolbar to hide.
 */
static PyObject *py_hideToolbar(PyObject* args)
{
	if ( !MainFrame::instance() )
		return PyInt_FromLong( 0 );

	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_showToolbar: Argument parsing error." );
		return NULL;
	}
	MainFrame::instance()->hideToolbarIndex( atoi( str ) );

    Py_Return;
}
PY_MODULE_FUNCTION(hideToolbar, ParticleEditor)

/*~ function ParticleEditor.updateShowToolbar
 *	@components{ particleeditor }
 *
 *	This function updates the status of the tick next to the specified toolbar in the
 *	View->Toolbars menu.
 *
 *	@param str	The name of the toolbar to update status.
 *
 *	@return Returns True (0) if the toolbar is currently shown, False (0) otherwise.
 */
static PyObject *py_updateShowToolbar(PyObject* args)
{
	if ( !MainFrame::instance() )
		return PyInt_FromLong( 0 );

	char * str;
	if (!PyArg_ParseTuple( args, "s", &str ))
	{
		PyErr_SetString( PyExc_TypeError,
			"py_showToolbar: Argument parsing error." );
		return NULL;
	}

	return PyInt_FromLong(
		MainFrame::instance()->updateToolbarIndex( atoi( str ) ) );
}
PY_MODULE_FUNCTION(updateShowToolbar, ParticleEditor)
 
/*~ function ParticleEditor.showStatusbar
 *	@components{ particleeditor }
 *
 *	This function shows the status bar.
 */
static PyObject *py_showStatusbar(PyObject * /*args*/)
{
    MainFrame::instance()->ShowControlBar
    (
        &MainFrame::instance()->getStatusBar(),
        TRUE,
        FALSE
    );
    Py_Return;
}
PY_MODULE_FUNCTION(showStatusbar, ParticleEditor)

/*~ function ParticleEditor.hideStatusbar
 *	@components{ particleeditor }
 *
 *	This function hides the status bar.
 */
static PyObject *py_hideStatusbar(PyObject * /*args*/)
{
    MainFrame::instance()->ShowControlBar
    (
        &MainFrame::instance()->getStatusBar(),
        FALSE,
        FALSE
    );
    Py_Return;
}
PY_MODULE_FUNCTION(hideStatusbar, ParticleEditor)

/*~ function ParticleEditor.updateShowStatusbar
 *	@components{ particleeditor }
 *
 *	This function updates the status of the tick next to the status bar in the view menu.
 *
 *	@return Returns True (0) if the toolbar is currently shown, False (1) otherwise.
 */
static PyObject *py_updateShowStatusbar(PyObject * /*args*/)
{
    bool valid = 
        MainFrame::instance() != NULL
        &&
        ::IsWindow(MainFrame::instance()->getStatusBar().GetSafeHwnd());
    bool visible = true;
    if (valid)
        visible = MainFrame::instance()->getStatusBar().IsWindowVisible() != FALSE;
    return PyInt_FromLong(visible ? 0 : 1);
}
PY_MODULE_FUNCTION(updateShowStatusbar, ParticleEditor)

/*~ function ParticleEditor.toggleShowPanels
 *	@components{ particleeditor }
 *
 *	This function toggles the panels' visibility.
 */
static PyObject *py_toggleShowPanels(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnViewShowSide();
    Py_Return;
}
PY_MODULE_FUNCTION(toggleShowPanels, ParticleEditor)

/*~ function ParticleEditor.toggleShowUALPanel
 *	@components{ particleeditor }
 *
 *	This function toggles the UAL panel's visibility.
 */
static PyObject *py_toggleShowUALPanel(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnViewShowUAL();
    Py_Return;
}
PY_MODULE_FUNCTION(toggleShowUALPanel, ParticleEditor)

/*~ function ParticleEditor.loadDefaultPanels
 *	@components{ particleeditor }
 *
 *	This function loads the default panel arrangement.
 */
static PyObject *py_loadDefaultPanels(PyObject * /*args*/)
{
    XMLSectionPtr data = new XMLSection("ActionSelection_state");
    ActionSelection::instance()->saveState(data);
    PanelManager   ::instance().loadDefaultPanels(NULL);
    ActionSelection::instance()->restoreState(data);
    Py_Return;
}
PY_MODULE_FUNCTION(loadDefaultPanels, ParticleEditor)

/*~ function ParticleEditor.loadRecentPanels
 *	@components{ particleeditor }
 *
 *	This function loads the most recent panel arrangement.
 */
static PyObject *py_loadRecentPanels(PyObject * /*args*/)
{
    XMLSectionPtr data = new XMLSection("ActionSelection_state");
    ActionSelection::instance()->saveState(data);
    PanelManager   ::instance().loadLastPanels(NULL);
    ActionSelection::instance()->restoreState(data);
    Py_Return;
}
PY_MODULE_FUNCTION(loadRecentPanels, ParticleEditor)



/*~ function ParticleEditor.aboutApp
 *	@components{ particleeditor }
 *
 *	This function displays the ParticleEditor's About box.
 */
static PyObject *py_aboutApp(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnAppAbout();
    Py_Return;
}
PY_MODULE_FUNCTION(aboutApp, ParticleEditor)

/*~ function ParticleEditor.doToolsReferenceGuide
 *	@components{ particleeditor }
 *
 *	This function opens the Content Tools Reference Guide PDF.
 */
static PyObject *py_doToolsReferenceGuide(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnToolsReferenceGuide();
    Py_Return;
}
PY_MODULE_FUNCTION(doToolsReferenceGuide, ParticleEditor)

/*~ function ParticleEditor.doContentCreation
 *	@components{ particleeditor }
 *
 *	This function opens the BigWorld Technology Content Creation Manual.
 */
static PyObject *py_doContentCreation(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnContentCreation();
    Py_Return;
}
PY_MODULE_FUNCTION(doContentCreation, ParticleEditor)

/*~ function ParticleEditor.doShortcuts
 *	@components{ particleeditor }
 *
 *	This function opens the ParticleEditor's Shortcuts dialog.
 */
static PyObject *py_doShortcuts(PyObject * /*args*/)
{
    ParticleEditorApp::instance().OnAppShortcuts();
    Py_Return;
}
PY_MODULE_FUNCTION(doShortcuts, ParticleEditor)

/*~ function ParticleEdition.zoomToExtents
 *
 *	This function centres the particle system in view and zooms the camera until
 *	the particle system just fits in view.
 */
static PyObject * py_zoomToExtents( PyObject * args )
{
	PeShell::instance().camera().zoomToExtents( true );
	Py_Return;
}
PY_MODULE_FUNCTION( zoomToExtents, ParticleEditor )

/*~ function ParticleEditor.doViewFree
 *	@components{ particleeditor }
 *
 *	This function enables the free view camera mode.
 */
static PyObject *py_doViewFree(PyObject * /*args*/)
{
    MainFrame::instance()->OnButtonViewFree();
    Py_Return;
}
PY_MODULE_FUNCTION(doViewFree, ParticleEditor)

/*~ function ParticleEditor.doViewX
 *	@components{ particleeditor }
 *
 *	This function positions the camera to look toward the origin along the X-axis.
 */
static PyObject *py_doViewX(PyObject * /*args*/)
{
    MainFrame::instance()->OnButtonViewX();
    Py_Return;
}
PY_MODULE_FUNCTION(doViewX, ParticleEditor)

/*~ function ParticleEditor.doViewY
 *	@components{ particleeditor }
 *
 *	This function positions the camera to look toward the origin along the Y-axis.
 */
static PyObject *py_doViewY(PyObject * /*args*/)
{
    MainFrame::instance()->OnButtonViewY();
    Py_Return;
}
PY_MODULE_FUNCTION(doViewY, ParticleEditor)

/*~ function ParticleEditor.doViewZ
 *	@components{ particleeditor }
 *
 *	This function positions the camera to look toward the origin along the Z-axis.
 */
static PyObject *py_doViewZ(PyObject * /*args*/)
{
    MainFrame::instance()->OnButtonViewZ();
    Py_Return;
}
PY_MODULE_FUNCTION(doViewZ, ParticleEditor)

/*~ function ParticleEditor.doViewOrbit
 *	@components{ particleeditor }
 *
 *	This function enables the orbit view camera mode.
 */
static PyObject *py_doViewOrbit(PyObject * /*args*/)
{
    MainFrame::instance()->OnButtonViewOrbit();
    Py_Return;
}
PY_MODULE_FUNCTION(doViewOrbit, ParticleEditor)

/*~ function ParticleEditor.cameraMode
 *	@components{ particleeditor }
 *
 *	This function returns which camera mode is currently being used.
 *
 *	@return Returns 0 if in free view, returns 1 if in x-locked view,
 *			returns 2 if in y-locked view, returns 3 if in z-locked view,
 *			returns 4 if in orbit view.
 */
static PyObject *py_cameraMode(PyObject * /*args*/)
{
    int result = PeShell::instance().camera().mode();
    return PyInt_FromLong(result);
}
PY_MODULE_FUNCTION(cameraMode, ParticleEditor)

/*~ function ParticleEditor.camera
 *	@components{ particleeditor }
 *
 *	This function returns the ParticleEditor camera.
 *
 *	@return The ParticleEditor camera object.
 */
static PyObject *py_camera(PyObject *args)
{
	Py_INCREF(&PeShell::instance().camera());
	return &PeShell::instance().camera();
}
PY_MODULE_FUNCTION(camera, ParticleEditor)

/*~ function ParticleEditor.doSetBkClr
 *	@components{ particleeditor }
 *
 *	This function enables ParticleEditor's Colour Picker dialog, which allows the 
 *	background colour to be changed.
 */
static PyObject *py_doSetBkClr(PyObject * /*args*/)
{
    MainFrame::instance()->OnBackgroundColor();
    Py_Return;
}
PY_MODULE_FUNCTION(doSetBkClr, ParticleEditor)

/*~ function ParticleEditor.updateBkClr
 *	@components{ particleeditor }
 *
 *	This function checks whether a background colour is currently set.
 *
 *	@return Returns True (0) if a background colour is set, False (1) otherwise.
 */
static PyObject *py_updateBkClr(PyObject * /*args*/)
{
    if (MainFrame::instance() != NULL)
    {
        return 
            PyInt_FromLong
            (
		        MainFrame::instance()->showingBackgroundColor() ? 0 : 1 
            );
    }
    else
    {
        return PyInt_FromLong(1);
    }
}
PY_MODULE_FUNCTION(updateBkClr, ParticleEditor)

/*~ function ParticleEditor.showGrid
 *	@components{ particleeditor }
 *
 *	This function toggles the display of the 1x1m measurement grid.
 */
static PyObject *py_showGrid(PyObject * /*args*/)
{
	Options::setOptionInt( "render/showGrid", !Options::getOptionInt( "render/showGrid", 0 ));
	GUI::Manager::instance().update();
    Py_Return;
}
PY_MODULE_FUNCTION(showGrid, ParticleEditor)

/*~ function ParticleEditor.isShowingGrid
 *	@components{ particleeditor }
 *
 *	This function checks whether the 1x1m measurement grid is currently being displayed.
 *
 *	@return Returns True (1) if the 1x1m measurement grid is being displayed, False (0) otherwise.
 */
static PyObject *py_isShowingGrid(PyObject * /*args*/)
{
    return PyInt_FromLong(Options::getOptionInt( "render/showGrid", 0 ));
}
PY_MODULE_FUNCTION(isShowingGrid, ParticleEditor)

/*~ function ParticleEditor.undo
 *	@components{ particleeditor }
 *
 *	This function undoes the most recent operation.
 */
static PyObject *py_undo(PyObject * /*args*/)
{
	if ( MainFrame::instance()->CanUndo() )
		MainFrame::instance()->OnUndo();
    Py_Return;
}
PY_MODULE_FUNCTION(undo, ParticleEditor)
   
/*~ function ParticleEditor.canUndo
 *	@components{ particleeditor }
 *
 *	This function checks whether it is possible to undo the most recent operation.
 *
 *	@return Returns True (1) if can undo, False (0) otherwise.
 */
static PyObject *py_canUndo(PyObject * /*args*/)
{
    int result = MainFrame::instance()->CanUndo() ? 1 : 0;
    return PyInt_FromLong(result);
}
PY_MODULE_FUNCTION(canUndo, ParticleEditor)

/*~ function ParticleEditor.redo
 *	@components{ particleeditor }
 *
 *	This function redoes the most recent undo operation.
 */
static PyObject *py_redo(PyObject * /*args*/)
{
	if ( MainFrame::instance()->CanRedo() )
		MainFrame::instance()->OnRedo();
    Py_Return;
}
PY_MODULE_FUNCTION(redo, ParticleEditor)

/*~ function ParticleEditor.canRedo
 *	@components{ particleeditor }
 *
 *	This function checks whether it is possible to redo the most recent undo operation.
 *
 *	@return Returns True (1) if can redo, False (0) otherwise.
 */
static PyObject *py_canRedo(PyObject * /*args*/)
{
    int result = MainFrame::instance()->CanRedo() ? 1 : 0;
    return PyInt_FromLong(result);
}
PY_MODULE_FUNCTION(canRedo, ParticleEditor)

/*~ function ParticleEditor.doPlay
 *	@components{ particleeditor }
 *
 *	This function spawns the currently selected Particle System and sets its state to 'playing'.
 */
static PyObject *py_doPlay(PyObject * /*args*/)
{
    ParticleEditorApp::instance().setState(ParticleEditorApp::PE_PLAYING);
    Py_Return;
}
PY_MODULE_FUNCTION(doPlay, ParticleEditor)

/*~ function ParticleEditor.doStop
 *	@components{ particleeditor }
 *
 *	This function sets the currently selected Particle System state to 'stopped'.
 */
static PyObject *py_doStop(PyObject * /*args*/)
{
    ParticleEditorApp::instance().setState(ParticleEditorApp::PE_STOPPED);
    Py_Return;
}
PY_MODULE_FUNCTION(doStop, ParticleEditor)

/*~ function ParticleEditor.doPause
 *	@components{ particleeditor }
 *
 *	This function sets the the currently selected Particle System state to 'paused'.
 */
static PyObject *py_doPause(PyObject * /*args*/)
{
    if 
    ( 
        ParticleEditorApp::instance().getState()
        ==
        ParticleEditorApp::PE_PAUSED
        )
    {
        ParticleEditorApp::instance().setState(ParticleEditorApp::PE_PLAYING);
    }
    else
    {
        ParticleEditorApp::instance().setState(ParticleEditorApp::PE_PAUSED);
    }
    Py_Return;
}
PY_MODULE_FUNCTION(doPause, ParticleEditor)

/*~ function ParticleEditor.getState
 *	@components{ particleeditor }
 *
 *	This function returns the current state of the Particle System, whether it is Playing, Paused or Stopped.
 *
 *	@return Returns 0 if the state is Playing, returns 1 if the state is Paused,
 *			and returns 2 if the state is Stopped.
 */
static PyObject *py_getState(PyObject * /*args*/)
{
    int result = (int)ParticleEditorApp::instance().getState();
    return PyInt_FromLong(result);
}
PY_MODULE_FUNCTION(getState, ParticleEditor)
