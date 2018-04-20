/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	@file
 */

#ifndef PROFILE_H
#define PROFILE_H

#include "cstdmf/config.hpp"
#include "cstdmf/stdmf.hpp"
#include "cstdmf/smartpointer.hpp"

#if ENABLE_WATCHERS


#include <iostream>
#include <vector>
#include <set>

#include "cstdmf/watcher.hpp"
#include "cstdmf/timestamp.hpp"

class ProfileVal;
class ProfileGroup;
typedef SmartPointer< ProfileGroup > ProfileGroupPtr;

/**
 *  A class to wrap up a group of profiles. The grouping associates profiles
 *  with each for internal time calculations, i.e. only one profile from a group
 *  can be accumulating internal time at once.
 *
 *  Profiles in the same group must be started and stopped in a stack-like
 *  fashion.  Attempting to bridge starts and stops will trigger an assertion.
 */
class ProfileGroup : public DirectoryWatcher
{
public:
	explicit ProfileGroup( const char * watcherPath = NULL );
	~ProfileGroup();

	typedef std::vector< ProfileVal* > Profiles;
	typedef Profiles::iterator iterator;

	iterator begin() { return profiles_.begin(); }
	iterator end() { return profiles_.end(); }

	Profiles & stack() { return stack_; }
	void add( ProfileVal * pVal );
	void reset();

	ProfileVal * pRunningTime() { return profiles_[0]; }
	const ProfileVal * pRunningTime() const { return profiles_[0]; }
	uint64 runningTime() const;

	static ProfileGroup & defaultGroup();

private:
	/// The profiles that are part of this group.
	Profiles profiles_;

	/// The stack of profiles currently executing in this group.
	Profiles stack_;

	/// The watcher subdirectories for this group
	DirectoryWatcherPtr pSummaries_;
	DirectoryWatcherPtr pDetails_;

	/// The default global group for profiles.
	static ProfileGroupPtr s_pDefaultGroup_;
};


/**
 *	This class is used to profile the performance of parts of the code.
 */
class ProfileVal
{
public:
	ProfileVal( const std::string & name, ProfileGroup * pGroup = NULL );
	~ProfileVal();

	/**
	 *	This method starts this profile.
	 */
	void start()
	{
		uint64 now = ::timestamp();

		if (inProgress_ == 0)
		{
			lastTime_ = now;
		}

		++inProgress_;

		ProfileGroup::Profiles & stack = pGroup_->stack();

		// Disable the existing internal profile
		if (!stack.empty())
		{
			ProfileVal & profile = *stack.back();
			profile.lastIntTime_ = now - profile.lastIntTime_;
			profile.sumIntTime_ += profile.lastIntTime_;
		}

		// This profile is now the active internal profile
		stack.push_back( this );
		lastIntTime_ = now;
	}


	/**
	 *  This method stops this profile.
	 */
	void stop( uint32 qty = 0 )
	{
		uint64 now = ::timestamp();

		if (--inProgress_ == 0)
		{
			lastTime_ = now - lastTime_;
			sumTime_ += lastTime_;

		}

		lastQuantity_ = qty;
		sumQuantity_ += qty;

		++count_;

		ProfileGroup::Profiles & stack = pGroup_->stack();
		MF_ASSERT( stack.back() == this );
		stack.pop_back();

		// Disable internal time counting for this profile
		lastIntTime_ = now - lastIntTime_;
		sumIntTime_ += lastIntTime_;

		// Re-enable the internal counter for the frame above this one.
		if (!stack.empty())
		{
			stack.back()->lastIntTime_ = now;
		}
	}


	/**
	 *  This method resets this profile.
	 */
	void reset()
	{
		lastTime_ = 0;
		sumTime_ = 0;
		lastIntTime_ = 0;
		sumIntTime_ = 0;
		lastQuantity_ = 0;
		sumQuantity_ = 0;
		count_ = 0;
		inProgress_ = 0;
	}


	/**
	 *	This method returns whether or not this profile is currently running.
	 *	That is, start has been called more times than stop.
	 */
	bool running() const
	{
		return inProgress_ > 0;
	}


	/**
	 *  Returns the readable description of this profile.
	 */
	const char * c_str() const { return name_.c_str(); }

	/// String description of this profile.
	std::string	name_;

	/// The profile group this profile belongs to, if any.
	ProfileGroup * pGroup_;

	uint64		lastTime_;		///< The time the profile was started.
	uint64		sumTime_;		///< The total time between all start/stops.
	uint64		lastIntTime_;	///< The last internal time for this profile.
	uint64      sumIntTime_;	///< The sum of internal time for this profile.
	uint32		lastQuantity_;	///< The last value passed into stop.
	uint32		sumQuantity_;	///< The total of all values passed into stop.
	uint32		count_;			///< The number of times stop has been called.
	int			inProgress_;	///< Whether the profile is currently timing.

	static Watcher & watcher();
	static Watcher & detailWatcher();
};

/**
 *	This function is the output stream operator for a ProfileVal.
 *
 *	@relates ProfileVal
 */
std::ostream& operator<<( std::ostream &s, const ProfileVal &v );

/**
 *	This function is the input stream operator for ProfileVal.
 *
 *	@relates ProfileVal
 */
std::istream& operator>>( std::istream &s, ProfileVal &v );


/**
 *	This structure wraps up a uint64 timestamp delta and has an operator defined
 *	on it to print it out nicely.
 */
struct NiceTime
{
	/// Constructor
	explicit NiceTime(uint64 t) : t_(t) {}

	uint64 t_;	///< Associated timestamp.
};

/**
 *	This function is the output stream operator for a NiceTime.
 *
 *	@relates NiceTime
 */
std::ostream& operator<<( std::ostream &o, const NiceTime &nt );


/**
 *	This singleton class resets all registered ProfileGroups when a nominated
 *	ProfileVal is reset.
 */
class ProfileGroupResetter
{
public:
	ProfileGroupResetter();
	~ProfileGroupResetter();

	void nominateProfileVal( ProfileVal * pVal = NULL );
	void addProfileGroup( ProfileGroup * pGroup );

	static ProfileGroupResetter & instance();

private:
	ProfileVal * nominee_;

	std::vector< ProfileGroup * >	groups_;

	bool doingReset_;

	// called by every ProfileVal when it is reset
	void resetIfDesired( ProfileVal & val );

	friend std::istream& operator>>( std::istream &s, ProfileVal &v );
};


/**
 *  This class helps to start and stop a ProfileVal in a method, or in fact,
 *  in any scoped block.
 *
 *  @see ProfileVal
 */
class MethodProfiler
{
public:
	MethodProfiler( ProfileVal & profileVal ) :
		profileVal_( profileVal )
	{
		profileVal_.start();
	}

	~MethodProfiler()
	{
		profileVal_.stop();
	}

private:
	ProfileVal & profileVal_;
};

#define SCOPED_PROFILER( val ) MethodProfiler __profilerScope( val );

#else

struct NiceTime
{
	explicit NiceTime(uint64 t) : t_(t) {}

	uint64 t_;
};

std::ostream& operator<<( std::ostream &o, const NiceTime &nt );

#define SCOPED_PROFILER( val )

#endif //ENABLE_WATCHERS


#endif	// PROFILE_H
