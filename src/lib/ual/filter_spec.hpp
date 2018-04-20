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
 *	FILTER_SPEC: filters text according to its include/exclude rules
 */


#ifndef FILTER_SPEC_HPP
#define FILTER_SPEC_HPP

#include "cstdmf/smartpointer.hpp"


// ListFilter
class FilterSpec : public ReferenceCount
{
public:
	FilterSpec( const std::string& name, bool active = false,
		const std::string& include = "", const std::string& exclude = "",
		const std::string& group = "" );
	virtual ~FilterSpec();

	virtual std::string getName() { return name_; };
	virtual void setActive( bool active ) { active_ = active; };
	virtual bool getActive() { return active_ && enabled_; };
	virtual std::string getGroup() { return group_; };

	virtual bool filter( const std::string& str );

	void enable( bool enable );
private:
	std::string name_;
	bool active_;
	bool enabled_;
	std::vector<std::string> includes_;
	std::vector<std::string> excludes_;
	std::string group_;
};
typedef SmartPointer<FilterSpec> FilterSpecPtr;


#endif // FILTER_SPEC_HPP
