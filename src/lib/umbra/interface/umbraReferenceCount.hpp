#ifndef __UMBRAREFERENCECOUNT_HPP
#define __UMBRAREFERENCECOUNT_HPP
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
 * \brief     Reference counting mechanism used by most Umbra public classes
 * 
 *//*-------------------------------------------------------------------*/

#if !defined (__UMBRADEFS_HPP)
#   include "umbraDefs.hpp"
#endif

namespace Umbra
{

/*-------------------------------------------------------------------*//*!
 *
 * \brief            Reference counting base class
 *
 * \note            Classes in the library that use reference counting are
 *                  all derived from this class. Instances of reference
 *                  counted classes cannot be destructed by calling
 *                  'delete', instead the member function release() should
 *                  be used.
 *
 *                  Note that only Umbra classes can inherit the
 *                  ReferenceCount class.
 *
 *                  Note that the user cannot inherit from Umbra classes.
 *
 * \sa              ReferenceCount::release()
 *
 *//*-------------------------------------------------------------------*/

class ReferenceCount
{
public:
    UMBRADEC void           addReference        (void);         
    UMBRADEC void           autoRelease         (void);         

    UMBRADEC const char*    getName             (void) const;
    UMBRADEC int            getReferenceCount   (void) const;   
    UMBRADEC void*          getUserPointer      (void) const;
    UMBRADEC bool           release             (void) const;   
    UMBRADEC void           setName             (const char*);
    UMBRADEC void           setUserPointer      (void*);

    static UMBRADEC bool	debugIsValidPointer    (const ReferenceCount*);
protected:
                            ReferenceCount      (class ImpReferenceCount*);         
    virtual                 ~ReferenceCount     (void);   
    virtual void            destruct            (void) const;
    INT32                   m_reserved0;                    // reserved for internal use
private:
    mutable INT32           m_referenceCount    :31;        // reference count of the object
    mutable UINT32          m_autoReleased      :1;         // has the object been auto-released?
    void*                   m_userPointer;                  // user data pointer
};

} // namespace Umbra

//------------------------------------------------------------------------
#endif //__UMBRAREFERENCECOUNT_HPP
