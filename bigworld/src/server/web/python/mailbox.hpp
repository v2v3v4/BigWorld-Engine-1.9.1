/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __MAILBOX_HPP__
#define __MAILBOX_HPP__
#include "Python.h"

#include "network/mercury.hpp"
#include "entitydef/mailbox_base.hpp"

class BlockingResponseHandler;

/**
 *	Mailbox to a remote base entity. Method calls on this object block until the
 *	return values are received, if any.
 */
class WebEntityMailBox : public PyEntityMailBox
{
	// Python object declarations
	Py_Header( WebEntityMailBox, PyEntityMailBox )


public:
	WebEntityMailBox( const EntityMailBoxRef & ref );
	virtual ~WebEntityMailBox();

public: // virtuals from PyEntityMailBox
	virtual const MethodDescription* findMethod( const char* ) const;
	virtual BinaryOStream* getStream( const MethodDescription & );
	virtual void sendStream();
	virtual PyObject* returnValue();
	virtual PyObject * pyRepr();
public: // virtuals from PyObjectPlus
	virtual PyObject * pyGetAttribute( const char * attr );
	virtual int pySetAttribute( const char * attr, PyObject * value );

public: // public methods
	static void initMailboxFactory();

	static PyObject * createFromRef( const EntityMailBoxRef & ref );

	EntityMailBoxRef ref() const;

	static EntityMailBoxRef staticRef( PyObject * pThis )
		{ return ( (const WebEntityMailBox* ) pThis )->ref(); }

	static uint32 defaultKeepAliveSeconds()
	{	return s_defaultKeepAliveSeconds; }

	static void defaultKeepAliveSeconds( uint32 newValue )
	{	s_defaultKeepAliveSeconds = newValue; }

	uint32 keepAliveSeconds() { return keepAliveSeconds_; }
	void keepAliveSeconds( uint32 value );

	// Python attributes
	PY_RO_ATTRIBUTE_DECLARE( ref_.id, id );

	PY_RW_ACCESSOR_ATTRIBUTE_DECLARE( uint32, keepAliveSeconds,
		keepAliveSeconds );

	PyObject * serialise();
	PY_AUTO_METHOD_DECLARE( RETOWN, serialise, END )

	static PyObject * deserialise( const std::string & serialised );
	PY_AUTO_MODULE_STATIC_METHOD_DECLARE( RETOWN, deserialise,
		ARG( std::string, END )  )

private:
	void sendKeepAlive();

private:
	EntityMailBoxRef 			ref_;
	Mercury::Bundle* 			pBundle_;
	BlockingResponseHandler* 	pHandler_;
	uint32 						keepAliveSeconds_;

	static uint32				s_defaultKeepAliveSeconds;

};

PY_SCRIPT_CONVERTERS_DECLARE( WebEntityMailBox )



#endif // __MAILBOX_HPP__
