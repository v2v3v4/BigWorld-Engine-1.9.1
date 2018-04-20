/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/


// AssetProcessorDLL.cpp : Defines the entry point for the DLL application.
//

#include "pch.hpp"
#include "assetprocessor.hpp"
#include "asset_processor_script.hpp"
#include "cstdmf/processor_affinity.hpp"
#include "resmgr/bwresource.hpp"
#include "entitydef/constants.hpp"
#include "pyscript/script.hpp"
#include <direct.h>

extern int ResMgr_token;


#ifdef _MANAGED
#pragma managed(push, off)
#endif

ASSETPROCESSOR_API void init_AssetProcessor()
{	
	uint32 processor = 0;
	FILE* f;

	//read current affinity
	f = fopen( "processor_affinity.bin", "rb" );
	if (f)
	{
		fread( &processor, sizeof(processor), 1, f );
		fclose(f);
	}

	processor = processor+1;
	ProcessorAffinity::set(processor);
	uint32 actual = ProcessorAffinity::get();

	//write current affinity
	f = fopen( "processor_affinity.bin", "wb" );
	fwrite( &actual, sizeof(actual), 1, f );
	fclose(f);	

	if (!BWResource::init( 0, NULL ))
	{
		return;
	}

	int tokens = ResMgr_token;

	if (!Script::init(
			EntityDef::Constants::entitiesClientPath(),
			"client" + tokens ) )
	{
		return;
	}

	AssetProcessorScript::init();

	PyErr_Clear();

    return;
}


BOOL __stdcall DllMain( HANDLE hModule, DWORD Reason, LPVOID Reserved )
{
	if (Reason == DLL_PROCESS_DETACH)
	{
		AssetProcessorScript::fini();

		Script::fini();

		BWResource::fini();
	}
	return TRUE;
}


#ifdef _MANAGED
#pragma managed(pop)
#endif
