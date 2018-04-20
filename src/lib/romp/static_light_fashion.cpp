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
#include "static_light_fashion.hpp"

#include "model/model_static_lighting.hpp"


// -----------------------------------------------------------------------------
// Section: StaticLightFashion
// -----------------------------------------------------------------------------

/**
 *	Factory method.
 */
SmartPointer<StaticLightFashion> StaticLightFashion::get(
	SuperModel & sm, const DataSectionPtr modelLightingSection )
{
	char * pBytes = new char[ sizeof(StaticLightFashion)+
		(sm.nModels()-1)*sizeof(ModelStaticLighting*) ];
	StaticLightFashion * pSLF =
		new ( pBytes ) StaticLightFashion( sm, modelLightingSection );

	for (int i = 0; i < pSLF->nModels_; i++)
		if (pSLF->lighting_[i] != NULL) return pSLF;

	delete pSLF;
	return NULL;
}

/**
 *	Constructor.
 */
StaticLightFashion::StaticLightFashion(
	SuperModel & sm, const DataSectionPtr modelLightingSection )
{
	nModels_ = sm.nModels();

	// .lighting file (only first model in the supermodel supported)
	std::string sectionName = modelLightingSection->sectionName();
	if ((sectionName.length() > 9) && (sectionName.substr(sectionName.length() - 9) == ".lighting"))
	{
		MF_ASSERT(modelLightingSection);
		ModelStaticLightingPtr pSL = sm.topModel(0)->getStaticLighting( modelLightingSection );
		lighting_[0] = &*pSL;
		if (pSL)
		{
			pSL->incRef();
		}
	}
	else
	{
		for (int i = 0; i < nModels_; i++)
		{
			// open sub section for each model
			DataSectionPtr theSection = modelLightingSection->openSection(
										lightingTag( i, nModels_ ) );

			if(theSection)
			{
				ModelStaticLightingPtr pSL = sm.topModel(i)->getStaticLighting( theSection );
				lighting_[i] = &*pSL;
				if (pSL)
				{
					pSL->incRef();
				}
			}
			else
				lighting_[i] = NULL;
		}
	}

	/*
	nModels_ = sm.nModels();
	if (nModels_ == 1)
	{
		Model::StaticLightingPtr pSL =
			sm.topModel(0)->getStaticLighting( resNamePrefix );
		lighting_[0] = &*pSL;
		if (pSL) pSL->incRef();

		if (pSL)
			dprintf( "got static lighting for %s\n", resNamePrefix.c_str() );
	}
	else
	{
		std::string resName = resNamePrefix + "_.";
		int rnl = resName.length();
		for (int i = 0; i < nModels_; i++)
		{
			resName[rnl-1] = 'a' + i;
			Model::StaticLightingPtr pSL =
				sm.topModel(i)->getStaticLighting( resName );
			lighting_[i] = &*pSL;
			if (pSL) pSL->incRef();
		}
	}
	*/
}

/**
 *	Destructor.
 */
StaticLightFashion::~StaticLightFashion()
{
	for (int i = 0; i < nModels_; i++)
		if (lighting_[i])
			lighting_[i]->decRef();
}


/**
 *	Dress method.
 */
void StaticLightFashion::dress( SuperModel & superModel )
{
	for (int i = 0; i < nModels_; i++)
	{
		if (lighting_[i] != NULL) lighting_[i]->set();
	}
}

void StaticLightFashion::undress( SuperModel & superModel )
{
	for (int i = 0; i < nModels_; i++)
	{
		if (lighting_[i] != NULL) lighting_[i]->unset();
	}
}


std::vector<StaticLightValues*> StaticLightFashion::staticLightValues()
{
	std::vector<StaticLightValues*> v;
	v.reserve( nModels_ );
	for (int i = 0; i < nModels_; i++)
		if (lighting_[i])
			v.push_back( lighting_[i]->staticLightValues() );
		else
			v.push_back( NULL );


	return v;
}

std::string StaticLightFashion::lightingTag( int index, int count )
{
	MF_ASSERT(index < 100000000);
	char st[10];
	bw_snprintf(st, sizeof(st), "%d", index);
	return st;
}

// static_light_fashion.cpp
