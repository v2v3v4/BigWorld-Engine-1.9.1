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
#include "worldeditor/world/items/editor_chunk_user_data_object.hpp"
#include "worldeditor/world/items/editor_chunk_substance.ipp"
#include "worldeditor/world/items/editor_chunk_station.hpp"
#include "worldeditor/world/items/editor_chunk_user_data_object_link.hpp"
#include "worldeditor/world/editor_chunk_item_linker.hpp"
#include "worldeditor/world/world_manager.hpp"
#include "worldeditor/world/editor_entity_proxy.hpp"
#include "worldeditor/world/editor_chunk.hpp"
#include "worldeditor/world/entity_property_parser.hpp"
#include "worldeditor/world/editor_chunk_item_linker_manager.hpp"
#include "worldeditor/editor/item_editor.hpp"
#include "worldeditor/editor/item_properties.hpp"
#include "worldeditor/editor/editor_property_manager.hpp"
#include "appmgr/options.hpp"
#include "model/super_model.hpp"
#include "romp/geometrics.hpp"
#include "chunk/chunk_model.hpp"
#include "chunk/chunk_manager.hpp"
#include "entitydef/data_description.hpp"
#include "entitydef/user_data_object_description.hpp"
#include "entitydef/data_types.hpp"
#include "entitydef/constants.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/xml_section.hpp"
#include "resmgr/string_provider.hpp"
#include "gizmo/link_property.hpp"
#include "cstdmf/debug.hpp"

#if UMBRA_ENABLE
#include <umbraModel.hpp>
#include <umbraObject.hpp>
#include "chunk/chunk_umbra.hpp"
#endif

DECLARE_DEBUG_COMPONENT2( "Editor", 0 )


SimpleMutex EditorChunkUserDataObject::s_dirtyModelMutex_;
SimpleMutex EditorChunkUserDataObject::s_loadingModelMutex_;
std::vector<EditorChunkUserDataObject*> EditorChunkUserDataObject::s_dirtyModelEntities_;


//TODO: find a good default model
static std::string s_defaultModel = "resources/models/user_data_object.model";


// Link to the UalUserDataObjectProvider, so entities get listed in the Asset Locator
// this token is defined in ual_udo_provider.cpp
extern int UalUserDataObjectProv_token;
static int total = UalUserDataObjectProv_token;


// Link to the UDO_REF data type
extern int UserDataObjectLinkDataType_token;
static int s_tokenSet = UserDataObjectLinkDataType_token;


// -----------------------------------------------------------------------------
// Section: EditorUserDataObjectType
// -----------------------------------------------------------------------------
EditorUserDataObjectType * EditorUserDataObjectType::s_instance_ = NULL;


void EditorUserDataObjectType::startup()
{
	MF_ASSERT(!s_instance_);
	s_instance_ = new EditorUserDataObjectType();
}


void EditorUserDataObjectType::shutdown()
{
	MF_ASSERT(s_instance_);
	delete s_instance_;
	s_instance_ = NULL;
}


EditorUserDataObjectType::EditorUserDataObjectType()
{
	// load normal entities
	cMap_.parse( BWResource::openSection(
		EntityDef::Constants::userDataObjectsFile() ) );

	// load the editor UserDataObject scripts
	INFO_MSG( "EditorUserDataObjectType constructor - Importing editor UserDataObject scripts\n" );
	DataSectionPtr edScripts = BWResource::openSection(
		EntityDef::Constants::userDataObjectsEditorPath() );

	if( edScripts )
	{
		for (UserDataObjectDescriptionMap::DescriptionMap::const_iterator it = cMap_.begin();
			it != cMap_.end(); it++)
		{
			std::string name = (*it).first;

			if ( edScripts->openSection( name + ".py" ) == NULL )
			{
				INFO_MSG( "EditorUserDataObjectType - no editor script found for %s\n", name.c_str() );
				continue;
			}

			// class name and module name are the same
			PyObject * pModule = PyImport_ImportModule( const_cast<char *>( name.c_str() ) );
			if (PyErr_Occurred())
			{
				ERROR_MSG( "EditorUserDataObjectType - fail to import editor script %s\n", name.c_str() );
				PyErr_Print();
				continue;
			}

			MF_ASSERT(pModule);

			PyObject * pyClass = PyObject_CallMethod( pModule, const_cast<char *>( name.c_str() ), "" );
			Py_XDECREF( pModule );

			if (PyErr_Occurred())
			{
				ERROR_MSG( "EditorUserDataObjectType - fail to open editor script %s\n", name.c_str() );
				PyErr_Print();
				continue;
			}

			MF_ASSERT(pyClass);

			pyClasses_[ name ] = pyClass;
		}
	}
}

const UserDataObjectDescription * EditorUserDataObjectType::get( const std::string & name )
{
	// try it as an UserDataObject
	if (cMap_.isUserDataObject( name))
		return &(cMap_.udoDescription(name));
	return NULL;
}

PyObject * EditorUserDataObjectType::getPyClass( const std::string & name )
{
	std::map<std::string, PyObject *>::iterator mit = pyClasses_.find( name );
	if (mit != pyClasses_.end())
	{
		return mit->second;
	}

	return NULL;
}

EditorUserDataObjectType& EditorUserDataObjectType::instance()
{
	MF_ASSERT(s_instance_);
	return *s_instance_;
}


// -----------------------------------------------------------------------------
// Section: EditorChunkUserDataObject
// -----------------------------------------------------------------------------

/**
 *	Constructor.
 */
EditorChunkUserDataObject::EditorChunkUserDataObject() :
	pType_( NULL ),
	transform_( Matrix::identity ),
	transformLoaded_( false ),
	pDict_( NULL ),
	pyClass_( NULL ),
	guid_(UniqueID::generate()),
	model_( NULL ),
	firstLinkFound_( false ),
	loadBackgroundTask_( NULL )
{
	pChunkItemLinker_ = new EditorChunkItemLinkable(this, guid_, &propHelper_);
}

/**
 *	Destructor.
 */
EditorChunkUserDataObject::~EditorChunkUserDataObject()
{
	(pChunkItemLinker_) ? delete pChunkItemLinker_ : 0;
	loadBackgroundTask_ = NULL;
	removeFromDirtyList();
	Py_XDECREF( pDict_ );
}

bool EditorChunkUserDataObject::initType( const std::string type, std::string* errorString )
{
	if ( pType_ != NULL )
		return true;

	pType_ = EditorUserDataObjectType::instance().get( type );

	if (!pType_)
	{
		std::string err = "No definition for user data object type '" + type + "'";
		if ( errorString )
		{
			*errorString = err;
		}
		else
		{
			ERROR_MSG( "EditorChunkUserDataObject::load - %s\n", err.c_str() );
		}
		return false;
	}

	return true;
}


/**
 *	This method returns a dictionary containing internal attributes of the
 *	User Data Object.
 *
 *	@return		A python dictionary containing chunk id, guid, position, etc.
 */
PyObject* EditorChunkUserDataObject::infoDict() const
{
	PyObject* dict = PyDict_New();

	// add general item data
	PyDict_SetItemString( dict, "chunk", Py_BuildValue( "s", chunkItemLinker()->getOutsideChunkId().c_str() ) );
	PyDict_SetItemString( dict, "guid", Py_BuildValue( "s", guid_.toString().c_str() ) );
	PyDict_SetItemString( dict, "type", Py_BuildValue( "s", pType_->name().c_str() ) );
	Vector3 pos = transform_.applyToOrigin();
	
	if (chunk())
	{
		pos = chunk()->transform().applyPoint( pos );
	}

	PyDict_SetItemString( dict, "position", Py_BuildValue( "(fff)", pos.x, pos.y, pos.z ) );

	// add back links
	PyObjectPtr backLinks( PyTuple_New( this->chunkItemLinker()->getBackLinksCount() ), PyObjectPtr::STEAL_REFERENCE );
	int ti = 0;
	for( EditorChunkItemLinkable::Links::const_iterator i = chunkItemLinker()->getBackLinksBegin(); i != chunkItemLinker()->getBackLinksEnd(); ++i, ++ti )
	{
		PyTuple_SetItem( backLinks.getObject(), ti,
			Py_BuildValue( "(ss)", (*i).UID_.toString().c_str(), (*i).CID_.c_str() ) );
	}
	PyDict_SetItemString( dict, "backLinks", backLinks.getObject() );

	// add the properties
	PyDict_SetItemString( dict, "properties", pDict_ );

	return dict;
}


/**
 *	This method asks the object's editor script to find out if this object can
 *	be linked to the other object.
 *
 *	@param propName	Name of the link property in this object.
 *	@param other	Object to link to.
 *	@return			true if this object can link to the other.
 */
bool EditorChunkUserDataObject::canLinkTo( const std::string& propName, const EditorChunkUserDataObject* other ) const
{
	if ( !pyClass_ )
		return true;

	PyObject* thisInfo = infoDict();
	PyObject* otherInfo = other->infoDict();

	PyObject * result = Script::ask(
		PyObject_GetAttrString( pyClass_, "canLink" ),
		Py_BuildValue( "(sOO)", propName.c_str(), thisInfo, otherInfo ),
		"EditorChunkUserDataObject::canLinkTo: ",
		true /* ok if function NULL */ );

	Py_DECREF( thisInfo );
	Py_DECREF( otherInfo );

	if ( !result || !PyBool_Check( result ) )
		return true;

	return result == Py_True;
}


/**
 *	This method asks the object's editor script whether it should show the
 *	"add node" gizmo, in addition to the "link" gizmo.
 *
 *	@param propName	Name of the link property in this object.
 *	@return			true if this "add node" gizmo should be shown.
 */
bool EditorChunkUserDataObject::showAddGizmo( const std::string& propName ) const
{
	if ( !pyClass_ )
		return false;

	PyObject* thisInfo = infoDict();

	PyObject * result = Script::ask(
		PyObject_GetAttrString( pyClass_, "showAddGizmo" ),
		Py_BuildValue( "(sO)", propName.c_str(), thisInfo ),
		"EditorChunkUserDataObject::showAddGizmo: ",
		true /* ok if function NULL */ );

	Py_DECREF( thisInfo );

	if ( !result || !PyBool_Check( result ) )
		return false;

	return result == Py_True;
}


/**
 *	This method tells the object's editor script that a UDO has been deleted.
 */
void EditorChunkUserDataObject::onDelete() const
{
	if ( !pyClass_ )
		return;

	PyObject* thisInfo = infoDict();

	Script::call(
		PyObject_GetAttrString( pyClass_, "onDeleteObject" ),
		Py_BuildValue( "(O)", thisInfo ),
		"EditorChunkUserDataObject::onDeleteObject: ",
		true /* ok if function NULL */ );

	Py_DECREF( thisInfo );
}


/**
 *	This method asks the object's editor script the context menu commands it
 *	handles. These commands will be displayed when right-clicking on top of a
 *	link. It only makes sense for links between UDOs of the same type.
 *
 *	@param commands		Vector of strings containing commands. This vector
 *						might already contain commands.
 *	@param other		UDO at the end of the link.
 */
void EditorChunkUserDataObject::getLinkCommands(
	std::vector<std::string>& commands, const EditorChunkUserDataObject* other ) const
{
	if ( !pyClass_ )
		return;

	// this only makes sense if the UDOs are the same type
	if ( this->typeGet() != other->typeGet() )
		return;

	PyObject* startInfo = infoDict();
	PyObject* endInfo = other->infoDict();

	PyObject * result = Script::ask(
		PyObject_GetAttrString( pyClass_, "onStartLinkMenu" ),
		Py_BuildValue( "(OO)", startInfo, endInfo ),
		"EditorChunkUserDataObject::onStartLinkMenu: ",
		true /* ok if function NULL */ );

	Py_DECREF( startInfo );
	Py_DECREF( endInfo );

	if ( !result )
		return;

	if ( PySequence_Check( result ) )
	{
		for( int i = 0; i < PySequence_Size( result ); ++i )
		{
			PyObject* command = PySequence_GetItem( result, i );
			if ( PyString_Check( command ) )
				commands.push_back( PyString_AsString( command ) );
			Py_DECREF( command );
		}
	}

	Py_DECREF( result );
}


/**
 *	This method asks the object's editor script to handle a one of its commands
 *	as returned in getLinkCommands.
 *
 *	@param cmdIdx		Zero-based index to the command clicked by the user.
 *	@param other		UDO at the end of the link.
 */
void EditorChunkUserDataObject::executeLinkCommand(
	int cmdIndex, const EditorChunkUserDataObject* other ) const
{
	if ( !pyClass_ )
		return;

	PyObject* startInfo = infoDict();
	PyObject* endInfo = other->infoDict();

	Script::call(
		PyObject_GetAttrString( pyClass_, "onEndLinkMenu" ),
		Py_BuildValue( "(iOO)", cmdIndex, startInfo, endInfo ),
		"EditorChunkUserDataObject::onEndLinkMenu: ",
		true /* ok if function NULL */ );

	Py_DECREF( startInfo );
	Py_DECREF( endInfo );
}


/**
 *	Our load method. We can't call (or reference) the base class's method
 *	because it would not compile (chunk item has no load method)
 */
bool EditorChunkUserDataObject::load( DataSectionPtr pSection, Chunk * pChunk, std::string* errorString )
{
	edCommonLoad( pSection );
	// read known properties
	model_ = Model::get( s_defaultModel );
	pOwnSect_ = pSection;
	std::string typeName = pOwnSect_->readString( "type" );

	bool ret = initType( typeName, errorString );

	if ( ret && Moo::g_renderThread )
	{
		// If loading from the main thread, load straight away
		ret &= edLoad( pOwnSect_ );
	}
	return ret;
}

void EditorChunkUserDataObject::edMainThreadLoad()
{
	// have to load this in the main thread to avoid multi-thread issues with
	// some python calls/objects in edLoad
	edLoad( pOwnSect_, false );
}

bool EditorChunkUserDataObject::edLoad( const std::string type, DataSectionPtr pSection, std::string* errorString )
{
	if ( pDict_ != NULL )
		return true; // already initialised

	if ( !initType( type, errorString ) )
		return false;

	return edLoad( pSection );
}

bool EditorChunkUserDataObject::edLoad( DataSectionPtr pSection, bool loadTransform )
{
	// get rid of any current state
	Py_XDECREF( pDict_ );
	pDict_ = NULL;

	if (!pType_)
	{
		return false;
	}

	//We dont need to read the domain section, but it should be there
	//MF_ASSERT( pSection->readInt("Domain", -1) != -1 );
	if (loadTransform || !transformLoaded_)
	{
		transform_ = pSection->readMatrix34( "transform", Matrix::identity );
		transformLoaded_ = true;
	}

	/* Read in the GUID that is this UserDataObject's guid */

	std::string idStr = pSection->readString( "guid" );
	if (!idStr.empty()){
		guid_ = UniqueID(idStr);
	} else{
		guid_ = UniqueID::generate();
	}
	chunkItemLinker()->guid(guid_);

	// read item properties (also from parents)

	pDict_ = PyDict_New();
	DataSectionPtr propertiesSection = pSection->openSection( "properties" );
	std::vector<bool> usingDefault( pType_->propertyCount(), true );
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		DataSectionPtr	pSubSection;

		PyObjectPtr pValue = NULL;

		// Can we get it from the DataSection?
		if (propertiesSection && (pSubSection = propertiesSection->openSection( pDD->name() )))
		{
			// TODO: support for UserDataType
			pValue = pDD->createFromSection( pSubSection );
			PyErr_Clear();
		}

		// ok, resort to the default then
		usingDefault[i] = ( !pValue );
		if (!pValue)
		{
			pValue = pDD->pInitialValue();
			if ( PySequence_Check( pValue.getObject() ) )
			{
				// Can't use initial value for sequences/arrays because
				// pInitialValue is a shared object, and WE's arrays need a
				// new object it can modify (i.e. add array elements)
				pValue = pDD->dataType()->pDefaultValue();
			}
			else
			{
				// Using the shared pInitialValue, so increment refcount
				Py_INCREF( &*pValue );
			}
		}

		PyDict_SetItemString( pDict_,
			const_cast<char*>( pDD->name().c_str() ), &*pValue );
	}

	// Load in the back links
	chunkItemLinker()->loadBackLinks(pSection);

	// record the links to other models
	recordBindingProps();

	// find the correct model
	markModelDirty();

	// find the reference to the editor python class
	std::string className = pType_->name();
	pyClass_ = EditorUserDataObjectType::instance().getPyClass( className );

	propHelper_.init(
		this,
		const_cast<UserDataObjectDescription*>( pType_ ),
		pDict_,
		new BWFunctor1<EditorChunkUserDataObject, int>(
			this, &EditorChunkUserDataObject::propertyChangedCallback ) );

	propHelper_.propUsingDefaults( usingDefault );

	return true;
}

void EditorChunkUserDataObject::clearProperties()
{
	propHelper_.clearProperties();
}

void EditorChunkUserDataObject::clearEditProps()
{
	propHelper_.clearEditProps( allowEdit_ );
}

void EditorChunkUserDataObject::setEditProps( const std::list< std::string > & names )
{
	propHelper_.setEditProps( names, allowEdit_ );
}


/**
 *	Save any property changes to this data section
 */
void EditorChunkUserDataObject::clearPropertySection()
{
	pType_ = NULL;

	if (!pOwnSect())
		return;

	propHelper_.clearPropertySection( pOwnSect() );
}


class UserDataType;

bool EditorChunkUserDataObject::edSave( DataSectionPtr pSection )
{
	MF_ASSERT( pSection );
	MF_ASSERT( pChunk_ );

	if (!edCommonSave( pSection ))
		return false;

	// write the easy ones
	pSection->delChild( "guid" );
	pSection->writeString( "type", pType_->name() );
	pSection->writeInt( "Domain", pType_->domain() );
	pSection->writeMatrix34( "transform", transform_ );
	pSection->writeString( "guid", guid_.toString() );

	if (pType_ == NULL)
	{
		ERROR_MSG( "EditorChunkUserDataObject::edSave - no properties for udo!\n" );
	}

	DataSectionPtr propertiesSection = pSection->openSection( "properties", true );
	propertiesSection->delChildren();

	// now write the properties from the dictionary
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		PyObject * pValue = PyDict_GetItemString( pDict_,
			const_cast<char*>( pDD->name().c_str() ) );
		if (!pValue)
		{
			PyErr_Print();
			ERROR_MSG( "EditorChunkUserDataObject::edSave: Failed to get prop %s\n",
				pDD->name().c_str() );
			continue;
		}

		if ( propHelper_.propUsingDefault( i ) )
		{
			propertiesSection->delChild( pDD->name() );
		}
		else
		{
			DataSectionPtr pDS = propertiesSection->openSection( pDD->name(), true );
			pDD->dataType()->addToSection( pValue, pDS );
		}
	}

	// Delete backLinks sectionClear the links
	chunkItemLinker()->saveBackLinks(pSection);

	return true;
}


/**
 *  This is called when the udo is removed from, or added to a Chunk.  Here
 *  we delete the link, it gets recreated if necessary later on, and call the
 *  base class.
 */
/*virtual*/ void EditorChunkUserDataObject::toss( Chunk * pChunk )
{
	// Remove references to links, i.e. break cyclic shared pointer use:
    if (pChunk)
	{
		// Call the base method first
		EditorChunkSubstance<ChunkItem>::toss( pChunk );

		// Add links for this nodes back links
		chunkItemLinker()->tossAdd();
	}
	else
	{
		// Add links for this nodes back links
		chunkItemLinker()->tossRemove();

		// Call the base method last
		EditorChunkSubstance<ChunkItem>::toss( pChunk );
	}
}


void EditorChunkUserDataObject::guid( UniqueID newGUID )
{
	guid_ = newGUID;
	chunkItemLinker()->guid(guid_);
}


/**
 *	Change our transform, temporarily or permanently
 */
bool EditorChunkUserDataObject::edTransform( const Matrix & m, bool transient )
{
	// Update the status of transient_
	transient_ = transient;

	// it's permanent, so find out where we belong now
	Chunk * pOldChunk = pChunk_;
	Chunk * pNewChunk = this->edDropChunk( m.applyToOrigin() );
	if (pNewChunk == NULL) return false;

	// make sure the chunks aren't readonly, and also make sure that all
	// affected chunks are writeable if changing chunks.
	if (!EditorChunkCache::instance( *pOldChunk ).edIsWriteable() ||
		!EditorChunkCache::instance( *pNewChunk ).edIsWriteable() ||
		( pNewChunk != pOldChunk && !chunkItemLinker()->linkedChunksWriteable() ) )
	{
		return false;
	}

	// if this is only a temporary change, keep it in the same chunk
	if (transient)
	{
		transform_ = m;
		// update chunk links
		chunkItemLinker()->updateChunkLinks();
		this->syncInit();
		return true;
	}

	// ok, accept the transform change then
	transform_.multiply( m, pOldChunk->transform() );
	transform_.postMultiply( pNewChunk->transformInverse() );

	// note that both affected chunks have seen changes
	WorldManager::instance().changedChunk( pOldChunk );
	WorldManager::instance().changedChunk( pNewChunk );

	// and move ourselves into the right chunk. we have to do this
	// even if it's the same chunk so the col scene gets recreated
	pOldChunk->delStaticItem( this );
	pNewChunk->addStaticItem( this );

	// update chunk links
	chunkItemLinker()->updateChunkLinks();
	this->syncInit();
	return true;
}


/**
 *	Get a description of this item
 */
std::string EditorChunkUserDataObject::edDescription()
{
	return L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/ED_DESCRIPTION", pType_->name() );
}


/**
 *	Add the properties of this chunk udo to the given editor
 */
bool EditorChunkUserDataObject::edEdit( class ChunkItemEditor & editor )
{
	editor.addProperty( new StaticTextProperty(
		L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/TYPE" ),
		new AccessorDataProxy< EditorChunkUserDataObject, StringProxy >(
			this, "type",
			&EditorChunkUserDataObject::typeGet,
			&EditorChunkUserDataObject::typeSet ) ) );
	editor.addProperty( new StaticTextProperty( "domain",
		new AccessorDataProxy< EditorChunkUserDataObject, StringProxy >(
			this, "domain",
			&EditorChunkUserDataObject::domainGet,
			&EditorChunkUserDataObject::domainSet ) ) );
	editor.addProperty( new StaticTextProperty( "guid",
		new AccessorDataProxy< EditorChunkUserDataObject, StringProxy >(
			this, "guid",
			&EditorChunkUserDataObject::idGet,
			&EditorChunkUserDataObject::idSet ) ) );
	MatrixProxy * pMP = new ChunkItemMatrix( this );
		editor.addProperty( new GenPositionProperty(
		L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/POSITION" ), pMP ) );
	editor.addProperty( new GenRotationProperty(
		L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/DIRECTION" ), pMP ) );
	editor.addProperty( new StaticTextProperty( L( "WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/LAST_SCRIPT_ERROR" ),
		new AccessorDataProxy<EditorChunkUserDataObject,StringProxy>(
			this, "READONLY",
			&EditorChunkUserDataObject::lseGet,
			&EditorChunkUserDataObject::lseSet ) ) );

	return edEditProperties( editor, pMP );
}


/**
 *	This method returns false if this UDO is linked to UDO(s) in chunk(s)
 *	that is not locked for writing.
 *
 *  @return		true if this UDO can be deleted, false otherwise.
 */
/*virtual*/ bool EditorChunkUserDataObject::edCanDelete()
{
	if ( !chunkItemLinker()->linkedChunksWriteable() )
	{
		ERROR_MSG( "Can't delete this User Object (guid %s) because it's linked to chunks that are not locked for writing.\n",
			guid().toString().c_str() );
		return false;
	}
	return true;
}


/*virtual*/ void EditorChunkUserDataObject::edPreDelete()
{
	// Let linker know that we are being deleted.
	onDelete();
	chunkItemLinker()->deleted();
	EditorChunkItem::edPreDelete();
}


bool EditorChunkUserDataObject::edEditProperties( class ChunkItemEditor & editor, MatrixProxy * pMP )
{
	recordBindingProps();

	if (pType_ == NULL)
	{
		ERROR_MSG( "EditorChunkUserDataObject::edEdit - no properties for udo!\n" );
		return false;
	}

	bool hasActions = false;

	// this flag is used to make the first link always show by default
	firstLinkFound_ = false;

	// ok, now finally add in all the udo properties!
	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;
		//TODO: this seems to return null except for patrol paths
		//TODO: create editor property needs to be fixed to work with
		//say chunk items and have each createEditorProperty cast to the correct type
     	GeneralProperty* prop = pDD->dataType()->createEditorProperty(
			pDD->name(), this, i );

		if (prop)
		{
			editor.addProperty( prop );
			continue;
		}

		EntityPropertyParserPtr parser = EntityPropertyParser::create(
			pyClass_, pDD->name(), pDD->dataType(), pDD->widget() );
		if ( parser )
		{
			prop = parser->createProperty(
				&propHelper_, i, pDD->name(), pDD->dataType(), pDD->widget(), pMP );
		}
		if ( prop )
		{
			editor.addProperty( prop );
		}
		else
		{
			// TODO: Should probably make this read-only. It may not work if the
			// Python object does not have a repr that can be eval'ed.
			// treat everything else as a generic Python property
			editor.addProperty( new PythonProperty( pDD->name(),
				new EntityPythonProxy( &propHelper_, i ) ) );
		}
	}

	return true;
}


std::vector<std::string> EditorChunkUserDataObject::edCommand( const std::string& path ) const
{
	EditorChunkUserDataObject *myself =
		const_cast<EditorChunkUserDataObject *>(this);

	return myself->propHelper()->command();
}


bool EditorChunkUserDataObject::edExecuteCommand( const std::string& path, std::vector<std::string>::size_type index )
{
	PropertyIndex pi = propHelper()->commandIndex( index );

	DataDescription* pDD = propHelper()->pType()->property( pi.valueAt( 0 ) );
	if (!pDD->editable())
		return false;

	bool link = propHelper()->isUserDataObjectLink( pi.valueAt(0) );
	bool arrayLink = propHelper()->isUserDataObjectLinkArray( pi.valueAt(0) );
	if ( link || ( arrayLink && pi.count() > 1 ) )
	{
		PyObjectPtr ob( propHelper()->propGetPy( pi ), PyObjectPtr::STEAL_REFERENCE );
		std::string uniqueId = PyString_AsString( PyTuple_GetItem( ob.getObject(), 0 ) );
		std::string chunkId = PyString_AsString( PyTuple_GetItem( ob.getObject(), 1 ) );

		EditorChunkItemLinkable* pEcil = WorldManager::instance().linkerManager().forceLoad( uniqueId, chunkId );
		if ( pEcil &&
			EditorChunkCache::instance( *(pEcil->chunkItem()->chunk()) ).edIsWriteable() )
		{
			WorldManager::instance().linkerManager().deleteLink( chunkItemLinker(), pEcil, pi );

			propHelper()->resetSelUpdate( true );
			// Refresh item
			propHelper()->refreshItem();

			if ( link )
				UndoRedo::instance().barrier( L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_CHUNK_USER_DATA_OBJECT/UNDO_DEL_ITEM"), false );
			else
				UndoRedo::instance().barrier( L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_ENTITY_PROXY/UNDO_DEL_ARRAY_ITEM"), false );
		}
		else
			ERROR_MSG( "Could not delete link %s as the link does not exist or the chunk %s is locked\n",
			uniqueId.c_str(), chunkId.c_str() );
	}
	else if ( arrayLink )
	{
		PyObjectPtr ob( propHelper()->propGetPy( pi ), PyObjectPtr::STEAL_REFERENCE );

		SequenceDataType* dataType =
			static_cast<SequenceDataType*>( pDD->dataType() );
		ArrayPropertiesHelper propArray;
		propArray.init( this, &(dataType->getElemType()), ob.getObject());

		// Iterate through the array of links
		for(int j = 0; j < propArray.propCount(); j++)
		{
			PyObjectPtr link( propArray.propGetPy( j ), PyObjectPtr::STEAL_REFERENCE );
			std::string uniqueId = PyString_AsString( PyTuple_GetItem( link.getObject(), 0 ) );
			std::string chunkId = PyString_AsString( PyTuple_GetItem( link.getObject(), 1 ) );

			EditorChunkItemLinkable* pEcil = WorldManager::instance().linkerManager().forceLoad( uniqueId, chunkId );
			if ( pEcil &&
				EditorChunkCache::instance( *(pEcil->chunkItem()->chunk()) ).edIsWriteable() )
			{
				PropertyIndex piArray( pi.valueAt( 0 ) );
				piArray.append( j-- );
				WorldManager::instance().linkerManager().deleteLink( chunkItemLinker(), pEcil, piArray );
			}
			else
				ERROR_MSG( "Could not delete link %s as the link does not exist or the chunk %s is locked\n",
					uniqueId.c_str(), chunkId.c_str() );

		}
		propHelper()->resetSelUpdate( true );
		// Refresh item
		propHelper()->refreshItem();

		UndoRedo::instance().barrier( L("WORLDEDITOR/WORLDEDITOR/CHUNK/EDITOR_ENTITY_PROXY/UNDO_CLEAR_ARRAY"), false );
	}

	return true;
}


bool EditorChunkUserDataObject::edShouldDraw()
{
	if (ChunkItem::edShouldDraw() &&
		!!Options::getOptionInt( "render/gameObjects", 1 ) &&
		!!Options::getOptionInt( "render/gameObjects/drawUserDataObjects", 1 ))
		return true;
	else
		return false;
}


void EditorChunkUserDataObject::propertyChangedCallback( int index )
{
	markModelDirty();
}


void EditorChunkUserDataObject::recordBindingProps()
{
	bindingProps_.clear();

	if (pType_ == NULL)
		return;

	for (uint i=0; i < pType_->propertyCount(); i++)
	{
		DataDescription * pDD = pType_->property(i);

		if (!pDD->editable())
			continue;

		if (pDD->dataType()->typeName().find( "ARRAY:" ) != std::string::npos)
		{
			PyObjectPtr actions( propHelper_.propGetPy(i), PyObjectPtr::STEAL_REFERENCE );

			const int listSize = PyList_Size( actions.getObject() );
			for (int listIndex = 0; listIndex < listSize; listIndex++)
			{
				PyObject * action = PyList_GetItem( actions.getObject(), listIndex );

				PyInstanceObject * pInst = (PyInstanceObject *)(action);
				PyClassObject * pClass = (PyClassObject *)(pInst->in_class);
				std::string userClassName = PyString_AsString( pClass->cl_name );

				bool argsAdded = false;
				PyObject * dict = PyObject_GetAttrString( action, "ARGS" );
				MF_ASSERT( dict );
				PyObject * items = PyDict_Items( dict );
				Py_XDECREF( dict );

				for (int j = 0; j < PyList_Size( items ); j++)
				{
					PyObject * item = PyList_GetItem( items, j );
					std::string argName = PyString_AsString( PyTuple_GetItem( item, 0 ) );
					std::string argType = PyString_AsString( PyTuple_GetItem( item, 1 ) );

					if (argType == "udo_ID")
						bindingProps_.push_back( BindingProperty(action, argName) );
				}
				Py_XDECREF( items );
			}
		}
	}
}



void EditorChunkUserDataObject::draw()
{
	if (edShouldDraw())
	{
		EditorChunkSubstance<ChunkItem>::draw();
	}
}


/*virtual*/ void EditorChunkUserDataObject::tick(float dtime)
{
    EditorChunkSubstance<ChunkItem>::tick(dtime);
}


std::string EditorChunkUserDataObject::typeGet() const
{
	return pType_->name();
}

std::string EditorChunkUserDataObject::domainGet() const
{
	UserDataObjectDescription::UserDataObjectDomain domain= pType_->domain();
	//Convert from enum to string
	if (domain == UserDataObjectDescription::CLIENT){
		return "CLIENT";
	}else if (domain == UserDataObjectDescription::CELL){
		return "CELL";
	} else if (domain == UserDataObjectDescription::BASE){
		return "BASE";
	}else{
		MF_ASSERT(false && "Error - udo has invalid domain value");
		return "ERROR";
	}

}

std::string EditorChunkUserDataObject::idGet() const
{
	return guid_.toString();
}


void EditorChunkUserDataObject::calculateModel()
{
	SimpleMutexHolder grabTry(s_loadingModelMutex_);

	// If the model is loading in the background but has not completed then
	// this is a request to change the model to something else.  The best we
	// can do is wait for the model to finish loading and then reissue another
	// background loading request.
	waitDoneLoading();

	modelToLoad_ = s_defaultModel;

	if (pyClass_)
	{
		// The tuple will dec this; we don't want it to
		Py_XINCREF( pDict_ );

		PyObject * args = PyTuple_New( 1 );
		PyTuple_SetItem( args, 0, pDict_ );

		PyObject * result = Script::ask(
			PyObject_GetAttrString( pyClass_, "modelName" ),
			args,
			"EditorChunkUserDataObject::calculateModel: ",
			true /* ok if function NULL */);

		if (result)
		{
			if ( PyString_Check( result ) )
				modelToLoad_ = PyString_AsString( result );

			if ( modelToLoad_.empty() )
				modelToLoad_ = s_defaultModel;

			if ( modelToLoad_.length() < 7  ||
					modelToLoad_.substr( modelToLoad_.length() - 6 ) != ".model" )
				modelToLoad_ += ".model";
		}
	}

	incRef(); // Don't delete until the loading is done!

	loadBackgroundTask_ =
		new CStyleBackgroundTask(
			loadModelTask    , this,
			loadModelTaskDone, this );
	BgTaskManager::instance().addBackgroundTask( loadBackgroundTask_ );
}


/**
 *	Get the representative model for this udo
 */
ModelPtr EditorChunkUserDataObject::reprModel() const
{
	return model_;
}

bool EditorChunkUserDataObject::isDefaultModel() const
{
	if (model_)
        return model_->resourceID() == s_defaultModel;

	// else assume it is!
	return true;
}

void EditorChunkUserDataObject::markModelDirty()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	if (std::find(s_dirtyModelEntities_.begin(), s_dirtyModelEntities_.end(), this) == s_dirtyModelEntities_.end())
		s_dirtyModelEntities_.push_back(this);
}

void EditorChunkUserDataObject::removeFromDirtyList()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	std::vector<EditorChunkUserDataObject*>::iterator it =
		std::find(s_dirtyModelEntities_.begin(), s_dirtyModelEntities_.end(), this);

	if (it != s_dirtyModelEntities_.end())
		s_dirtyModelEntities_.erase( it );
}

void EditorChunkUserDataObject::calculateDirtyModels()
{
	SimpleMutexHolder permission( s_dirtyModelMutex_ );

	uint i = 0;
	while (i < EditorChunkUserDataObject::s_dirtyModelEntities_.size())
	{
		EditorChunkUserDataObject* ent = EditorChunkUserDataObject::s_dirtyModelEntities_[i];

		if (!ent->chunk() || !ent->chunk()->online())
		{
			++i;
		}
		else
		{
			ent->calculateModel();
 			EditorChunkUserDataObject::s_dirtyModelEntities_.erase(EditorChunkUserDataObject::s_dirtyModelEntities_.begin() + i);
		}
	}
}

Vector3 EditorChunkUserDataObject::edMovementDeltaSnaps()
{
	if (pType_->name() == std::string( "Door" ))
	{
		return Vector3( 1.f, 1.f, 1.f );
	}

	return EditorChunkItem::edMovementDeltaSnaps();
}

float EditorChunkUserDataObject::edAngleSnaps()
{
	if (pType_->name() == std::string( "Door" ))
	{
		return 90.f;
	}

	return EditorChunkItem::edAngleSnaps();
}


/*virtual*/ void EditorChunkUserDataObject::edPostClone( EditorChunkItem* srcItem )
{
	if ( !pyClass_ || !srcItem )
		return;

	EditorChunkUserDataObject* other = static_cast<EditorChunkUserDataObject*>( srcItem );

	PyObject* thisInfo = infoDict();
	PyObject* otherInfo = other->infoDict();

	PyObject * result = Script::ask(
		PyObject_GetAttrString( pyClass_, "postClone" ),
		Py_BuildValue( "(OO)", thisInfo, otherInfo ),
		"EditorChunkUserDataObject::postClone: ",
		true /* ok if function NULL */ );

	Py_DECREF( thisInfo );
	Py_DECREF( otherInfo );
	this->syncInit();
}


/*static*/ void EditorChunkUserDataObject::loadModelTask(void *self)
{
	EditorChunkUserDataObject *udo =
		static_cast<EditorChunkUserDataObject *>(self);
	try
	{
		udo->loadingModel_ = Model::get(udo->modelToLoad_);
		if (!udo->loadingModel_)
		{
			ERROR_MSG( "EditorChunkEntity::calculateModel - fail to find model %s\n"
						"Substituting with default model\n",
						udo->modelToLoad_.c_str() );

			udo->loadingModel_ = Model::get( s_defaultModel );
		}
	}
	catch (...)
	{
		ERROR_MSG("EditorChunkUserDataObject::loadModelTask crash in load\n");
	}
}


/*static*/ void EditorChunkUserDataObject::loadModelTaskDone(void *self)
{
	EditorChunkUserDataObject *udo =
		static_cast<EditorChunkUserDataObject *>(self);

	// the variable 'oldModelHolder' will keep the model alive until the method
	// returns, so the ChunkModelObstacle doesn't crash with an BB reference to
	// a deleted object (we should really change that bb referencing)
	ModelPtr oldModelHolder = udo->model_;

	udo->model_ = udo->loadingModel_;
	if (!udo->model_)
	{
		std::string errMsg = L("Unable to load UserDataObject's model %0", udo->modelToLoad_);
		ERROR_MSG(errMsg.c_str());
	}

	// Update the collision scene
	Chunk* c = udo->chunk();
	if (c != NULL)
	{
		// don't let the ref count go to 0 in the following commands
		ChunkItemPtr ourself = udo;

		c->delStaticItem( udo );
		c->addStaticItem( udo );
		udo->syncInit();
	}

	// Cleanup some unused memory:
	udo->loadingModel_ = NULL;
	udo->modelToLoad_.clear();
	udo->loadBackgroundTask_ = NULL;
	udo->decRef(); // Entity can be deleted now if necessary
}


bool EditorChunkUserDataObject::loading() const
{
	return loadBackgroundTask_ != NULL;
}


void EditorChunkUserDataObject::syncInit()
{
	#if UMBRA_ENABLE
	/* We need to clear the model here
	   as UDOs can change model
	 */	
	pUmbraModel_ = NULL;
	pUmbraObject_ = NULL;
	if (!this->reprModel())
	{	
		return;
	}
	BoundingBox bb = BoundingBox::s_insideOut_;
	// Grab the visibility bounding box
	bb = this->reprModel()->visibilityBox();	
	if (!pUmbraObject_.hasObject())
	{
		pUmbraModel_ = UmbraModelProxy::getObbModel( bb.minBounds(), bb.maxBounds() );
		pUmbraObject_ = UmbraObjectProxy::get( pUmbraModel_ );
	}
	
	// Set the user pointer up to point at this chunk item
	pUmbraObject_->object()->setUserPointer( (void*)this );

	// Set up object transforms
	Matrix m = pChunk_->transform();
	m.preMultiply( this->transform_ );
	pUmbraObject_->object()->setObjectToCellMatrix( (Umbra::Matrix4x4&)m );
	pUmbraObject_->object()->setCell( pChunk_->getUmbraCell() );
	#endif
}

void EditorChunkUserDataObject::waitDoneLoading()
{
	while (loading())
	{
		::Sleep(20);
		BgTaskManager::instance().tick();
	}
}


/// Write the factory statics stuff
#undef IMPLEMENT_CHUNK_ITEM_ARGS
#define IMPLEMENT_CHUNK_ITEM_ARGS (pSection, pChunk, &errorString)
IMPLEMENT_CHUNK_ITEM( EditorChunkUserDataObject, UserDataObject, 1 )

// editor_chunk_udo.cpp
