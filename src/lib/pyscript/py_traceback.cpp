/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "script.hpp"
#include "pyobject_plus.hpp"
#include "network/interfaces.hpp"
#include "network/nub.hpp"
#include "resmgr/bwresource.hpp"
#include "resmgr/file_system.hpp"
#include "resmgr/unix_file_system.hpp"
#include "resmgr/multi_file_system.hpp"
#include "python/Include/traceback.h"
#include "python/Include/frameobject.h"
#include <cstring>

namespace Script
{


// A class for reading in a stacktrace, will become asynchronous if IO blocks
class TraceBack : public Mercury::InputNotificationHandler, 
				  public ReferenceCount
{
	const static int BUFFLEN = 0x2000; // 8k

	PyObjectPtr exception_, value_, tbObject_;
	PyObject * stderr_;
	PyTracebackObject *tb_;
	char lineBuf_[BUFFLEN];
	const char * line_;
	int lineBufUpTo_;
	char filename_[BUFFLEN];
	int lineUpTo_;
	int fd_;

	// Set up to read another frame from the backtrace
	void startLine()
	{
		lineUpTo_ = 1;
		lineBufUpTo_ = 0;
		line_ = "Source file not found";
		std::strncpy( filename_, 
					  PyString_AsString( tb_->tb_frame->f_code->co_filename ),
					  BUFFLEN );
		fd_ = open( BWResource::instance().fileSystem()->
						getAbsolutePath( filename_ ).c_str(), 
					O_RDONLY | O_NONBLOCK );
		if (fd_ == -1)
		{
			this->printLine();
		}
		else
		{
			this->handleInputNotification( fd_ );
		}
	}

	// Read in the file context
	virtual int handleInputNotification( int fd )
	{

		MF_ASSERT( fd == fd_ );
		bool isDone = false;

		// Read as much as we can, note: all context is saved from this loop
		while (!isDone)
		{
			// Only append to the current buffer if this is the final line
			// or if we risk an overflow if we don't
			if (lineUpTo_ < tb_->tb_lineno - 1 || lineBufUpTo_ > BUFFLEN / 2)
				lineBufUpTo_ = 0;
			
			// Half fill the buffer
			int output = read(fd_, lineBuf_ + lineBufUpTo_, BUFFLEN / 2);
			
			// IO isn't ready right now, call back later
			if (output == EAGAIN)
			{
				// Make sure is registered with the nub
				s_pNub->registerFileDescriptor( fd_, this );
				return 0;
			}			
			
			// EOF, just forget about it
			if (output <= 0)
			{
				ERROR_MSG( "TraceBack::handleInputNotification: source file "
						   "has %i lines, error on line %i\n",
						   lineUpTo_, tb_->tb_lineno);
				line_ = "";
			    isDone = true;
			}

			int oldLineBufUpTo = lineBufUpTo_;
			// Process what we currently have in the buffer
			while (!isDone && (lineBufUpTo_ < oldLineBufUpTo + output))
			{
				if (lineBuf_[lineBufUpTo_] == 0x0A) // linefeed
				{
					// We have found the line we're after
					if (lineUpTo_ == tb_->tb_lineno)
					{
						lineBuf_[ lineBufUpTo_ ] = '\0';
						isDone = true;
   					}
					else
					{
						lineUpTo_++;
						line_ = lineBuf_ + lineBufUpTo_ + 1;
					}
				}

				lineBufUpTo_++;	
			}
		}

		// Make sure we're not registered with the nub
		s_pNub->deregisterFileDescriptor( fd_ );

		close( fd_ );

		this->printLine();
		return 0;
	}


	// When all has been read, write out the line
	void printLine()
	{
		// Write out a line into the buffer
		char buffer[BUFFLEN];
		std::snprintf( buffer, BUFFLEN, "\tFile \"%s\", line %i\n%s\n", 
					   filename_, tb_->tb_lineno, line_ );
		PyFile_WriteString( buffer, stderr_ );
		this->nextLine();
	}

	// Iterate down a frame
	void nextLine()
	{
		// Traverse the stack
		tb_ = tb_->tb_next;

		// Is there another frame?
		if (!tb_)
		{
			// We're finished here, print out the actual error
			PyErr_Display( exception_.get(), value_.get(), NULL );
			// And free ourself
			this->decRef();
			return;
		}

		// It's not over yet, start the cycle anew
		this->startLine();
	}

	

public:
	static Mercury::Nub * s_pNub;

	TraceBack( PyObjectPtr exception, PyObjectPtr value, PyObjectPtr tb ) :
		exception_( exception ),
		value_( value ),
		tbObject_( tb ),
		tb_( (PyTracebackObject *)tb.get() )
	{
		stderr_ = PySys_GetObject("stderr");
	}

	void display()
	{
		// Make sure we're not freed while waiting for IO
		this->incRef();
		this->startLine();
	}
};

Mercury::Nub * TraceBack::s_pNub;

extern "C"
{
	/* function printTraceBack
	 *	@components{ all }
	 *	PrintTraceBack emulates the internal python stack trace but without
	 *  any risk of blocking the main thread.
	 * 
	 *  @param exception	The exception object
	 *  @param v			The value causing the exception
	 *  @param tb			The status of the stack when the exception occured
	 */
	static PyObject * py_printTraceBack( PyObject * self, PyObject * args )
	{
		PyObject *exception, *v, *tb;
		if (PyArg_ParseTuple( args, "OOO", &exception, &v, &tb ))
		{
			// If tb is not valid, assume no traceback is required and
			// call back into the default handler
			if (!PyTraceBack_Check(tb)) {
				PyErr_Display( exception, v, tb );
				Py_Return;
			}

			// These are all borrowed, but we want to hang onto them
			Py_INCREF( exception );
			Py_INCREF( v );
			Py_INCREF( tb );

			// Create a new context for writing the trace
			SmartPointer<TraceBack> traceBack = new TraceBack( exception,
															   v, tb );

			// Write out the top message
			PyFile_WriteString( "Traceback (most recent call last):\n", 
								 PySys_GetObject("stderr") );
			traceBack->display();
		}
		else
		{
			ERROR_MSG("py_printTraceBack(): Could not parse args\n");
			PyErr_Clear();
		}
		Py_Return;
	}
}


/**
 *  Will turn on our own traceback mechanism which uses non-blocking IO 
 */
void initExceptionHook( Mercury::Nub * pNub )
{
	static PyMethodDef printTraceBackDef =
		{
			const_cast< char * >( "printTraceBack" ),
			(PyCFunction)py_printTraceBack,
			METH_VARARGS | METH_STATIC,
			const_cast< char * >( "")
		};

	//Take the opportunity to remember the nub
	TraceBack::s_pNub = pNub;
	PySys_SetObject( "excepthook", 
					 PyCFunction_New( &printTraceBackDef, NULL ));
}

}

// py_traceback.cpp
