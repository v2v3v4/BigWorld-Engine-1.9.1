/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef __WEB_INTEGRATION_HPP__
#define __WEB_INTEGRATION_HPP__

#include "entitydef/entity_description_map.hpp"

#include "cstdmf/singleton.hpp"
#include "network/nub.hpp"

#include "network/logger_message_forwarder.hpp"

#include <string>

class BlockingDbLookUpHandler;


/**
 *	This class represents the web integration component.
 */
class WebIntegration : public Singleton< WebIntegration >
{
public:

	WebIntegration();
	virtual ~WebIntegration();

	bool init();

	Mercury::Nub & nub();

	/**
	 *	Return the last known address of the DbMgr component.
	 *
	 *	@return reference to the last known address of the DbMgr
	 */
	const Mercury::Address & dbMgrAddr() const
	{ return dbMgrAddr_; }

	Mercury::Address & dbMgrAddr( bool forget = false );

	/**
	 *	Accessor for the entity description map.
	 */
	EntityDescriptionMap & entityDescriptions()
	{ return *pEntities_; }

	/**
	 *	Const-accessor for the entity description map.
	 */
	const EntityDescriptionMap & entityDescriptions() const
	{ return *pEntities_; }

	void setNubPort( uint16 port );

	int logOn( const std::string & username, const std::string & password,
		bool allowAlreadyLoggedOn = true );

	PyObject * lookUpEntityByName( const std::string & entityTypeName,
		const std::string & entityName );

	PyObject * lookUpEntityByDBID( const std::string & entityTypeName,
		DatabaseID dbID );

private: // private methods

	PyObject * lookUpEntityComplete( BlockingDbLookUpHandler & handler,
		Mercury::Bundle & bundle );

	bool lookUpEntityTypeByName( const std::string & name,
		EntityTypeID & id );

private:
	Mercury::Nub * 			pNub_;
	pid_t					nubPid_;		// the pid of the process that
											// created the nub
	Mercury::Address 		dbMgrAddr_;
	EntityDescriptionMap * 	pEntities_;
	bool					hasInited_;

	/* 
	   A superclass of Endpoint that automatically opens itself to a random port and
	   may switch to another random port with a simple function call. Useful here
	   because LoggerMessageForwarder uses a socke in its constructor but both are
	   static members of WebIntegration.
	*/
	class LoggerEndpoint : public Endpoint
	{
	public:
		LoggerEndpoint();
		bool switchSocket();
	};


	LoggerEndpoint loggerSocket_;
	SimpleLoggerMessageForwarder loggerMessageForwarder_;
};

#endif // __WEB_INTEGRATION_HPP__

// web_integration.hpp
