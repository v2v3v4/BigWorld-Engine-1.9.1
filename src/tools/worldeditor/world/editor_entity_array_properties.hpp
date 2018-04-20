/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef EDITOR_ARRAY_PROPERTIES_HPP
#define EDITOR_ARRAY_PROPERTIES_HPP


#include "worldeditor/config.hpp"
#include "worldeditor/forward.hpp"
#include "common/editor_views.hpp"
#include "common/property_list.hpp"
#include "cstdmf/smartpointer.hpp"


///////////////////////////////////////////////////////////////////////////////
// Section: GeneralProperty-related classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class wraps up an array with virtual functions to get and set it.
 */
class ArrayProxy : public ReferenceCount
{
public:
	virtual void elect( GeneralProperty* parent ) = 0;
	virtual void expel( GeneralProperty* parent ) = 0;
	virtual void select( GeneralProperty* parent ) = 0;

	virtual bool addItem() = 0;
	virtual bool delItems() = 0;
};
typedef SmartPointer<ArrayProxy> ArrayProxyPtr;


/**
 *	This is a array property.
 */
class ArrayProperty : public GeneralProperty
{
public:
	ArrayProperty( const std::string & name, ArrayProxyPtr proxy, EditorChunkItem* item );

	virtual void elect();
	virtual void expel();
	virtual void select();

	virtual ArrayProxyPtr proxy() const;
	virtual EditorChunkItem* item() const;

private:
	ArrayProxyPtr proxy_;
	EditorChunkItem* item_;

	GENPROPERTY_VIEW_FACTORY_DECLARE( ArrayProperty )
};


///////////////////////////////////////////////////////////////////////////////
// Section: PropertyItem-related classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class implements the PropertyList property item for an array.
 */
class ArrayPropertyItem : public LabelPropertyItem
{
public:
	ArrayPropertyItem(const CString& name, const CString& str, ArrayProxyPtr proxy);
	virtual ~ArrayPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	virtual void onCustom( UINT nID );

	virtual ItemType getType() { return Type_Label; }

	virtual void clear();

private:
	ArrayProxyPtr proxy_;
	static std::map<CWnd*, CButton*>				addButton_;
	static std::map<CWnd*, CButton*>				delButton_;
};


///////////////////////////////////////////////////////////////////////////////
// Section: BaseView-related classes
///////////////////////////////////////////////////////////////////////////////

/**
 *	This class implements a PropertyList view of the array.
 */
class ArrayView : public BaseView
{
public:
	ArrayView( ArrayProperty & property );

	ArrayPropertyItem* item() { return (ArrayPropertyItem*)&*(propertyItems_[0]); }

	virtual void EDCALL elect();
	
	virtual void onSelect() { property_.select(); }
	virtual void onChange( bool transient );

	virtual void updateGUI();

	static ArrayProperty::View * create( ArrayProperty & property )
	{ return new ArrayView( property ); }

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:

	ArrayProperty&	property_;

	class Enroller {
		Enroller() {
			ArrayProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


#endif // EDITOR_ARRAY_PROPERTIES_HPP
