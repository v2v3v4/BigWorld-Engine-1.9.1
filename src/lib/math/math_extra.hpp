/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MATH_EXTRA_HPP
#define MATH_EXTRA_HPP

#include "math_namespace.hpp"
#include "vector2.hpp"
#include "vector3.hpp"
#include "rectt.hpp"

BEGIN_BW_MATH


inline
bool watcherStringToValue( const char * valueStr, BW::Rect & rect )
{
	return sscanf( valueStr, "%f,%f,%f,%f",
			&rect.xMin_, &rect.yMin_,
			&rect.xMax_, &rect.yMax_ ) == 4;
}

inline
std::string watcherValueToString( const BW::Rect & rect )
{
	char buf[256];
	bw_snprintf( buf, sizeof(buf), "%.3f, %.3f, %.3f, %.3f",
			rect.xMin_, rect.yMin_,
			rect.xMax_, rect.yMax_ );

	return buf;
}


void orthogonalize(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3,
				 Vector3 &e1, Vector3 &e2, Vector3 &e3);


END_BW_MATH


#endif // MATH_EXTRA_HPP
