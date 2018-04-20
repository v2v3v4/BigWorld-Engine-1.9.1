/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "worldeditor/world/editor_entity_array_properties.hpp"
#include "common/user_messages.hpp"


///////////////////////////////////////////////////////////////////////////////
// Section: GeneralProperty-related classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	Constructor.
 *
 *	@param name		name of the property
 *	@param proxy	proxy to use to forward changes to the property.
 *	@param item		chunk item the property belongs to.
 */
ArrayProperty::ArrayProperty( const std::string & name, ArrayProxyPtr proxy, EditorChunkItem* item ) :
	GeneralProperty( name ),
	proxy_( proxy ),
	item_( item )
{
	GENPROPERTY_MAKE_VIEWS()
}


/**
 *	Elects the property and calls elect in the proxy so the array's items
 *	are added as PropertyItems as well.
 */
void ArrayProperty::elect()
{
	GeneralProperty::elect();
	proxy_->elect( this );
}


/**
 *	Expels the array's items property items and the expelts itself.
 */
void ArrayProperty::expel()
{
	proxy_->expel( this );
	GeneralProperty::expel();
}


/**
 *	Called when the array property item is selected. It calls the proxy, which
 *	might create extra gizmos, etc.
 */
void ArrayProperty::select()
{
	GeneralProperty::select();
	proxy_->select( this );
}


/**
 *	Returns the proxy used in this property.
 *
 *	@return		proxy used in this property.
 */
ArrayProxyPtr ArrayProperty::proxy() const
{
	return proxy_;
}


/**
 *	Returns the chunk item this property belongs to.
 *
 *	@return		proxy used in this property.
 */
EditorChunkItem* ArrayProperty::item() const
{
	return item_;
}

GENPROPERTY_VIEW_FACTORY( ArrayProperty )


///////////////////////////////////////////////////////////////////////////////
// Section: PropertyItem-related classes
///////////////////////////////////////////////////////////////////////////////

/*static*/ std::map<CWnd*, CButton*> ArrayPropertyItem::addButton_;
/*static*/ std::map<CWnd*, CButton*> ArrayPropertyItem::delButton_;


/**
 *	Constructor.
 *
 *	@param name		Name of the property item.
 *	@param str		Initial string value (not displayed at the moment)
 *	@param proxy	Proxy to use to manage the array's items.
 */
ArrayPropertyItem::ArrayPropertyItem(const CString& name, const CString& str, ArrayProxyPtr proxy)
	: LabelPropertyItem( name, false )
	, proxy_( proxy )
{
	stringValue_ = str;
}


/**
 *	Destructor
 */
ArrayPropertyItem::~ArrayPropertyItem()
{
	clear();
}


/**
 *	Creates the array's top list item.
 *
 *	@param parent	Parent window.
 */
void ArrayPropertyItem::create(CWnd* parent)
{
	LabelPropertyItem::create( parent );
}


/**
 *	Called when the user selects the array's property item. It draws buttons
 *	that help in managing the array's items.
 *
 *	@param rect				screen rectangle available to the property item.
 *	@param showDropDown		not used.
 */
void ArrayPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if ( addButton_.find( parent_ ) == addButton_.end() )
	{
		// build init values
		CRect rect( 0, 0, 1, 1 );
		DWORD style = BS_PUSHBUTTON | WS_CHILD;
		CFont* pFont = parent_->GetParent()->GetFont();

		// Create "Add Item" button
		CButton* addButton = new CButton();

		addButton->Create( "+", style, rect, parent_, IDC_PROPERTYLIST_CUSTOM_MIN );
		addButton->SetFont(pFont);

		addButton_[ parent_ ] = addButton;

		// Create "Delete All Items" button
		CButton* delButton = new CButton();

		delButton->Create( "-", style, rect, parent_, IDC_PROPERTYLIST_CUSTOM_MIN + 1 );
		delButton->SetFont(pFont);

		delButton_[ parent_ ] = delButton;
	}

	if ( addButton_.find( parent_ ) != addButton_.end() )
	{
		// set up the screen positions for the buttons
		int btnSize = rect.Height();
		rect.top -= 1;
		rect.bottom -= 1;
		rect.right -= btnSize;
		rect.left = rect.right - btnSize;
		
		// Position the "Add Item" button
		CButton* addButton = addButton_[ parent_ ];
		addButton->MoveWindow(rect);
		addButton->ShowWindow(SW_SHOW);
		addButton->SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
		addButton->SetFocus();

		// Position the "Delete All Items" button
		rect.left += btnSize;
		rect.right += btnSize;

		CButton* delButton = delButton_[ parent_ ];
		delButton->MoveWindow(rect);
		delButton->ShowWindow(SW_SHOW);
		delButton->SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
		delButton->SetFocus();
	}

	// call the base class' implementation.
	LabelPropertyItem::select( rect, showDropDown );
}


/**
 *	Called when the property item is deselected by the user clicking something else.
 */
void ArrayPropertyItem::deselect()
{
	if ( selectedItem_ != this )
		return;

	LabelPropertyItem::deselect();

	clear();
}


/**
 *	Event handler for the add/delete buttons.
 *
 *	@param nID	command id of the button pressed.
 */
void ArrayPropertyItem::onCustom( UINT nID )
{
	if ( nID == IDC_PROPERTYLIST_CUSTOM_MIN )
	{
		// add
		proxy_->addItem();
		return;
	}
	else if ( nID == IDC_PROPERTYLIST_CUSTOM_MIN + 1 )
	{
		// delete
		proxy_->delItems();
	}
}


/**
 *	Cleans up, destroy the add/delete buttons
 */
void ArrayPropertyItem::clear()
{
	if ( addButton_.find( parent_ ) != addButton_.end() )
	{
		delete addButton_[ parent_ ];
		addButton_.erase( parent_ );
		delete delButton_[ parent_ ];
		delButton_.erase( parent_ );
	}
}


///////////////////////////////////////////////////////////////////////////////
// Section: BaseView-related classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	Constructor
 */
ArrayView::ArrayView( ArrayProperty & property ) 
	: property_( property )
{
}


/**
 *	Called when the ArrayProperty is elected. It creates a relevant property
 *	item and adds it the property list.
 */
void EDCALL ArrayView::elect()
{
	propTable_ = PropTable::table();

	std::string label = property_.name();
	label += " (array)";
	ArrayPropertyItem* newItem = new ArrayPropertyItem( label.c_str(), "", property_.proxy() );
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	propertyItems_.push_back(newItem);

	propTable_->addView( this );
}


/**
 *	Unused implementation of pure virtual method
 */
void ArrayView::onChange( bool transient )
{
}


/**
 *	Unused implementation of pure virtual method
 */
void ArrayView::updateGUI()
{
}

// View factory initialiser.
ArrayView::Enroller ArrayView::Enroller::s_instance;
