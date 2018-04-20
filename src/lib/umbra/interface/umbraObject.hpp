#ifndef __UMBRAOBJECT_HPP
#define __UMBRAOBJECT_HPP
/*-------------------------------------------------------------------*//*!
 *
 * Umbra
 * -----------------------------------------
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
 * \brief Objects, Portals and Regions of Influence
 * 
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRAREFERENCECOUNT_HPP)
#   include "umbraReferenceCount.hpp"
#endif

namespace Umbra
{
class Cell;
class Model;

/*-------------------------------------------------------------------*//*!
 * \brief	Physical instance of a model
 *
 * \note    Objects are used to place models into the world.
 *          For each object separate test/write models can be defined.
 *//*-------------------------------------------------------------------*/

class Object : public ReferenceCount
{
public:
    enum Property
    {
        ENABLED                 = 0,        /*!< is the object active (all objects) */
        INFORM_VISIBLE          = 1,        /*!< should the user be informed when object becomes visible (all objects) */
        CONTRIBUTION_CULLING    = 2,        /*!< is contribution culling allowed (all objects) */
        INFORM_PORTAL_ENTER     = 3,        /*!< inform when traversing through the portal (portal objects only) */
        INFORM_PORTAL_EXIT      = 4,        /*!< inform when traversing backwards through the portal (portal objects only) */
        FLOATING_PORTAL         = 5,        /*!< object is a floating portal (portal objects only) */
        REPORT_IMMEDIATELY		= 6,		/*!< report visibility immediately */
        UNBOUNDED				= 7,		/*!< object test model ignored, object regarded to be of infinite size and reported as always visible */
        OCCLUDER				= 8,		/*!< can object ever be used as an occluder? (enabled by default) */
        INFORM_PORTAL_PRE_EXIT  = 9         /*!< inform when about to traverse backwards through the portal (portal objects only) */
    };

static UMBRADEC Object*	create					(Model* testModel);
UMBRADEC void			getAABB					(Vector3& mn,Vector3& mx) const;
UMBRADEC Cell*			getCell                 (void) const;
UMBRADEC void			getOBB					(Matrix4x4&) const;
UMBRADEC void			getObjectToCellMatrix   (Matrix4x4&) const;
UMBRADEC void			getObjectToCellMatrix   (Matrix4x4d&) const;
UMBRADEC void			getSphere				(Vector3& center, float& radius) const;
UMBRADEC Model*			getTestModel            (void) const;
UMBRADEC Object*		getVisibilityParent     (void) const;
UMBRADEC Model*			getWriteModel           (void) const;
UMBRADEC void			set                     (Property, bool);
UMBRADEC void			setCell                 (Cell*);
UMBRADEC void			setCost                 (int nVertices, int nTriangles, float complexity);
UMBRADEC void			setObjectToCellMatrix   (const Matrix4x4&);
UMBRADEC void			setObjectToCellMatrix   (const Matrix4x4d&);
UMBRADEC void			setTestModel            (Model*);
UMBRADEC void			setVisibilityParent     (Object*);
UMBRADEC void			setWriteModel           (Model*);
UMBRADEC bool			test                    (Property) const;

UMBRADEC void           setBitMask              (UINT32 bm);

class ImpObject*        getImplementation       (void) const;               // internal
protected:
                        Object					(class ImpReferenceCount*);
    virtual void		destruct				(void) const;
private:
                        Object                  (const Object&);            // not allowed
    Object&             operator=               (const Object&);            // not allowed
};

/*-------------------------------------------------------------------*//*!
 *
 * \brief			Region Of Influence
 *
 * \note	        Regions Of Influence (or ROIs for short) are used
 *                  to place light sources and similar objects into the
 *                  scene. The camera's visibility resolving process will
 *                  determine overlaps between ROIs and other objects.
 *
 *                  The actual region is defined by assigning a test model
 *                  to the ROI. In order to model a spot light, a mesh model
 *                  of the corresponding shape can be used.
 *
 *                  ROIs are tested during the visibility query in a similar
 *                  fashion to other objects. This means that if a ROI is
 *                  fully outside the view frustum or fully occluded, it will
 *                  be rejected prior to the ROI vs. object overlap tests.
 *
 *//*-------------------------------------------------------------------*/

class RegionOfInfluence : public Object
{
public:
static UMBRADEC RegionOfInfluence*	create					(Model* testModel);
protected:
                                    RegionOfInfluence		(class ImpReferenceCount*);
private:
                                    RegionOfInfluence       (const RegionOfInfluence&); // not allowed
    RegionOfInfluence&				operator=               (const RegionOfInfluence&); // not allowed
};

/*-------------------------------------------------------------------*//*!
 * 
 * \brief		    Class describing a physical link between two cells
 *
 * \note            Physical portals are physical links between cells. Such links
 *                  cannot cause discontinuities in space and are only used for
 *                  visibility determination.
 *
 *//*-------------------------------------------------------------------*/
    
class PhysicalPortal : public Object
{
public:
static UMBRADEC PhysicalPortal*		create					(Model* testModel, Cell* targetCell);
UMBRADEC float						getImportanceDecay      (void) const;
UMBRADEC Model*						getStencilModel         (void) const;
UMBRADEC Cell*						getTargetCell           (void) const;
UMBRADEC void						setImportanceDecay      (float);
UMBRADEC void						setStencilModel         (Model*);
UMBRADEC void						setTargetCell           (Cell*);
protected:
                                    PhysicalPortal			(class ImpReferenceCount*);
private:
                                    PhysicalPortal          (const PhysicalPortal&);    // not allowed
    PhysicalPortal&					operator=               (const PhysicalPortal&);    // not allowed
};

/*-------------------------------------------------------------------*//*!
 *
 * \brief		    Class describing an arbitrary link between two cells
 *
 * \note            Virtual portals involve free transformation between source
 *                  and target cell. Special effects such as mirrors,
 *                  surveillance cameras, teleports etc. can be created with
 *                  VirtualPortals.
 *
 *                  There are some limitations in using virtual portals.
 *                  Consult User's Manual for detailed description.
 *
 *//*-------------------------------------------------------------------*/

class VirtualPortal : public PhysicalPortal
{
public:
static UMBRADEC VirtualPortal*	create					(Model* testModel, PhysicalPortal* targetPortal);
UMBRADEC PhysicalPortal*		getTargetPortal         (void) const;
UMBRADEC void					getWarpMatrix           (Matrix4x4&) const;
UMBRADEC void					getWarpMatrix           (Matrix4x4d&) const;
UMBRADEC void					setTargetPortal         (PhysicalPortal*);
UMBRADEC void					setWarpMatrix           (const Matrix4x4&);
UMBRADEC void					setWarpMatrix           (const Matrix4x4d&);
protected:
                                VirtualPortal			(class ImpReferenceCount*);
private:
                                VirtualPortal           (const VirtualPortal&);     // not allowed
    VirtualPortal&				operator=               (const VirtualPortal&);     // not allowed
};

} // namespace Umbra

//------------------------------------------------------------------------
#endif // __UMBRAOBJECT_HPP
