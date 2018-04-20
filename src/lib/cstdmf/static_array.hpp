/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __CSTDMF_STATIC_ARRAY_HP___
#define __CSTDMF_STATIC_ARRAY_HP___

#include "stdmf.hpp"
#include "debug.hpp"

/**
 * This is a simple constant size array - essentially a checked wrapper around
 * a C style array with some "sugar" methods. 
 *
 * This array has no additional space overhead, and range checking is only 
 * done in DEBUG configurations.
 *
 * Example usage: 
 *	See unit tests.
 */
template < typename TYPE, size_t COUNT >
	class StaticArray
	{
	public:
		typedef TYPE ElementType ;
		enum {	ARRAY_SIZE = COUNT };
		
		/**
		 * These methods provide indexed access.
		 */
		inline ElementType& operator[] (const size_t i )
		{
			MF_ASSERT_DEBUG( i < ARRAY_SIZE );
			return data_[i];
		}

		inline const ElementType& operator[] (const size_t i ) const
		{
			MF_ASSERT_DEBUG( i < ARRAY_SIZE );
			return data_[i];
		}

		/**
		 * This method returns the number of elements in this array.
		 */
		inline size_t size() const
		{
			return ARRAY_SIZE;
		}

		/**
		 * This method fills all elements of this array with a given value.
		 */
		inline void assign( ElementType assignValue )
		{
			for ( size_t i = 0; i < ARRAY_SIZE; i++ )
			{
				data_[i] = assignValue;
			}
		}

	private:
		ElementType data_[ ARRAY_SIZE ];
	};

#endif
