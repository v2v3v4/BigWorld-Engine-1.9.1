#ifndef __UMBRACELL_HPP
#define __UMBRACELL_HPP
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
 * \brief     Cell interface
 * 
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRAREFERENCECOUNT_HPP)
#   include "umbraReferenceCount.hpp"
#endif
    
namespace Umbra
{

/*-------------------------------------------------------------------*//*!
 *
 * \brief           A cell defines a region in space with arbitrary topology.
 *
 * \note            
 *					Every object and camera in the world must belong to some
 *					cell in order to be used in a visibility query. The effects of 
 *					ROIs are limited to the cell where they belong.
 *					
 * \note			Multiple cells can be connected by using portals.
 *
 * \note			Because the class is derived from ReferenceCount, 
 *					it does not have a public destructor. Use the function 
 *					ReferenceCount::release() to release instances of the class. 
 *
 * \sa              Umbra::PhysicalPortal
 *
 *//*-------------------------------------------------------------------*/

class Cell : public ReferenceCount
{
public:

	/*-------------------------------------------------------------------*//*!
	 * \brief	Enumeration of cell properties
	 *
	 * \note	If the ENABLED flag of the cell is not set, a visibility query
	 *			can never enter the cell. This flag is an easy method of disabling
	 *			large portions of the world without having to disable individual objects.
	 *			Note that if a visibility query starts from a disabled cell, i.e. the
	 *			camera's cell has been disabled, no objects are reported as visible.
     *
	 * \note	The REPORT_IMMEDIATELY flag can be used to request that Commander::CELL_IMMEDIATE_REPORT
	 *			commands are sent whenever the visibility query enters a new cell. These commands are considered 
	 *			immediate in the sense that they are sent during the actual traversal, unlike some other commands
	 *			that are buffered internally and sent just when the visibility query finishes. The main purpose
	 *			of this flag is to allow modifications to the object database based on visibility of cells. 
     *
	 * \note	The cell properties are initialized as follows: \n
     *			\n
	 *			ENABLED true \n
	 *			REPORT_IMMEDIATELY false \n
	 *			
	 * \sa		Cell::set()
	 * \sa		Cell::test()
	 *//*-------------------------------------------------------------------*/

    enum Property
    {
        ENABLED					= 0,    /*!< Enables or disables the entire cell */
        REPORT_IMMEDIATELY		= 1		/*!< Reports immediately entrance to cell during a visibility query  */
    };

    static UMBRADEC Cell*	create					(void);
    UMBRADEC void			getCellToWorldMatrix    (Matrix4x4&) const;
    UMBRADEC void			getCellToWorldMatrix    (Matrix4x4d&) const;
    UMBRADEC void			getWorldToCellMatrix    (Matrix4x4&) const;
    UMBRADEC void			getWorldToCellMatrix    (Matrix4x4d&) const;
    UMBRADEC void			set                     (Property,bool);
    UMBRADEC void			setCellToWorldMatrix    (const Matrix4x4&);
    UMBRADEC void			setCellToWorldMatrix    (const Matrix4x4d&);
    UMBRADEC bool			test                    (Property) const;
    class ImpCell*          getImplementation       (void) const;
protected:
                            Cell					(class ImpReferenceCount*);
    virtual void			destruct				(void) const;
private:                                        
                            Cell                    (const Cell&);              // not allowed
    Cell&                   operator=               (const Cell&);              // not allowed
};

} // namespace Umbra

//------------------------------------------------------------------------
#endif // __UMBRACELL_HPP
