/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "mailbox.hpp"
#include "web_integration.hpp"

#include "baseapp/baseapp_int_interface.hpp"
#include "entitydef/method_response.hpp"

#include "cstdmf/base64.h"

DECLARE_DEBUG_COMPONENT( 0 )
#include "autotrace.hpp"




// -----------------------------------------------------------------------------
// Section: WebEntityMailBox
// -----------------------------------------------------------------------------

PY_TYPEOBJECT( WebEntityMailBox )

PY_BEGIN_METHODS( WebEntityMailBox )
	PY_METHOD( serialise )
PY_END_METHODS()

PY_BEGIN_ATTRIBUTES( WebEntityMailBox )
	PY_ATTRIBUTE( id )
	PY_ATTRIBUTE( keepAliveSeconds )
PY_END_ATTRIBUTES()

PY_SCRIPT_CONVERTERS( WebEntityMailBox )

// default keep alive interval for new mailboxes
uint32 WebEntityMailBox::s_defaultKeepAliveSeconds = 0;

/**
 *	Register with the mailbox factory.
 */
void WebEntityMailBox::initMailboxFactory()
{

	PyEntityMailBox::registerMailBoxComponentFactory(
		EntityMailBoxRef::BASE,
		WebEntityMailBox::createFromRef,
		&WebEntityMailBox::s_type_
	);

	PyEntityMailBox::registerMailBoxRefEquivalent(
		WebEntityMailBox::Check, WebEntityMailBox::staticRef );
}


/**
 *	Creates a new base entity mailbox. The reference must be to a remote base
 *	entity.
 *
 *	@param	ref entity mailbox reference to a remote base entity
 *	@return a new web entity mailbox
 */
PyObject * WebEntityMailBox::createFromRef( const EntityMailBoxRef & ref )
{
	return new WebEntityMailBox( ref );
}

/**
 *	Constructor
 *
 *	@param	ref	the entity mailbox reference to the remote entity
 */
WebEntityMailBox::WebEntityMailBox( const EntityMailBoxRef& ref ):
		PyEntityMailBox( &WebEntityMailBox::s_type_ ),
		ref_( ref ),
		pBundle_( NULL ),
		pHandler_( NULL ),
		keepAliveSeconds_( s_defaultKeepAliveSeconds )
{
	this->sendKeepAlive();
}


/**
 *	Sends the keep-alive message with the configured keep-alive interval
 *	to the base.
 */
void WebEntityMailBox::sendKeepAlive()
{

	if (!keepAliveSeconds_)
	{
		return;
	}

	// Send keep alive with the configured keep alive interval period
	Mercury::Bundle * pBundle = new Mercury::Bundle();

	Mercury::Bundle & b = *pBundle;

	b.startMessage( BaseAppIntInterface::setClient );
	b << ( EntityID ) ref_.id;

	BaseAppIntInterface::startKeepAliveArgs & startKeepAliveArgs =
		BaseAppIntInterface::startKeepAliveArgs::start( b );
	startKeepAliveArgs.interval = keepAliveSeconds_;

	try
	{
		WebIntegration::instance().nub().send( ref_.addr, b );
		WebIntegration::instance().nub().processUntilChannelsEmpty();
	}
	catch (Mercury::NubException & e)
	{
		ERROR_MSG( "WebEntityMailBox::sendKeepAlive: failed: %s\n",
			Mercury::reasonToString( e.reason() ) );
	}
}

/**
 *	Destructor.
 */
WebEntityMailBox::~WebEntityMailBox()
{
	if (pBundle_)
		delete pBundle_;
	if (pHandler_)
		delete pHandler_;
}


/**
 *	Overridden from PyEntityMailBox.
 *
 *	@param	methodName
 */
const MethodDescription* WebEntityMailBox::findMethod(
	const char* methodName ) const
{
	EntityDescriptionMap & entities =
		WebIntegration::instance().entityDescriptions();

	const EntityDescription& entity =
		entities.entityDescription( ref_.type() );
	const EntityDescription::Methods& baseMethods = entity.base();
	MethodDescription* pMethod = baseMethods.find( std::string( methodName ) );

	return pMethod;
}


/**
 *	Overridden from PyEntityMailBox. Returns an initialised stream particular
 *	to a remote method.
 *
 *	@param	methodDesc the method description
 */
BinaryOStream* WebEntityMailBox::getStream( const MethodDescription& methodDesc )
{
	if (pBundle_)
	{
		delete pBundle_;
	}
	pBundle_ = new Mercury::Bundle();

	Mercury::Bundle & b = *pBundle_;

	b.startMessage( BaseAppIntInterface::setClient );
	b << ( EntityID ) ref_.id;

	TRACE_MSG( "WebEntityMailBox( %d )::getStream( %s )\n", ref_.id,
		methodDesc.name().c_str() );

	// Send keep alive with the configured keep alive interval period
	if (keepAliveSeconds_)
	{
		BaseAppIntInterface::startKeepAliveArgs & startKeepAliveArgs =
			BaseAppIntInterface::startKeepAliveArgs::start( b );
		startKeepAliveArgs.interval = keepAliveSeconds_;
	}

	if (pHandler_)
	{
		delete pHandler_;
		pHandler_ = NULL;
	}

	if (methodDesc.returnValues() > 0)
	{
		TRACE_MSG( "WebEntityMailBox( %d )::getStream: "
			"num method return values = %u\n", ref_.id,
				methodDesc.returnValues() );

		pHandler_ = new BlockingResponseHandler( methodDesc,
			WebIntegration::instance().nub() );

		b.startRequest( BaseAppIntInterface::callBaseMethod, pHandler_ );
	}
	else
	{
		TRACE_MSG( "WebEntityMailBox( %d )::getStream: no return values\n",
			ref_.id );
		b.startMessage( BaseAppIntInterface::callBaseMethod );
	}
	b << uint16( methodDesc.internalIndex() );

	return &b;
}


/**
 *	Overridden from PyEntityMailBox.
 */
void WebEntityMailBox::sendStream()
{
	if (!pBundle_)
	{
		ERROR_MSG( "WebEntityMailBox::sendStream: No stream to send!\n" );
		return;
	}

	Mercury::Nub & nub = WebIntegration::instance().nub();

	std::string exceptionDescString = "sending method stream";
	try
	{
		nub.send( ref_.addr, *pBundle_ );
		exceptionDescString = "processing pending events";

		if (pHandler_)
		{
			pHandler_->await();
		}
		nub.processUntilChannelsEmpty();
	}
	catch (Mercury::NubException & ne)
	{
		Mercury::Address exceptionAddr;
		if (ne.getAddress( exceptionAddr ))
		{
			ERROR_MSG( "WebEntityMailBox::sendStream: "
					"exception while %s: %s from %s\n",
				Mercury::reasonToString( ne.reason() ),
				exceptionDescString.c_str(),
				exceptionAddr.c_str() );
		}
		else
		{
			ERROR_MSG( "WebEntityMailBox::sendStream: "
					"exception while %s: %s\n",
				exceptionDescString.c_str(),
				Mercury::reasonToString( ne.reason() ) );
		}
	}

	delete pBundle_;
	pBundle_ = NULL;


}


/**
 *	Overridden from PyEntityMailBox.
 */
PyObject * WebEntityMailBox::returnValue()
{
	if (!pHandler_) Py_Return;

	PyObject* dict = pHandler_->getDict();
	if (dict != NULL)
	{
		Py_INCREF( dict );

		return pHandler_->getDict();
	}
	else
	{
		PyErr_SetString( PyExc_RuntimeError, "No return value received" );
		return NULL;
	}
}


/**
 *	Overridden from PyEntityMailBox.
 */
PyObject * WebEntityMailBox::pyRepr()
{
	EntityDescriptionMap & entityMap =
		WebIntegration::instance().entityDescriptions();
	const EntityDescription& entityDesc =
		entityMap.entityDescription( ref_.type() );

	const char * location = "???";
	switch (ref_.component())
	{
		case EntityMailBoxRef::CELL:
			location = "Cell";
		break;
		case EntityMailBoxRef::BASE:
			location = "Base";
		break;
		case EntityMailBoxRef::CLIENT:
			location = "Client";
		break;
		case EntityMailBoxRef::BASE_VIA_CELL:
			location = "BaseViaCell";
		break;
		case EntityMailBoxRef::CLIENT_VIA_CELL:
			location = "ClientViaCell";
		break;
		case EntityMailBoxRef::CELL_VIA_BASE:
			location = "CellViaBase";
		break;
		case EntityMailBoxRef::CLIENT_VIA_BASE:
			location = "ClientViaBase";
		break;

		default: break;
	}

	return PyString_FromFormat( "%s mailbox id: %d type: %s[%u] addr: %s",
		location, ref_.id,
		entityDesc.name().c_str(), ref_.type(), ref_.addr.c_str() );

}


/**
 *	Returns the entity mailbox reference for this mailbox.
 */
EntityMailBoxRef WebEntityMailBox::ref() const
{
	return ref_;
}

/**
 *	PyObjectPlus's pyGetAttribute method.
 */
PyObject * WebEntityMailBox::pyGetAttribute( const char * attr )
{
	// Note: This is basically the same as the implementation present in
	// PyObjectPlus It uses the PY_SETATTR_STD macro which iterates through
	// s_attributes_, a class static instance which is used with
	// PY_RW_ATTRIBUTE_* and PY_RO_ATTRIBUTE_* macros.
	//
	// Calling PyObjectPlus's pyGetAttribute is not enough to get the
	// attributes we want, as it will be using PyObjectPlus's s_attributes_
	// instead of WebEntityMailBox's.

	PY_GETATTR_STD();

	return PyEntityMailBox::pyGetAttribute( attr );
}

/**
 *	PyObjectPlus's pySetAttribute method.
 */
int WebEntityMailBox::pySetAttribute( const char * attr, PyObject * value )
{
	// See note in pyGetAttribute above.
	PY_SETATTR_STD();

	return PyEntityMailBox::pySetAttribute( attr, value );
}

/**
 *	Set the keep-alive interval.
 *
 *	@param value interval in seconds.
 */
void WebEntityMailBox::keepAliveSeconds( uint32 value )
{
	keepAliveSeconds_ = value;
	this->sendKeepAlive();
}


/**
 *	This methods serialises a mailbox's data to a string, so that it can be
 *	recreated with WebEntityMailBox::deserialise(). It actually uses base64
 *	so that it can be safely passed as a null-terminated C-string.
 *
 *	@see WebEntityMailBox::deserialise()
 *	@return	a new reference to a Python string object containing a
 *	serialised mailbox string
 */
PyObject * WebEntityMailBox::serialise()
{
	PyObjectPtr pickleModule( PyImport_ImportModule( "cPickle" ),
		PyObjectPtr::STEAL_REFERENCE );
	if (!pickleModule)
	{
		return NULL;
	}

	MemoryOStream serialised;

	serialised << keepAliveSeconds_ << ref_;

	const char * data = reinterpret_cast<char *>( serialised.data() );
	return PyString_FromString(
		Base64::encode( data, serialised.size() ).c_str() );

}


/**
 *	This method deserialises a serialised mailbox string and recreates a
 *	mailbox object.
 *
 *	@see WebEntityMailBox::serialise()
 *	@return a new reference to the Python mailbox as represented in the
 *	serialised mailbox string , or NULL or the given string was an
 *	invalid serialised mailbox string.
 */
PyObject * WebEntityMailBox::deserialise( const std::string & serialisedB64 )
{
	std::string serialisedData;
	if (-1 == Base64::decode( serialisedB64, serialisedData ) ||
			serialisedData.size() <
				sizeof( uint32 ) + sizeof( EntityMailBoxRef ))
	{
		PyErr_SetString( PyExc_ValueError,
			"invalid mailbox serialised string" );
		return NULL;
	}

	// first 4 bytes is keep alive seconds value
	uint32 keepAliveSeconds = 0;

	// the rest is the mailbox reference
	EntityMailBoxRef ref;

	MemoryIStream serialised( serialisedData.data(), serialisedData.size() );

	serialised >> keepAliveSeconds >> ref;

	// Returns a new reference
	WebEntityMailBox * mailbox = new WebEntityMailBox( ref );
	mailbox->keepAliveSeconds( keepAliveSeconds );

	return mailbox;
}

PY_MODULE_STATIC_METHOD( WebEntityMailBox, deserialise, BigWorld )

// mailbox.cpp
