/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#include <vector>

#include "cstdmf/stdmf.hpp"
#include "cstdmf/debug.hpp"
#include "logging_string_handler.hpp"

DECLARE_DEBUG_COMPONENT( 0 );

LoggingStringHandler::LoggingStringHandler( const std::string &fmt ) :
	fmt_( fmt ), fileOffset_( 0xffffffff )
{
	handleFormatString( fmt_.c_str(), *this );
}

void LoggingStringHandler::onString( int start, int end )
{
	stringOffsets_.push_back( StringOffset( start, end ) );
	components_.push_back( 's' );
}


void LoggingStringHandler::onToken( char type, int cflags, int min,
	int max, int flags, uint8 base, int vflags )
{
	fmtData_.push_back( FormatData( type, cflags, base, min,
			max, flags, vflags ) );
	components_.push_back( 't' );
}

void LoggingStringHandler::write( FileStream &fs )
{
	if (fileOffset_ != 0xffffffff)
		return;

	fileOffset_ = fs.tell();
	fs << fmt_ << components_ << stringOffsets_ << fmtData_;
	fs.commit();
}

void LoggingStringHandler::read( FileStream &fs )
{
	fileOffset_ = fs.tell();
	fs >> fmt_ >> components_ >> stringOffsets_ >> fmtData_;
}

bool LoggingStringHandler::streamToString( BinaryIStream &is, std::string &str )
{
	static PrintingParser parser;
	parser.pStr_ = &str;
	return this->parseStream( parser, is );
}

bool LoggingStringHandler::streamToLog(
	LogWritingParser &parser, BinaryIStream &is )
{
	if (!this->parseStream( parser, is ))
		return false;
	parser.blobFile_.commit();
	return parser.blobFile_.good();
}

template <class Parser>
bool LoggingStringHandler::parseStream(
	Parser &parser, BinaryIStream &is )
{
	std::vector< StringOffset >::iterator sit = stringOffsets_.begin();
	std::vector< FormatData >::iterator fit = fmtData_.begin();

	for (unsigned i=0; i < components_.size(); i++)
	{
		if (components_[i] == 's')
		{
			StringOffset &so = *sit++;
			parser.onFmtStringSection( fmt_, so.start_, so.end_ );
		}
		else
		{
			FormatData &fd = *fit++;

			// Macro to terminate parsing on stream error
#			define CHECK_STREAM()											\
			if (is.error())													\
			{																\
				ERROR_MSG( "Stream too short for '%s'\n", fmt_.c_str() );	\
				return false;												\
			}

			if (fd.vflags_ & VARIABLE_MIN_WIDTH)
			{
				WidthType w; is >> w; CHECK_STREAM();
				parser.onMinWidth( w, fd );
			}
			if (fd.vflags_ & VARIABLE_MAX_WIDTH)
			{
				WidthType w; is >> w; CHECK_STREAM();
				parser.onMaxWidth( w, fd );
			}

			switch (fd.type_)
			{
			case 'd':
			{
				switch (fd.cflags_)
				{
				case DP_C_SHORT:
				{
					short val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				case DP_C_LONG:
				{
					long val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				case DP_C_LLONG:
				{
					long long val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				default:
				{
					int val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}
				}
				break;
			}

			case 'o':
			case 'u':
			case 'x':
			{
				switch (fd.cflags_)
				{
				case DP_C_SHORT:
				{
					unsigned short val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				case DP_C_LONG:
				{
					unsigned long val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				case DP_C_LLONG:
				{
					unsigned long long val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}

				default:
				{
					unsigned int val; is >> val; CHECK_STREAM();
					parser.onInt( val, fd );
					break;
				}
				}
				break;
			}

			case 'f':
			case 'e':
			case 'g':
			{
				switch (fd.cflags_)
				{
				case DP_C_LDOUBLE:
				{
					LDOUBLE val; is >> val; CHECK_STREAM();
					parser.onFloat( val, fd );
					break;
				}
				default:
				{
					double val; is >> val; CHECK_STREAM();
					parser.onFloat( val, fd );
					break;
				}
				}
				break;
			}

			case 's':
			{
				std::string val; is >> val; CHECK_STREAM();
				parser.onString( val.c_str(), fd );
				break;
			}

			case 'p':
			{
				void *ptr; is >> ptr; CHECK_STREAM();
				parser.onPointer( ptr, fd );
				break;
			}

			case 'c':
			{
				char c; is >> c; CHECK_STREAM();
				parser.onChar( c, fd );
				break;
			}

			default:
				ERROR_MSG( "Unhandled format: '%c'\n", fd.type_ );
				return false;
			}
		}
	}

	return true;
}
