/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __FX_PACKER_HPP__
#define __FX_PACKER_HPP__


#include "base_packer.hpp"


/**
 *	This class currently only copies .fx and .fxh files, and discards .fxo
 *	files. It could be also rewritten to compile the effects to .fxo files, and
 *	then discard the .fx and .fxh, but this would require compiling the effects
 *	to every combination of graphics settings.
 */
class FxPacker : public BasePacker
{
public:
	enum Type {
		FX,
		FXH,
		FXO
	};

	FxPacker() : type_( FX ) {}

	virtual bool prepare( const std::string& src, const std::string& dst );
	virtual bool print();
	virtual bool pack();

private:
	DECLARE_PACKER()
	std::string src_;
	std::string dst_;
	Type type_;
};

#endif // __FX_PACKER_HPP__
