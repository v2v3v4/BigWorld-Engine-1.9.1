/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	ThumbnailManager: Thumbnail generator class
 */

#ifndef THUMBNAIL_MANAGER_HPP
#define THUMBNAIL_MANAGER_HPP

#include <vector>
#include <list>
#include <set>
#include "cstdmf/smartpointer.hpp"
#include "moo/render_target.hpp"
#include "moo/render_context.hpp"
#include "cstdmf/aligned.hpp"
#include "cstdmf/concurrency.hpp"

#include "atlimage.h"

class BoundingBox;
class ThumbnailManager;

/**
 *  Thumbnail Provider base class
 *	Derived classes must have a default constructor, or declare+implement
 *	the factory static themselves instead of using the macros.
 */
class ThumbnailProvider : public ReferenceCount
{
public:

	/**
	 *  This method allows common zoom to extent functionality to all providers
	 *  The bounding box is the one used for calculating the zoom amount.
	 *  The scale represents the extra scale of zoom required for this new
	 *  positioning. 
	 *  @param bb is the bounding box to zoom on
	 *  @param scale is the extra zoom scale (<1 closer ; >1 further)
	 */
	virtual void zoomToExtents( const BoundingBox& bb, const float scale = 1.f );

	/**
	 *  This method is called by the thumbnail manager class to find out if
	 *  the provider supports this file type. If the provider returns true,
	 *  no other providers will be iterated on, so this provider should handle
	 *  the thumbnail.
	 *  NOTE: THIS METHOD IS PERFORMANCE-CRITICAL
	 *  @param manager ThumbnailManager that is requesting the thumbnail.
	 *  @param file full path of the file
	 *  @return true if the file can be handled by the provider, false if not
	 */
	virtual bool isValid( const ThumbnailManager& manager, const std::string& file ) = 0;
	
	/**
	 *  This method is called by the thumbnail manager class to find out if
	 *  the file needs a new new thumbnail to be created. If it returns true,
	 *  the prepare and create methods will get called in that order. If it
	 *  returns false, the manager will try to load directly a thumbnail from
	 *  the file matching the "thumb" parameter, so if the provider wishes to 
	 *  override the default thumbnail path and name, it can change it inside
	 *  this method by assigning the desired path to this parameter. That being
	 *  said, it is not recommended to change the default thumbnail name and/or
	 *  path. The default implementation returns true if the thumb file is older
	 *  than the main file, false otherwise.
	 *  NOTE: THIS METHOD IS PERFORMANCE-CRITICAL. RETURN FALSE WHENEVER POSIBLE
	 *  @param manager ThumbnailManager that is requesting the thumbnail.
	 *  @param file full path of the file
	 *  @param thumb recommended/desired path for the thumbnail (in and out)
	 *  @param size recommended/desired size (in and out)
	 *  @return true to create a new thumbnail, false to load it from "thumb"
	 */
	virtual bool needsCreate( const ThumbnailManager& manager, const std::string& file, std::string& thumb, int& size );

	/**
	 *  This method is called by the thumbnail manager class to prepare an
	 *  asset before rendering. It's called from a separate thread, so be
	 *  careful with what calls you make. After this method returns, the main
	 *  thread will be notified and the create method of the provider will be
	 *  called.
	 *  NOTE: this method shouldn't get called frequently, only for new items
	 *  or items that require a new thumbnail.
	 *  @param manager ThumbnailManager that is requesting the thumbnail.
	 *  @param file full path of the file
	 *  @return true if successful
	 */
	virtual bool prepare( const ThumbnailManager& manager, const std::string& file ) = 0;

	/**
	 *  This method is called by the thumbnail manager class to render the last
	 *  loaded thumbnail in the provider. A render target is passed as a param
	 *  for the provider to render it's results. If this method returns true,
	 *  the Thumbnail manager class will save the render context to disk to a
	 *  file named as the string "thumb" passed to the "needsCreate" method.
	 *  NOTE: this method shouldn't get called frequently, only for new items
	 *  or items that require a new thumbnail.
	 *  @param manager ThumbnailManager that is requesting the thumbnail.
	 *  @param file full path of the file (the provider shoudn't need it)
	 *  @param rt render target where the primitives will be rendered and later
	 *  saved to disk
	 *  @return true if successful
	 */
	virtual bool render( const ThumbnailManager& manager, const std::string& file, Moo::RenderTarget* rt ) = 0;
};
typedef SmartPointer<ThumbnailProvider> ThumbnailProviderPtr;


// interface class for classes that need to receive thumbnail updates
class ThumbnailUpdater
{
public:
	virtual void thumbManagerUpdate( const std::string& longText ) = 0;
};


typedef SmartPointer<ThumbnailManager> ThumbnailManagerPtr;


// Thumbnail manager class

class ThumbnailManager : public Aligned, public ReferenceCount
{
public:
	ThumbnailManager();	
	virtual ~ThumbnailManager();

	static void registerProvider( ThumbnailProviderPtr provider );

	void resetPendingRequests( ThumbnailUpdater* updater );
	void stop();

	std::string postfix() const { return postfix_; };
	std::string folder() const { return folder_; };
	int size() const { return size_; };
	COLORREF backColour() const { return backColour_; };

	void postfix( const std::string& postfix ) { postfix_ = postfix; };
	void folder( const std::string& folder ) { folder_ = folder; };
	void size( int size ) { size_ = size; };
	void backColour( COLORREF backColour ) { backColour_ = backColour; };

	void create( const std::string& file, CImage& img, int w, int h,
		ThumbnailUpdater* updater, bool loadDirectly = false );

	void tick();

	void recalcSizeKeepAspect( int w, int h, int& origW, int& origH ) const;

	// legacy methods
	void stretchImage( CImage& img, int w, int h, bool highQuality ) const;

private:
	ThumbnailManager( const ThumbnailManager& );
	ThumbnailManager& operator=( const ThumbnailManager& );

	// helper class that contains data useful for a thread
	class ThreadData : public SafeReferenceCount
	{
	public:
		ThreadData( const std::string& f, const std::string& t,
			const std::string& p, int w, int h,
			ThumbnailUpdater* updater ) :
			file_( f ),
			thumb_( t ),
			path_( p ),
			memFile_( NULL ),
			provider_( NULL ),
			w_( w ),
			h_( h ),
			updater_( updater )
		{}
		std::string file_;
		std::string thumb_;
		std::string path_;
		LPD3DXBUFFER memFile_;
		ThumbnailProviderPtr provider_;
		int w_;		// actual width of the final image
		int h_;		// actual height of the final image
		ThumbnailUpdater* updater_; // called when the thumb is ready
	};
	typedef SmartPointer<ThreadData> ThreadDataPtr;	

	// helper class that contains results from a thread
	class ThreadResult : public SafeReferenceCount
	{
	public:
		ThreadResult( const std::string& file, CImage* image, ThumbnailUpdater* updater ) :
			file_( file ),
			image_( image ),
			updater_( updater )
		{}
		~ThreadResult() { delete image_; }
		std::string file_;
		CImage* image_;
		ThumbnailUpdater* updater_; // here only used to identify the request
	};
	typedef SmartPointer<ThreadResult> ThreadResultPtr;
	
	static std::vector<ThumbnailProviderPtr> * s_providers_;

	std::string postfix_;
	std::string folder_;
	int size_;
	COLORREF backColour_;

	SimpleThread* thread_;
	SimpleMutex mutex_;
	ThreadDataPtr renderData_;		// used to render in the main thread
	Moo::RenderTarget renderRT_;	// used to render in the main thread
	bool renderRequested_;			// used to render in the main thread
	int renderSize_;				// render size that the provider requests
	std::list<ThreadDataPtr> pending_;
	SimpleMutex pendingMutex_;
	std::list<ThreadResultPtr> results_;
	SimpleMutex resultsMutex_;
	std::list<ThreadResultPtr> ready_;
	std::set<std::string> errorFiles_;
	bool stopThreadRequested_;

	// methods used to control thread consumption / production
	bool pendingAvailable();
	bool resultsAvailable();
	// render methods, used to control rendering in the main thread
	void requestRender( int size );
	bool renderRequested();
	bool renderDone();
	void render();
	// thread methods
	static void s_startThread( void *extraData );
	void startThread();
	void stopThread();
	void stopThreadRequest( bool set );
	bool stopThreadRequested();

	// default lights used for render thumbs
	Moo::LightContainerPtr pNewLights_;
};


/**
 *  Thumbnail provider factory
 */
class ThumbProvFactory
{
public:
	ThumbProvFactory( ThumbnailProviderPtr provider )
	{
		ThumbnailManager::registerProvider( provider );
	};
};

/**
 *	This macro is used to declare a class as a thumbnail provider. It is used
 *	to declare the factory functionality. It should appear in the declaration
 *	of the class.
 *
 *	Classes using this macro should also use the IMPLEMENT_THUMBNAIL_PROVIDER
 *  macro.
 */
#define DECLARE_THUMBNAIL_PROVIDER()										\
	static ThumbProvFactory s_factory_;

/**
 *	This macro is used to implement the thumbnail provider factory
 *  functionality.
 */
#define IMPLEMENT_THUMBNAIL_PROVIDER( CLASS )								\
	ThumbProvFactory CLASS::s_factory_( new CLASS() );



#endif // THUMBNAIL_MANAGER_HPP
