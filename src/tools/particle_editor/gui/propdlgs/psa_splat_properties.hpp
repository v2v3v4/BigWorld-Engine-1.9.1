/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PSA_SPLAT_PROPERTIES_HPP
#define PSA_SPLAT_PROPERTIES_HPP

#include "resource.h"
#include "psa_properties.hpp"

class PsaSplatProperties : public PsaProperties
{
public:
    DECLARE_DYNCREATE(PsaSplatProperties)

    enum { IDD = IDD_PSA_SPLAT_PROPERTIES };

    //
    // Constructor.
    //
    PsaSplatProperties();

    //
    // Set the parameters.
    //
    // @param task      The parameters to set.
    //
    /*virtual*/ void SetParameters(SetOperation /*task*/);
};

#endif // PSA_SPLAT_PROPERTIES_HPP
