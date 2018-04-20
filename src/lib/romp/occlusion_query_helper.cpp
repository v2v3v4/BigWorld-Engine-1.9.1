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
#include "occlusion_query_helper.hpp"
#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "romp", 0 )


/**
 *	Constructor.
 */
OcclusionQueryHelper::OcclusionQueryHelper(
	uint16 numHandles,
	uint32 defaultValue,
	uint8 numFrames ):
frameNum_( 0 ),
numFrames_( numFrames ),
numHandles_( numHandles ),
defaultValue_( defaultValue )
{
	//total number of results are number of handles * number of frames storage.
	//this is needed because the occlusion test is asynchronous, and the CPU
	//can get up to 3 frames ahead of the GPU (meaning numFrames should
	//always be at least 3)
	numResults_ = numFrames_ * numHandles;

	for ( int i=numHandles_-1; i>=0; i-- )
	{
		unusedHandles_.push( i );
	}

	results_ = new uint32[numResults_];
	resultPending_ = new bool[numResults_];
	queries_ = new DX::Query*[numResults_];

	for (uint32 i=0; i<numResults_; i++)
		results_[i] = defaultValue_;

	memset( resultPending_, 0, sizeof(bool) * numResults_ );
	memset( queries_, 0, sizeof(DX::Query*) * numResults_ );
};


/**
 *	Destructor.
 */
OcclusionQueryHelper::~OcclusionQueryHelper()
{
	for ( uint16 i=0; i<numResults_; i++ )
	{
		if ( queries_[i] )
			queries_[i]->Release();
	}

	delete[] results_;
	delete[] resultPending_;
	delete[] queries_;
}


/**
 *	This method begins a frame of occlusion queries.  It must be
 *	called before calling beginQuery() or handleFromId().
 *	If you call this, you must call end() too.
 *
 *	@see OcclusionQueryHelper::begin
 */
void OcclusionQueryHelper::begin()
{
	HandleMap::iterator it = handleMap_.begin();
	HandleMap::iterator end = handleMap_.end();

	while ( it != end )
	{
		HandleInfo& info = it->second;
		info.used_ = false;
		it++;
	}
}


/**
 *	This method ends a frame of occlusion queries.  It must be called to
 *	make a pair of begin() and end() calls.
 *
 *	@see OcclusionQueryHelper::begin
 */
void OcclusionQueryHelper::end()
{
	//find all unused ids - remove from map, and push onto the unused ids stack.
	std::vector<uint32>	eraseMe;

	HandleMap::iterator it = handleMap_.begin();
	HandleMap::iterator end = handleMap_.end();
	while ( it != end )
	{
		HandleInfo& info = it->second;
		if ( info.used_ == false )
		{
			unusedHandles_.push( info.handle_ );

			//flush the query before we re-add it to the available queries list
			//this ensures we don't re-use it while it is in the issued state
			getVizResults( info.handle_, true );

			for ( int f=0; f<MAX_FRAME_LAG; f++ )
			{
				int handle = info.handle_ + numHandles_*f;
				results_[handle] = defaultValue_;				
				resultPending_[handle] = false;
			}

			eraseMe.push_back( it->first );
			//DEBUG_MSG( "Handle %d no longer being used %d\n", info.handle_, it->first );
		}
		it++;
	}

	std::vector<uint32>::iterator iit = eraseMe.begin();
	std::vector<uint32>::iterator iend = eraseMe.end();
	while ( iit != iend )
	{
		handleMap_.erase( *iit++ );
	}

	eraseMe.clear();

	//update the frame parity		
	frameNum_ = (frameNum_ + 1) % numFrames_;	
}


/**
 *	This method returns a handle to be used for occlusion queries.
 *
 *	@param	id		some arbitrary 32 bit id (perhaps a pointer...?)
 *
 *	@return	Handle	a handle that can be used for occlusion querying.
 */
OcclusionQueryHelper::Handle OcclusionQueryHelper::handleFromId( uint32 id )
{
	HandleMap::iterator it = handleMap_.find( id );
	if (it == handleMap_.end())
	{
		//allocate a new id
		if( !unusedHandles_.empty() )
		{
			int newHandle = unusedHandles_.top();
			unusedHandles_.pop();
			HandleInfo info;
			info.handle_ = newHandle;
			info.used_ = true;
			handleMap_.insert( std::make_pair( id,info ) );
			//DEBUG_MSG( "New ID %d for lens flare %d\n", newID, id );
			return newHandle;
		}
		else
		{
			//No more handles available, this lens effect will not work.
			return INVALID_HANDLE;
		}
	}
	else
	{
		//flag as used, and return the handle
		HandleInfo& info = it->second;
		info.used_ = true;
		return info.handle_;
	}

	//uh-oh, somebody wants to go out of bounds.
	WARNING_MSG( "OcclusionQueryHelper::idxFromId called with id (%d) > "
				"numHandles (%d)\n", id, numHandles_ );
	return 0;
}


/**
 *	This method is private, and asks the device for results for a particular
 *	query handle.
 */
void OcclusionQueryHelper::getVizResults( Handle h, bool flush )
{
	for ( int f=0; f<numFrames_; f++ )
	{
		uint16 handle = h + numHandles_*f;

		if ( resultPending_[handle] )
		{
			HRESULT hr = queries_[handle]->GetData(
				&results_[handle], sizeof(DWORD), flush?D3DGETDATA_FLUSH:0 );

			resultPending_[handle] = ( hr != D3D_OK );
		}
	}
}


/**
 *	This method is called to begin an occlusion query.  Call it just before
 *	drawing your geometry that you are using to test visibility with.
 *
 *	@param	h		an OcclusionQueryHelper::Handle identifying the test
 *
 *	@return bool	success of the query.  fails if the device is lost.
 */
bool OcclusionQueryHelper::beginQuery( Handle h )
{
	if (h != INVALID_HANDLE)
	{
		uint16 handle = h + numHandles_*frameNum_;
		
		if (resultPending_[handle])
		{
			this->getVizResults(h);

			//we are still using this query, can't issue another just yet.
			if (resultPending_[handle])
				return false;
		}

		HRESULT hr;
		DX::Query* query = queries_[handle];
		if ( !query )
		{
			hr = Moo::rc().device()->CreateQuery(D3DQUERYTYPE_OCCLUSION, &queries_[handle]);
			if (FAILED(hr))
			{
				queries_[handle] = NULL;
				return false;
			}
			query = queries_[handle];
		}

		hr = query->Issue(D3DISSUE_BEGIN);
		return (SUCCEEDED(hr));
	}
	else
	{
		return false;
	}
}


/**
 *	This method is called to notify this class that the visibility testing
 *	geometry has been drawn.  Must be matched with beginQuery()
 *
 *	@param	h		an OcclusionQueryHelper::Handle identifying the test
 *	@see	OcclusionQueryHelper::beginQuery
 */
void OcclusionQueryHelper::endQuery( Handle h )
{
	MF_ASSERT( h!= INVALID_HANDLE );
	uint16 handle = h + numHandles_*frameNum_;

	DX::Query* query = queries_[handle];
	MF_ASSERT( query );
	HRESULT hr = query->Issue(D3DISSUE_END);
	resultPending_[handle] = true;

	//won't have finished this particular one by now,
	//but we check our other frame_lag queries
	this->getVizResults(h);
}


/**
 *	This method returns the average number of pixels drawn by visibility
 *	testing geometry represented by the handle.
 *
 *	@param	h		an OcclusionQueryHelper::Handle identifying the test
 *	@return int		average number of pixels.
 */
int OcclusionQueryHelper::avgResult( Handle h )
{
	if ( h == INVALID_HANDLE )
		return 0;
	
	int acc = 0;
	int tot = 0;
	for ( int f=0; f<numFrames_; f++ )
	{
		uint16 handle = h + numHandles_*f;

		if (!resultPending_[handle])
		{
			acc += results_[handle];
			tot++;
		}
	}

	if ( tot > 0 )
		return acc/tot;

	//all of our tests are outstanding.  This is very unusual but
	//it can happen (for this to happen the CPU has to get ahead
	//of the GPU by 4 frames, should never happen in practice but
	//may happen in empty spaces on some video cards.  In this case
	//we return the average of the last 4 known results
	for ( int f=0; f<numFrames_; f++ )
	{
		int handle = h + numHandles_*f;
		acc += results_[handle];		
	}	

	return acc/numFrames_;
}


void OcclusionQueryHelper::deleteUnmanagedObjects()
{
	for ( uint16 i=0; i<numResults_; i++ )
	{
		if ( queries_[i] )
		{
			if (resultPending_[i])
			{
				// This was causing DX to crash internally, although all input looks alright. 
				// Commenting it out avoids the crash and seems to have no  side effects 
				// (the query is successfully released, as the assertion demonstrate).
				// HRESULT hr = queries_[i]->GetData( &results_[i], sizeof(int), D3DGETDATA_FLUSH );
			}
			uint refcount = queries_[i]->Release();
			MF_ASSERT(refcount == 0);
		}
	}

	memset( results_, defaultValue_, sizeof(results_[0]) * numResults_ );
	memset( resultPending_, 0, sizeof(bool) * numResults_ );
	memset( queries_, 0, sizeof(DX::Query*) * numResults_ );

	handleMap_.clear();
	while ( !unusedHandles_.empty() )
	{
		unusedHandles_.pop();
	}

	for ( int i=numHandles_-1; i>=0; i-- )
	{
		unusedHandles_.push( i );
	}

	frameNum_ = 0;
}
