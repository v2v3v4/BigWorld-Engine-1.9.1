/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef PY_PATROLPATH_HPP
#define PY_PATROLPATH_HPP

#include "pyobject_plus.hpp"
#include "script.hpp"
#include "math/vector3.hpp"

/*~ class BigWorld.PatrolPath
 *  @components{ client, cell, worldeditor }
 *  An instance of PatrolPath provides access to a waypoint station graph,
 *  and exposes various methods useful for AI entities that want to navigate
 *	around the graph.
 *	PatrolPaths tend to be used in conjunction with navigation controllers,
 *	but it is up to the entity to use this information as they like.
 *
 *  Code Example:
 *  @{
 *	# this returns a PatrolPath object.
 *  path = BigWorld.PatrolPath( graphID )
 *
 *	# you must wait until the PatrolPath is ready for use.  The graph
 *	# is loaded in the loading thread when the first chunk station node
 *	# is found.  Until then, you cannot retrieve useful information.
 *	if not path.isReady():
 *		self.comeBackLater()
 *
 *	# this returns a handle to a node in the patrol path
 *	(self.targetPatrolNode,position) = path.nearestNode(self.position)
 *
 *	# navigation style is up to the entity script.  this is but one example.
 *  navigation = BigWorld.navigationController()
 *  navigation.navigateTo( position, self.onReachNode )
 *
 *	# this example callback is called when a destination is reached.
 *	# it chooses the first node that is reachable from the current one,
 *	# and navigates there.
 *	def onReachNode( self ):
 *		availableDestinations = self.path.nodesTraversableFrom( self.targetPatrolNode )
 *		(self.targetPatrolNode, position) = availableDestinations(0)
 *		navigation.navigateTo( position, self.onReachNode )
 *  @}
 */
/**
 *	This class is used to expose waypoint station graphs to scripting.
 */
class PatrolPath : public PyObjectPlus
{
	Py_Header( PatrolPath, PyObjectPlus )

public:	
	PatrolPath( class StationGraph& graph,
		PyTypePlus * pType = &PatrolPath::s_type_ );	

	PyObject * pyGetAttribute( const char * attr );

	const class UniqueID& graphID() const;

	///This method returns the graphID as a string object
	std::string graphIDAsString() const;
	PY_AUTO_METHOD_DECLARE( RETDATA, graphIDAsString, END )

    /// This method returns whether the graph is loaded and ready for use
	bool isReady() const;
	PY_AUTO_METHOD_DECLARE( RETDATA, isReady, END )

	///This method returns a tuple of (node handle, world position)
	PyObject* nearestNode( Vector3 worldPosition );
	PY_AUTO_METHOD_DECLARE( RETOWN, nearestNode, ARG( Vector3, END ) )

	///This method returns the world position of (node handle)
	Vector3 worldPosition( const std::string& );
	PY_AUTO_METHOD_DECLARE( RETDATA, worldPosition, ARG( std::string, END ) )

	///This method takes a node handle as a parameter, and returns a
	///tuple of tuples of (node handle, world position)
	PyObject* nodesTraversableFrom( const std::string& );
	PY_AUTO_METHOD_DECLARE( RETOWN, nodesTraversableFrom, ARG( std::string, END ) )

	PY_FACTORY_DECLARE()

private:
	class StationGraph&	graph_;
};

#ifdef CODE_INLINE
// #include "py_patrolpath.ipp"
#endif

#endif // PY_PATROLPATH_HPP
