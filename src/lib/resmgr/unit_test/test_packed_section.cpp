/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "stdafx.h"

#include "cstdmf/cstdmf.hpp"
#include "resmgr/packed_section.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/multi_file_system.hpp"
#include "cstdmf/debug.hpp"

struct Fixture
{
	Fixture():
		fixture_( NULL )
	{
		new CStdMf;
		// Open section
		fixture_ = BWResource::openSection( "test_packed_section" );
	}

	~Fixture()
	{
		fixture_ = NULL;
		BWResource::instance().purgeAll();	// Clear any cached values
		delete CStdMf::pInstance();
	}

	DataSectionPtr fixture_;
};

// Conversion tests

TEST_F( Fixture, PackedSection_AsBool )
{
	CHECK( fixture_ != NULL );
	DataSectionPtr boolSection = fixture_->openSection( "test_bool" );
	CHECK( boolSection != NULL );

	CHECK_EQUAL( true, boolSection->asBool() );
}

TEST_F( Fixture, PackedSection_AsInt )
{
	DataSectionPtr intSection = fixture_->openSection( "test_int" );
	CHECK( intSection != NULL );

	CHECK_EQUAL( -123, intSection->asInt() );
}

TEST_F( Fixture, PackedSection_AsFloat )
{
	DataSectionPtr floatSection = fixture_->openSection( "test_float" );
	CHECK( floatSection != NULL );

	CHECK_EQUAL( 3.142f, floatSection->asFloat() );
}

TEST_F( Fixture, PackedSection_AsVector3 )
{
	DataSectionPtr vec3Section = fixture_->openSection( "test_vector3" );
	CHECK( vec3Section != NULL );

	Vector3 result( vec3Section->asVector3() );
	CHECK_EQUAL( 1.0f, result.x );
	CHECK_EQUAL( 2.0f, result.y );
	CHECK_EQUAL( 3.0f, result.z );
}

TEST_F( Fixture, PackedSection_AsMatrix34 )
{
	DataSectionPtr mat34Section = fixture_->openSection( "test_matrix34" );
	CHECK( mat34Section != NULL );

	Matrix result( mat34Section->asMatrix34() );

	CHECK_EQUAL( 1.0f, result[0].x );
	CHECK_EQUAL( 2.0f, result[0].y );
	CHECK_EQUAL( 3.0f, result[0].z );

	CHECK_EQUAL( 4.0f, result[1].x );
	CHECK_EQUAL( 5.0f, result[1].y );
	CHECK_EQUAL( 6.0f, result[1].z );

	CHECK_EQUAL( 7.0f, result[2].x );
	CHECK_EQUAL( 8.0f, result[2].y );
	CHECK_EQUAL( 9.0f, result[2].z );

	CHECK_EQUAL( 10.0f, result[3].x );
	CHECK_EQUAL( 11.0f, result[3].y );
	CHECK_EQUAL( 12.0f, result[3].z );
}

// Conversion tests

TEST_F( Fixture, PackedSection_countChildren_prepacked )
{
 	// Count children for each subsection, using pre-packed section, and compare
	// with xml output
 
	DataSectionPtr x = BWResource::openSection( "test_xml_section.xml" );
	CHECK( x != NULL );
	CHECK( fixture_->countChildren() > 0 );

	if ( x != NULL )
	{
		CHECK_EQUAL( x->countChildren(), fixture_->countChildren() );
	}
}

TEST_F( Fixture, PackedSection_countChildren_convertInMemory )
{
	MultiFileSystemPtr	fileSystem = BWResource::instance().fileSystem();
	fileSystem->eraseFileOrDirectory( "result_packed_section" );

	// Count children for each subsection, after converting from an xml section 
	// to a binary section on disk.
	
	DataSectionPtr	x = BWResource::openSection( "test_xml_section.xml" );
	CHECK( x != NULL );

	if ( x != NULL )
	{
		int		n = x->countChildren();
		bool	r = PackedSection::convert( x , "result_packed_section" );

		CHECK_EQUAL( true, r );

		if ( r )
		{
			CHECK_EQUAL( n, x->countChildren() );
		}
	}
}

TEST_F( Fixture, PackedSection_countChildren_convertOnDisk )
{
	MultiFileSystemPtr	fileSystem = BWResource::instance().fileSystem();
	fileSystem->eraseFileOrDirectory( "result_packed_section2" );

	// Count children for each subsection, after converting from an xml section 
	// to a binary section on disk. This uses the "other" convert method.

	DataSectionPtr	x = BWResource::openSection( "test_xml_section.xml" );
	std::string		i = BWResolver::resolveFilename( "test_xml_section.xml" );
	std::string		o = BWResolver::resolveFilename( "result_packed_section2" );
	bool			r = PackedSection::convert( i, o, NULL, false );

	CHECK_EQUAL( true, r );

	DataSectionPtr p = BWResource::openSection("result_packed_section2");

	CHECK( p != NULL );

	if ( x != NULL && p != NULL )
	{
		CHECK_EQUAL( x->countChildren(), p->countChildren() );
	}
}

// test_packed_section.cpp
