/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "config.hpp"
#include "packers.hpp"
#include "packer_helper.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/packed_section.hpp"
#include "font_packer.hpp"
#ifndef MF_SERVER
#include "romp/font.hpp"
#endif // MF_SERVER


IMPLEMENT_PACKER( FontPacker )

int FontPacker_token = 0;


bool FontPacker::prepare( const std::string& src, const std::string& dst )
{
	std::string ext = BWResource::getExtension( src );
	if (bw_stricmp( ext.c_str(), "font" ))
		return false;

	src_ = src;
	dst_ = dst;

	return true;
}

bool FontPacker::print()
{
	if ( src_.empty() )
	{
		printf( "Error: FontPacker not initialised properly\n" );
		return false;
	}

	printf( "FontFile: %s\n", src_.c_str() );
	return true;
}

bool FontPacker::pack()
{
#ifndef MF_SERVER

	if (src_.empty() || dst_.empty())
	{
		printf( "Error: FontPacker not initialised properly\n" );
		return false;
	}

#ifndef PACK_FONT_DDS
	// copy font file
	if (!PackerHelper::copyFile( src_, dst_ ))
	{
		return false;
	}

	// and remove the "generated" section
	DataSectionPtr pDS = BWResource::openSection( BWResolver::dissolveFilename( dst_ ) );
	if (!pDS)
	{
		ERROR_MSG( "Error opening font %s as a datasection\n", dst_.c_str() );
		return false;
	}
	pDS->delChild( "generated" );
	pDS->save();

	return true;

#else // PACK_FONT_DDS

	// Copy to a temp file in the destination folder before packing, in order
	// to be able to edit the file (a PackedSection is not editable)
	std::string tempFile = dst_ + ".packerTemp.font";
	if ( !PackerHelper::copyFile( src_, tempFile ) )
		return false;

	// declare a deleter for the temp file
	PackerHelper::FileDeleter deleter( tempFile );

	// generate the actual DDS resource, saving changes to the temp file
	FontPtr tempFont = FontManager::instance().get( BWResource::getFilename( tempFile ) );
	if ( !tempFont.exists() )
	{
		printf( "Couldn't generate the font's DDS file\n" );
		return false;
	}
	else
	{
		std::string ddsFileName = tempFont->pTexture()->resourceID();
		
		std::string inFile = PackerHelper::inPath();
		if (inFile.empty())
		{
			inFile = BWResource::getFilePath( src_ ) +
				BWResource::getFilename( ddsFileName );
		}
		else
		{
			inFile += "/" + ddsFileName;
		}

		std::string outFile = PackerHelper::outPath();
		if (outFile.empty())
		{
			outFile = BWResource::getFilePath( dst_ ) +
				BWResource::getFilename( ddsFileName );
		}
		else
		{
			outFile += "/" + ddsFileName;
		}

		if (!PackerHelper::fileExists( outFile.c_str() ))
		{
			// DDS was generated in the src folder, so we need to copy it.
			PackerHelper::FileDeleter deleter( inFile );
			if (!PackerHelper::copyFile( inFile.c_str(), outFile.c_str() ))
			{
				return false;
			}
		}
	}

	// pack the temp file to the desired destination folder
	return PackedSection::convert( tempFile, dst_, NULL );

#endif // PACK_FONT_DDS

#endif // MF_SERVER
	
	return true;
}
