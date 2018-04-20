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
 *	ListFileProvider: Inherits from SmartListProvider to implement a file virtual list provider
 */

#include "pch.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <time.h>
#include "list_file_provider.hpp"
#include "thumbnail_manager.hpp"

#include "common/string_utils.hpp"
#include "resmgr/bwresource.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT( 0 );


// ListFileProvider
ListFileProvider::ListFileProvider( const std::string& thumbnailPostfix ) :
	hasImages_( false ),
	thread_( 0 ),
	threadWorking_( false ),
	threadFlushMsec_( 200 ),
	threadYieldMsec_( 0 ),
	threadPriority_( 0 ),
	thumbnailPostfix_( thumbnailPostfix ),
	flags_( LISTFILEPROV_DEFAULT )
{
	init( "", "", "", "", "", flags_ );
}

ListFileProvider::~ListFileProvider()
{
	stopThread();

	clearItems();
	clearThreadItems();
}

void ListFileProvider::init(
	const std::string& type,
	const std::string& paths,
	const std::string& extensions,
	const std::string& includeFolders,
	const std::string& excludeFolders,
	int flags )
{
	stopThread();

	// member variables
	type_ = type;

	flags_ = flags;

	paths_.clear();
	extensions_.clear();
	includeFolders_.clear();
	excludeFolders_.clear();

	std::string pathsL = paths;
	std::replace( pathsL.begin(), pathsL.end(), '/', '\\' );
	StringUtils::vectorFromString( pathsL, paths_ );

	std::string extL = extensions;
	StringUtils::toLowerCase( extL );
	StringUtils::vectorFromString( extL, extensions_ );
	hasImages_ = false;
	for( std::vector<std::string>::iterator i = extensions_.begin();
		i != extensions_.end(); ++i )
	{
		if ( (*i) == "dds" )
		{
			hasImages_ = true;
			break;
		}
	}

	std::string includeFoldersL = includeFolders;
	std::replace( includeFoldersL.begin(), includeFoldersL.end(), '/', '\\' );
	StringUtils::vectorFromString( includeFoldersL, includeFolders_ );

	std::string excludeFoldersL = excludeFolders;
	std::replace( excludeFoldersL.begin(), excludeFoldersL.end(), '/', '\\' );
	StringUtils::vectorFromString( excludeFoldersL, excludeFolders_ );

	StringUtils::filterSpecVector( paths_, excludeFolders_ );

	// clear items and start file-seeking thread
	clearItems();

	if ( !paths_.empty() ) 
		startThread();
}

void ListFileProvider::refresh()
{
	stopThread();

	clearItems();

	if ( !paths_.empty() ) 
		startThread();
}

void ListFileProvider::clearItems()
{
	mutex_.grab();
	items_.clear();
	searchResults_.clear();
	mutex_.give();
}

void ListFileProvider::clearThreadItems()
{
	threadMutex_.grab();
	threadItems_.clear();
	tempItems_.clear();
	threadMutex_.give();
}

bool ListFileProvider::finished()
{
	return !getThreadWorking();
}

int ListFileProvider::getNumItems()
{
	mutex_.grab();
	int ret;
	if ( isFiltering() )
		ret = (int)searchResults_.size();
	else
		ret = (int)items_.size();
	mutex_.give();

	return ret;
}

const AssetInfo ListFileProvider::getAssetInfo( int index )
{
	if ( index < 0 || getNumItems() <= index )
		return AssetInfo();

	SimpleMutexHolder smh( mutex_ );
	ListItemPtr item;
	if ( isFiltering() )
		item = searchResults_[ index ];
	else
		item = items_[ index ];

	return AssetInfo(
		type_,
		item->title,
		item->fileName );
}

HICON ListFileProvider::getExtensionIcons( const std::string& name )
{
	std::string ext = BWResource::getExtension( name );

	if ( !ext.length() )
		return 0;

	StringUtils::toLowerCase( ext );
	HICON icon = 0;

	for( std::vector<ExtensionsIcons>::iterator i = extensionsIcons_.begin(); i != extensionsIcons_.end(); ++i )
	{
		for( std::vector<std::string>::iterator j = (*i).extensions.begin(); j != (*i).extensions.end(); ++j )
		{
			if ( (*j) == ext )
			{
				icon = (*i).icon;
				break;
			}
		}
	}

	return icon;
}

void ListFileProvider::getThumbnail( ThumbnailManager& manager,
									int index, CImage& img, int w, int h,
									ThumbnailUpdater* updater )
{
	if ( index < 0 || getNumItems() <= index )
		return;

	mutex_.grab();
	std::string item;
	if ( isFiltering() )
		item = searchResults_[ index ]->fileName;
	else
		item = items_[ index ]->fileName;
	mutex_.give();

	// find thumbnail

	HICON extIcon = getExtensionIcons( item );
	if ( extIcon )
	{
		CBrush back;
		back.CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
		img.Create( w, h, 32 );
		CDC* pDC = CDC::FromHandle( img.GetDC() );
		DrawIconEx( pDC->m_hDC, 0, 0, extIcon, w, h, 0, (HBRUSH)back, DI_NORMAL );
		img.ReleaseDC();
	}

	manager.create( item, img, w, h, updater );
}

void ListFileProvider::filterItems()
{
	mutex_.grab();
	if ( !isFiltering() )
	{
		mutex_.give();
		return;
	}

	// start
	searchResults_.clear();

	// start filtering
	for( ItemsItr i = items_.begin(); i != items_.end(); ++i )
	{
		if ( filterHolder_->filter( (*i)->title, (*i)->fileName ) )
		{
			if ( (searchResults_.size() % VECTOR_BLOCK_SIZE) == 0 )
				searchResults_.reserve( searchResults_.size() + VECTOR_BLOCK_SIZE );
			searchResults_.push_back( (*i) );
		}
	}
	mutex_.give();
}

bool ListFileProvider::isFiltering()
{
	return filterHolder_ && filterHolder_->isFiltering();
}

void ListFileProvider::setExtensionsIcon( std::string extensions, HICON icon )
{
	if ( !icon )
		return;

	ExtensionsIcons extIcons;

	StringUtils::vectorFromString( extensions, extIcons.extensions );
	
	extIcons.icon = icon;

	extensionsIcons_.push_back( extIcons );
}

void ListFileProvider::setThreadYieldMsec( int msec )
{
	threadYieldMsec_ = max( msec, 0 );
}

int ListFileProvider::getThreadYieldMsec()
{
	return threadYieldMsec_;
}

void ListFileProvider::setThreadPriority( int priority )
{
	threadPriority_ = priority;
}

int ListFileProvider::getThreadPriority()
{
	return threadPriority_;
}

//private methods
// private thread methods
void ListFileProvider::setThreadWorking( bool working )
{
	mutex_.grab();
	threadWorking_ = working;
	mutex_.give();
}

bool ListFileProvider::getThreadWorking()
{
	mutex_.grab();
	bool ret = threadWorking_; 
	mutex_.give();
	return ret;
}

void ListFileProvider::s_startThread( void* provider )
{
	ListFileProvider* p = (ListFileProvider*)provider;

	p->clearThreadItems();

	p->flushClock_ = clock();

	int lastFlushMsec = p->threadFlushMsec_; // save original flush Msecs
	if ( p->threadYieldMsec_ > 0 )
		p->yieldClock_ = clock();

	for ( std::vector<std::string>::iterator i = p->paths_.begin();
		p->getThreadWorking() && i != p->paths_.end();
		++i )
		p->fillFiles( (*i).c_str() );

	p->flushThreadBuf();

	p->threadFlushMsec_ = lastFlushMsec; // restore original flush Msecs

	p->setThreadWorking( false );
}

void ListFileProvider::startThread()
{
	stopThread();

	setThreadWorking( true );

	thread_ = new SimpleThread( s_startThread, this );
	if ( threadPriority_ > 0 )
	{
		// the user wants a lot of priority for the thread
		SetThreadPriority( thread_->handle(), THREAD_PRIORITY_ABOVE_NORMAL );
	}
	else if ( threadPriority_ < 0 )
	{
		// the user wants the thread to be highly cooperative
		SetThreadPriority( thread_->handle(), THREAD_PRIORITY_BELOW_NORMAL );
	}
}

void ListFileProvider::stopThread()
{
	if ( !thread_ )
		return;

	setThreadWorking( false );
	delete thread_;
	thread_ = 0;

	clearThreadItems();
}


void ListFileProvider::fillFiles( const CString& path )
{
	CFileFind finder;
	
	CString p = path + "\\*.*";
	if ( !finder.FindFile( p ) )
		return;

	// hack to avoid reading 
	std::string legacyThumbnailPostfix = ".thumbnail.bmp";
	int legacyThumbSize = (int)legacyThumbnailPostfix.length();
	int thumbSize = (int)thumbnailPostfix_.length();

	bool ignoreFiles = false;
	if ( !includeFolders_.empty()
		&& !StringUtils::matchSpec( (LPCTSTR)path, includeFolders_ ) )
		ignoreFiles = true;

	BOOL notEOF = TRUE;
	while( notEOF && getThreadWorking() )
	{
		notEOF = finder.FindNextFile();
		if ( !finder.IsDirectory() )
		{
			if ( !ignoreFiles )
			{
				std::string fname( (LPCTSTR)finder.GetFileName() );
				if ( StringUtils::matchExtension( fname, extensions_ )
					&& ( (int)fname.length() <= thumbSize
						|| fname.substr( fname.length() - thumbSize ) != thumbnailPostfix_ )
					&& ( (int)fname.length() <= legacyThumbSize
						|| fname.substr( fname.length() - legacyThumbSize ) != legacyThumbnailPostfix )
					)
				{
					ListItemPtr item = new ListItem();
					item->fileName = (LPCTSTR)finder.GetFilePath();
					item->dissolved = BWResource::dissolveFilename( item->fileName );
					item->title = (LPCTSTR)finder.GetFileName();
					threadMutex_.grab();
					if ( (threadItems_.size() % VECTOR_BLOCK_SIZE) == 0 )
						threadItems_.reserve( threadItems_.size() + VECTOR_BLOCK_SIZE );
					threadItems_.push_back( item );
					threadMutex_.give();
				}
			}
		}
		else if ( !finder.IsDots()
			&& !( flags_ & LISTFILEPROV_DONTRECURSE )
			&& ( excludeFolders_.empty()
				|| !StringUtils::matchSpec( (LPCTSTR)finder.GetFilePath(), excludeFolders_ ) )
			)
		{
			fillFiles( finder.GetFilePath() );
		}

		if ( threadYieldMsec_ > 0 )
		{
			if ( ( clock() - yieldClock_ ) * 1000 / CLOCKS_PER_SEC > threadYieldMsec_ )
			{
				Sleep( 50 ); // yield
				yieldClock_ = clock();
			}
		}

		if ( ( clock() - flushClock_ ) * 1000 / CLOCKS_PER_SEC >= threadFlushMsec_  )
		{
			flushThreadBuf();
			flushClock_ = clock();
		}
	}
}

bool ListFileProvider::s_comparator( const ListFileProvider::ListItemPtr& a, const ListFileProvider::ListItemPtr& b )
{
	return _stricmp( a->title.c_str(), b->title.c_str() ) < 0;
}

void ListFileProvider::flushThreadBuf()
{
	threadMutex_.grab();
	if ( !threadItems_.empty() )
	{
		tempItems_.reserve( tempItems_.size() + threadItems_.size() );
		for( ItemsItr i = threadItems_.begin(); i != threadItems_.end(); ++i )
			tempItems_.push_back( *i );

		threadItems_.clear();

		std::sort< ItemsItr >( tempItems_.begin(), tempItems_.end(), s_comparator );

		if ( paths_.size() > 1 )
			removeDuplicateFileNames();

		if ( !(flags_ & LISTFILEPROV_DONTFILTERDDS) && hasImages_ )
			removeRedundantDdsFiles();

		threadFlushMsec_ = 1000;	// after the first flush, update every second

		// copy the actual items in tempItems_ to the items_ vector
		mutex_.grab();
		items_.clear();
		items_.reserve( tempItems_.size() );
		items_ = tempItems_;
		mutex_.give();

		// finished doing stuff with thread-only data, so release mutex
		threadMutex_.give();

		// and filter if filtering is on
		filterItems();
	}
	else
	{
		threadMutex_.give();
	}
}

void ListFileProvider::removeDuplicateFileNames()
{
	for( ItemsItr i = tempItems_.begin(); i != tempItems_.end(); ++i )
	{
		// remove items marked as duplicates in the inner loop
		while ( i != tempItems_.end() && !(*i) )
			i = tempItems_.erase( i );
		if ( i == tempItems_.end() )
			break;

		ItemsItr next = i;
		++next;
		if ( next != tempItems_.end() )
		{
			// check all items named the same as i ( and not marked for erasure! )
			for( ItemsItr j = next; j != tempItems_.end(); ++j )
			{
				if ( (*j) )
				{
					// not marked for erasure, so process

					// files are sorted, so check if the next is duplicate of the current
					if ( (*i)->title != (*j)->title )
						break; // no more duplicate names, get out!

					// handle files that are duplicated
					if ( (*i)->dissolved == (*j)->dissolved )
					{
						// make sure that the file surviving has the correct path
						(*i)->fileName = BWResource::resolveFilename( (*i)->dissolved );
						// keep using windows-style slashes
						std::replace( (*i)->fileName.begin(), (*i)->fileName.end(), '/', '\\' );
						// mark as duplicate, so it gets erased later
						*j = 0;
					}
				}
			}
		}
	}
}

void ListFileProvider::removeRedundantDdsFiles()
{
	for( ItemsItr i = tempItems_.begin(); i != tempItems_.end(); )
	{
		// remove DDS files if a corresponding source image file exists
		if ( BWResource::getExtension( (*i)->title ) == "dds" &&
			( PathFileExists( BWResource::changeExtension( (*i)->fileName, ".bmp" ).c_str() ) ||
			  PathFileExists( BWResource::changeExtension( (*i)->fileName, ".png" ).c_str() ) ||
			  PathFileExists( BWResource::changeExtension( (*i)->fileName, ".tga" ).c_str() ) ) )
		{
			// the DDS already has a source image, so don't show the DDS file
			i = tempItems_.erase( i );
		}
		else
		{
			++i;
		}
	}
}
