/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GLOBAL_BASES_HPP
#define GLOBAL_BASES_HPP

#include "Python.h"

#include "cstdmf/binary_stream.hpp"
#include "pyscript/pyobject_plus.hpp"

#include <string>
#include <map>

class Base;
class BaseEntityMailBox;

/*~ class NoModule.GlobalBases
 *  @components{ base }
 *  An instance of this class emulates a dictionary of bases or base mailboxes.
 *
 *  Code Example:
 *  @{
 *  globalBases = BigWorld.globalBases
 *  print "The main mission entity is", globalBases[ "MainMission" ]
 *  print "There are", len( globalBases ), "global bases."
 *  @}
 */
/**
 *	This class is used to expose the collection of global bases.
 */
class GlobalBases : public PyObjectPlus
{
Py_Header( GlobalBases, PyObjectPlus )

public:
	GlobalBases( PyTypePlus * pType = &GlobalBases::s_type_ );
	~GlobalBases();

	PyObject * 			pyGetAttribute( const char * attr );

	PyObject * 			subscript( PyObject * entityID );
	int					length();

	PY_METHOD_DECLARE( py_has_key )
	PY_METHOD_DECLARE( py_keys )
	PY_METHOD_DECLARE( py_values )
	PY_METHOD_DECLARE( py_items )

	static PyObject * 	s_subscript( PyObject * self, PyObject * entityID );
	static Py_ssize_t	s_length( PyObject * self );

	void registerRequest( Base * pBase, char * label, PyObject * pCallback );
	bool deregister( Base * pBase, char * label );

	void onBaseDestroyed( Base * pBase );

	void add( Base * pBase, const char * label );
	void add( BaseEntityMailBox * pMailbox, const std::string & label );
	void remove( const std::string & label );

	void addLocalsToStream( BinaryOStream & stream ) const;

private:
	PyObject * pMap_;

	typedef std::multimap< Base *, std::string > Locals;
	Locals locals_;
};

#ifdef CODE_INLINE
// #include "global_bases.ipp"
#endif

#endif // GLOBAL_BASES_HPP
