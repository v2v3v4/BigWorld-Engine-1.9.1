#ifndef __UMBRAMODEL_HPP
#define __UMBRAMODEL_HPP
/*-------------------------------------------------------------------*//*!
 *
 * Umbra
 * 
 * (C) 1999-2005 Hybrid Graphics, Ltd., 2006-2007 Umbra Software Ltd. 
 * All Rights Reserved.
 *
 *
 * This file consists of unpublished, proprietary source code of
 * Umbra Software, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Umbra Software and legal action against the party in breach.
 *
 * \file
 * \brief Model interfaces
 *
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRAREFERENCECOUNT_HPP)
#   include "umbraReferenceCount.hpp"
#endif

namespace Umbra
{
/*-------------------------------------------------------------------*//*!
 *
 * \brief	Base class for all models in Umbra
 *
 * \note    Models are used to describe shapes of objects. They don't
 *          "exist" by themselves - in order to physically place a model
 *          into a world, an Umbra::Object must be created to provide
 *          the location and orientation information. Models can
 *          be shared by multiple objects.
 *
 * \sa		MeshModel, OBBModel, SphereModel
 *
 *//*-------------------------------------------------------------------*/

class Model : public ReferenceCount
{
public:
    enum Property
    {
        BACKFACE_CULLABLE   = 1,                        /*!< model can be FULLY backface culled    */
        SOLID				= 2							/*!< model is solid (see reference manual) */
    };

    UMBRADEC void		getAABB				(Vector3& mn, Vector3& mx) const;
    UMBRADEC void		getOBB				(Matrix4x4&) const;
    UMBRADEC void		getSphere			(Vector3& center, float& radius) const;
    UMBRADEC bool		test                (Property) const;
    UMBRADEC void		set                 (Property, bool);
    class ImpModel*     getImplementation   (void) const;
protected:
                        Model               (class ImpReferenceCount*);	// internal
    virtual void		destruct			(void) const;
private:
                        Model               (const Model&); // not permitted
    Model&              operator=           (const Model&); // not permitted
};

/*-------------------------------------------------------------------*//*!
 *
 * \brief           A model type where topology is described using triangles and vertices
 *
 * \note            This is the most common model type used. Note that when
 *                  using Umbra::MeshModel as a test model, any simplified
 *                  model that is (at least) larger than the true model can
 *                  be used. When using them as write models, the shape
 *                  must be conservatively smaller - otherwise artifacts will
 *                  occur.
 *
 *//*-------------------------------------------------------------------*/

class MeshModel : public Model
{
public:
    static UMBRADEC MeshModel*	create				(const Vector3* vertices, const Vector3i* triangles,int numVertices,int numTriangles, bool clockwise = true);
protected:
                                MeshModel			(class ImpReferenceCount*);	// internal
private:
                                MeshModel           (const MeshModel&);			// not permitted
    MeshModel&					operator=           (const MeshModel&);			// not permitted
};

/*-------------------------------------------------------------------*//*!
 *
 * \brief           A model type describing an oriented bounding box. This
 *					class is also used to describe an axis-aligned bounding
 *					box (there's no separate AABBModel class).
 *
 *//*-------------------------------------------------------------------*/

class OBBModel : public Model
{
public:
    static UMBRADEC OBBModel*	create				(const Matrix4x4& obb);
    static UMBRADEC OBBModel*	create				(const Vector3* vertices, int numVertices);
    static UMBRADEC OBBModel*	create				(const Vector3& mn, const Vector3& mx);
protected:
                                OBBModel			(class ImpReferenceCount*);		// internal
private:									
                                OBBModel			(const OBBModel&);		// not permitted
    OBBModel&					operator=			(const OBBModel&);		// not permitted
};

/*-------------------------------------------------------------------*//*!
 *
 * \brief     A model type describing a sphere with a center position and 
 *		      a radius
 *
 *//*-------------------------------------------------------------------*/

class SphereModel : public Model
{
public:
    static UMBRADEC SphereModel*	create				(const Vector3& center,   float radius);
    static UMBRADEC SphereModel*	create				(const Vector3* vertices, int numVertices);
protected:	
                                    SphereModel			(class ImpReferenceCount*);	// internal
private:
                                    SphereModel         (const SphereModel&);   // not permitted
    SphereModel&					operator=           (const SphereModel&);   // not permitted
};

} // namespace Umbra

//------------------------------------------------------------------------
#endif //__UMBRAMODEL_HPP
