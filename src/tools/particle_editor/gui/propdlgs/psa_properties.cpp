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
#include "main_frame.hpp"
#include "gui/propdlgs/psa_properties.hpp"
#include "cstdmf/debug.hpp"
#include "particle/actions/particle_system_action.hpp"
#include "resmgr/string_provider.hpp"

DECLARE_DEBUG_COMPONENT2( "PE", 2 )

BEGIN_MESSAGE_MAP(PsaProperties, CFormView)
	ON_MESSAGE(WM_EDITNUMERIC_CHANGE, OnUpdatePsaProperties)
END_MESSAGE_MAP()

PsaProperties::PsaProperties(UINT nIDTemplate)
:	
CFormView(nIDTemplate),
action_(NULL),
initialised_(false)
{
}

void PsaProperties::SetPSA(ParticleSystemActionPtr action)
{
	action_ = action;
}

void PsaProperties::CopyDataToControls()
{
	SetParameters(SET_CONTROL);
}

void PsaProperties::CopyDataToPSA()
{
	if ( action_ )
	{
		MainFrame::instance()->PotentiallyDirty
        (
            true,
            UndoRedoOp::AK_PARAMETER,
            L("PARTICLEEDITOR/GUI/PSA_PROPERTIES/CHANGE", action_->nameID())
        );
	}
	else
	{
		MainFrame::instance()->PotentiallyDirty
        (
            true,
            UndoRedoOp::AK_PARAMETER,
            L("PARTICLEEDITOR/GUI/PSA_PROPERTIES/COPY_DATA")
        );
	}
	SetParameters(SET_PSA);
}

void PsaProperties::PostNcDestroy()
{
	CFormView::PostNcDestroy();
}

void PsaProperties::OnInitialUpdate()
{
    CFormView::OnInitialUpdate();

	if (action_)
	{
		// set the values into the controls
		CopyDataToControls();
		initialised_ = true;
	}
    INIT_AUTO_TOOLTIP();
}

BOOL PsaProperties::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:
			{
				// Update the data whenever enter is pressed
				CopyDataToPSA();
				return TRUE;
			}
		}
	}
    CALL_TOOLTIPS(pMsg);	
	return CFormView::PreTranslateMessage(pMsg);
}

afx_msg LRESULT PsaProperties::OnUpdatePsaProperties(WPARAM mParam, LPARAM lParam)
{
	if (initialised_)
		CopyDataToPSA();

	return 0;
}
