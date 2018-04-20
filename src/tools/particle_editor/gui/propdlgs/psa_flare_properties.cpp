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
#include "particle_editor.hpp"
#include "main_frame.hpp"
#include "gui/propdlgs/psa_flare_properties.hpp"
#include "controls/dir_dialog.hpp"
#include "gui/gui_utilities.hpp"
#include "appmgr/options.hpp"
#include "particle/actions/flare_psa.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/string_provider.hpp"

DECLARE_DEBUG_COMPONENT2( "GUI", 0 )

const CString c_defaultDirectoryText("No Directory");

IMPLEMENT_DYNCREATE(PsaFlareProperties, PsaProperties)

PsaFlareProperties::PsaFlareProperties()
: 
PsaProperties(PsaFlareProperties::IDD)
{
	flareStep_.SetNumericType( controls::EditNumeric::ENT_INTEGER);
}

PsaFlareProperties::~PsaFlareProperties()
{
}

static bool validFlareFilename( const std::string & filename )
{
	if (BWResource::getExtension( filename ) == "xml")
	{
		return LensEffect::isLensEffect( filename );
	}

	return false;
}

void PsaFlareProperties::OnInitialUpdate()
{
    PsaProperties::OnInitialUpdate();	// data is copied to controls

	SetParameters(SET_CONTROL);
	initialised_ = true;

	// By doing this the lens effect is loaded and thus checked for validity
	CopyDataToPSA();

    flareNameDirectoryBtn_.setBitmapID(IDB_OPEN, IDB_OPEND);

	ParticleSystemPtr pSystem = 
        MainFrame::instance()->GetCurrentParticleSystem();
 
	if (pSystem)
	{
		bool isMesh = pSystem->pRenderer()->isMeshStyle();
		useParticleSize_.EnableWindow( !isMesh );
	}
}

afx_msg LRESULT PsaFlareProperties::OnUpdatePsRenderProperties(WPARAM mParam, LPARAM lParam)
{
	if (initialised_)
		SetParameters(SET_PSA);

	return 0;
}

void PsaFlareProperties::SetParameters(SetOperation task)
{
	ASSERT(action_);

	SET_INT_PARAMETER(task, flareStep);
	SET_CHECK_PARAMETER(task, colourize);
	SET_CHECK_PARAMETER(task, useParticleSize);

	if (task == SET_CONTROL)
	{
		// read in
		FlarePSA * flarePSA = action();

		CString longFilename = flarePSA->flareName().c_str();
		CString filename("");
		CString directory("");

		if (longFilename.IsEmpty())
		{
			// nothing set.. give an appropriate directory by looking where the sun flare is
			longFilename = Options::getOptionString( "resourceGlue/environment/sunFlareXML" ).c_str();

			// see if already have saved one
			longFilename = Options::getOptionString( "defaults/flareXML", longFilename.GetBuffer() ).c_str();
		}

		GetFilenameAndDirectory(longFilename, filename, directory);

		// remember for next time
		Options::setOptionString( "defaults/flareXML", longFilename.GetBuffer() );

		// populate with all the textures in that directory
		std::string relativeDirectory = BWResource::dissolveFilename(directory.GetBuffer());
		PopulateComboBoxWithFilenames
        (
            flareNameSelection_, 
            relativeDirectory, 
            validFlareFilename
        );
		flareNameDirectoryEdit_.SetWindowText(relativeDirectory.c_str());
		flareNameSelection_.SelectString(-1, filename);
	}
	else
	{
		// write out
		FlarePSA * flarePSA = action();
		
		int selected = flareNameSelection_.GetCurSel();
		if (selected != -1)
		{
			CString texName, dirName;
			flareNameSelection_.GetLBText(selected, texName);
			flareNameDirectoryEdit_.GetWindowText(dirName);
			flarePSA->flareName((dirName + "/" + texName).GetBuffer());

			// remember for next time
			Options::setOptionString( "defaults/flareXML", (dirName + "/" + texName).GetBuffer() );
		}
	}
}

void PsaFlareProperties::DoDataExchange(CDataExchange* pDX)
{
	PsaProperties::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PSA_FLARE_FLARENAME, flareNameSelection_);
	DDX_Control(pDX, IDC_PSA_FLARE_FLARESTEP, flareStep_);
	DDX_Control(pDX, IDC_PSA_FLARE_COLOURIZE, colourize_);
	DDX_Control(pDX, IDC_PSA_FLARE_USEPARTICLESIZE, useParticleSize_);
	DDX_Control(pDX, IDC_PSA_FLARE_FLARENAME_DIRECTORY_BTN, flareNameDirectoryBtn_);
    DDX_Control(pDX, IDC_PSA_FLARE_FLARENAME_DIRECTORY_EDIT, flareNameDirectoryEdit_);
}

BEGIN_MESSAGE_MAP(PsaFlareProperties, PsaProperties)
	ON_BN_CLICKED(IDC_PSA_FLARE_COLOURIZE, OnBnClickedPsaFlareButton)
	ON_BN_CLICKED(IDC_PSA_FLARE_USEPARTICLESIZE, OnBnClickedPsaFlareButton)
	ON_CBN_SELCHANGE(IDC_PSA_FLARE_FLARENAME, OnBnClickedPsaFlareButton)
	ON_BN_CLICKED(IDC_PSA_FLARE_FLARENAME_DIRECTORY_BTN, OnBnClickedPsaFlareFlarenameDirectory)
END_MESSAGE_MAP()


// PsaFlareProperties diagnostics

#ifdef _DEBUG
void PsaFlareProperties::AssertValid() const
{
	PsaProperties::AssertValid();
}

void PsaFlareProperties::Dump(CDumpContext& dc) const
{
	PsaProperties::Dump(dc);
}
#endif //_DEBUG


// PsaFlareProperties message handlers

void PsaFlareProperties::OnBnClickedPsaFlareButton()
{
	CopyDataToPSA();
}

void PsaFlareProperties::OnCbnSelchangeFlarename()
{
	SetParameters(SET_PSA);
}

void PsaFlareProperties::OnBnClickedPsaFlareFlarenameDirectory()
{
	DirDialog dlg; 

	dlg.windowTitle_ = L("PARTICLEEDITOR/OPEN");
	dlg.promptText_ = L("PARTICLEEDITOR/CHOOSE_DIR");
	dlg.fakeRootDirectory_ = dlg.basePath();

	CString startDir;
	flareNameDirectoryEdit_.GetWindowText(startDir);
	if (startDir != c_defaultDirectoryText)
		dlg.startDirectory_ = BWResource::resolveFilename(startDir.GetBuffer()).c_str();

	if (dlg.doBrowse( AfxGetApp()->m_pMainWnd )) 
	{
		dlg.userSelectedDirectory_ += "/";
		std::string relativeDirectory = BWResource::dissolveFilename(dlg.userSelectedDirectory_.GetBuffer());
		flareNameDirectoryEdit_.SetWindowText(relativeDirectory.c_str());

		PopulateComboBoxWithFilenames
        (
            flareNameSelection_, 
            relativeDirectory, 
            validFlareFilename
        );
		flareNameSelection_.SetCurSel(-1);
	}
}
