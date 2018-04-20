/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MOO_MATH_HPP
#define MOO_MATH_HPP

#include <cstdmf/stdmf.hpp>
#include <cstdmf/debug.hpp>
#include <vector>
#include <list>

#ifndef MF_SERVER
#include "d3dx9math.h"
#endif

#include "math/vector2.hpp"
#include "math/vector3.hpp"
#include "math/vector4.hpp"
#include "math/matrix.hpp"
#include "math/quat.hpp"

class Quaternion;
#include "math/angle.hpp"

#ifndef MF_SERVER
namespace Moo
{
	typedef D3DXCOLOR				Colour;
	typedef D3DCOLOR				PackedColour;
} // namespace Moo
#endif



#endif
