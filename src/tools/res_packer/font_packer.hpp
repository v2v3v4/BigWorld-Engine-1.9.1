/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __FONT_PACKER_HPP__
#define __FONT_PACKER_HPP__


#include "base_packer.hpp"


/**
 *	This class currently copies .font files and generates the needed matching
 *	DDS files, or removes the generated portion of the .font files and excludes
 *  matching DDS files, depending on PACK_FONT_DDS preprocessor macro
 */
class FontPacker : public BasePacker
{
public:
	virtual bool prepare( const std::string& src, const std::string& dst );
	virtual bool print();
	virtual bool pack();

private:
	DECLARE_PACKER()
	std::string src_;
	std::string dst_;
};

#endif // __FONT_PACKER_HPP__
