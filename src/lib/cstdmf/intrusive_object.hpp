/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef INTRUSIVE_OBJECT_HPP
#define INTRUSIVE_OBJECT_HPP

#include <vector>
#include <algorithm>


/**
 *	This is the base class for all intrusive objects. An intrusive object is an
 *	object that automatically inserts itself into a collection when it is
 *	created and removes itself when it is destroyed.
 */
template < class ELEMENT >
class IntrusiveObject
{
public:
	/// This typedef specifies the container type that these intrusive objects
	/// will insert themselves into.
	typedef std::vector< ELEMENT * > Container;

protected:
	/**
	 *	The constructor takes reference to the a pointer that points to the
	 *	container that the object will insert itself into. If the container does
	 *	not yet exist, it will be created.
	 */
	IntrusiveObject( Container *& pContainer, bool shouldAdd = true ) :
		pContainer_( pContainer )
	{
		if (shouldAdd)
		{
			if (pContainer_ == NULL)
			{
				pContainer_ = new Container;
			}

			pContainer_->push_back( pThis() );
		}
	}


	/**
	 *	The destructor automatically removes this object from the list it was
	 *	inserted into. If this leaves the collection empty, the collection is
	 *	deleted.
	 */
	virtual ~IntrusiveObject()
	{
		if (pContainer_ != NULL)
		{
			typename IntrusiveObject::Container::iterator iter =
				std::find( pContainer_->begin(),
				pContainer_->end(),
				pThis() );

			pContainer_->erase( iter );

			if (pContainer_->empty())
			{
				delete pContainer_;
				pContainer_ = NULL;
			}
		}
	}

private:
	/// This method casts to the derived class type.
	ELEMENT * pThis()				{ return (ELEMENT *)this; }
	Container *& pContainer_;
};

#endif // INTRUSIVE_OBJECT_HPP

/* intrusive_object.hpp */
