/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MAILBOX_HPP
#define MAILBOX_HPP


#include "entitydef/entity_description.hpp"
#include "entitydef/mailbox_base.hpp"
#include "network/channel.hpp"
#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"
#include "server/backup_hash.hpp"

#include "entity_type.hpp"


/**
 *	This class implements a mailbox that can send to a server object. This
 *	object may be on a cell or may be a base.
 *
 *	@see CellEntityMailBox
 *	@see BaseEntityMailBox
 */
class ServerEntityMailBox: public PyEntityMailBox
{
	Py_Header( ServerEntityMailBox, PyEntityMailBox )

public:
	ServerEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, EntityID id,
			PyTypePlus * pType = &s_type_ );
	~ServerEntityMailBox();

	virtual PyObject * pyGetAttribute( const char * attr );
	void sendStream();

	const Mercury::Address		address() const		{ return addr_; }
	void address( const Mercury::Address & addr );

	virtual Mercury::Channel & channel() const;
	Mercury::Bundle & bundle() const { return this->channel().bundle(); }

	EntityID					id() const			{ return id_; }

	PY_RO_ATTRIBUTE_DECLARE( this->componentName(), component );
	PY_RO_ATTRIBUTE_DECLARE( pLocalType_->name(), className );
	PY_RO_ATTRIBUTE_DECLARE( id_, id );

	EntityMailBoxRef ref() const;
	virtual EntityMailBoxRef::Component component() const = 0;
	const char * componentName() const;

	static EntityMailBoxRef static_ref( PyObject * pThis )
		{ return ((const ServerEntityMailBox*)pThis)->ref(); }

	static void migrate();
	static void adjustForDeadBaseApp( const Mercury::Address & deadAddr,
			const BackupHash & hash );

protected:
	Mercury::Address			addr_;
	EntityID					id_;

	EntityTypePtr				pLocalType_;

private:
	ServerEntityMailBox * pPrev_;
	ServerEntityMailBox * pNext_;
	static ServerEntityMailBox * s_pHead_;
	static ServerEntityMailBox * s_pTail_;
};

PY_SCRIPT_CONVERTERS_DECLARE( ServerEntityMailBox )

/**
 *	This class is common to all mailboxes that send to the cell entity or via
 *	the cell entity.
 */
class CommonCellEntityMailBox : public ServerEntityMailBox
{
	Py_Header( CommonCellEntityMailBox, ServerEntityMailBox )

public:
	CommonCellEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, EntityID id,
			PyTypePlus * pType = &s_type_ ) :
		ServerEntityMailBox( pBaseType, addr, id, pType )
	{}

	virtual Mercury::Channel & channel() const;

	virtual PyObject * pyGetAttribute( const char * attr );

protected:
	BinaryOStream * getStreamCommon( const MethodDescription & methodDesc,
		   const Mercury::InterfaceElement & ie );

};


/**
 *	This class implements a mailbox that can send to an object on a cell.
 */
class CellEntityMailBox: public CommonCellEntityMailBox
{
	Py_Header( CellEntityMailBox, CommonCellEntityMailBox )

public:
	CellEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, EntityID id,
			PyTypePlus * pType = &s_type_ ) :
		CommonCellEntityMailBox( pBaseType, addr, id, pType )
	{}

	virtual PyObject * pyGetAttribute( const char * attr );

	// Mailbox getter methods and generated setters.
	PyObject * pyGet_base();
	PY_RO_ATTRIBUTE_SET( base )

	PyObject * pyGet_client();
	PY_RO_ATTRIBUTE_SET( client )

	virtual BinaryOStream * getStream( const MethodDescription & methodDesc );

	virtual const MethodDescription * findMethod( const char * attr ) const;
	virtual EntityMailBoxRef::Component component() const;
};

PY_SCRIPT_CONVERTERS_DECLARE( CellEntityMailBox )

typedef SmartPointer<CellEntityMailBox> CellEntityMailBoxPtr;



/**
 *	This class implements a mailbox that can send to a base object.
 */
class BaseEntityMailBox: public ServerEntityMailBox
{
	Py_Header( BaseEntityMailBox, ServerEntityMailBox )

public:
	BaseEntityMailBox( EntityTypePtr pBaseType,
			const Mercury::Address & addr, EntityID id,
			PyTypePlus * pType = &s_type_ ) :
		ServerEntityMailBox( pBaseType, addr, id, pType )
	{}

	virtual PyObject * pyGetAttribute( const char * attr );

	// Mailbox getter methods and generated setters.
	PyObject * pyGet_cell();
	PY_RO_ATTRIBUTE_SET( cell )

	PyObject * pyGet_client();
	PY_RO_ATTRIBUTE_SET( client )

	virtual BinaryOStream * getStream( const MethodDescription & methodDesc );
	virtual const MethodDescription * findMethod( const char * attr ) const;
	virtual EntityMailBoxRef::Component component() const;

//	static PyObject * New( EntityID id, PyObjectPtr pEntityType,
//		uint32 ip, uint16 port );
//	PY_AUTO_FACTORY_DECLARE( BaseEntityMailBox, ARG( EntityID,
//		ARG( PyObjectPtr, ARG( uint32, ARG( uint16, END ) ) ) ) )
};

PY_SCRIPT_CONVERTERS_DECLARE( BaseEntityMailBox )






#endif // MAILBOX_HPP
