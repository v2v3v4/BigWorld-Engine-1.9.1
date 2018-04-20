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
#include "effect_state_manager.hpp"

DECLARE_DEBUG_COMPONENT2( "Moo", 0 );

extern void * gRenderTgtTexture;

namespace Moo
{

PROFILER_DECLARE( StateManager_SetPixelShader, "StateManager SetPixelShader" );
PROFILER_DECLARE( StateManager_SetPixelShaderConstant, "StateManager SetPixelShaderConstant" );
PROFILER_DECLARE( StateManager_SetVertexShader, "StateManager SetVertexShader" );
PROFILER_DECLARE( StateManager_SetVertexShaderConstant, "StateManager SetVertexShaderConstant" );

/*
 * Com QueryInterface method
 */
HRESULT __stdcall StateManager::QueryInterface( REFIID iid, LPVOID *ppv)
{
    if (iid == IID_IUnknown || iid == IID_ID3DXEffectStateManager)
    {
        *ppv = static_cast<ID3DXEffectStateManager*>(this);
    } 
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    reinterpret_cast<IUnknown*>(this)->AddRef();
    return S_OK;
}

/*
 * Com AddRef method
 */
ULONG __stdcall StateManager::AddRef()
{
    this->incRef();
	return this->refCount();
}

/*
 * Com Release method
 */
ULONG __stdcall StateManager::Release()
{
    this->decRef();
	return this->refCount();
}

/*
 *	LightEnable
 */
HRESULT __stdcall StateManager::LightEnable( DWORD index, BOOL enable )
{
	return rc().device()->LightEnable( index, enable );
}

/*
 *	SetFVF
 */
HRESULT __stdcall StateManager::SetFVF( DWORD fvf )
{
	return rc().setFVF( fvf );
}

/*
 *	SetLight
 */
HRESULT __stdcall StateManager::SetLight( DWORD index, CONST D3DLIGHT9* pLight )
{
	return rc().device()->SetLight( index, pLight );
}

/*
 *	SetMaterial
 */
HRESULT __stdcall StateManager::SetMaterial( CONST D3DMATERIAL9* pMaterial )
{
	return rc().device()->SetMaterial( pMaterial );
}

/*
 *	SetNPatchMode
 */
HRESULT __stdcall StateManager::SetNPatchMode( FLOAT nSegments )
{
	return rc().device()->SetNPatchMode( nSegments );
}

/*
 *	SetPixelShader
 */
HRESULT __stdcall StateManager::SetPixelShader( LPDIRECT3DPIXELSHADER9 pShader )
{
	PROFILER_SCOPED( StateManager_SetPixelShader );
	return rc().device()->SetPixelShader( pShader );
}

/*
 *	SetPixelShaderConstantB
 */
HRESULT __stdcall StateManager::SetPixelShaderConstantB( UINT reg, CONST BOOL* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetPixelShaderConstant );
	return rc().device()->SetPixelShaderConstantB( reg, pData, count );
}

/*
 *	SetPixelShaderConstantF
 */
HRESULT __stdcall StateManager::SetPixelShaderConstantF( UINT reg, CONST FLOAT* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetPixelShaderConstant );
	return rc().device()->SetPixelShaderConstantF( reg, pData, count );

}

/*
 *	SetPixelShaderConstantI
 */
HRESULT __stdcall StateManager::SetPixelShaderConstantI( UINT reg, CONST INT* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetPixelShaderConstant );
	return rc().device()->SetPixelShaderConstantI( reg, pData, count );
}

/*
 *	SetVertexShader
 */
HRESULT __stdcall StateManager::SetVertexShader( LPDIRECT3DVERTEXSHADER9 pShader )
{
	PROFILER_SCOPED( StateManager_SetVertexShader );
	return rc().setVertexShader( pShader );
}

/*
 *	SetVertexShaderConstantB
 */
HRESULT __stdcall StateManager::SetVertexShaderConstantB( UINT reg, CONST BOOL* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetVertexShaderConstant );
	return rc().device()->SetVertexShaderConstantB( reg, pData, count );
}

/*
 *	SetVertexShaderConstantF
 */
HRESULT __stdcall StateManager::SetVertexShaderConstantF( UINT reg, CONST FLOAT* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetVertexShaderConstant );
	HRESULT hr;
	hr = rc().device()->SetVertexShaderConstantF( reg, pData, count );
	return hr;
}

/*
 *	SetVertexShaderConstantI
 */
HRESULT __stdcall StateManager::SetVertexShaderConstantI( UINT reg, CONST INT* pData, UINT count )
{
	PROFILER_SCOPED( StateManager_SetVertexShaderConstant );
	return rc().device()->SetVertexShaderConstantI( reg, pData, count );
}

/*
 *	SetRenderState
 */
HRESULT __stdcall StateManager::SetRenderState( D3DRENDERSTATETYPE state, DWORD value )
{
	if (Moo::rc().mirroredTransform() && state == D3DRS_CULLMODE)
		value = value ^ (value >> 1);
	return rc().setRenderState( state, value );
}

/*
 *	SetVertexSamplerState
 */
HRESULT __stdcall StateManager::SetSamplerState( DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value )
{
	return rc().setSamplerState( sampler, type, value );
}

/*
 *	SetTexture
 */
HRESULT __stdcall StateManager::SetTexture( DWORD stage, LPDIRECT3DBASETEXTURE9 pTexture)
{
	return rc().setTexture( stage, pTexture );
}

/*
 *	SetTextureStageState
 */
HRESULT __stdcall StateManager::SetTextureStageState( DWORD stage, D3DTEXTURESTAGESTATETYPE type,
	DWORD value )
{
	return rc().setTextureStageState( stage, type, value );
}

/*
 *	SetTransform
 */
HRESULT __stdcall StateManager::SetTransform( D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* pMatrix )
{
	return rc().device()->SetTransform( state, pMatrix );
}


StateManager::StateManager()
{
}

// The constant allocator typedefs
typedef ConstantAllocator<BOOL> BoolAllocator;
typedef ConstantAllocator<Vector4> Vector4Allocator;
typedef ConstantAllocator<IntVector4> IntVector4Allocator;

/*
 *	Record LightEnable state
 */
HRESULT __stdcall StateRecorder::LightEnable( DWORD index, BOOL enable )
{
	lightEnable_.push_back( std::make_pair( index, enable ) );
	return S_OK;
}

/*
 *	Record FVF
 */
HRESULT __stdcall StateRecorder::SetFVF( DWORD fvf )
{
	fvf_.first = true;
	fvf_.second = fvf;
	return S_OK;
}

/*
 *	Record Light
 */
HRESULT __stdcall StateRecorder::SetLight( DWORD index, CONST D3DLIGHT9* pLight )
{
	lights_.push_back( std::make_pair( index, *pLight ) );
	return S_OK;
}

/*
 *	Record Material
 */
HRESULT __stdcall StateRecorder::SetMaterial( CONST D3DMATERIAL9* pMaterial )
{
	material_.first = true;
	material_.second = *pMaterial;
	return S_OK;
}

/*
 *	Record NPatchMode
 */
HRESULT __stdcall StateRecorder::SetNPatchMode( FLOAT nSegments )
{
	nPatchMode_.first = true;
	nPatchMode_.second = nSegments;
	return S_OK;
}

/*
 *	Record PixelShader
 */
HRESULT __stdcall StateRecorder::SetPixelShader( LPDIRECT3DPIXELSHADER9 pShader )
{
	pixelShader_.first = true;
	pixelShader_.second = pShader;
	return S_OK;
}

/*
 *	Record bool pixel shader constant
 */
HRESULT __stdcall StateRecorder::SetPixelShaderConstantB( UINT reg, CONST BOOL* pData, UINT count )
{
	pixelShaderConstantsB_.push_back( std::make_pair( reg, BoolAllocator::instance().init( pData, count ) ) );
	return S_OK;
}

/*
 *	Record float pixel shader constant
 */
HRESULT __stdcall StateRecorder::SetPixelShaderConstantF( UINT reg, CONST FLOAT* pData, UINT count )
{
	pixelShaderConstantsF_.push_back( std::make_pair( reg, Vector4Allocator::instance().init( (const Vector4*)pData, count ) ) );
	return S_OK;
}

/*
 *	Record int pixel shader constant
 */
HRESULT __stdcall StateRecorder::SetPixelShaderConstantI( UINT reg, CONST INT* pData, UINT count )
{
	pixelShaderConstantsI_.push_back( std::make_pair( reg, IntVector4Allocator::instance().init( (const IntVector4*)pData, count ) ) );
	return S_OK;
}

/*
 *	Record vertex shader
 */
HRESULT __stdcall StateRecorder::SetVertexShader( LPDIRECT3DVERTEXSHADER9 pShader )
{
	vertexShader_.first = true;
	vertexShader_.second = pShader;
	return S_OK;
}

/*
 *	Record bool vertex shader constant
 */
HRESULT __stdcall StateRecorder::SetVertexShaderConstantB( UINT reg, CONST BOOL* pData, UINT count )
{
	vertexShaderConstantsB_.push_back( std::make_pair( reg, BoolAllocator::instance().init( pData, count ) ) );
	return S_OK;
}

/*
 *	Record float vertex shader constant
 */
HRESULT __stdcall StateRecorder::SetVertexShaderConstantF( UINT reg, CONST FLOAT* pData, UINT count )
{
	vertexShaderConstantsF_.push_back( std::make_pair( reg, Vector4Allocator::instance().init( (const Vector4*)pData, count ) ) );
	return S_OK;
}

/*
 *	Record int vertex shader constant
 */
HRESULT __stdcall StateRecorder::SetVertexShaderConstantI( UINT reg, CONST INT* pData, UINT count )
{
	vertexShaderConstantsI_.push_back( std::make_pair( reg, IntVector4Allocator::instance().init( (const IntVector4*)pData, count ) ) );
	return S_OK;
}

/*
 *	Record render state
 */
HRESULT __stdcall StateRecorder::SetRenderState( D3DRENDERSTATETYPE state, DWORD value )
{
	static RenderState rs;

	if (Moo::rc().mirroredTransform() && state == D3DRS_CULLMODE)
		value = value ^ (value >> 1);

	rs.state = state;
	rs.value = value;
	renderStates_.push_back( rs );
	
	return S_OK;
}

/*
 *	Record sampler state
 */
HRESULT __stdcall StateRecorder::SetSamplerState( DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value )
{
	static SamplerState ss;
	ss.sampler = sampler;
	ss.type = type;
	ss.value = value;
	samplerStates_.push_back( ss );
	return S_OK;
}



/*
 *	Record texture
 */
HRESULT __stdcall StateRecorder::SetTexture( DWORD stage, LPDIRECT3DBASETEXTURE9 pTexture )
{
	textures_.push_back( std::make_pair( stage, pTexture ) );
	return S_OK;
}

/*
 *	Record texture stage state
 */
HRESULT __stdcall StateRecorder::SetTextureStageState( DWORD stage, D3DTEXTURESTAGESTATETYPE type,
	DWORD value )
{
	static TextureStageState tss;
	tss.stage = stage;
	tss.type = type;
	tss.value = value;
	textureStageStates_.push_back( tss );
	return S_OK;
}

/*
 *	Record transform
 */
HRESULT __stdcall StateRecorder::SetTransform( D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX* pMatrix )
{
	transformStates_.push_back( std::make_pair( state, *pMatrix ) );
	return S_OK;
}

/*
 *	Helper method for setting shader constants
 */
template <class Container, class InputFormat >
void setConstants( const Container& container, 
	HRESULT ( __stdcall DX::Device::*functor)(UINT, InputFormat*, UINT) )
{
	Container::const_iterator it = container.begin();
	Container::const_iterator end = container.end();
	while (it != end)
	{
		(rc().device()->*functor)( it->first, (InputFormat*)it->second->data(), it->second->size() );
		it++;
	}
}

/*
 *	Flush the recorded states, constants etc.
 */
void StateRecorder::setStates()
{
	DX::Device* pDev = rc().device();

	if (fvf_.first) { rc().setFVF( fvf_.second ); }
	if (vertexShader_.first) { rc().setVertexShader( vertexShader_.second.pComObject() ); }
	if (pixelShader_.first) { pDev->SetPixelShader( pixelShader_.second.pComObject() ); }

	setConstants( vertexShaderConstantsF_, &DX::Device::SetVertexShaderConstantF );
	setConstants( vertexShaderConstantsI_, &DX::Device::SetVertexShaderConstantI );
	setConstants( vertexShaderConstantsB_, &DX::Device::SetVertexShaderConstantB );

	setConstants( pixelShaderConstantsF_, &DX::Device::SetPixelShaderConstantF );
	setConstants( pixelShaderConstantsI_, &DX::Device::SetPixelShaderConstantI );
	setConstants( pixelShaderConstantsB_, &DX::Device::SetPixelShaderConstantB );

	setRenderStates();
	setTextureStageStates();
	setSamplerStates();
	setTransforms();
	setTextures();
	setLights();

	if (material_.first) { pDev->SetMaterial( &material_.second ); };
	if (nPatchMode_.first) { pDev->SetNPatchMode( nPatchMode_.second ); }
}

/*
 *	Init the state recorder, 
 */
void StateRecorder::init()
{
	vertexShaderConstantsF_.clear();
	vertexShaderConstantsI_.clear();
	vertexShaderConstantsB_.clear();

	pixelShaderConstantsF_.clear();
	pixelShaderConstantsI_.clear();
	pixelShaderConstantsB_.clear();

	renderStates_.clear();
	textureStageStates_.clear();
	samplerStates_.clear();

	transformStates_.clear();
	textures_.clear();
	lightEnable_.clear();
	lights_.clear();

	vertexShader_.first = false;
	vertexShader_.second = NULL;
	pixelShader_.first = false;
	pixelShader_.second = NULL;
	fvf_.first = false;
	material_.first = false;
	nPatchMode_.first = false;
}

/*
 *	Flush the render states
 */
void StateRecorder::setRenderStates()
{
	RenderStates::iterator it = renderStates_.begin();
	RenderStates::iterator end = renderStates_.end();
	while (it != end)
	{
		rc().setRenderState( it->state, it->value );
		it++;
	}
}

/*
 *	Flush the texture stage states
 */
void StateRecorder::setTextureStageStates()
{
	TextureStageStates::iterator it = textureStageStates_.begin();
	TextureStageStates::iterator end = textureStageStates_.end();
	while (it != end)
	{
		rc().setTextureStageState( it->stage, it->type, it->value );
		it++;
	}
}

/*
 *	Flush the sampler states
 */
void StateRecorder::setSamplerStates()
{
	SamplerStates::iterator it = samplerStates_.begin();
	SamplerStates::iterator end = samplerStates_.end();
	while (it != end)
	{
		rc().setSamplerState( it->sampler, it->type, it->value );
		it++;
	}
}

/*
 *	Flush the transforms
 */
void StateRecorder::setTransforms()
{
	DX::Device* pDev = rc().device();
	TransformStates::iterator it = transformStates_.begin();
	TransformStates::iterator end = transformStates_.end();
	while (it != end)
	{
		pDev->SetTransform( it->first, &it->second );
		it++;
	}
}

/*
 *	Flush the textures
 */
void StateRecorder::setTextures()
{
	Textures::iterator it = textures_.begin();
	Textures::iterator end = textures_.end();
	while (it != end)
	{
		rc().setTexture( it->first, it->second.pComObject() );
		it++;
	}
}

/*
 *	Flush the lights
 */
void StateRecorder::setLights()
{
	DX::Device* pDev = rc().device();
	LightStates::iterator it = lightEnable_.begin();
	LightStates::iterator end = lightEnable_.end();

	while (it != end)
	{
		pDev->LightEnable( it->first, it->second );
		it++;
	}

	Lights::iterator lit = lights_.begin();
	Lights::iterator lend = lights_.end();

	while (lit != lend)
	{
		pDev->SetLight( lit->first, &lit->second );
		lit++;
	}
	

}


/**
 *	This static method gets a new staterecorder that is valid until the next frame.
 *	@return new state recorder
 */
StateRecorder* StateRecorder::get()
{
	static uint32 timeStamp = rc().frameTimestamp();
	if (!rc().frameDrawn( timeStamp ))
		s_nextAlloc_ = 0;

	if (s_nextAlloc_ == s_stateRecorders_.size())
		s_stateRecorders_.push_back( new StateRecorder );
	return s_stateRecorders_[s_nextAlloc_++].getObject();
}

/**
 *	This static method clears out any recorded state, and drops all the
 *	resource references that they hold.
 */
void StateRecorder::clear()
{
	s_stateRecorders_.clear();
	s_nextAlloc_ = 0;
}

uint32 StateRecorder::s_nextAlloc_ = 0;
std::vector< SmartPointer<StateRecorder> > StateRecorder::s_stateRecorders_;

}

// effect_state_manager.cpp
