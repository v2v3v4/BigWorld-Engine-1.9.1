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
 *	GUI Tearoff panel framework - Datatypes
 */

#ifndef GUITABS_DATATYPES_HPP
#define GUITABS_DATATYPES_HPP

namespace GUITABS
{

enum InsertAt {
	UNDEFINED_INSERTAT,
	TOP,
	BOTTOM,
	LEFT,
	RIGHT,
	TAB,
	FLOATING,
	SUBCONTENT // as a content inside a ContentContainer panel
};

enum Orientation
{
	UNDEFINED_ORIENTATION,
	VERTICAL,
	HORIZONTAL
};


class DragManager;
class Manager;
class ContentFactory;
class Content;
class ContentContainer;
class Tab;
class TabCtrl;
class Panel;
class DockedPanelNode;
class SplitterNode;
class DockNode;
class Floater;
class Dock;

typedef SmartPointer<Manager> ManagerPtr; // just a pointer to a singleton

typedef SmartPointer<DragManager> DragManagerPtr;

typedef Content* PanelHandle; // This type is used only for weak references.

typedef SmartPointer<ContentFactory> ContentFactoryPtr;
typedef SmartPointer<Content> ContentPtr;
typedef SmartPointer<Tab> TabPtr;
typedef SmartPointer<TabCtrl> TabCtrlPtr;
typedef SmartPointer<Panel> PanelPtr;
typedef SmartPointer<DockedPanelNode> DockedPanelNodePtr;
typedef SmartPointer<SplitterNode> SplitterNodePtr;
typedef SmartPointer<DockNode> DockNodePtr;
typedef SmartPointer<Floater> FloaterPtr;
typedef SmartPointer<Dock> DockPtr;



} // namespace

#endif // GUITABS_DATATYPES_HPP