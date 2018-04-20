/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __CSTDMF_MEMORY_TRACKER_HPP__
#define __CSTDMF_MEMORY_TRACKER_HPP__

#include "stdmf.hpp"
#include "concurrency.hpp"
#include "list_node.hpp"

// -----------------------------------------------------------------------------
// Section: Macros
// -----------------------------------------------------------------------------

#ifndef ENABLE_MEMTRACKER
#define MEMTRACKER_DECLARE( id, name, flags )
#define MEMTRACKER_BEGIN( id )
#define MEMTRACKER_END()
#define MEMTRACKER_SCOPED( id )
#endif

//-----------------------------------------------------------------------------

#ifdef ENABLE_MEMTRACKER

#define ENABLE_STACKWALKER 0

#define MEMTRACKER_DECLARE( id, name, flags )	\
	int g_memTrackerSlot_##id = g_memTracker.declareSlot( name, flags )

#define MEMTRACKER_BEGIN( id )				\
	extern int g_memTrackerSlot_##id;		\
	g_memTracker.begin( g_memTrackerSlot_##id )

#define MEMTRACKER_END()					\
	g_memTracker.end()

#define MEMTRACKER_SCOPED( id )				\
	extern int g_memTrackerSlot_##id;		\
	ScopedMemTracker scopedMemTracker_##id( g_memTrackerSlot_##id )

#define MEMTRACKER_BREAK_ON_ALLOC( slotId, allocId )					\
	int break_##slotId_##allocId =										\
		g_memTracker.declareBreak( g_memTrackerSlot_##slotId, allocId )

//-----------------------------------------------------------------------------

#if ENABLE_STACKWALKER

#ifdef WIN32

// The third party StackWalker object
#include "third_party/stack_walker/StackWalker.h"

class MemTrackerStackWalker : public StackWalker
{
public:
  MemTrackerStackWalker() : StackWalker() {}
  MemTrackerStackWalker(DWORD dwProcessId, HANDLE hProcess) : StackWalker(dwProcessId, hProcess) {}
  virtual void OnOutput(LPCSTR szText);
};

#else

// Linux has native functionality
#include "execinfo.h"

#endif

#endif

//-----------------------------------------------------------------------------

#undef malloc
#undef realloc
#undef free
#undef strdup
#undef _strdup

class MemTracker
{
public:
	// Slot flags, controlling the behaviour of a slot
	enum FLAG
	{
		FLAG_CALLSTACK		= ( 1 << 0 ),
		FLAG_DONT_REPORT	= ( 1 << 1 ),
		FLAG_DONT_TRASH		= ( 1 << 2 ),
	};

	// Allocation statistics. Represents a particular slot or global memory
	struct AllocStats
	{
		uint				curBytes_;		// Bytes currently allocated
		uint				curBlocks_;		// Number of blocks currently allocated
		uint				peakBytes_;		// The most bytes ever allocated
		uint				peakBlocks_;	// The most blocks ever allocated
		uint				curOverhead_;	// The current MemTracker overhead in bytes
		uint				peakOverhead_;	// The peak MemTracker overhead in bytes
	};

private:
	enum
	{
		MAX_SLOTS			= 256,			// The maximum number of slots
		MAX_THREADS			= 16,			// The maximum number of threads
		SLOT_STACK_DEPTH	= 64,			// The slot stack size
		MAX_BREAKS			= 16			// The maximum number of user breaks
	};

	// The header represents a single block of tracked memory
	struct Header
	{
		ListNode			node;			// Node for list of allocated blocks
		uint				slot;			// The user assigned slot for this allocation
		uint				id;				// The allocation id, unique for this slot
		uint				size;			// Size of the block, not counting overhead
		uint				callStackSize;	// Size of the callstack data
	};

	// The slot represents a collection of blocks. Each tracked block belongs
	// to exactly one slot.
	struct Slot
	{
		const char*			name_;
		uint32				flags_;
		uint				allocCounter_;
		AllocStats			stats_;
	};

	// Stores the slot stack for each thread
	struct ThreadState
	{
		unsigned long		threadId_;			// The thread for which we're storing state
		int					curSlot_;			// Current slot
		int					slotStack_[SLOT_STACK_DEPTH];	// The stack of slots
		uint				slotStackPos_;					// The number of slots on the stack
	};

	// Stores a user defined break on an allocation within a slot
	struct Break
	{
		uint				slotId;
		uint				allocId;
	};

public:
						MemTracker();
						~MemTracker();

	static MemTracker&	instance();

	// Functions that allocate and free tracked memory
	void*				malloc( size_t size );
	void*				realloc( void* mem, size_t size );
	void				free( void* mem );
	char*				strdup( const char* s );

	// These functions are used by the MEMTRACKER_ macros to control
	// declaration and usage of slots
	int					declareSlot( const char* name, uint32 flags );
	void				begin( uint slotId );
	void				end();

	// User defined break on an allocation within a slot.
	int					declareBreak( uint slotId, uint allocId );

	// Query functions - they fill an AllocStats structure for global
	// memory usage or for a particular slot
	void				readStats( AllocStats& stats ) const;
	void				readStats( AllocStats& stats, uint slotId ) const;

	// Prints all memory stats to the log
	void				reportStats() const;

	// Unit tests need to control reporting and failing on leaks
	void				setReportOnExit( bool reportOnExit );
	void				setCrashOnLeak( bool crashOnLeak );

private:
	void				getCallStack();
	void				updateStats( int size, uint slotId );
	ThreadState*		findThreadState();
	void				breakIfRequested( uint slotId, uint allocId );

private:
	ListNode			list_;				// List of all allocated blocks
	RecursionCheckingMutex	mutex_;

	AllocStats			stats_;				// Global allocation stats

	uint				numSlots_;			// Number of declared slots
	Slot				slots_[MAX_SLOTS];	// Array of all slots

	uint				numThreads_;		// Number of threads we've seen
	ThreadState			threadStates_[MAX_THREADS];	// Array of all threads

	uint				numBreaks_;			// Number of declared breaks
	Break				breaks_[MAX_BREAKS];// Array of all breaks

	bool				reportOnExit_;		// Report leaks from destructor
	bool				crashOnLeak_;		// Will cause unit tests to fail if they leak

#if ENABLE_STACKWALKER && defined( WIN32 )
	MemTrackerStackWalker	stackWalker_;	// StackWalker class (third party library)
#endif
};

#define malloc bw_malloc
#define realloc bw_realloc
#define free bw_free
#define strdup bw_strdup
#define _strdup bw_strdup

extern MemTracker	g_memTracker;
extern int			g_memTrackerSlot_Default; // Dummy declaration, see .cpp

// -----------------------------------------------------------------------------

inline void MemTracker::setReportOnExit( bool reportOnExit )
{
	reportOnExit_ = reportOnExit;
}

inline void MemTracker::setCrashOnLeak( bool crashOnLeak )
{
	crashOnLeak_ = crashOnLeak;
}

// -----------------------------------------------------------------------------

// Class that pushes and pops a slot while its in scope, used by the
// MEMTRACKER_SCOPED macro.

class ScopedMemTracker
{
public:
	ScopedMemTracker( int id )
	{
		g_memTracker.begin( id );
	}

	~ScopedMemTracker()
	{
		g_memTracker.end();
	}
};

#endif	// ENABLE_MEMTRACKER

// -----------------------------------------------------------------------------

#endif
