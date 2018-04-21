/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef REVIVER_HPP
#define REVIVER_HPP

#include "cstdmf/singleton.hpp"
#include "network/machine_guard.hpp"
#include "network/nub.hpp"

class ComponentReviver;
typedef std::vector< ComponentReviver * > ComponentRevivers;

/**
 *	This class is used to represent the reviver process. It monitors for the
 *	unexpected death of processes and starts new ones.
 */
class Reviver : public Mercury::TimerExpiryHandler,
	public Singleton< Reviver >
{
public:
	Reviver( Mercury::Nub & nub );
	virtual ~Reviver();

	bool init( int argc, char * argv[] );
	bool queryMachinedSettings();

	void run();
	void shutDown();

	void revive( const char * createComponent );

	bool hasEnabledComponents() const;

	// Overrides from TimerExpiryHandler
	virtual int handleTimeout( Mercury::TimerID id, void * arg );

	int pingPeriod() const		{ return pingPeriod_; }
	int maxPingsToMiss() const	{ return maxPingsToMiss_; }

	void markAsDirty()			{ isDirty_ = true; }

	Mercury::Nub & nub()		{ return nub_; }

	/**
	 *	This class is used to handle a reply from BWMachined telling us the tags
	 *	associated with this machine.
	 */
	class TagsHandler : public MachineGuardMessage::ReplyHandler
	{
	public:
		TagsHandler( Reviver &reviver ) : reviver_( reviver ) {}
		virtual bool onTagsMessage( TagsMessage &tm, uint32 addr );

	private:
		Reviver &reviver_;
	};

private:
	enum TimeoutType
	{
		TIMEOUT_REATTACH
	};

	Mercury::Nub &		nub_;
	int					pingPeriod_;
	int					maxPingsToMiss_;
	ComponentRevivers	components_;

	bool				shuttingDown_;
	bool				shutDownOnRevive_;
	bool				isDirty_; // For output
};

#endif // REVIVER_HPP
