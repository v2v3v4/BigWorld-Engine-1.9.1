/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

//	This file provides the implementation of the MethodResponse class and
// 	the BlockingResponseHandler class.

#ifndef METHOD_RESPONSE_HPP
#define METHOD_RESPONSE_HPP

#include "pyscript/pyobject_plus.hpp"
#include "entitydef/method_description.hpp"
#include "entitydef/data_description.hpp"
#include "entitydef/mailbox_base.hpp"
#include "network/interfaces.hpp"

/**
 *	Helper class for blocking return-value reply message handling.
 *	Puts return values into a dictionary object.
 *
 *	@ingroup entity
 */
class BlockingResponseHandler: public Mercury::ReplyMessageHandler
{
public:
	/**
	 *	Constructor.
	 *	@param methodDesc the method description to be handled
	 *	@param nub the nub to listen for a reply on
	 */
	BlockingResponseHandler(
			const MethodDescription & methodDesc,
			Mercury::Nub & nub ):
		methodDesc_( methodDesc ),
		nub_( nub ),
		returnValueDict_( NULL ),
		done_( false ),
		err_( Mercury::REASON_SUCCESS )
	{}

	/**
	 * Destructor.
	 */
	virtual ~BlockingResponseHandler()
	{
		Py_XDECREF( returnValueDict_ );
	}

	/**
	 *	Returns the dictionary object, or NULL if the reply has not been
	 *	received.
	 */
	PyObject * getDict() { return returnValueDict_; }

	/**
	 *	Blocks until a reply message is received, or an exception occurs.
	 */
	void await();

	/** Returns the error reason. */
	int err() { return err_; }

	/**
	 *	Returns whether the reply has been received, or an error condition has
	 *	occurred.
	 */
	bool done() { return done_; }

public: // from ReplyMessageHandler

	/**
	 *	Handle the reply message for our request.
	 */
	virtual void handleMessage( const Mercury::Address& source,
		Mercury::UnpackedMessageHeader & header,
		BinaryIStream& data,
		void* arg  );

	/**
	 *	Handle an exception.
	 */
	virtual void handleException( const Mercury::NubException& exception,
		void* arg );

private:

	const MethodDescription & 		methodDesc_;
	Mercury::Nub & 					nub_;
	PyObject * 						returnValueDict_;
	bool 							done_;
	int 							err_;
};

/**
 *	Instances of this class are used to supply return values back to remote
 *	callers.  It has Python attributes that correspond to return values
 *	described by
 *	the MethodDescription.
 *
 *	@ingroup entity
 */
class MethodResponse: public PyObjectPlus
{
	Py_Header( MethodResponse, PyObjectPlus )

public:
	MethodResponse( int replyID,
		const Mercury::Address & replyAddr,
		Mercury::Nub & nub,
		const MethodDescription & methodDesc );

	virtual ~MethodResponse();

	int replyID() const { return replyID_; }

	const Mercury::Address& addr() const { return replyAddr_; }

	PyObjectPtr returnValueData( const std::string & name );

	PY_METHOD_DECLARE( py_done )

	PY_RO_ATTRIBUTE_DECLARE( replyID_, id )

public: // from PyObjectPlus
	virtual PyObject * pyGetAttribute( const char * attr );

	virtual int pySetAttribute( const char * attr, PyObject* value );



private:
	typedef std::map<std::string, PyObjectPtr> ReturnValueData;

	int 						replyID_;
	Mercury::Address 			replyAddr_;
	Mercury::Nub & 				nub_;
	const MethodDescription & 	methodDesc_;
	ReturnValueData 			returnValueData_;


};

PY_SCRIPT_CONVERTERS_DECLARE( MethodResponse )

#endif //METHOD_RESPONSE_HPP

// method_response.hpp
