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
 *	ContentContainer class.
 *	Implements a panel that contains other Content-inheriting classes, useful
 *  in making switching between different contents modal. It basically behaves
 *  as a single tab that changes its content dynamically.
 */

#ifndef __CONTENT_CONTAINER_HPP__
#define __CONTENT_CONTAINER_HPP__

#include "datatypes.hpp"
#include "content.hpp"
#include "content_factory.hpp"

namespace GUITABS {

class ContentContainer : public CDialog, public Content
{
public:
	static const std::string contentID;

	ContentContainer();
	~ContentContainer();

	// ContentContainer methods
	void addContent( ContentPtr content );
	void addContent( std::string subcontentID );
	void currentContent( ContentPtr content );
	void currentContent( std::string subcontentID );
	void currentContent( int index );
	ContentPtr currentContent();

	bool contains( ContentPtr content );
	int contains( const std::string subcontentID );
	ContentPtr getContent( const std::string subcontentID );
	ContentPtr getContent( const std::string subcontentID, int& index );

	void broadcastMessage( UINT msg, WPARAM wParam, LPARAM lParam );

	// Content methods
	std::string getContentID();
	bool load( DataSectionPtr section );
	bool save( DataSectionPtr section );
	std::string getDisplayString();
	std::string getTabDisplayString();
	HICON getIcon();
	CWnd* getCWnd();
	void getPreferredSize( int& width, int& height );
	OnCloseAction onClose( bool isLastContent );
	void handleRightClick( int x, int y );
	// clone not supported
	ContentPtr clone() { return 0; };
	bool isClonable() { return false; };

private:
	typedef std::vector<ContentPtr> ContentVec;
	typedef ContentVec::iterator ContentVecIt;
	ContentVec contents_;
	ContentPtr currentContent_;

	void createContentWnd( ContentPtr content );

	// MFC needed overrides
	void OnOK() {};

	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnSetFocus( CWnd* pOldWnd );
	DECLARE_MESSAGE_MAP();
};

class ContentContainerFactory : public ContentFactory
{
public:
	ContentPtr create() { return new ContentContainer(); };
	std::string getContentID() { return ContentContainer::contentID; };
};

typedef SmartPointer<ContentContainer> ContentContainerPtr;

} // namespace

#endif // __CONTENT_CONTAINER_HPP__