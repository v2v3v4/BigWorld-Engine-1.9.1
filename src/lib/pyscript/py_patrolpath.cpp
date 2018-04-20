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
#include "py_patrolpath.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/md5.hpp"
#include "chunk/station_graph.hpp"
#include "entitydef/data_types.hpp"
#include "cstdmf/binary_stream.hpp"

int PyPatrolPath_token = 1;

DECLARE_DEBUG_COMPONENT( 0 )

PY_TYPEOBJECT( PatrolPath )

PY_BEGIN_METHODS( PatrolPath )
/*~ function PatrolPath.graphIDAsString
 *  @components{ cell, worldeditor }
 *  This method returns the id of the graph as a string 
 *  @return The id of the graph as a string
 */
PY_METHOD( graphIDAsString )
/*~ function PatrolPath.isReady
 *	@components{ cell, worldeditor }
 *	This method returns whether or not the graph is ready to use.
 *	The graph is created synchronously but its nodes load independently
 *	in the loading thread.  Once a single node is loaded, the whole graph
 *	is ready to be used (the first node loads the graph file in the loading
 *	thread.)
 *	If the patrol path is not ready, then call again later, for example in
 *	one second, and try again.
 *	@return Bool, True if ready, False if not ready.
 */
PY_METHOD( isReady )
/*~ function PatrolPath.nearestNode
 *	@components{ cell, worldeditor }
 *	This method returns the nearest node to the given world position.
 *	ValueError is thrown if the graph is not yet ready.
 *	@param worldPosition A Vector3 world position for node distance 
 *	checking.
 *	@return	2-tuple (nodeID, worldPosition) where nodeId is a string of the 
 *	id of the nearest node and worldPosition is a Vector3 corresponding to 
 *	the node's position.
 */
PY_METHOD( nearestNode )
/*~ function PatrolPath.worldPosition
 *	@components{ cell, worldeditor }
 *	This method returns the world position of the node handle.
 *	ValueError is thrown if the graph is not yet ready.
 *	@param nodeID Node ID to retrieve the position for.
 *	@return	Vector3 World position of the node, or (0,0,0) if the node
 *	does not exist.
 */
PY_METHOD( worldPosition )
/*~ function PatrolPath.nodesTraversableFrom
 *	@components{ cell, worldeditor }
 *	This method returns the list of node that are traversable
 *	from the given node.
 *	ValueError is thrown if the graph is not yet ready.
 *	@param nodeID Node ID representing the source node
 *	@return	A tuple of 2-tuples (((nodeID_0, worldPosition_0), 
 *	(nodeID_1, worldPosition_1), ...) where each entry is a node id (String)
 *	and position (Vector3) of a node that is traversable from the given 
 *	node.  An empty tuple is returned if the node does not exist, or if
 *	there are no nodes traversible from the given node.
 */
PY_METHOD( nodesTraversableFrom )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( PatrolPath )
PY_END_ATTRIBUTES()


/**
 *	The constructor for PatrolPath.
 */
PatrolPath::PatrolPath( StationGraph& graph, PyTypePlus * pType ) :
	PyObjectPlus( pType ),
	graph_( graph )
{	
}


/**
 *	This method overrides the PyObjectPlus method.
 */
PyObject * PatrolPath::pyGetAttribute( const char * attr )
{
	PY_GETATTR_STD();

	return PyObjectPlus::pyGetAttribute( attr );
}


/** 
 *	This method returns the ID of the graph associated with the patrol path.
 *
 *	@return UniqueID	reference to the graph ID
 */
const UniqueID& PatrolPath::graphID() const
{
	return graph_.name();
}


/** 
 *	This method returns the id of the graph as a std::string 
 */
std::string PatrolPath::graphIDAsString() const
{
	return graphID().toString();
}

/**
 *	This method returns whether or not the graph is ready to use.
 *	The graph is created synchronously but its nodes load independently
 *	in the loading thread.  Once a single node is loaded, the whole graph
 *	is ready to be used (the first node loads the graph file in the loading
 *	thread.)
 *	If the patrol path is not ready, then callback later, for example in
 *	one second, and try again.
 *
 *	@return	bool	whether or not the patrol path is ready to use.
 */
bool PatrolPath::isReady() const
{
	return graph_.isReady();
}


/**
 *	This method returns the nearest node to the given world position.
 *
 *	@param	worldPosition	world position for node distance checking.
 *	@return	PyObject*		(nodeID, worldPosition) of nearest nodes.
 */
PyObject* PatrolPath::nearestNode( Vector3 worldPosition )
{
	if (!graph_.isReady())
	{
		PyErr_SetString( PyExc_ValueError, "PatrolPath: "
			"Graph is not ready for use.  Check isReady() before using." );
		return NULL;    
	}

	const UniqueID& id = graph_.nearestNode( worldPosition );
	PyObject * ret = PyTuple_New(2);
	PyTuple_SetItem(ret, 0, Script::getData(id.toString()));
	Vector3 nodeWorldPos;
	graph_.worldPosition(id,nodeWorldPos);
	PyTuple_SetItem(ret, 1, Script::getData(nodeWorldPos));
	return ret;
}


/**
 *	This method returns the world position of (node handle)
 *
 *	@param	nodeID		node ID to retrieve the position for.
 *	@return	Vector3		world position of the node, or (0,0,0)
 */
Vector3 PatrolPath::worldPosition( const std::string& nodeID )
{
	Vector3 pos(0,0,0);

	if (!graph_.isReady())
	{
		PyErr_SetString( PyExc_ValueError, "PatrolPath: "
			"Graph is not ready for use.  Check isReady() before using." );
		return pos;
	}

	if (!graph_.worldPosition( nodeID, pos ))
	{
		ERROR_MSG( "Invalid node handle %s for graph %s\n",
			nodeID.c_str(),
			graph_.name().toString().c_str() );
	}
	return pos;
}


/**
 *	This method returns the list of node that are traversable
 *	from the given node.
 *	@param	nodeID		node ID representing the source node
 *	@return	PyObject*	tuple of (nodeID, worldPos) representing nodes
 *						reachable from the source node.
 */
PyObject* PatrolPath::nodesTraversableFrom( const std::string& nodeID )
{
	if (!graph_.isReady())
	{
		PyErr_SetString( PyExc_ValueError, "PatrolPath: "
			"Graph is not ready for use.  Check isReady() before using." );
		return NULL;    
	}

	std::vector<UniqueID> reachableNodes;
	uint32 num = graph_.traversableNodes( nodeID, reachableNodes );
	if (num == 0)
	{
		return PyTuple_New(0);
	}	

	bool isOkay = true;

	PyObject * ret = PyTuple_New(num);
	for (uint32 i=0; i<num; i++)
	{
		UniqueID& id = reachableNodes[i];		
		PyObject * entry = PyTuple_New(2);		
		PyTuple_SetItem(entry, 0, Script::getData(id));
		Vector3 nodeWorldPos( 0.f, 0.f, 0.f );
		if (!graph_.worldPosition( id, nodeWorldPos ))
		{
			// Set the error but continue so that the destruction can happen
			// cleanly.
			isOkay = false;
			PyErr_Format( PyExc_EnvironmentError,
				"Graph %s has link from %s to %s but destination does not exist",
				this->graphID().toString().c_str(),
				nodeID.c_str(), id.toString().c_str() );
		}
		PyTuple_SetItem(entry, 1, Script::getData(nodeWorldPos));
		PyTuple_SetItem(ret, i, entry);
	}

	if (!isOkay)
	{
		Py_DECREF( ret );
		return NULL;
	}

	return ret;
}


/*~ function BigWorld.PatrolPath
 *	@components{ client, base, cell, worldeditor }
 *	Factory function to create and return a PatrolPath object.
 *	@return A new PatrolPath object.
 */
PY_FACTORY( PatrolPath, BigWorld )


/**
 *	This is a static Python factory method. This is declared through the
 *	factory declaration in the class definition.
 *
 *	@param args		The list of parameters passed from Python. None are
 *					expected or checked.
 *	@return PyObject*	A new PatrolPath object, or NULL.
 */
PyObject * PatrolPath::pyNew( PyObject * args )
{
	char * graphID;
	if (!PyArg_ParseTuple( args, "s", &graphID ))
	{
		PyErr_SetString( PyExc_TypeError, "PatrolPath: "
			"Argument parsing error: Expected a graph ID (string)" );
		return NULL;
	}

	//check the graph exists.
	StationGraph* graph = StationGraph::getGraph( std::string(graphID) );
	if (!graph)
	{
		PyErr_SetString( PyExc_AttributeError, "PatrolPath: "
			"Specified graph does not exist" );
		return NULL;
	}

	return new PatrolPath( *graph );
}



// -----------------------------------------------------------------------------
// Section: PatrolPathDataType
// -----------------------------------------------------------------------------

// The editor has its own implementation of this type.
#ifndef EDITOR_ENABLED

/**
 *	This template class is used to represent the patrol path data type.
 *
 *	@ingroup entity
 */
class PatrolPathDataType : public DataType
{
public:
	PatrolPathDataType( MetaDataType * pMeta ) : DataType( pMeta ) { }

protected:
	virtual bool isSameType( PyObject * pValue );
	virtual void setDefaultValue( DataSectionPtr pSection );
	virtual PyObjectPtr pDefaultValue() const;

	virtual void addToStream( PyObject * pNewValue,
			BinaryOStream & stream, bool isPersistentOnly ) const;
	virtual PyObjectPtr createFromStream( BinaryIStream & stream,
			bool isPersistentOnly ) const;

	virtual void addToSection( PyObject * pNewValue, DataSectionPtr pSection )
			const;
	virtual PyObjectPtr createFromSection( DataSectionPtr pSection ) const;

	virtual bool fromStreamToSection( BinaryIStream & stream,
			DataSectionPtr pSection, bool isPersistentOnly ) const;
	virtual bool fromSectionToStream( DataSectionPtr pSection,
			BinaryOStream & stream, bool isPersistentOnly ) const;

	virtual void addToMD5( MD5 & md5 ) const;

	virtual bool operator<( const DataType & other ) const;

private:
	PyObjectPtr pDefaultValue_;
};


/**
 *	Overrides the DataType method.
 *
 *	@see DataType::isSameType
 */
bool PatrolPathDataType::isSameType( PyObject * pValue )
{
	return PatrolPath::Check( pValue ) || (pValue == Py_None);
}

/**
 *	This method sets the default value for this type.
 *
 *	@see DataType::setDefaultValue
 */
void PatrolPathDataType::setDefaultValue( DataSectionPtr pSection )
{
	pDefaultValue_ = NULL;	
}

/**
 *	Overrides the DataType method.
 *
 *	The default value for PatrolPath is None.
 *
 *	@see DataType::pDefaultValue
 */
PyObjectPtr PatrolPathDataType::pDefaultValue() const
{
	Py_Return;
}

/**
 *	Overrides the DataType method.
 *
 *	@see DataType::addToStream
 */
void PatrolPathDataType::addToStream( PyObject * pNewValue,
		BinaryOStream & stream, bool /*isPersistentOnly*/ ) const
{
	if (PatrolPath::Check( pNewValue ))
	{
		PatrolPath * pPath = static_cast< PatrolPath * >( pNewValue );	
		stream << pPath->graphID();	
	}
	else
	{
		if (pNewValue != Py_None)
		{
			ERROR_MSG( "PatrolPathDataType::addToStream must be called with a "
						"Patrol Path\n" );
		}
		stream << UniqueID::zero();
	}
}

/**
 *	Overrides the DataType method.
 *
 *	@see DataType::createFromStream
 */
PyObjectPtr PatrolPathDataType::createFromStream( BinaryIStream & stream,
		bool /*isPersistentOnly*/ ) const
{
	UniqueID value;
	stream >> value;

	if (value != UniqueID::zero())
	{
		StationGraph* graph = StationGraph::getGraph( value );
		return PyObjectPtr( new PatrolPath( *graph ), PyObjectPtr::STEAL_REFERENCE );	
	}
	else
	{
		Py_Return;
	}
}


/*
 *	Overrides the DataType method.
 */
void PatrolPathDataType::addToSection( PyObject * pNewValue,
		DataSectionPtr pSection ) const
{
	if (PatrolPath::Check( pNewValue ))
	{
		PatrolPath * pp = static_cast<PatrolPath*>(pNewValue);
		pSection->setString( pp->graphID().toString() );
	}
	else
	{
		if (pNewValue != Py_None)
		{
			ERROR_MSG( "PatrolPathDataType::addToSection: "
					"must be called with a PatrolPath\n" );		
		}
		pSection->setString( "" );
	}
}


/**
 *	Overrides the DataType method.
 *
 *	If the graph ID is not null, return a PatrolPath PyObject.
 *	If the graph ID is null, then return None.
 *
 *	@see DataType::createFromSection
 */
PyObjectPtr PatrolPathDataType::createFromSection(DataSectionPtr pSection) const
{
	std::string value;
	value = pSection->asString();
	if (!value.empty())
	{
		StationGraph* graph = StationGraph::getGraph( value );
		return PyObjectPtr( new PatrolPath(*graph), PyObjectPtr::STEAL_REFERENCE );
	}
	else
	{
		Py_Return;
	}
}


/**
 *	Overrides the DataType method.
 *
 *	Pass the string from the stream and write it into the data section.
 *
 *	@see DataType::fromStreamToSection
 */
bool PatrolPathDataType::fromStreamToSection( BinaryIStream & stream,
		DataSectionPtr pSection, bool /*isPersistentOnly*/ ) const
{
	std::string value;
	stream >> value;
	if (stream.error()) return false;

	pSection->setString( value );
	return true;
}


/**
 *	Overrides the DataType method.
 *
 *	Read the string from the data section and stream it out.
 *
 *	@see DataType::fromSectionToStream
 */
bool PatrolPathDataType::fromSectionToStream( DataSectionPtr pSection,
			BinaryOStream & stream, bool /*isPersistentOnly*/ ) const
{
	stream << pSection->asString();
	return true;
}


/**
 *	Overrides the DataType method.
 *
 *	Adds the Patrol Path instance to the md5.
 *
 *	@see DataType::addToMD5
 */
void PatrolPathDataType::addToMD5( MD5 & md5 ) const
{
	md5.append( "PatrolPath", sizeof( "PatrolPath" ) );
}


/**
 *	Overrides the DataType method.
 */
bool PatrolPathDataType::operator<( const DataType & other ) const
{
	if (this->DataType::operator<( other )) return true;
	if (other.DataType::operator<( *this )) return false;

	const PatrolPathDataType& otherStr =
		static_cast< const PatrolPathDataType& >( other );
	return (Script::compare( pDefaultValue_.getObject(),
		otherStr.pDefaultValue_.getObject() ) < 0);
}


SIMPLE_DATA_TYPE( PatrolPathDataType, PATROL_PATH )
#endif // EDITOR_ENABLED
