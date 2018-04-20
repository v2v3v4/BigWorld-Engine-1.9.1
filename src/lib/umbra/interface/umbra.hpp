#ifndef __UMBRA_HPP
#define __UMBRA_HPP
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
 * \brief     Public interface main header file that includes
 *            all other Umbra public header files. Include
 *            this file in your application if you don't know
 *            which specific Umbra headers are needed.
 * 
 *//*-------------------------------------------------------------------*/

#if !defined(__UMBRACAMERA_HPP)
#   include "umbraCamera.hpp"
#endif
#if !defined(__UMBRACELL_HPP)
#   include "umbraCell.hpp"
#endif
#if !defined(__UMBRACOMMANDER_HPP)
#   include "umbraCommander.hpp"
#endif
#if !defined (__UMBRALIBRARY_HPP)       
#   include "umbraLibrary.hpp"
#endif
#if !defined(__UMBRAMODEL_HPP)
#   include "umbraModel.hpp"
#endif
#if !defined(__UMBRAOBJECT_HPP)
#   include "umbraObject.hpp"
#endif

//------------------------------------------------------------------------
#endif // __UMBRA_HPP
