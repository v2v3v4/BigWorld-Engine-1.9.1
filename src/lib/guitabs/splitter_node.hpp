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
 *	GUI Tearoff panel framework - SplitterNode class
 */

#ifndef GUITABS_SPLITTER_NODE_HPP
#define GUITABS_SPLITTER_NODE_HPP


namespace GUITABS
{

/**
 *  This class represents a splitter window in the dock tree. It mantains a
 *	splitter window, its orientation (vertical or horizontal), and pointers to
 *  the child nodes, which can be of any class inherited from DockNode.
 *  Additionally, this class switches from a splitter window to a single
 *  container window when one of the children is not visible.
 */
class SplitterNode : public DockNode, public SplitterEventHandler
{
public:
	SplitterNode();
	SplitterNode( Orientation dir, CWnd* parent, int wndID );
	void init( Orientation dir, CWnd* parent, int wndID );
	~SplitterNode();

	void setLeftChild( DockNodePtr child );
	void setRightChild( DockNodePtr child );

	void finishInsert( const CRect* destRect, int leftChildSize, int rightChildSize );

	DockNodePtr getLeftChild();
	DockNodePtr getRightChild();

	bool isLeaf();

	bool isExpanded();

	CWnd* getCWnd();
	
/**
 * Makes space in the splitter tree for a node by resizing recursively
 */
	bool adjustSizeToNode( DockNodePtr newNode, bool nodeIsNew );

/**
 * calls RecalcLayout for the splitter and it's subtrees recursively
 */
	void recalcLayout();

	bool load( DataSectionPtr section, CWnd* parent, int wndID );
	bool save( DataSectionPtr section );

	Orientation getSplitOrientation();

	void setParentWnd( CWnd* parent );

	void getPreferredSize( int& w, int& h );

	bool getNodeByWnd( CWnd* ptr, DockNodePtr& childNode, DockNodePtr& parentNode );
	DockNodePtr getNodeByPoint( int x, int y );

	void destroy();

	// Event handler methods
	void resizeSplitter( int lastWidth, int lastHeight, int width, int height );

private:
	NiceSplitterWnd splitterWnd_;

	DockNodePtr leftChild_;
	DockNodePtr rightChild_;

	Orientation dir_;

	// hack members, to better resize panes
	int delayedLeftSize_;
	int delayedRightSize_;


	int getLeftSize();
	int getRightSize();
	void setLeftSize( int size );
	void setRightSize( int size );

/**
 *	Resizes a splitter in one dimension and adjusts pane sizes. Called inside OnSize().
 */
	void resizeTreeDimension( Orientation dir, int lastSize, int size );
};


} // namespace

#endif // GUITABS_SPLITTER_NODE_HPP