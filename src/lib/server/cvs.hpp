/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CVS_HPP
#define CVS_HPP

#include "resmgr/file_system.hpp"

bool extractFileFromCVS(
	IFileSystem * pResFS, const std::string & resName, const std::string & cvsInfo,
	IFileSystem * pDstFS, const std::string & dstName );

#endif
