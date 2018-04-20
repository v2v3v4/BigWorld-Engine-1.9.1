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
#include "gui/propdlgs/psa_force_properties.hpp"
#include "gui/vector_generator_proxies.hpp"
#include "particle/actions/force_psa.hpp"
#include "gizmo/general_editor.hpp"
#include "gizmo/combination_gizmos.hpp"

DECLARE_DEBUG_COMPONENT2( "GUI", 0 )

IMPLEMENT_DYNCREATE(PsaForceProperties, PsaProperties)

PsaForceProperties::PsaForceProperties()
: 
PsaProperties(PsaForceProperties::IDD),
positionGizmo_(NULL),
positionMatrixProxy_(NULL)
{
}

PsaForceProperties::~PsaForceProperties()
{
	removePositionGizmo();
}

void PsaForceProperties::SetParameters(SetOperation task)
{
	ASSERT(action_);

	SET_VECTOR3_PARAMETER(task, vector, x, y, z);

	// tell the gizmo of the new position
	addPositionGizmo();
	Matrix mat;
	mat.setTranslate(position());
	positionMatrixProxy_->setMatrixAlone(mat);
}

void PsaForceProperties::position(const Vector3 & position)
{
	// set the values into the edit
	x_.SetValue(position.x);
	y_.SetValue(position.y);
	z_.SetValue(position.z);

    // notify the psa
	SetParameters(SET_PSA);

    // note that this function is ultimately called by a gizmo change, and so
    // the undo/redo history is set up elsewhere (in PeModule).
}

Vector3 PsaForceProperties::position() const
{
	// should get this from the psa directly, but there are const issues
	return Vector3(x_.GetValue(), y_.GetValue(), z_.GetValue());
}

void PsaForceProperties::addPositionGizmo()
{
	if (positionGizmo_)
		return;	// already been created

	GeneralEditorPtr generalsDaughter = new GeneralEditor();
	positionMatrixProxy_ = new VectorGeneratorMatrixProxy<PsaForceProperties>(this, 
		&PsaForceProperties::position, 
		&PsaForceProperties::position);
	generalsDaughter->addProperty(new GenPositionProperty("vector", positionMatrixProxy_));
	positionGizmo_ = new VectorGizmo(MODIFIER_ALT, positionMatrixProxy_, 0xFFFFFF00, 0.015f, NULL, 0.1f);
	GizmoManager::instance().addGizmo(positionGizmo_);
	GeneralEditor::Editors newEditors;
	newEditors.push_back(generalsDaughter);
	GeneralEditor::currentEditors(newEditors);
}

void PsaForceProperties::removePositionGizmo()
{
	GizmoManager::instance().removeGizmo(positionGizmo_);
	positionGizmo_ = NULL;
}

void PsaForceProperties::DoDataExchange(CDataExchange* pDX)
{
	PsaProperties::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PSA_FORCE_X, x_);
	DDX_Control(pDX, IDC_PSA_FORCE_Y, y_);
	DDX_Control(pDX, IDC_PSA_FORCE_Z, z_);
}

BEGIN_MESSAGE_MAP(PsaForceProperties, PsaProperties)
END_MESSAGE_MAP()


// PsaForceProperties diagnostics

#ifdef _DEBUG
void PsaForceProperties::AssertValid() const
{
	PsaProperties::AssertValid();
}

void PsaForceProperties::Dump(CDumpContext& dc) const
{
	PsaProperties::Dump(dc);
}
#endif //_DEBUG


// PsaForceProperties message handlers
