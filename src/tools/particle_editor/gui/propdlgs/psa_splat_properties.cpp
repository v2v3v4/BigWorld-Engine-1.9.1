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
#include "psa_splat_properties.hpp"

DECLARE_DEBUG_COMPONENT2("GUI", 0)

IMPLEMENT_DYNCREATE(PsaSplatProperties, PsaProperties)

PsaSplatProperties::PsaSplatProperties()
:
PsaProperties(IDD)
{
}

/*virtual*/ void PsaSplatProperties::SetParameters(SetOperation /*task*/)
{
}
