#ifndef __UMBRACAMERA_HPP
#define __UMBRACAMERA_HPP
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
 * \brief     Camera interface
 * 
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRAREFERENCECOUNT_HPP)
#   include "umbraReferenceCount.hpp"
#endif

namespace Umbra
{

/*-------------------------------------------------------------------*//*!
 *
 * \brief			Class for representing a virtual viewer
 *
 * \note            The Camera class is used for determining visible 
 *					objects in a scene. The camera defines the 
 *					position and orientation of the viewer and the shape of the
 *					view frustum. The camera interface is also used to
 *					select culling methods used by the visibility query.
 *
 * \note			Because the class is derived from ReferenceCount, 
 *					it does not have a public destructor. Use the function 
 *					ReferenceCount::release() to release instances of the class. 
 *
 *//*-------------------------------------------------------------------*/

class Camera : public ReferenceCount
{
public:

	/*-------------------------------------------------------------------*//*!
	 * \brief		Enumeration of culling methods used by visibility queries
	 *
	 * \note		These enumerations are used when constructing the
	 *				property mask for the API calls Umbra::Camera::setParameters()
	 *				and Umbra::Camera::getProperties()
	 *//*-------------------------------------------------------------------*/

    enum Property
    {
        VIEWFRUSTUM_CULLING         = (1<<0),       /*!< enable view frustum culling */
        OCCLUSION_CULLING           = (1<<1),       /*!< enable occlusion culling (note that VIEWFRUSTUM_CULLING and DEPTH_PASS get automatically enabled if this is set) */
        DISABLE_VIRTUALPORTALS      = (1<<2),       /*!< disables traversal through any virtual portals */
        SCOUT                       = (1<<3),       /*!< scout camera, consult manual for details */
        PREPARE_RESEND			    = (1<<4),		/*!< prepare to resend the results of this query. (DEBUG feature) */
        RESEND                      = (1<<5),       /*!< resend the results of the previous query (without re-resolving the visibility). (DEBUG feature) */
        OPTIMIZE                    = (1<<6),       /*!< allow visibility query to spend additional time for optimizing future queries */
        DEPTH_PASS				    = (1<<7),		/*!< reports a depth pass for visible objects which have OCCLUDER flag enabled */
        IMMEDIATE_INSTANCE_VISIBLE  = (1<<8)        /*!< reports objects visibility as early as possible already during the depth pass. Objects are not reported again during the color pass. Currently does not work with Regions of Influence. */
    };

    static UMBRADEC Camera*	create						(void);
    UMBRADEC void			getCameraToCellMatrix       (Matrix4x4&) const;
    UMBRADEC void			getCameraToCellMatrix       (Matrix4x4d&) const;
    UMBRADEC void			getCameraToWorldMatrix      (Matrix4x4&) const;
    UMBRADEC void			getCameraToWorldMatrix      (Matrix4x4d&) const;
    UMBRADEC class Cell*	getCell                     (void) const;
    UMBRADEC void			getFrustum                  (Frustum&) const;
    UMBRADEC int			getHeight                   (void) const;
    UMBRADEC void			getObjectMinimumCoverage    (float& width, float& height, float& opacity) const;
    UMBRADEC void			getPixelCenter				(float& xoffset, float& yoffset) const;
    UMBRADEC unsigned int	getProperties               (void) const;
    UMBRADEC void			getScissor                  (int& left, int& top, int& right, int& bottom) const;
    UMBRADEC int			getWidth                    (void) const;
    UMBRADEC void			resolveVisibility           (class Commander*, int recursionDepth, float importanceThreshold=0.0) const;
    UMBRADEC void			setCameraToCellMatrix       (const Matrix4x4&);
    UMBRADEC void			setCameraToCellMatrix       (const Matrix4x4d&);
    UMBRADEC void			setCell                     (class Cell*);
    UMBRADEC void			setFrustum                  (const Frustum&);
    UMBRADEC void			setObjectMinimumCoverage    (float pixelWidth, float pixelHeight, float opacity);
    UMBRADEC void			setParameters               (int screenWidth, int screenHeight, unsigned int propertyMask=VIEWFRUSTUM_CULLING|OCCLUSION_CULLING, float imageSpaceScalingX=1.f, float imageSpaceScalingY=1.f);
    UMBRADEC void			setPixelCenter				(float xOffset, float yOffset);
    UMBRADEC void			setScissor                  (int left, int top, int right, int bottom);
    UMBRADEC void			setTilingScenario			(const Tile* tiles, int numTiles);

    UMBRADEC void           setBitMask                  (UINT32 bm);

    class ImpCamera*        getImplementation           (void) const;
protected:                                          
                            Camera						(class ImpCamera*);
    UMBRADEC virtual		~Camera                     (void);
private:                                                    
                            Camera                      (const Camera&);    // not allowed
    Camera&                 operator=                   (const Camera&);    // not allowed
    class ImpCamera*        m_imp;                                          // opaque pointer
};
} // namespace Umbra
                                            
//------------------------------------------------------------------------
#endif // __UMBRACAMERA_HPP
