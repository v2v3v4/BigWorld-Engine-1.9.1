/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifdef CODE_INLINE
#define INLINE    inline
#else
#define INLINE
#endif

/**
 *	This method returns the base with the input id.
 */
INLINE Base * BaseApp::findBase( EntityID id ) const
{
	Bases::const_iterator iter = bases_.find( id );

	return (iter != bases_.end()) ? iter->second : NULL;
}

// baseapp.ipp
