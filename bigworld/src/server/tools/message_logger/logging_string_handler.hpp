/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LOGGING_STRING_HANDLER_HPP
#define LOGGING_STRING_HANDLER_HPP

#include <vector>

#include "network/file_stream.hpp"
#include "network/format_string_handler.hpp"
#include "network/bsd_snprintf.h"

class BinaryFile;

/**
 * This class handles both reading and writing bwlogs.
 */
class LoggingStringHandler : public FormatStringHandler
{
public:
	LoggingStringHandler() {}
	LoggingStringHandler( const std::string &fmt );
	~LoggingStringHandler() {}

	// FormatStringHandler interface
	void onString( int start, int end );
	void onToken( char type, int cflags, int min, int max,
		int flags, uint8 base, int vflags );

	uint32 fileOffset() const { return fileOffset_; }
	void write( FileStream &fs );
	void read( FileStream &fs );

	const std::string &fmt() const { return fmt_; }

private:
#	pragma pack( push, 1 )
	class StringOffset
	{
	public:
		StringOffset() {}
		StringOffset( int start, int end ) : start_( start ), end_( end ) {}

		uint16 start_;
		uint16 end_;
	};
#	pragma pack( pop )

public:
#	pragma pack( push, 1 )
	class FormatData
	{
	public:
		FormatData() {}
		FormatData( char type, int cflags, int base,
			int min, int max, int flags, int vflags ) :
			type_( type ), cflags_( cflags ), vflags_( vflags ), base_( base ),
			min_( min ), max_( max ), flags_( flags ) {}

		char type_;
		unsigned cflags_:4;
		unsigned vflags_:4;
		uint8 base_;
		int min_;
		int max_;
		int flags_;
	};
#	pragma pack( pop )

	/**
	 * StreamParsers - for reading either network or file streams of log data
	 */
	class PrintingParser
	{
	public:
		void onFmtStringSection( const std::string &fmt, int start, int end )
		{
			bsdFormatString( fmt.c_str() + start, 0, 0, end-start, *pStr_ );
		}

		void onMinWidth( WidthType w, FormatData &fd )
		{
			fd.min_ = w;
		}

		void onMaxWidth( WidthType w, FormatData &fd )
		{
			fd.max_ = w;
		}

		template <class IntType>
		void onInt( IntType i, const FormatData &fd )
		{
			bsdFormatInt( i, fd.base_, fd.min_, fd.max_, fd.flags_, *pStr_ );
		}

		template <class FloatType>
		void onFloat( FloatType f, const FormatData &fd )
		{
			bsdFormatFloat( f, fd.min_, fd.max_, fd.flags_, *pStr_ );
		}

		void onString( const char *s, const FormatData &fd )
		{
			bsdFormatString( s, fd.flags_, fd.min_, fd.max_, *pStr_ );
		}

		void onPointer( void *ptr, const FormatData &fd )
		{
			char buf[11];
			bw_snprintf( buf, sizeof(buf), "%p", ptr );
			this->onString( buf, fd );
		}

		void onChar( char c, const FormatData &fd )
		{
			char buf[2] = { c, 0 };
			this->onString( buf, fd );
		}

		std::string *pStr_;
	};

	class LogWritingParser
	{
	public:
		LogWritingParser( FileStream &blobFile ) : blobFile_( blobFile ) {}

		void onFmtStringSection( const std::string &fmt, int start, int end )
		{
		}

		void onMinWidth( WidthType w, FormatData &fd )
		{
			blobFile_ << w;
		}

		void onMaxWidth( WidthType w, FormatData &fd )
		{
			blobFile_ << w;
		}

		template <class IntType>
		void onInt( IntType i, const FormatData &fd )
		{
			blobFile_ << i;
		}

		template <class FloatType>
		void onFloat( FloatType f, const FormatData &fd )
		{
			blobFile_ << f;
		}

		void onString( const char *s, const FormatData &fd )
		{
			blobFile_ << s;
		}

		void onPointer( void *ptr, const FormatData &fd )
		{
			blobFile_ << ptr;
		}

		void onChar( char c, const FormatData &fd )
		{
			blobFile_ << c;
		}

		FileStream &blobFile_;
	};

private:
	template <class Parser>
	bool parseStream( Parser &parser, BinaryIStream &is );

public:
	bool streamToLog( LogWritingParser &parser, BinaryIStream &is );
	bool streamToString( BinaryIStream &is, std::string &str );

protected:
	std::string fmt_;
	std::string components_;
	std::vector< StringOffset > stringOffsets_;
	std::vector< FormatData > fmtData_;
	uint32 fileOffset_;
};

#endif
