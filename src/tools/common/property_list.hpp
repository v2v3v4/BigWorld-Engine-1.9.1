/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

#include "resmgr/string_provider.hpp"
#include "controls/edit_numeric.hpp"
#include "controls/slider.hpp"
#include "cstdmf/bw_functor.hpp"
#include <stack>


// PropertyItem
class PropertyItem
{
public:
	PropertyItem(const CString& name);
	virtual ~PropertyItem() {}

	virtual void create(CWnd* parent) = 0;
	virtual void select(CRect rect, bool showDropDown = true) = 0;
	virtual void deselect() = 0;
	virtual void loseFocus();

	virtual CString name();
	CString value() { return stringValue_; }

	void setSelectable(bool option) { selectable_ = option; }
	bool getSelectable() { return selectable_; }

	void setChangeBuddy(void* buddy) { changeBuddy_ = buddy; }
	void * getChangeBuddy() { return changeBuddy_; }

	static PropertyItem* selectedItem() { return selectedItem_; }

	virtual controls::EditNumeric* ownEdit() { return NULL; }
		
	virtual void comboChange() {}
	virtual void onBrowse() {}
	virtual void sliderChange( int value, bool transient ) {}
	virtual void editChange() {}
	virtual void onDefault() {}
	virtual void onKeyDown( UINT key ) {}
	virtual void onCustom( UINT nID ) {}
	virtual std::string menuOptions() { return ""; }
	virtual std::string textureFeed() { return ""; }

	enum ItemType
	{
		Type_Unknown,
		Type_Group,
		Type_Colour,
		Type_Vector,
		Type_Label,
		Type_Label_Highlight,
		Type_String,
		Type_String_ReadOnly,
		Type_ID
	};
	virtual ItemType getType() { return Type_Unknown; }

	virtual const std::string& descName() { return descName_; }
	virtual void descName( const std::string& desc) { descName_ = desc; }

	virtual std::string UIDescL() { return L(uiDesc_.c_str()); }
	virtual void UIDesc( const std::string& desc) { uiDesc_ = desc; }

	virtual const std::string& exposedToScriptName() { return exposedToScriptName_; }
	virtual void exposedToScriptName( const std::string& name) { exposedToScriptName_ = name; }

	virtual void canExposeToScript( bool canExposeToScript ) { canExposeToScript_ = canExposeToScript; }
	virtual bool canExposeToScript() { return canExposeToScript_; }

	virtual std::string UIDescExtra();

	void setGroup( const std::string& group );
	std::string getGroup() { return group_; }
	void setGroupDepth( int depth ) { groupDepth_ = depth; }
	int getGroupDepth() { return groupDepth_; }

	void arrayData( int arrayIndex, BWBaseFunctor1<int>* arrayCallback )
	{
		arrayIndex_ = arrayIndex;
		arrayCallback_ = arrayCallback;
	}
	int arrayIndex() {	return arrayIndex_;	}
	BWBaseFunctor1<int>* arrayCallback() {	return arrayCallback_.getObject();	}

protected:
	CString name_;
	CString stringValue_;

	std::string descName_;
	std::string uiDesc_;
	std::string exposedToScriptName_;

	bool canExposeToScript_;

	bool selectable_;
	static PropertyItem* selectedItem_;

	CWnd* parent_;
	void* changeBuddy_;

	std::string group_;
	int groupDepth_;

	int arrayIndex_;
	SmartPointer< BWBaseFunctor1<int> > arrayCallback_;
};

typedef std::vector< PropertyItem * > PropertyItemVector;


class GroupPropertyItem : public PropertyItem
{
public:
	GroupPropertyItem(const CString& name, int depth);
	virtual ~GroupPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	virtual ItemType getType() { return Type_Group; }

	void addChild( PropertyItem * item );

	PropertyItemVector & getChildren() { return children_; }

	void setExpanded( bool option ) { expanded_ = option; }
	bool getExpanded() { return expanded_; }

	void setGroupDepth( int depth ) { groupDepth_ = depth; }

protected:
	PropertyItemVector children_;
	bool expanded_;
};

class ColourPropertyItem : public GroupPropertyItem
{
public:
	ColourPropertyItem(const CString& name, const CString& init, int depth, bool colour = true);
	virtual ~ColourPropertyItem();

	virtual CString name();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	void set(const std::string& value);
	std::string get();

	virtual ItemType getType() { return colour_ ? Type_Colour : Type_Vector; }

	virtual void onBrowse();
	virtual std::string menuOptions();

private:
	static std::map<CWnd*, CEdit*> edit_;
	static std::map<CWnd*, CButton*> button_;
	bool colour_;
};


class LabelPropertyItem : public PropertyItem
{
public:
	LabelPropertyItem(const CString& name, bool highlight = false);
	virtual ~LabelPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	virtual ItemType getType();

private:
	bool highlight_;
};


class StringPropertyItem : public PropertyItem
{
public:
	StringPropertyItem(const CString& name, const CString& currentValue, bool readOnly = false);
	virtual ~StringPropertyItem();

	virtual CString name();
	
	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	void set(const std::string& value);
	std::string get();
	
	void fileFilter( const std::string& filter ) { fileFilter_ = filter; }
	std::string fileFilter() { return fileFilter_; }

	void defaultDir( const std::string& dir ) { defaultDir_ = dir; }
	std::string defaultDir() { return defaultDir_; }

	void canTextureFeed( bool val ) { canTextureFeed_ = val; }
	bool canTextureFeed() { return canTextureFeed_; }
	
	void textureFeed( const std::string& textureFeed )	{ textureFeed_ = textureFeed; }
	std::string textureFeed()	{ return textureFeed_; }

	std::string UIDescExtra()
	{
		if ( !canTextureFeed_ ) return "";
		if ( textureFeed_ == "" )
		{
			return L("COMMON/PROPERTY_LIST/ASSIGN_FEED");
		}
		else
		{
			return L("COMMON/PROPERTY_LIST/ASSIGN_FEED", textureFeed_ );
		}
			
	}

	virtual void onBrowse();
	virtual std::string menuOptions();

	virtual ItemType getType();
	bool isHexColor() const	{	return stringValue_.GetLength() == 7 && stringValue_[0] == '#';	}
	bool isVectColor() const { std::string val = stringValue_; return std::count( val.begin( ), val.end( ), ',' ) == 2; }
private:
	static std::map<CWnd*, CEdit*> edit_;
	static std::map<CWnd*, CButton*> button_;
	std::string fileFilter_;
	std::string defaultDir_;
	bool canTextureFeed_;
	std::string textureFeed_;
	bool readOnly_;
};


class IDPropertyItem : public PropertyItem
{
public:
	IDPropertyItem(const CString& name, const CString& currentValue);
	virtual ~IDPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	void set(const std::string& value);
	std::string get();

	virtual ItemType getType();

private:
	static std::map<CWnd*, CEdit*> edit_;
};


class ComboPropertyItem : public PropertyItem
{
public:
	ComboPropertyItem(const CString& name, CString currentValue,
				const std::vector<std::string>& possibleValues);
	ComboPropertyItem(const CString& name, int currentValueIndex,
				const std::vector<std::string>& possibleValues);
	virtual ~ComboPropertyItem();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();
	virtual void loseFocus();

	void set(const std::string& value);
	void set(int index);
	std::string get();

	virtual void comboChange();

private:
	static std::map<CWnd*, CComboBox*> comboBox_;
	std::vector<std::string> possibleValues_;
};


class BoolPropertyItem : public PropertyItem
{
public:
	BoolPropertyItem(const CString& name, int currentValue);
	virtual ~BoolPropertyItem();

	virtual CString name();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();
	virtual void loseFocus();

	void set(bool value);
	bool get();

	virtual void comboChange();

	virtual std::string menuOptions();

private:
	static std::map<CWnd*, CComboBox*> comboBox_;
	int value_;
};


class FloatPropertyItem : public PropertyItem
{
public:
	FloatPropertyItem(const CString& name, float currentValue);
	virtual ~FloatPropertyItem();

	virtual CString name();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	void setRange( float min, float max, int digits );
	void setDefault( float def );

	void set(float value);
	float get();

	virtual void sliderChange( int value, bool transient );
	virtual void editChange();
	virtual void onDefault();

	virtual std::string menuOptions();

	virtual controls::EditNumeric* ownEdit();

private:
	static std::map<CWnd*, controls::EditNumeric*> editNumeric_;
	static std::map<CWnd*, controls::EditNumeric*> editNumericFormatting_;
	static std::map<CWnd*, controls::Slider*> slider_;
	static std::map<CWnd*, CButton*> button_;

	float value_;
	float min_;
	float max_;
	int digits_;
	bool ranged_;
	bool changing_;
	float def_;
	bool hasDef_;
};


class IntPropertyItem : public PropertyItem
{
public:
	IntPropertyItem(const CString& name, int currentValue);
	virtual ~IntPropertyItem();

	virtual CString name();

	virtual void create(CWnd* parent);
	virtual void select(CRect rect, bool showDropDown = true);
	virtual void deselect();

	void setRange( int min, int max );
	void set(int value);
	int get();

	virtual void sliderChange( int value, bool transient );
	virtual void editChange();

	virtual std::string menuOptions();

	virtual controls::EditNumeric* ownEdit();

private:
	static std::map<CWnd*, controls::EditNumeric*> editNumeric_;
	static std::map<CWnd*, controls::EditNumeric*> editNumericFormatting_;
	static std::map<CWnd*, controls::Slider*> slider_;
	int value_;
	int min_;
	int max_;
	bool ranged_;
	bool changing_;
};

// PropertyList window

class PropertyList : public CListBox
{
public:
	static void mainFrame( CFrameWnd* mainFrame ) { mainFrame_ = mainFrame; }

	PropertyList();
	virtual ~PropertyList();

	void enable( bool enable );

	int AddPropItem(PropertyItem* item);

	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

	void clear();

	bool changeSelectItem( int delta );
	bool selectItem( int itemIndex );
	static void deselectCurrentItem();
	void selectPrevItem();
	void selectNextItem();

	void setDividerPos( int x );

	PropertyItem * getHighlightedItem();

	void collapseGroup(GroupPropertyItem* gItem, int index);
	void expandGroup(GroupPropertyItem* gItem, int index);

	void startArray( BWBaseFunctor1<int>* callback );
	void arrayIndex( int index );
	void endArray();

	CRect dropTest( CPoint point, const std::string& fileName );
	bool doDrop( CPoint point, const std::string& fileName );

	static WCHAR s_tooltipBuffer_[512];
	
	int OnToolHitTest(CPoint point, TOOLINFO * pTI) const;
	BOOL OnToolTipText( UINT id, NMHDR* pTTTStruct, LRESULT* pResult );

protected:
	virtual void PreSubclassWindow();

	afx_msg void OnSize( UINT nType, int cx, int cy );
	afx_msg void OnPaint();
	afx_msg void OnSelchange();
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg LRESULT OnChangePropertyItem(WPARAM wParam, LPARAM lParam);
	afx_msg void OnComboChange();
	afx_msg void OnBrowse();
	afx_msg void OnDefault();
	afx_msg void OnCustom(UINT nID);
	afx_msg void OnArrayDelete();
	afx_msg void OnEditChange( ); 
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point );
	afx_msg void OnRButtonUp( UINT, CPoint );
	afx_msg HBRUSH OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );

	DECLARE_MESSAGE_MAP()

private:
	static CFrameWnd* mainFrame_;

	void DrawDivider(int xpos);
	void selChange( bool showDropDown );
	void Select(int selected);

	CToolTipCtrl toolTip_;

	bool enabled_;
	
	int selected_;

	int dividerPos_;
	int dividerTop_;
	int dividerBottom_;
	int dividerLastX_;
	bool dividerMove_;
	HCURSOR cursorArrow_;
	HCURSOR cursorSize_;

	bool tooltipsEnabled_;

	std::vector< GroupPropertyItem * > parentGroupStack_;

	bool delayRedraw_;

	std::stack<int> arrayIndex_;
	std::stack<SmartPointer< BWBaseFunctor1<int> > > arrayCallback_;
	static CButton s_arrayDeleteButton_;

	void establishGroup( PropertyItem* item );
	void makeSubGroup( const std::string & subGroup, PropertyItem* item );
	void addGroupToStack( const std::string & label, PropertyItem* item = NULL );

};
