/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef DYNAMIC_BUFFER_COMMON_HPP
#define DYNAMIC_BUFFER_COMMON_HPP

#include "cstdmf/stdmf.hpp"

namespace Moo
{
	
/**
 * Represents a single requested allocation inside a dynamic buffer.
 */
class DynamicBufferSlot : public ReferenceCount
{
public:
	DynamicBufferSlot(uint32 offset, uint32 size) :
		offset_( offset ), size_( size ), isValid_( true ) 
	{}

	bool valid() const { return isValid_; }
	void valid(bool val) { isValid_=val; }
	uint32 offset() const { return offset_; }
	uint32 size() const { return size_; }

private:
	uint32 offset_, size_;
	bool isValid_;
};
typedef SmartPointer<DynamicBufferSlot> DynamicBufferSlotPtr;

/**
 * Base class for all dynamic resource buffers.
 */
class DynamicBuffer
{
public:
	/**
	 *	This method returns the usage slot of the last lock
	 *  @return Buffer Usage Slot.
	 */
	DynamicBufferSlotPtr lastSlot() const
	{
		return currentUsage_.back();
	}

protected:
	typedef std::vector<DynamicBufferSlotPtr> UsageList;
	
	void resetUsage()
	{
		BW_GUARD;
		UsageList::iterator it=currentUsage_.begin();
		UsageList::iterator itEnd=currentUsage_.end();
		for (;it!=itEnd;++it)
		{
			(*it)->valid(false);
		}
		currentUsage_.clear();
	}
	UsageList currentUsage_; //vector no dest?

	void addSlot(uint32 offset, uint32 size)
	{
		BW_GUARD;
		currentUsage_.push_back( new DynamicBufferSlot(offset, size) );
	}
};

} //namespace Moo

#endif //DYNAMIC_BUFFER_COMMON_HPP
/*dynamic_buffer_common.hpp*/
