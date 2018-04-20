/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef BACK_BUFFER_COPY_HPP
#define BACK_BUFFER_COPY_HPP


#include "moo/moo_dx.hpp"
#include "custom_mesh.hpp"
#include "math/vector2.hpp"
#include "moo/vertex_formats.hpp"
#include "moo/material.hpp"


/**
 *	This class allows you to use the back buffer as a texture, and copy and
 *	part of it to a render target.
 */
class BackBufferCopy
{
public:
	BackBufferCopy();

	virtual ~BackBufferCopy()	{};

	virtual bool init();
	virtual void finz();

	///this method assumes an appropriate render target has been set.
	virtual void draw( const Vector2& fromTL, const Vector2& fromBR, const Vector2& toTL, const Vector2& toBR, bool useEffect = false ) = 0;
	const float multisample(){ return multisample_; };
	void setupBackBufferHeader();

protected:
	DX::BaseTexture* pTexture_;
	bool inited_;
	Moo::Material material_;
	float multisample_;
};


/**
 * This class inherits from BackBufferCopy, and copies a rectangular portion of
 * the back buffer into the destination render target.
 */
class RectBackBufferCopy : public BackBufferCopy
{
public:
	RectBackBufferCopy();

	virtual void draw( const Vector2& fromTL, const Vector2& fromBR, const Vector2& toTL, const Vector2& toBR, bool useEffect = false );
private:
	CustomMesh<Moo::VertexTUV> screenCopyMesh_;
};


#endif