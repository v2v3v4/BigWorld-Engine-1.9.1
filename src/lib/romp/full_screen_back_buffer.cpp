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
#include "full_screen_back_buffer.hpp"
#include "geometrics.hpp"
#include "texture_feeds.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "Romp", 0 );

FullScreenBackBuffer::Users* FullScreenBackBuffer::s_pUsers_ = NULL;
FullScreenBackBuffer* FullScreenBackBuffer::s_instance_ = NULL;

PyTextureProviderPtr feed;
PyTextureProviderPtr feed2;

FullScreenBackBuffer::FullScreenBackBuffer():
	pRT_( NULL ),
	pRT2_( NULL),
	inited_( false ),
	failed_( false )
{
}


FullScreenBackBuffer::~FullScreenBackBuffer()
{
	if (pRT_)
	{
		pRT_=NULL;
	}
	if (pRT2_)
	{
		pRT2_=NULL;
	}

	if (feed)
	{
		TextureFeeds::delTextureFeed( "backBuffer" );
		feed=NULL;
	}

	if (feed2)
	{
		TextureFeeds::delTextureFeed( "depthBuffer" );
		feed2=NULL;
	}
}

namespace
{

const uint32 largerPow2( uint32 number )
{
	const float LOG2_10 = 3.3219280948873626f;
	float n = log10f( float(number) ) * LOG2_10;
	uint32 shift = uint32(ceil(n));
	if (n-floor(n) < 0.01f)
		shift = uint32(floor(n));
	return 1 << shift;
}

}

void FullScreenBackBuffer::deleteUnmanagedObjects()
{
	if (pRT_)
	{
		pRT_->release();
		pRT_ = NULL;
	
		feed = NULL;
		TextureFeeds::delTextureFeed( "backBuffer" );
	}

	if (pRT2_)
	{
		pRT2_->release();
		pRT2_ = NULL;

		feed2 = NULL;
		TextureFeeds::delTextureFeed( "depthBuffer" );
	}

	inited_ = false;
	failed_ = false;
}

void FullScreenBackBuffer::createUnmanagedObjects()
{
	if (!inited_)
	{
		initInternal();
	}
}

static FSBBSettings s_fsbbSettings;

/**
  *
  */
void FSBBSettings::configureKeywordSetting(Moo::EffectMacroSetting & setting)
{	
	bool supported = Moo::rc().mrtSupported();
		//Moo::rc().supportsTextureFormat( D3DFMT_R32F, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING );
	  
	setting.addOption("ON", "On", supported, "1");
	setting.addOption("OFF", "Off", true, "0");

	Moo::EffectManager::instance().addListener( &s_fsbbSettings );
}

/**
 *
 */
void FSBBSettings::fini()
{
	Moo::EffectManager::instance().delListener( &s_fsbbSettings );
	s_mrtSetting_ = NULL;
}

Moo::EffectMacroSetting::EffectMacroSettingPtr FSBBSettings::s_mrtSetting_ = 
	new Moo::EffectMacroSetting(
		"MRT_DEPTH", "Advanced Post Processing", "USE_MRT_DEPTH",
		&FSBBSettings::configureKeywordSetting);

void FSBBSettings::onSelectPSVersionCap(int psVerCap)
{	
	if (psVerCap < 3 && s_mrtSetting_->activeOption()==0)
		s_mrtSetting_->selectOption(1); //disable
}

bool FSBBSettings::isEnabled()
{
	return s_mrtSetting_->activeOption() == 0;
}

bool FullScreenBackBuffer::mrtEnabled()
{
	return FSBBSettings::isEnabled();
}

/*static*/ bool FullScreenBackBuffer::init()
{
	if ( instance().hasEnabledUsers() )
		return instance().initInternal();
	else
		return false;
}

bool FullScreenBackBuffer::initInternal()
{
	if (pRT_ == NULL)
		pRT_ = new Moo::RenderTarget( "FullScreenBackBufferCopy" );
	if (mrtEnabled())
	{
		if (pRT2_ == NULL)
			pRT2_ = new Moo::RenderTarget( "FullScreenBackBufferCopy2" );

		//TODO: better support for the second RT
		pRT_->setRT2(pRT2_.getObject());
	}

	uint32 w = (uint32)Moo::rc().screenWidth();
	uint32 h = (uint32)Moo::rc().screenHeight();
	uint32 bufferW = w;
	uint32 bufferH = h;
	uint32 caps = Moo::rc().deviceInfo( Moo::rc().deviceIndex() ).caps_.TextureCaps;
	reuseZBuffer_ = true;
	if (caps & D3DPTEXTURECAPS_POW2 && !(caps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		bufferW = largerPow2( w );
		bufferH = largerPow2( h );
		if (caps & D3DPTEXTURECAPS_SQUAREONLY)
			bufferW = bufferH = max( bufferW, bufferW );
		reuseZBuffer_ = false;
	}
	vp_.X = 0;
	vp_.Y = 0;
	vp_.MinZ = 0;
	vp_.MaxZ = 1;
	vp_.Width = DWORD(w);
	vp_.Height = DWORD(h);

	// the EXTRA_MRT_INFO #def adds experimental, higher bit depth render targets
	// which could be used for things like motion blur velocity / hdr etc..
#ifdef EXTRA_MRT_INFO
	if (pRT_->create( bufferW, bufferH, reuseZBuffer_, D3DFMT_A16B16G16R16F ))
#else
	if (pRT_->create( bufferW, bufferH, reuseZBuffer_ ))
#endif //EXTRA_MRT_INFO
	{
		feed = PyTextureProviderPtr( new PyTextureProvider( NULL, pRT_ ), true);
		TextureFeeds::addTextureFeed( "backBuffer",  feed );
		inited_ = true;
	}
	else
	{
		inited_ = false;
		failed_ = true;
		pRT_ = NULL;
		return false;
	}
	if (pRT2_)
	{
#ifdef EXTRA_MRT_INFO
		pRT2_->create( bufferW, bufferH, true, D3DFMT_A16B16G16R16F, pRT_.getObject() );
#else
		//pRT2_->create( bufferW, bufferH, true, D3DFMT_R32F, pRT_.getObject() );
		pRT2_->create( bufferW, bufferH, true, D3DFMT_A8R8G8B8, pRT_.getObject() );
#endif
		feed2 = PyTextureProviderPtr( new PyTextureProvider( NULL, pRT2_ ), true);
		TextureFeeds::addTextureFeed( "depthBuffer",  feed2 );
	}

	return (pRT_->pTexture() != NULL);
}


bool FullScreenBackBuffer::beginSceneInternal()
{
	//Check if anyone actually wants a full screen buffer
	if ( !hasEnabledUsers() )
	{
		return false;
	}
	
	if ( !inited_ && !failed_ )
	{
		this->initInternal();
	}

	if ( !inited_ )
		return false;

	
	//Note pRT_->push sets the width and height of the render context, but not the viewport.
	pRT_->push();
	Moo::rc().setViewport( &vp_ );
	Moo::rc().device()->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, 0x00000080, 1, 0 );

	//Now set the viewport to be screen sized.
	Moo::rc().screenWidth( vp_.Width );
	Moo::rc().screenHeight( vp_.Height );
	Moo::rc().setViewport( &vp_ );

	MF_ASSERT_DEBUG(s_pUsers_ != NULL);
	Users::iterator it = s_pUsers_->begin();
	while (it != s_pUsers_->end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->beginScene();
		}
	}

	return true;
}


void FullScreenBackBuffer::endSceneInternal()
{
	//If nobody wanted us, we don't need to end scene.
	if ( !hasEnabledUsers() || !inited_ )
	{
		return;
	}

	MF_ASSERT_DEBUG(s_pUsers_ != NULL);

	Users::iterator it = s_pUsers_->begin();
	while (it != s_pUsers_->end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->endScene();
		}
	}	
	
	pRT_->pop();

	//Now do the transfer
	bool transferred = false;
	it = s_pUsers_->begin();
	while (it != s_pUsers_->end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			transferred |= user->doTransfer( transferred );
		}
	}

	//If nobody has transferred the offscreen back buffer over
	//to the main back buffer, then do it now.
	if (!transferred)
	{		
		Moo::rc().device()->SetPixelShader(NULL);

		//this is just a plain-vanilla copy.
		Moo::rc().setTexture( 0, FullScreenBackBuffer::renderTarget().pTexture() );

		Geometrics::texturedRect( Vector2(0.f,0.f),
			Vector2(Moo::rc().screenWidth(),Moo::rc().screenHeight()),
			Vector2(0,0),
			Vector2(FullScreenBackBuffer::uSize(),FullScreenBackBuffer::vSize()),
			Moo::Colour( 1.f, 1.f, 1.f, 1.f ), true );
	}

	//Now do post-transfer filters	
	it = s_pUsers_->begin();
	while (it != s_pUsers_->end())
	{
		User* user = *it++;
		if (user->isEnabled())
		{
			user->doPostTransferFilter();
		}
	}
}


/*static*/ void FullScreenBackBuffer::addUserInternal( User* u )
{
	if (s_pUsers_ == NULL)
	{
		s_pUsers_ = new Users();
	}
	MF_ASSERT_DEBUG(s_pUsers_ != NULL);
	s_pUsers_->push_back(u);	
}


/*static*/ void FullScreenBackBuffer::delUserInternal( User* u )
{
	MF_ASSERT_DEBUG(s_pUsers_ != NULL);
	Users::iterator it = s_pUsers_->begin();
	while (it != s_pUsers_->end())
	{		
		if (*it == u)
		{
			s_pUsers_->erase(it);
			//last user.. delete
			if (s_pUsers_->size() == 0)
			{
				delete s_pUsers_;
				s_pUsers_ = NULL;
			}
			return;
		}
		it++;
	}
	WARNING_MSG( "Tried to remove a FSBB user not in the list\n" );
}


bool FullScreenBackBuffer::hasEnabledUsers()
{
	if (s_pUsers_)
	{
		Users::iterator it = s_pUsers_->begin();
		while (it != s_pUsers_->end())
		{
			User* user = *it++;
			if (user->isEnabled())
				return true;
		}
	}
	return false;
}

int FullScreenBackBufferSetting_token = 0;
