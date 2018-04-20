/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "terrain/height_map_compress.hpp"
#include "moo/png.hpp"


namespace
{
	/**
	 *	This is a magic number for checking whether the data was compressed 
	 *	using quantized PNG.
	 */
	const uint32 QUANTIZED_PNG_VERSION	= 0x71706e67;	// 'qpng'


	/**
	 *	The following are used to compress/decompress using quantized PNG.
	 *	Here we quantize each floating point height to integer coordinates on
	 *	a millimeter grid.  We then do a 32bpp PNG compression on the result.
	 */
	const float	QUANTIZATION_LEVEL	= 0.001f;	// quantize heights to mm

	/**
	 *	This converts a height value to an int32.
	 *
	 *	@param h		The height value.
	 *	@return			The quantized height value.
	 */
	inline int32 quantize(float h)
	{
		return (int32)(h/QUANTIZATION_LEVEL + 0.5f);
	}


	/**
	 *	This converts a quantized height value to a float.
	 *
	 *	@param q		The quantized height.
	 *	@return			The unquantized height.
	 */
	inline float unquantize(int32 q)
	{
		return q*QUANTIZATION_LEVEL;
	}


	/**
	 *	This checks whether the data was compressed with quantized PNG.
	 *
	 *	@param data		The compressed data.
	 *	@return			True if the data was compressed as quantized PNG.
	 */
	bool isQuantizedPNG(BinaryPtr data)
	{
		BW_GUARD;
		if (!data || (size_t)data->len() <= sizeof(uint32))
			return false;

		uint32 magick = *(uint32 const*)data->data();
		return magick == QUANTIZED_PNG_VERSION;
	}


	/**
	 *	This decompresses the height map using the quantized PNG.
	 *
	 *	@param data			The data to decompress.
	 *	@param heightMap	The decompressed height map.
	 *	@return			True upon successful decompression, false otherwise
	 */
	bool quantizedPNGDecompress(BinaryPtr data, Moo::Image<float> &heightMap)
	{
		BW_GUARD;
		if (!isQuantizedPNG(data))
			return false;

		// Skip past the header
		uint32 const *udata = (uint32 const *)data->data();
		++udata;

		BinaryPtr subdata = 
			new BinaryBlock
			(
				udata, 
				data->len() - sizeof(uint32), 
				"Terrain/HeightMapCompression/Image",
				data
			);

		// Decompress the quantized heights:
		Moo::PNGImageData pngData;
		if (!Moo::decompressPNG(subdata, pngData))
			return false;

		// Create a dummy image around the integerised height data:	
		Moo::Image<int32> 
			qimage
			(
				pngData.width_, pngData.height_, 
				(int32 *)pngData.data_, 
				false // only wrap the data
			);

		// Unquantize the heights:
		heightMap.resize(pngData.width_, pngData.height_);		
		for (uint32 y = 0; y < pngData.height_; ++y)
		{
			int32 const *src  = qimage   .getRow(y);
			float       *dst  = heightMap.getRow(y);
			float		*dend = dst + pngData.width_;
			for (; dst != dend; ++src, ++dst)
				*dst = unquantize(*src);
		}

		// Cleanup:
		delete[] pngData.data_; 
		pngData.data_ = NULL;
		
		return true;
	}


	/**
	 *	This compresses the height map using quantized PNG.
	 *
	 *	@param heightMap	The height map to compress.
	 *	@return				The compressed height map in binary form.
	 */
	BinaryPtr quantizedPNGCompress(Moo::Image<float> const &heightMap)
	{
		BW_GUARD;
		if (heightMap.isEmpty())
			return NULL;

		uint32 width  = heightMap.width();
		uint32 height = heightMap.height();

		// Convert to a quantized image:
		Moo::Image<int32> qheightMap(width, height);	
		for (uint32 y = 0; y < height; ++y)
		{
			const float *src  = heightMap .getRow(y);
			int32       *dst  = qheightMap.getRow(y);
			int32		*dend = dst + width;
			for (; dst != dend; ++src, ++dst)
				*dst = quantize(*src);
		}

		// Return a PNG compression of the quantized heights:
		Moo::PNGImageData pngData;
		pngData.data_		= (uint8 *)qheightMap.getRow(0);
		pngData.width_		= width;
		pngData.height_		= height;
		pngData.bpp_		= 32;
		pngData.stride_		= width*sizeof(int32);
		pngData.upsideDown_	= false;
		BinaryPtr imgData = Moo::compressPNG(pngData, "Terrain/HeightMapCompression/Image");

		// Now add the header to the front:
		std::vector<uint8> data(imgData->len() + sizeof(uint32));
		*((uint32 *)&data[0]) = QUANTIZED_PNG_VERSION;
		::memcpy(&data[sizeof(uint32)], imgData->data(), imgData->len());

		// Return the result:
		return new BinaryBlock(&data[0], data.size(), "Terrain/HeightMapCompression/BinaryBlock");
	}
}


/**
 *	This compresses the height map into something that can be stored on disk.
 *
 *	@param heightMap	The height map to compress.
 *	@return			The compressed height map in binary form.
 */
BinaryPtr Terrain::compressHeightMap(Moo::Image<float> const &heightMap)
{
	BW_GUARD;
	return quantizedPNGCompress(heightMap);
}


/**
 *	This decompresses the height map.
 *
 *	@param data		The data to decompress.
 *	@param heightMap	The decompressed height map.
 *	@return			True upon successful decompression.
 */
bool Terrain::decompressHeightMap(BinaryPtr data, Moo::Image<float> &heightMap)
{
	BW_GUARD;
	if (isQuantizedPNG(data))
		return quantizedPNGDecompress(data, heightMap); 
	return false; // unknown format
}
