/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef FULL_SCREEN_BACK_BUFFER_HPP
#define FULL_SCREEN_BACK_BUFFER_HPP

#include "moo/render_target.hpp"

namespace Moo {
	class GraphicsSetting;
	class EffectMacroSetting;
	class EffectManager::IListener;
};

// defines extra bit-depth render target usage (experimental)
//#define EXTRA_MRT_INFO


/**
 *	FullScreenBackBuffer setting management.
 */
class FSBBSettings : private Moo::EffectManager::IListener
{
public:
	static bool isEnabled();
	virtual void onSelectPSVersionCap(int psVerCap);	
	static void fini();
private:
	static void configureKeywordSetting(Moo::EffectMacroSetting & setting);
	static Moo::EffectMacroSetting::EffectMacroSettingPtr s_mrtSetting_;
};

/**
 *	This class acts as a single sink for full-screen back buffer copies.  It 
 *  does this by hijacking the render target before the main draw loop, and 
 *  transferring the image to the back buffer later on.
 */
class FullScreenBackBuffer :	public Moo::DeviceCallback
{
public:
	/**
	 * TODO: to be documented.
	 */
	class User
	{
	public:
		virtual bool isEnabled() = 0;
		virtual void beginScene() = 0;
		virtual void endScene() = 0;
		virtual bool doTransfer( bool alreadyTransferred ) = 0;
		virtual void doPostTransferFilter() = 0;
	};

	static void initInstance() { if (s_instance_==NULL) s_instance_ = new FullScreenBackBuffer(); }

	static Moo::RenderTarget& renderTarget(){ return *instance().pRT_; }
	static Moo::RenderTarget& renderTarget2(){ return *instance().pRT2_; }
	static bool init();
	static bool beginScene()				{ return instance().beginSceneInternal(); }
	static void endScene()					{ instance().endSceneInternal(); }
	static void addUser( User* u )			{ addUserInternal( u ); }
	static void removeUser( User* u )		{ delUserInternal( u ); }	
	static bool reuseZBuffer()				{ return instance().reuseZBuffer_; }
	static float uSize( ) 
	{ 
		return float(instance().vp_.Width) / 
			float(instance().pRT_->width() ); 
	}
	static float vSize( )					
	{ 
		return float(instance().vp_.Height) / 
			float(instance().pRT_->height() ); 
	}
	void deleteUnmanagedObjects();
	void createUnmanagedObjects();

	~FullScreenBackBuffer();

	static bool initialised() { return instance().inited_; }


	static void fini()
	{
		FSBBSettings::fini();
		if (s_instance_)
		{
			delete s_instance_;
			s_instance_ = NULL;
		}
	}

	static FullScreenBackBuffer& instance()
	{ 
		return *s_instance_;
	}

	static bool mrtEnabled();

	DX::Viewport* getViewport() { return &vp_; }

private:
	bool hasEnabledUsers();
	bool beginSceneInternal();
	void endSceneInternal();
	static void addUserInternal( User* u );
	static void delUserInternal( User* u );
	bool initInternal();
	FullScreenBackBuffer();
	static FullScreenBackBuffer* s_instance_;
	DX::Viewport vp_;
	Moo::RenderTargetPtr pRT_;
	Moo::RenderTargetPtr pRT2_;
	bool	inited_;
	bool	failed_;
	bool	reuseZBuffer_;
	typedef std::vector<User*> Users;
	static Users* s_pUsers_;
};


#endif
