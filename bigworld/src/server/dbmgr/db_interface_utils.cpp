/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "db_interface_utils.hpp"

#include "cstdmf/debug.hpp"
#include "db_interface.hpp"

DECLARE_DEBUG_COMPONENT(0)

namespace DBInterfaceUtils
{
	/*~ function BigWorld.executeRawDatabaseCommand
	 *	@components{ base }
	 *	This script function executes the given raw database command on the
	 *	database. There is no attempt to parse the command - it is passed
	 * 	straight through to the underlying database. The interpretation of the
	 *	command is left solely up to the specific database interface layer.
	 *	i.e. an xml database will expect different commands (currently a Python
	 *	string to execute with the database datasection in a global) to those
	 *	expected by a mysql database (currently raw SQL commands). The return
	 *	values of the commands will differ similarly (the stdout/the sql result).
	 *
	 *  Please note that the entity data in the database may not be up to date,
	 * 	especially when secondary databases are enabled. It is highly
	 *  recommended that this function is not used to read or modify entity
	 * 	data.
	 *
	 *	@param command The command string specific to the present database layer.
	 *  For XML database it should be Python codes.
	 *  For MySQL database it is an SQL query.
	 *	@param callback The object to call back (e.g. a function) with the result
	 *	of the command execution. The callback will be called with 3
	 * 	parameters: result set, number of affected rows and error string.
	 *
	 * 	The result set parameter is a list of rows. Each row is a list of
	 * 	strings containing field values. The XML database will always return
	 * 	a result set with 1 row and 1 column containing the return code of the
	 * 	command. The result set will be None for commands to do not return a
	 * 	result set e.g. DELETE, or if there was an error in executing the
	 * 	command.
	 *
	 * 	The number of a affected rows parameter is a number indicating the
	 * 	number of rows affected by the command. This parameter is only relevant
	 * 	for commands to do not return a result set e.g. DELETE. This parameter
	 * 	is None for commands that do return a result set or if there was and
	 * 	error in executing the command.
	 *
	 * 	The error string parameter is a string describing the error that
	 * 	occurred if there was an error in executing the command. This parameter
	 * 	is None if there was no error in executing the command.
	 */
	/**
	 * 	NOTE: The comment block below is a copy of the comment block above with
	 * 	an additional note for cell entity callbacks.
	 */
	/*~ function BigWorld.executeRawDatabaseCommand
	 *	@components{ cell }
	 *	This script function executes the given raw database command on the
	 *	database. There is no attempt to parse the command - it is passed
	 * 	straight through to the underlying database. The interpretation of the
	 *	command is left solely up to the specific database interface layer.
	 *	i.e. an xml database will expect different commands (currently a Python
	 *	string to execute with the database datasection in a global) to those
	 *	expected by a mysql database (currently raw SQL commands). The return
	 *	values of the commands will differ similarly (the stdout/the sql result).
	 *
	 *	@param command The command string specific to the present database layer.
	 *  For XML database it should be Python codes.
	 *  For MySQL database it is an SQL query.
	 *	@param callback The object to call back (e.g. a function) with the result
	 *	of the command execution. The callback will be called with 3
	 * 	parameters: result set, number of affected rows and error string.
	 *
	 * 	The result set parameter is a list of rows. Each row is a list of
	 * 	strings containing field values. The XML database will always return
	 * 	a result set with 1 row and 1 column containing the return code of the
	 * 	command. The result set will be None for commands to do not return a
	 * 	result set e.g. DELETE, or if there was an error in executing the
	 * 	command.
	 *
	 * 	The number of a affected rows parameter is a number indicating the
	 * 	number of rows affected by the command. This parameter is only relevant
	 * 	for commands to do not return a result set e.g. DELETE. This parameter
	 * 	is None for commands that do return a result set or if there was and
	 * 	error in executing the command.
	 *
	 * 	The error string parameter is a string describing the error that
	 * 	occurred if there was an error in executing the command. This parameter
	 * 	is None if there was no error in executing the command.
	 *
	 * 	Please note that due to the itinerant nature of cell entities, the
	 * 	callback function should handle cases where the cell
	 * 	entity has been converted to a ghost or has been destroyed. In general,
	 * 	the callback function can be a simple function that calls an entity
	 * 	method defined in the entity definition file to do the actual work.
	 * 	This way, the call will be forwarded to the real entity if the current
	 * 	entity has been converted to a ghost.
	 */
	/**
	 *	This class handles the response from DbMgr to an executeRawCommand
	 * 	request.
	 */
	class ExecRawDBCmdWaiter : public Mercury::ReplyMessageHandler
	{
	public:
		ExecRawDBCmdWaiter( PyObjectPtr pResultHandler ) :
			pResultHandler_( pResultHandler.getObject() )
		{
			Py_XINCREF( pResultHandler_ );
		}

		// Mercury::ReplyMessageHandler overrides
		virtual void handleMessage(const Mercury::Address& source,
			Mercury::UnpackedMessageHeader& header,
			BinaryIStream& data, void * arg)
		{
			TRACE_MSG( "ExecRawDBCmdWaiter::handleMessage: "
				"DB call response received\n" );

			this->processTabularResult( data );
		}

		void processTabularResult( BinaryIStream& data )
		{
			PyObject* pResultSet;
			PyObject* pAffectedRows;
			PyObject* pErrorMsg;

			std::string errorMsg;
			data >> errorMsg;

			if (errorMsg.empty())
			{
				pErrorMsg = this->newPyNone();

				uint32 numColumns;
				data >> numColumns;

				if (numColumns > 0)
				{	// Command returned tabular data.
					pAffectedRows = this->newPyNone();

					uint32 numRows;
					data >> numRows;
					// Make list of list of strings.
					pResultSet = PyList_New( numRows );
					for ( uint32 i = 0; i < numRows; ++i )
					{
						PyObject* pRow = PyList_New( numColumns );
						for ( uint32 j = 0; j < numColumns; ++j )
						{
							Blob cell = getPotentialNullBlobFromStream( data );
							PyObject* pCell = (cell.isNull()) ?
									this->newPyNone() :
									PyString_FromStringAndSize( cell.pBlob,
												cell.length );
							PyList_SET_ITEM( pRow, j, pCell );
						}
						PyList_SET_ITEM( pResultSet, i, pRow );
					}
				}
				else
				{	// Empty result set - only affected rows returned.
					uint64	numAffectedRows;
					data >> numAffectedRows;

					pResultSet = this->newPyNone();
					pAffectedRows = Script::getData( numAffectedRows );
					pErrorMsg = this->newPyNone();
				}
			}
			else	// Error has occurred.
			{
				pResultSet = this->newPyNone();
				pAffectedRows = this->newPyNone();
				pErrorMsg = Script::getData( errorMsg );
			}

			this->done( pResultSet, pAffectedRows, pErrorMsg );
		}

		void handleException(const Mercury::NubException& exception, void* arg)
		{
			// This can be called during Channel destruction which can happen
			// after Script has been finalised.
			if (!Script::isFinalised())
			{
				std::stringstream errorStrm;
				errorStrm << "Nub exception " <<
						Mercury::reasonToString( exception.reason() );
				ERROR_MSG( "ExecRawDBCmdWaiter::handleException: %s\n",
						errorStrm.str().c_str() );
				this->done( this->newPyNone(), this->newPyNone(),
						Script::getData( errorStrm.str() ) );
			}
		}

	private:
		void done( PyObject * resultSet, PyObject * affectedRows,
				PyObject * errorMsg )
		{
			if (pResultHandler_)
			{
				Script::call( pResultHandler_,
					Py_BuildValue( "(OOO)", resultSet, affectedRows, errorMsg ),
					"ExecRawDBCmdWaiter callback", /*okIfFnNull:*/false );
				// 'call' does the decref of pResultHandler_ for us
			}

			Py_DECREF( resultSet );
			Py_DECREF( affectedRows );
			Py_DECREF( errorMsg );

			delete this;
		}

		static PyObject* newPyNone()
		{
			PyObject* pNone = Py_None;
			Py_INCREF( pNone );
			return pNone;
		}

		PyObject* 				pResultHandler_;
	};

	/**
	 *	This function sends a message to the DbMgr to an run an
	 * 	executeRawDatabaseCommand request. When the result is sent back from
	 * 	DbMgr, pResultHandler will be called if specified.
	 */
	bool executeRawDatabaseCommand( const std::string & command,
			PyObjectPtr pResultHandler, Mercury::Channel & channel )
	{
		if (pResultHandler && !PyCallable_Check( &*pResultHandler) )
		{
			PyErr_SetString( PyExc_TypeError,
				"BigWorld.executeRawDatabaseCommand() "
				"callback must be callable if specified" );
			return false;
		}

		Mercury::Bundle & bundle = channel.bundle();
		bundle.startRequest( DBInterface::executeRawCommand,
			new ExecRawDBCmdWaiter( pResultHandler ) );
		bundle.addBlob( command.data(), command.size() );

		channel.send();

		return true;
	}

	/**
	 *	This function sends a message to the DbMgr to an run an
	 * 	executeRawDatabaseCommand request. When the result is sent back from
	 * 	DbMgr, pResultHandler will be called if specified.
	 */
	bool executeRawDatabaseCommand( const std::string & command,
			PyObjectPtr pResultHandler, Mercury::Nub& nub,
			const Mercury::Address& dbMgrAddr )
	{
		if (pResultHandler && !PyCallable_Check( &*pResultHandler) )
		{
			PyErr_SetString( PyExc_TypeError,
				"BigWorld.executeRawDatabaseCommand() "
				"callback must be callable if specified" );
			return false;
		}

		Mercury::ChannelSender sender( nub.findOrCreateChannel( dbMgrAddr ) );
		Mercury::Bundle & bundle = sender.bundle();

		bundle.startRequest( DBInterface::executeRawCommand,
			new ExecRawDBCmdWaiter( pResultHandler ) );
		bundle.addBlob( command.data(), command.size() );

		return true;
	}

	/**
	 *	This function is used to serialise a potentially NULL blob into the
	 * 	stream. Use getPotentialNullBlobFromStream() to retrieve the value from
	 * 	the stream.
	 */
	void addPotentialNullBlobToStream( BinaryOStream& stream, const Blob& blob )
	{
		if (blob.pBlob && blob.length)
		{
			stream.appendString( blob.pBlob, blob.length );
		}
		else	// NULL value or just empty string
		{
			stream.appendString( "", 0 );
			stream << uint8((blob.pBlob) ? 1 : 0);
		}
	}

	/**
	 *	This function is used to deserialise a potentially NULL blob from the
	 * 	stream. If the value in the stream is NULL then Blob.pBlob == NULL is
	 * 	returned. Otherwise, Blob.pBlob points to a location in the stream
	 * 	where the blob starts and Blob.length is set to the length of the blob.
	 */
	Blob getPotentialNullBlobFromStream( BinaryIStream& stream )
	{
		int length = stream.readStringLength();
		if (length > 0)
		{
			return Blob( (char*) stream.retrieve( length ), length );
		}
		else
		{
			uint8 isNotNull;
			stream >> isNotNull;

			return (isNotNull) ? Blob( "", 0 ) : Blob();
		}
	}
}
