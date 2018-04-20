/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PSA_NODECLAMP_PROPERTIES_HPP
#define PSA_NODECLAMP_PROPERTIES_HPP

#include "resource.h"
#include "psa_properties.hpp"

class PsaNodeClampProperties : public PsaProperties
{
public:
    DECLARE_DYNCREATE(PsaNodeClampProperties)

    enum { IDD = IDD_PSA_NODECLAMP_PROPERTIES };

    //
    // Constructor.
    //
    PsaNodeClampProperties();

    //
    // Set the parameters.
    //
    // @param task      The parameters to set.
    //
    /*virtual*/ void SetParameters(SetOperation /*task*/);
};

#endif // PSA_NODECLAMP_PROPERTIES_HPP
