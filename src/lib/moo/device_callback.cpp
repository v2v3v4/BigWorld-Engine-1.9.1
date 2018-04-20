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

#include "device_callback.hpp"

#ifndef CODE_INLINE
#include "device_callback.ipp"
#endif


#include <algorithm>
#include "cstdmf/concurrency.hpp"
#include "cstdmf/debug.hpp"


namespace Moo
{

DeviceCallback::CallbackList* DeviceCallback::callbacks_ = NULL;

// There's was a problem when callback objects were deleted in a background
// thread that led to invalid pointers.... this list should stop that.
DeviceCallback::CallbackList* DeviceCallback::deletedList_ = NULL;
static SimpleMutex * deletionLock_ = NULL;

// we assume that there will not be thread contention for the
// creation of the first DeviceCallback and the deletion of the last.
static SimpleMutex * callbacksLock_ = NULL;

/**
 *	Check to see the callback objects have all been destructed properly.
 */
void DeviceCallback::fini()
{
	BW_GUARD;
	// check leaking
	if (callbacks_)
	{
		callbacksLock_->grab();
		uint count = callbacks_->size();
		WARNING_MSG("%d DeviceCallback object(s) NOT DELETED\n", count);
		CallbackList::iterator it = callbacks_->begin();
		for (; it!=callbacks_->end(); it++)
		{
			WARNING_MSG("DeviceCallback: NOT DELETED : %lx\n", (long)(*it) );
		}
		callbacksLock_->give();
	}
}


/**
 *	Constructor.
 */
DeviceCallback::DeviceCallback()
{
	BW_GUARD;
	if (!callbacks_)	// assumed to be unthreaded
	{
    	callbacks_ = new DeviceCallback::CallbackList;
		callbacksLock_ = new SimpleMutex;

		deletedList_ = new DeviceCallback::CallbackList;		
		deletionLock_ = new SimpleMutex;		
	}

	SimpleMutexHolder smh( *callbacksLock_ );
	callbacks_->push_back( this );
}


/**
 *	Destructor.
 */
DeviceCallback::~DeviceCallback()
{
	BW_GUARD;
	if (callbacks_)
    {
		callbacksLock_->grab();

        CallbackList::iterator it = std::find( callbacks_->begin(), callbacks_->end(), this );
        if( it != callbacks_->end() )
        {
            callbacks_->erase( it );
        }

	    bool wasEmpty = callbacks_->empty();

		callbacksLock_->give();

		if (wasEmpty)	// assumed to be unthreaded
        {
	    	delete callbacks_;
			callbacks_ = NULL;
			delete callbacksLock_;
			callbacksLock_ = NULL;

			delete deletedList_;
			deletedList_ = NULL;

			delete deletionLock_;
			deletionLock_ = NULL;

        }
		if ( deletedList_ )
		{
			deletionLock_->grab();
			deletedList_->push_back(this);
			deletionLock_->give();
		}
    }
}

void DeviceCallback::deleteUnmanagedObjects( )
{

}

void DeviceCallback::createUnmanagedObjects( )
{

}

void DeviceCallback::deleteManagedObjects( )
{

}

void DeviceCallback::createManagedObjects( )
{

}

void DeviceCallback::deleteAllUnmanaged( )
{
	BW_GUARD;
	if ( callbacks_ )
    {
		callbacksLock_->grab();
		CallbackList cbCopy = *callbacks_;
		deletedList_->clear();
		callbacksLock_->give();

        CallbackList::iterator it = cbCopy.begin();
        CallbackList::iterator end = cbCopy.end();

        while( it != end )
        {
			deletionLock_->grab();
			CallbackList::iterator deleted_iter = std::find(deletedList_->begin(), deletedList_->end(), (*it));
			if (deleted_iter != deletedList_->end())
			{
				//found this object in the recently deleted list.. dont call the callback
				it++;
				deletionLock_->give();
				continue;
			}
			deletionLock_->give();

            (*it)->deleteUnmanagedObjects();
            it++;
        }

		deletionLock_->grab();
		deletedList_->clear();
		deletionLock_->give();
    }

#if ENABLE_RESOURCE_COUNTERS
	ResourceCounters::instance().printPoolContents(D3DPOOL_DEFAULT);
#endif
}

void DeviceCallback::createAllUnmanaged( )
{
	BW_GUARD;
	if ( callbacks_ )
    {
		callbacksLock_->grab();
		CallbackList cbCopy = *callbacks_;
		deletedList_->clear();
		callbacksLock_->give();

        CallbackList::iterator it = cbCopy.begin();
        CallbackList::iterator end = cbCopy.end();

        while( it != end )
        {
			deletionLock_->grab();
			CallbackList::iterator deleted_iter = std::find(deletedList_->begin(), deletedList_->end(), (*it));
			if (deleted_iter != deletedList_->end())
			{
				//found in the recently deleted list.. dont call the callback
				it++;
				deletionLock_->give();
				continue;
			}
			deletionLock_->give();

            (*it)->createUnmanagedObjects();
            it++;
        }

		deletionLock_->grab();
		deletedList_->clear();
		deletionLock_->give();
    }
}

void DeviceCallback::deleteAllManaged( )
{
	BW_GUARD;
	if ( callbacks_ )
    {
        callbacksLock_->grab();
		CallbackList cbCopy = *callbacks_;
		deletedList_->clear();
		callbacksLock_->give();

        CallbackList::iterator it = cbCopy.begin();
        CallbackList::iterator end = cbCopy.end();

        while( it != end )
        {
			deletionLock_->grab();
			CallbackList::iterator deleted_iter = std::find(deletedList_->begin(), deletedList_->end(), (*it));
			if (deleted_iter != deletedList_->end())
			{
				//found in the recently deleted list.. dont call the callback
				it++;
				deletionLock_->give();
				continue;
			}
			deletionLock_->give();

            (*it)->deleteManagedObjects();
            it++;
        }

		deletionLock_->grab();
		deletedList_->clear();
		deletionLock_->give();
    }
}

void DeviceCallback::createAllManaged( )
{
	BW_GUARD;
	if ( callbacks_ )
    {
        callbacksLock_->grab();
		CallbackList cbCopy = *callbacks_;
		deletedList_->clear();
		callbacksLock_->give();

        CallbackList::iterator it = cbCopy.begin();
        CallbackList::iterator end = cbCopy.end();

        while( it != end )
        {
			deletionLock_->grab();
			CallbackList::iterator deleted_iter = std::find(deletedList_->begin(), deletedList_->end(), (*it));
			if (deleted_iter != deletedList_->end())
			{
				//found in the recently deleted list.. dont call the callback
				it++;
				deletionLock_->give();
				continue;
			}
			deletionLock_->give();

            (*it)->createManagedObjects();
            it++;
        }
		deletionLock_->grab();
		deletedList_->clear();
		deletionLock_->give();
    }
}

GenericUnmanagedCallback::GenericUnmanagedCallback( Function* createFunction, Function* destructFunction  )
: createFunction_( createFunction ),
  destructFunction_( destructFunction )
{
}

GenericUnmanagedCallback::~GenericUnmanagedCallback( )
{
}

void GenericUnmanagedCallback::deleteUnmanagedObjects( )
{
	destructFunction_( );
}

void GenericUnmanagedCallback::createUnmanagedObjects( )
{
	createFunction_( );
}

}

// device_callback.cpp
