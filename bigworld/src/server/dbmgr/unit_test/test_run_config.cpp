/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef TEST_RUN_CONFIG_HPP
#define TEST_RUN_CONFIG_HPP

#include "third_party/CppUnitLite2/src/CppUnitLite2.h"

#include "dbmgr/db_interface_extras.hpp"

#include "cstdmf/memory_stream.hpp"

TEST( RunConfig_Defaults )
{
	DBInterface::RunConfig 	orig;
	MemoryOStream			strm;
	strm << orig;
	DBInterface::RunConfig 	copy;
	strm >> copy;
	CHECK( !copy.shouldDisableSecondaryDatabases() );
	CHECK( copy.runId().empty() );
}

TEST( RunConfig_Set )
{
	// Keep values after assignment
	DBInterface::RunConfig 	orig;
	orig.shouldDisableSecondaryDatabases( true );
	orig.runId( "Hello World" );
	CHECK( orig.shouldDisableSecondaryDatabases() );
	CHECK( orig.runId() == std::string( "Hello World" ) );
}

TEST( RunConfig_Stream )
{
	// Keep values after streaming/destreaming
	DBInterface::RunConfig 	orig;
	orig.shouldDisableSecondaryDatabases( true );
	orig.runId( "Hello World" );
	MemoryOStream			strm;
	strm << orig;
	DBInterface::RunConfig 	copy;
	strm >> copy;
	MF_ASSERT( copy.shouldDisableSecondaryDatabases() );
	MF_ASSERT( copy.runId() == "Hello World" );
}

#endif // TEST_RUN_CONFIG_HPP
