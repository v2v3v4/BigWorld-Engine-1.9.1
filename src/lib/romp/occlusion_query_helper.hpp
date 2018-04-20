/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef OCCLUSION_QUERY_HELPER_HPP
#define OCCLUSION_QUERY_HELPER_HPP

#include "moo/device_callback.hpp"
#include <stack>

//must be greater than 0 and less than 256 (uint8)
#define MAX_FRAME_LAG 4


/** 
 *	This helper class handles asynchronous occlusion queries.  To use it,
 *	obtain query handles by passing in an ID, and draw some geometry in
 *	between beginQuery and endQuery calls.  Internally, the helper will
 *	handle the asynchronous occlusion query calls by storing multiple
 *	frames of results - call avgResult to obtain the answer. 
 *	Note that if you are testing 'coverage' instead of 'visibility'
 *	then you may not want to have 0 as your default value, instead you
 *	may like to use the full coverage value.
 */
class OcclusionQueryHelper : public Moo::DeviceCallback
{
public:
	typedef uint16 Handle;
	static const Handle INVALID_HANDLE = 0xffff;

	OcclusionQueryHelper(
		uint16 maxHandles,
		uint32 defaultValue = 0,
		uint8 numFrames = MAX_FRAME_LAG );
	~OcclusionQueryHelper();	

	//public interface - call these in order.
	void		begin();
	Handle		handleFromId( uint32 id );
	bool		beginQuery( Handle idx );
	void		endQuery( Handle idx );
	void		end();
	
	//returns the average number of pixels drawn over the last n frames for
	//this query handle.
	int			avgResult( Handle idx );

	//from the Moo::DeviceCallback interface
	void		deleteUnmanagedObjects();
	
private:
	void		getVizResults( Handle idx, bool flush = false );

	bool		*resultPending_;
	uint32		*results_;
	DX::Query** queries_;
	uint8		frameNum_;
	uint8		numFrames_;
	uint16		numHandles_;
	uint16		numResults_;
	uint32		defaultValue_;

	struct HandleInfo
	{
		Handle handle_;
		bool used_;
	};
	
	typedef std::map<uint32,HandleInfo>	HandleMap;
	HandleMap							handleMap_;
	std::stack<Handle>					unusedHandles_;
};

#endif	//OCCLUSION_QUERY_HELPER_HPP