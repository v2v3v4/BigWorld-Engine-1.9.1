#ifndef __UMBRACOMMANDER_HPP
#define __UMBRACOMMANDER_HPP
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
 * \brief     Commander interface
 * 
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRALIBRARY_HPP)
#   include "umbraLibrary.hpp"
#endif

namespace Umbra
{
class Object;
class ImpCommander;
class ImpOcclusionQuery;
class Database;
class Cell;

/*-------------------------------------------------------------------*//*!
 *
 * \brief		    Class used for Umbra-to-user message passing
 *
 * \note            The Commander interface handles Umbra-to-user communication.
 *					This approach was taken both to avoid extending the API in
 *					future releases, and to keep future libraries binary-compatible.
 *
 * \note			The application user should create a new class derived
 *					from Commander and override the Commander::command() function 
 *					to provide a valid implementation. See documentation entry 
 *					for the function for further information.
 *
 * \sa				Commander::command()
 *//*-------------------------------------------------------------------*/

class Commander
{
public:

	/*-------------------------------------------------------------------*//*!
	 * \brief		    Enumeration of different commands that Umbra may 
	 *					send to the Commander::command() function
	 *
	 * \sa				Commander::command()
	 *//*-------------------------------------------------------------------*/

    enum Command
    {
        QUERY_BEGIN                     = 0x00, /*!< Start of Camera::resolveVisibility() [NONE]      */
        QUERY_END                       = 0x01, /*!< End of Camera::resolveVisibility()   [NONE]      */
        QUERY_ABORT						= 0x02,	// NOT USED (RESERVED FOR FUTURE)                   
        PORTAL_ENTER                    = 0x10, /*!< Traversal passed through a portal        [getInstance()] */
        PORTAL_EXIT                     = 0x11, /*!< Traversal came back through a portal     [getInstance()] */
        PORTAL_PRE_EXIT					= 0x13,	/*!< Traversal is about to come back through a portal [NONE]  */
        CELL_IMMEDIATE_REPORT			= 0x12,	/*!< Cell has been entered			          [getCell()]	  */
        VIEW_PARAMETERS_CHANGED         = 0x20, /*!< New viewing parameters                   [getViewer()]   */
        INSTANCE_VISIBLE                = 0x30, /*!< Object should be rendered                [getInstance()] */
        REMOVAL_SUGGESTED               = 0x31, /*!< Object should be garbage collected       [getInstance()] */
        INSTANCE_IMMEDIATE_REPORT		= 0x32,	/*!< Change write model if necessary		  [getInstance()] */
        REGION_OF_INFLUENCE_ACTIVE      = 0x40, /*!< Region of influence turned on            [getInstance()] */
        REGION_OF_INFLUENCE_INACTIVE    = 0x41, /*!< Region of influence turned off           [getInstance()] */
        STENCIL_MASK                    = 0x51, /*!< New object should be written into the stencil buffer [getInstance()] */
        RESERVED0						= 0x52,	// Reserved for future use  
        TEXT_MESSAGE                    = 0x60, /*!< Debug function: Text message from the library [getTextMessage()] */
        DRAW_LINE_2D                    = 0x61, /*!< Debug function: Draw two-dimensional line     [getLine2D()]      */
        DRAW_LINE_3D                    = 0x62, /*!< Debug function: Draw three-dimensional line   [getLine3D()]      */
        DRAW_BUFFER                     = 0x63, /*!< Debug function: Draw monochromatic buffer     [getBuffer()]      */
        RESERVED1			            = 0x70, // Reserved for future use  
        RESERVED2						= 0x71, // Reserved for future use  

        OCCLUSION_QUERY_BEGIN			= 0x80, /*!< Begin occlusion query   [getOcclusionQuery()]                 */
        OCCLUSION_QUERY_END				= 0x81, /*!< End occlusion query     [getOcclusionQuery()]                 */
        OCCLUSION_QUERY_GET_RESULT		= 0x82, /*!< Read back query results [getOcclusionQuery()]                 */
        OCCLUSION_QUERY_DRAW_TEST_DEPTH	= 0x83, /*!< Render test shape into the depth buffer [getOcclusionQuery()] */
        INSTANCE_DRAW_DEPTH				= 0x90, /*!< Render object into the depth buffer     [getInstance()]       */
        FLUSH_DEPTH						= 0x91, /*!< Flush all rendering calls from INSTANCE_DRAW_DEPTH            */

        DEPTH_PASS_BEGIN				= 0xa0, /*!< Beginning of depth pass                    */
        DEPTH_PASS_END					= 0xa1, /*!< End of depth pass                          */
        COLOR_PASS_BEGIN				= 0xa2, /*!< Beginning of color pass                    */
        COLOR_PASS_END					= 0xa3, /*!< End of color pass                          */
        TILE_BEGIN						= 0xa4, /*!< Begin rendering into a tile  [getViewer()] */
        TILE_END						= 0xa5, /*!< End rendering into a tile	  [getViewer()] */

        FLUSH_GPU_COMMAND_BUFFER        = 0xb0, /*!< Hint for flushing the GPU command buffer   */

        COMMAND_MAX                     = 0x7FFFFFFF // force enumerations to be int       
    };

	/*-------------------------------------------------------------------*//*!
	 * \brief     Used to return occlusion query data from the user
	 *
	 *//*-------------------------------------------------------------------*/

    class OcclusionQuery
    {
    public:

        UMBRADEC int				getIndex				(void) const;				
        UMBRADEC void				getToCameraMatrix		(Matrix4x4& mtx) const;
        UMBRADEC bool				getWaitForResult		(void) const;

        UMBRADEC int				getVertexCount			(void) const;
        UMBRADEC const Vector3*		getVertices				(void) const;
        UMBRADEC int				getTriangleCount		(void) const;
        UMBRADEC const Vector3i*	getTriangles			(void) const;

		UMBRADEC void				setResult				(bool resultAvailable, int visiblePixelCount);

    private:           
        OcclusionQuery&				operator=				(const OcclusionQuery&);          // not allowed
                                    OcclusionQuery			(const OcclusionQuery&);          // not allowed
                                    OcclusionQuery          (void);
                                    ~OcclusionQuery         (void);
        
        friend class ImpCommander;
        class ImpOcclusionQuery* m_imp;
    };

	/*-------------------------------------------------------------------*//*!
	 * \brief			Class used for passing object-related information 
	 *                  to the user during certain Commander callbacks
	 *
	 * \note			This class is used solely by the Commander for providing 
	 *					additional object-specific information during Commander::command()
	 *					callbacks.
	 *
	 * \note			The application user cannot construct or destruct Instances. The
	 *					only way to get an access to an Instance is through the 
	 *					Commander::getInstance() method.
	 *
	 *//*-------------------------------------------------------------------*/

    class Instance
    {
    public:

		/*-------------------------------------------------------------------*//*!
		 * \brief			Structure for passing object projection information
		 *
		 * \note			This structure is used for passing data to the member 
		 *					function Commander::Instance::getProjectionSize().
		 *
		 * \sa				Commander::Instance::getProjectionSize()
		 *//*-------------------------------------------------------------------*/

        struct Projection
        {
            int     left;                   /*!< Left screen coordinate               */               
            int     right;                  /*!< Right screen coordinate (exclusive)  */
            int     top;                    /*!< Top screen coordinate                */
            int     bottom;                 /*!< Bottom screen coordinate (exclusive) */
            float   zNear;                  /*!< Near depth value                     */
            float   zFar;                   /*!< Far depth value                      */
        };

        enum Clip
        {
            FRONT	= (1<<0),				/*!< Front clip plane	*/
            BACK	= (1<<1),				/*!< Back clip plane    */
            LEFT	= (1<<2),				/*!< Left clip plane    */
            RIGHT	= (1<<3),				/*!< Right clip plane   */
            TOP		= (1<<4),				/*!< Top clip plane     */
            BOTTOM	= (1<<5)				/*!< Bottom clip plane  */
        };

		enum Tile
		{
			TILE0	= (1<<0), 
			TILE1	= (1<<1), 
			TILE2	= (1<<2), 
			TILE3	= (1<<3), 
			TILE4	= (1<<4), 
			TILE5	= (1<<5), 
			TILE6	= (1<<6), 
			TILE7	= (1<<7)
		};

		UMBRADEC UINT32		getTileMask				(void) const; 
        UMBRADEC UINT32		getClipMask				(void) const;
        UMBRADEC float		getImportance           (void) const;
        UMBRADEC Object*	getObject               (void) const;               // pointer to object
        UMBRADEC void		getObjectToCameraMatrix	(Matrix4x4&) const;         // view without projection
        UMBRADEC void		getObjectToCameraMatrix (Matrix4x4d&) const;        // view without projection
        UMBRADEC bool		getProjectionSize       (Projection&) const;
        UMBRADEC void*		getUserPointer			(void) const;
    private:
        Instance&       operator=               (const Instance&);          // not allowed
                        Instance                (const Instance&);          // not allowed
                        Instance                (void);
                        ~Instance               (void);
        friend class ImpCommander;                                              
        INT32 m_index;                                                      // internal data
    };

	/*-------------------------------------------------------------------*//*!
	 * \brief	A smart object for passing view-related information to 
	 *			the user during certain Commander callbacks
	 *
	 * \note	This class is used by Commander to provide
	 *			additional view-specific information during certain
	 *			Commander::command() callbacks.
	 *
	 * \note	The application user cannot construct or destruct Viewers. The
	 *			only way to gain access to a Viewer is through the method
	 *			Commander::getViewer().
	 *//*-------------------------------------------------------------------*/

    class Viewer
    {
    public:

		/*-------------------------------------------------------------------*//*!
		 * \brief		Enumeration specifying handedness of matrix returned
		 *
		 * \note		This enumeration is used as a parameter to some Viewer 
		 *				member functions to specify the handedness of the matrix
		 *				returned.
		 *
		 * \sa			Commander::Viewer::getProjectionMatrix()
		 *//*-------------------------------------------------------------------*/

        enum Handedness
        {
            LEFT_HANDED      = 0,    /*!< Matrix is left-handed  */
            RIGHT_HANDED     = 1,    /*!< Matrix is right-handed */
            LEFT_HANDED_D3D  = 2,    /*!< Matrix is left-handed and in D3D projection matrix format  */
            RIGHT_HANDED_D3D = 3     /*!< Matrix is right-handed and in D3D projection matrix format */
        };

        UMBRADEC void   getFrustum              (Frustum&) const;
        UMBRADEC int    getFrustumPlaneCount    (void) const;
        UMBRADEC void   getFrustumPlane         (int index, Vector4& plane) const;
        UMBRADEC void   getScissor              (int& left,int& top,int& right,int& bottom) const;
        UMBRADEC void   getProjectionMatrix     (Matrix4x4&,  Handedness) const;    
        UMBRADEC void   getProjectionMatrix     (Matrix4x4d&, Handedness) const;
        UMBRADEC void   getCameraToWorldMatrix  (Matrix4x4&) const; 
        UMBRADEC void   getCameraToWorldMatrix  (Matrix4x4d&) const;
        UMBRADEC void   getCellToCameraMatrix   (Matrix4x4&) const;
        UMBRADEC int    getTile                 (Tile&) const;      // returns tile index

        UMBRADEC bool   isMirrored              (void) const;

    private:                                
                        Viewer                  (const Viewer&);    // not allowed
        Viewer&         operator=               (const Viewer&);    // not allowed
                        Viewer                  (void);
                        ~Viewer                 (void);
        friend class ImpCamera;                             
        class ImpCamera* m_imp;                                         // implementation pointer
    };

    UMBRADEC virtual            ~Commander              (void);
    UMBRADEC virtual void       command                 (Command c) = 0;      
    class ImpCommander*         getImplementation       (void) const;

protected:

    UMBRADEC                    Commander               (void);
    UMBRADEC Library::BufferType getBuffer              (const unsigned char*&s, int& w,int& h) const;
    UMBRADEC Cell*              getCell                 (void) const;
    UMBRADEC const Instance*    getInstance             (void) const;
    UMBRADEC Library::LineType  getLine2D               (Vector2& a, Vector2& b, Vector4& color) const;
    UMBRADEC Library::LineType  getLine3D               (Vector3& a, Vector3& b, Vector4& color) const;
    UMBRADEC void               getStencilValues        (int& test,int& write) const;
    UMBRADEC const char*        getTextMessage          (void) const;
    UMBRADEC const Viewer*      getViewer               (void) const;

    UMBRADEC OcclusionQuery*    getOcclusionQuery       (void) const;

private:
    friend class ImpCommander;
                                Commander				(const Commander&);     // not allowed
    Commander&                  operator=				(const Commander&);     // not allowed
    ImpCommander*				m_imp;											// implementation pointer
};

} // namespace Umbra
                                            
//------------------------------------------------------------------------
#endif // __UMBRACAMERA_HPP
