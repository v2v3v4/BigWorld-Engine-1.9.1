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
 *	FiltersCtrl: Manages a set of push-like checkbox buttons.
 */

#ifndef FILTERS_CTRL_HPP
#define FILTERS_CTRL_HPP

#include "cstdmf/smartpointer.hpp"


// Filters Control
class FiltersCtrlEventHandler
{
public:
	virtual void filterClicked( const char* name, bool pushed, void* data ) = 0;
};

class FiltersCtrl : public CWnd
{
public:
	FiltersCtrl();
	virtual ~FiltersCtrl();

	void setPushlike( bool pushlike ) { pushlike_ = pushlike; };

	void clear();
	bool empty();
	void add( const char* name, bool pushed, void* data );
	void addSeparator();
	int getHeight();
	void recalcWidth( int width );
	void enableAll( bool enable );
	void enable( const std::string& name, bool enable );

	void setEventHandler( FiltersCtrlEventHandler* eventHandler );
private:
	static const int FILTERCTRL_ID_BASE = 3000;
	FiltersCtrlEventHandler* eventHandler_;
	class Filter : public ReferenceCount
	{
	public:
		std::string name;
		CButton button;
		CStatic separator;
		void* data;
	};
	typedef SmartPointer<Filter> FilterPtr;
	std::vector<FilterPtr> filters_;
	typedef std::vector<FilterPtr>::iterator FilterItr;
	int lines_;
	int separatorWidth_;
	int butSeparation_;
	bool pushlike_;

	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnFilterClicked( UINT nID );
	DECLARE_MESSAGE_MAP()
};




#endif // FILTERS_CTRL_HPP