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
#include "math/math_extra.hpp"


/**
 *	This applies Gram-Schmidt orgthonalization to the vectors v1, v2 and v3 and
 *	returns the result in e1, e2 and e3.  The vectors v1, v2, v3 should be
 *	linearly indepedant, non-zero vectors.
 *
 *  See http://en.wikipedia.org/wiki/Gram-Schmidt_process for details of 
 *	Gram-Schmidt orthogonalization.
 *
 *  @param v1, v2, v3	The vectors to orthonormalize.
 *  @param e1, e2, e3	The orthonormalized vectors.
 */
void BW::orthogonalize(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3,
				 Vector3 &e1, Vector3 &e2, Vector3 &e3)
{
	Vector3 u1 = v1; 

	Vector3 p1; 
	p1.projectOnto(v2, u1);
	Vector3 u2 = v2 - p1;
	
	Vector3 p2; 
	p2.projectOnto(v3, u1);
	Vector3 p3; 
	p3.projectOnto(v3, u2);
	Vector3 u3 = v3 - p2 - p3;
	
	u1.normalise();
	u2.normalise();
	u3.normalise();

	e1 = u1; e2 = u2; e3 = u3;
}
