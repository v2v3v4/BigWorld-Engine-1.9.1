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

#include "property_list.hpp"
#include "user_messages.hpp"
#include "common/file_dialog.hpp"

DECLARE_DEBUG_COMPONENT2( "GUI", 2 )

/*static*/ CFrameWnd* PropertyList::mainFrame_ = NULL;

/*static*/ PropertyItem* PropertyItem::selectedItem_ = NULL;

PropertyItem::PropertyItem(const CString& name) 
	: name_( name )
	, stringValue_( "" )
	, selectable_( true )
	, changeBuddy_( NULL )
	, parent_( NULL )
	, groupDepth_( 0 )
	, descName_( "" )
	, uiDesc_( "" )
	, exposedToScriptName_( "" )
	, canExposeToScript_( false )
	, arrayIndex_( -1 )
{
}


void PropertyItem::loseFocus()
{
	PropertyList::deselectCurrentItem();
}


CString PropertyItem::name()
{
	if (exposedToScriptName_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + exposedToScriptName_ + ")";
		return result.c_str();
	}
}

void PropertyItem::setGroup( const std::string& group )
{
//	MF_ASSERT( getType() != Type_Group );
	MF_ASSERT( (group.size() == 0) || (group.at(0) != '/') );

	// trim extra '/' from tail
	group_ = group;
	while ( (group_.end() != group_.begin()) &&
			(*(--group_.end()) == '/') )
	{
		group_.erase( --group_.end() );
	}

	if (group_.empty())
		groupDepth_ = 0;
	else
		groupDepth_ = std::count( group_.begin( ), group_.end( ), '/' ) + 1;
}

/*virtual*/ std::string PropertyItem::UIDescExtra()
{
	if (!canExposeToScript_) return "";
	
	if (exposedToScriptName_ == "")
	{
		return L("COMMON/PROPERTY_LIST/EXPOSE_PYTHON");
	}
	else
	{
		return L("COMMON/PROPERTY_LIST/PYTHON_EXPOSED", exposedToScriptName_ );
	}
}

// GroupPropertyItem
GroupPropertyItem::GroupPropertyItem(const CString& name, int depth)
	: PropertyItem( name )
	, expanded_( true )
{
	groupDepth_ = depth;
}

GroupPropertyItem::~GroupPropertyItem()
{
}

void GroupPropertyItem::create( CWnd* parent )
{
	parent_ = parent;
}

void GroupPropertyItem::select( CRect rect, bool showDropDown /*= true*/ )
{
	selectedItem_ = this;
}

void GroupPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	selectedItem_ = NULL;
}

void GroupPropertyItem::addChild( PropertyItem * child )
{
	children_.push_back( child );

	child->setGroupDepth( getGroupDepth() ); 
}


// ColourPropertyItem
/*static*/ std::map<CWnd*, CEdit*> ColourPropertyItem::edit_;
/*static*/ std::map<CWnd*, CButton*> ColourPropertyItem::button_;

ColourPropertyItem::ColourPropertyItem(const CString& name, const CString& init, int depth, bool colour)
	: GroupPropertyItem( name, depth )
{
	stringValue_ = init;
	groupDepth_ = depth;
	colour_ = colour;
	exposedToScriptName_ = "";
}

ColourPropertyItem::~ColourPropertyItem()
{
	delete edit_[ parent_ ];
	edit_.erase( parent_ );
	delete button_[ parent_ ];
	button_.erase( parent_ );
}

/*virtual*/ CString ColourPropertyItem::name()
{
	if (exposedToScriptName_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + exposedToScriptName_ + ")";
		return result.c_str();
	}
}

void ColourPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (edit_.find( parent_ ) == edit_.end())
	{
		edit_[ parent_ ] = new CEdit();
		
		CRect rect(10, 10, 10, 10);
		DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER;
		edit_[ parent_ ]->Create(style, rect, parent, IDC_PROPERTYLIST_STRING);

		CFont* pFont = parent->GetParent()->GetFont();
		edit_[ parent_ ]->SetFont(pFont);
	}

	if ((colour_) && (button_.find( parent_ ) == button_.end()))
	{
		button_[ parent_ ] = new CButton();
		
		CRect rect(10, 10, 10, 10);
		DWORD style = BS_PUSHBUTTON | WS_CHILD | WS_BORDER;
		button_[ parent_ ]->Create( "...", style, rect, parent, IDC_PROPERTYLIST_BROWSE);

		CFont* pFont = parent->GetParent()->GetFont();
		button_[ parent_ ]->SetFont(pFont);
	}
}

void ColourPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return;
	if ((edit_.find(parent_) == edit_.end()) && (button_.find(parent_) == button_.end()))
		return;
	
	// check the readonly attribute
	// note: must use SendMessage, ModifyStyle does not work on CEdit
	//if (readOnly_)
	//	edit_[ parent_ ]->SendMessage(EM_SETREADONLY, TRUE, 0);
	//else
		edit_[ parent_ ]->SendMessage(EM_SETREADONLY, FALSE, 0);

	const int BUTTON_WIDTH = 20;

	// display edit box
	rect.bottom -= 1;		// keep it inside the selection
	if (colour_)
	{
		rect.left += rect.Height(); // Avoid stomping on the colour preview
		rect.right -= BUTTON_WIDTH; // Add room for button
	}
	edit_[ parent_ ]->MoveWindow(rect);

	edit_[ parent_ ]->SetWindowText(stringValue_);
	edit_[ parent_ ]->ShowWindow(SW_SHOW);
	edit_[ parent_ ]->SetFocus();
	edit_[ parent_ ]->SetSel(0, -1);

	if (colour_)
	{
		rect.left = rect.right;
		rect.right += BUTTON_WIDTH;
		
		button_[ parent_ ]->MoveWindow(rect);

		button_[ parent_ ]->ShowWindow(SW_SHOW);
		button_[ parent_ ]->SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
	}

	selectedItem_ = this;
}

void ColourPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()) && (button_.find(parent_) == button_.end()))
		return;
	if (selectedItem_ != this)
		return;

	CString newStr;
	edit_[ parent_ ]->GetWindowText(newStr);
	if (stringValue_ != newStr)
	{
		stringValue_ = newStr;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	edit_[ parent_ ]->ShowWindow(SW_HIDE);
	edit_[ parent_ ]->Invalidate();

	if (colour_)
	{
		button_[ parent_ ]->ShowWindow(SW_HIDE);
		button_[ parent_ ]->Invalidate();
	}

	selectedItem_ = NULL;
}

void ColourPropertyItem::set(const std::string& value)
{
	stringValue_ = value.c_str();

	if ((selectedItem_ == this) && (parent_) && (edit_.find(parent_) != edit_.end()))
	{
		edit_[ parent_ ]->SetWindowText(stringValue_);
		// force the list to redraw 
		parent_->Invalidate();
	}
}

std::string ColourPropertyItem::get()
{
	return stringValue_.GetBuffer();
}

void ColourPropertyItem::onBrowse()
{
	if (!colour_) return; // Don't show a colour picker for vectors
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()))
		return;
	
	CString color;
	edit_[ parent_ ]->GetWindowText( color );
	int r,g,b,a;
	sscanf( color, "%d , %d , %d , %d", &r, &g, &b, &a );
	CColorDialog colorDlg( RGB( r, g, b ), CC_FULLOPEN );
	if( colorDlg.DoModal() == IDOK )
	{
		char s[256];
		COLORREF col = colorDlg.GetColor();
		r = col & 0xff;
		g = (col / 256) & 0xff;
		b = (col / 65536) & 0xff;
		bw_snprintf( s, sizeof(s), "%d , %d , %d , %d", r,g,b,a );
		stringValue_ = s;
		edit_[ parent_ ]->SetWindowText( s );
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}
	parent_->Invalidate();
}

/*virtual*/ std::string ColourPropertyItem::menuOptions()
{
	if (exposedToScriptName_ == "")
		return L("COMMON/PROPERTY_LIST/PYTHON_OFF");
	else
		return L("COMMON/PROPERTY_LIST/PYTHON_ON");
}

// LabelPropertyItem
LabelPropertyItem::LabelPropertyItem(const CString& name, bool highlight)
	: PropertyItem( name )
	, highlight_( highlight )
{
}

LabelPropertyItem::~LabelPropertyItem()
{
}

void LabelPropertyItem::create(CWnd* parent)
{
	parent_ = parent;
}

void LabelPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	selectedItem_ = this;
}

void LabelPropertyItem::deselect()
{
	if (selectedItem_ != this)
		return;

	selectedItem_ = NULL;
}

PropertyItem::ItemType LabelPropertyItem::getType()
{
	if (highlight_)
        return Type_Label_Highlight;
	else
		return Type_Label;
}


// StringPropertyItem
/*static*/ std::map<CWnd*, CEdit*> StringPropertyItem::edit_;
/*static*/ std::map<CWnd*, CButton*> StringPropertyItem::button_;

StringPropertyItem::StringPropertyItem(const CString& name, const CString& currentValue, bool readOnly)
	: PropertyItem( name )
	, readOnly_( readOnly )
	, defaultDir_("")
	, canTextureFeed_( false )
{
	stringValue_ = currentValue;
}

StringPropertyItem::~StringPropertyItem()
{
	delete edit_[ parent_ ];
	edit_.erase( parent_ );
	delete button_[ parent_ ];
	button_.erase( parent_ );
}

/*virtual*/ CString StringPropertyItem::name()
{
	if (textureFeed_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + textureFeed_ + ")";
		return result.c_str();
	}
}

void StringPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (edit_.find( parent_ ) == edit_.end())
	{
		edit_[ parent_ ] = new CEdit();

		CRect rect(10, 10, 10, 10);
		DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER;
		edit_[ parent_ ]->Create(style, rect, parent, IDC_PROPERTYLIST_STRING);

		CFont* pFont = parent->GetParent()->GetFont();
		edit_[ parent_ ]->SetFont(pFont);
	}

	if (button_.find( parent_ ) == button_.end())
	{
		button_[ parent_ ] = new CButton();

		CRect rect(10, 10, 10, 10);
		DWORD style = BS_PUSHBUTTON | WS_CHILD | WS_BORDER;
		button_[ parent_ ]->Create( "...", style, rect, parent, IDC_PROPERTYLIST_BROWSE);

		CFont* pFont = parent->GetParent()->GetFont();
		button_[ parent_ ]->SetFont(pFont);
	}
}

void StringPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()) && (button_.find(parent_) == button_.end()))
		return;
	// check the readonly attribute
	// note: must use SendMessage, ModifyStyle does not work on CEdit
	if (readOnly_)
		edit_[ parent_ ]->SendMessage(EM_SETREADONLY, TRUE, 0);
	else
		edit_[ parent_ ]->SendMessage(EM_SETREADONLY, FALSE, 0);

	const int BUTTON_WIDTH = ( ( fileFilter_.size() == 0 && !isHexColor() && !isVectColor() ) || readOnly_ ) ? 0 : 20;

	// display edit box
	rect.bottom -= 1;		// keep it inside the selection
	rect.right -= BUTTON_WIDTH;
	edit_[ parent_ ]->MoveWindow(rect);

	edit_[ parent_ ]->SetWindowText(stringValue_);
	edit_[ parent_ ]->ShowWindow(SW_SHOW);
	edit_[ parent_ ]->SetFocus();
	edit_[ parent_ ]->SetSel(0, -1);

	rect.left = rect.right;
	rect.right += BUTTON_WIDTH;
	
	button_[ parent_ ]->MoveWindow(rect);

	button_[ parent_ ]->ShowWindow(SW_SHOW);
	button_[ parent_ ]->SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );

	selectedItem_ = this;
}

void StringPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()) && (button_.find(parent_) == button_.end()))
		return;
	if (selectedItem_ != this)
		return;

	CString newStr;
	edit_[ parent_ ]->GetWindowText(newStr);

	if (fileFilter_.size())
	{
		std::string disolvedFileName = BWResource::dissolveFilename( std::string( newStr ));
		if ((newStr != "") && (!BWResource::validPath( disolvedFileName )))
		{
			::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
				L("COMMON/PROPERTY_LIST/FILE_IN_FOLDERS_ONLY"),
				L("COMMON/PROPERTY_LIST/UNABLE_RESOLVE"),
				MB_OK | MB_ICONWARNING );
			edit_[ parent_ ]->SetWindowText( stringValue_ );
			newStr = stringValue_;
		}
	}

	if (stringValue_ != newStr)
	{
		stringValue_ = newStr;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	edit_[ parent_ ]->ShowWindow(SW_HIDE);
	edit_[ parent_ ]->Invalidate();

	button_[ parent_ ]->ShowWindow(SW_HIDE);
	button_[ parent_ ]->Invalidate();

	selectedItem_ = NULL;
}

void StringPropertyItem::set(const std::string& value)
{
	stringValue_ = value.c_str();
	
	if ((selectedItem_ == this) && (parent_) && (edit_.find(parent_) != edit_.end()))
	{
		edit_[ parent_ ]->SetWindowText(stringValue_);
		
	}
	// force the list to redraw 
	parent_->Invalidate();
	parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
}

std::string StringPropertyItem::get()
{
	// don't need this to dynamically change
	return stringValue_.GetBuffer(); 
}

void StringPropertyItem::onBrowse()
{
	if (!parent_ || readOnly_) return; 
	if ((edit_.find(parent_) == edit_.end()))
		return;
	if( isHexColor() )
	{
		struct MapHex
		{
			int h2d( char ch )
			{
				if( ch >= '0' && ch <= '9' )
					return ch - '0';
				if( ch >= 'A' && ch <= 'F' )
					return ch - 'A' + 10;
				return ch - 'a' + 10;
			}
			int operator()( LPCTSTR str )
			{
				int result = 0;
				while( *str )
				{
					result *= 16;
					result += h2d( *str );
					++str;
				}
				return result;
			}
		}
		MapHex;
		struct SwapRB
		{
			COLORREF operator()( COLORREF ref )
			{
				return ( ( ref / 65536 ) & 0xff ) + ( ( ref / 256 ) & 0xff ) * 256 + ( ref & 0xff ) * 65536;
			}
		}
		SwapRB;
		CString color;
		edit_[ parent_ ]->GetWindowText( color );
		CColorDialog colorDlg( SwapRB( MapHex( (LPCTSTR)color + 1 ) ), CC_FULLOPEN );
		if( colorDlg.DoModal() == IDOK )
		{
			CString s;
			s.Format( "#%06x", SwapRB( colorDlg.GetColor() ) & 0xffffff );
			stringValue_ = s;
			edit_[ parent_ ]->SetWindowText( s );
			parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
		}
	}
	else if ( isVectColor() )
	{
		CString color;
		edit_[ parent_ ]->GetWindowText( color );
		int r,g,b;
		sscanf( color, "%d , %d , %d", &r, &g, &b );
		CColorDialog colorDlg( RGB( r, g, b ), CC_FULLOPEN );
		if( colorDlg.DoModal() == IDOK )
		{
			char s[256];
			COLORREF col = colorDlg.GetColor();
			r = col & 0xff;
			g = (col / 256) & 0xff;
			b = (col / 65536) & 0xff;
			bw_snprintf( s, sizeof(s), "%d , %d , %d", r,g,b );
			stringValue_ = s;
			edit_[ parent_ ]->SetWindowText( s );
			parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
		}
	}
	else
	{
		ASSERT( fileFilter_.size() );
		BWFileDialog fileDialog( TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, fileFilter_.c_str(), edit_[ parent_ ]->GetParent() );

		std::string initDir = stringValue_ != "" ? std::string(stringValue_) : defaultDir_;
		initDir = BWResource::resolveFilename( initDir );
		initDir = initDir.substr( 0, initDir.find_last_of( '/' ));
		std::replace( initDir.begin(), initDir.end(), '/', '\\' );
		fileDialog.m_ofn.lpstrInitialDir = initDir.c_str();

		if (fileDialog.DoModal()==IDOK)
		{
			
			std::string disolvedFileName = BWResource::dissolveFilename( (LPCTSTR)(fileDialog.GetPathName()) );
			if (BWResource::validPath( disolvedFileName ))
			{
				stringValue_ = disolvedFileName.c_str();
				edit_[ parent_ ]->SetWindowText( stringValue_ );
				parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
			}
			else
			{
				::MessageBox( AfxGetApp()->m_pMainWnd->GetSafeHwnd(),
					L("COMMON/PROPERTY_LIST/FILE_IN_FOLDERS_ONLY"),
					L("COMMON/PROPERTY_LIST/UNABLE_RESOLVE"),
					MB_OK | MB_ICONWARNING );
			}
		}
	}
}

/*virtual*/ std::string StringPropertyItem::menuOptions()
{
	if (textureFeed_ == "")
		return L("COMMON/PROPERTY_LIST/FEED_OFF");
	else
		return L("COMMON/PROPERTY_LIST/FEED_ON");
}

PropertyItem::ItemType StringPropertyItem::getType()
{
	if (readOnly_)
		return Type_String_ReadOnly;

	return Type_String;
}


// IDPropertyItem
/*static*/ std::map<CWnd*, CEdit*> IDPropertyItem::edit_;

IDPropertyItem::IDPropertyItem(const CString& name, const CString& currentValue)
	: PropertyItem( name )
{
	stringValue_ = currentValue;
}

IDPropertyItem::~IDPropertyItem()
{
	delete edit_[ parent_ ];
	edit_.erase( parent_ );
}

void IDPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (edit_.find( parent_ ) == edit_.end())
	{
		edit_[ parent_ ] = new CEdit();

		CRect rect(10, 10, 10, 10);
		DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER | EM_SETREADONLY;
		edit_[ parent_ ]->Create(style, rect, parent, IDC_PROPERTYLIST_STRING);

		CFont* pFont = parent->GetParent()->GetFont();
		edit_[ parent_ ]->SetFont(pFont);
	}
}

void IDPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()))
		return;

	edit_[ parent_ ]->SendMessage(EM_SETREADONLY, TRUE, 0);

	// display edit box
	rect.bottom -= 1;		// keep it inside the selection
	edit_[ parent_ ]->MoveWindow(rect);

	edit_[ parent_ ]->SetWindowText(stringValue_);
	edit_[ parent_ ]->ShowWindow(SW_SHOW);
	edit_[ parent_ ]->SetFocus();
	edit_[ parent_ ]->SetSel(0, -1);

	selectedItem_ = this;
}

void IDPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((edit_.find(parent_) == edit_.end()))
		return;
	if (selectedItem_ != this)
		return;

	CString newStr;
	edit_[ parent_ ]->GetWindowText(newStr);
	if (stringValue_ != newStr)
	{
		stringValue_ = newStr;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	edit_[ parent_ ]->ShowWindow(SW_HIDE);
	edit_[ parent_ ]->Invalidate();

	selectedItem_ = NULL;
}

void IDPropertyItem::set(const std::string& value)
{
	stringValue_ = value.c_str();

	if ((selectedItem_ == this) && (parent_) && (edit_.find(parent_) != edit_.end()))
	{
		edit_[ parent_ ]->SetWindowText(stringValue_);
		// force the list to redraw 
		parent_->Invalidate();
	}
}

std::string IDPropertyItem::get()
{
	// don't need this to dynamically change
	return stringValue_.GetBuffer(); 
}

PropertyItem::ItemType IDPropertyItem::getType()
{
	return Type_ID;
}


// ComboPropertyItem
/*static*/ std::map<CWnd*, CComboBox*> ComboPropertyItem::comboBox_;

ComboPropertyItem::ComboPropertyItem(const CString& name, CString currentValue,
	const std::vector<std::string>& possibleValues)
	: PropertyItem( name )
	, possibleValues_( possibleValues )
{
	std::string currValue = currentValue.GetBuffer();
//	MF_ASSERT( std::find( possibleValues_.begin(), possibleValues_.end(), currValue ) != possibleValues_.end() );
	stringValue_ = currentValue;
}

ComboPropertyItem::ComboPropertyItem(const CString& name, int currentValueIndex,
	const std::vector<std::string>& possibleValues)
	: PropertyItem( name )
	, possibleValues_( possibleValues )
{
	MF_ASSERT( currentValueIndex < (int) possibleValues_.size() );
	stringValue_ = possibleValues_[currentValueIndex].c_str();
}

ComboPropertyItem::~ComboPropertyItem()
{
	if (comboBox_[ parent_ ])
	{
		comboBox_[ parent_ ]->DestroyWindow();
		delete comboBox_[ parent_ ];
	}
	comboBox_.erase( parent_ );
}

void ComboPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (comboBox_.find( parent_ ) == comboBox_.end())
	{
		comboBox_[ parent_ ] = new CComboBox();

		CRect rect(10, 10, 10, 10);
		comboBox_[ parent_ ]->Create(CBS_DROPDOWNLIST | CBS_DISABLENOSCROLL |  
					WS_CHILD | WS_BORDER | WS_VSCROLL,
					rect, parent, IDC_PROPERTYLIST_LIST);

		CFont* pFont = parent->GetParent()->GetFont();
		comboBox_[ parent_ ]->SetFont(pFont);
	}
}

void ComboPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	
	// add the possible values and get the size of the longest
	int biggestCX = 0;
	CDC* pDC = comboBox_[ parent_ ]->GetDC();

	comboBox_[ parent_ ]->ResetContent();
	for (std::vector<std::string>::iterator it = possibleValues_.begin();
		it != possibleValues_.end();
		it++)
	{
		comboBox_[ parent_ ]->AddString(it->c_str());

		CSize stringSize = pDC->GetTextExtent(it->c_str());
		if (stringSize.cx > biggestCX)
			biggestCX = stringSize.cx;
	}

	comboBox_[ parent_ ]->ReleaseDC(pDC);

	rect.top -= 2;
	rect.bottom += 256;	// extend the bottom to fit a reasonable number of choices

	// extend the left to fit all the text
	int widthDiff = biggestCX - rect.Width();
	rect.left -= widthDiff > 0 ? widthDiff : 0; 
	comboBox_[ parent_ ]->MoveWindow(rect);

	comboBox_[ parent_ ]->SelectString(-1, stringValue_);
	comboBox_[ parent_ ]->ShowWindow(SW_SHOW);
	comboBox_[ parent_ ]->SetFocus();
	if (showDropDown)
		comboBox_[ parent_ ]->ShowDropDown();

	selectedItem_ = this;
}

void ComboPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	if (selectedItem_ != this)
		return;

	// hide the list
	comboBox_[ parent_ ]->ShowWindow(SW_HIDE);
	comboBox_[ parent_ ]->Invalidate();

	selectedItem_ = NULL;
}

void ComboPropertyItem::loseFocus()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	if (selectedItem_ != this)
		return;

	if (comboBox_[ parent_ ]->GetDroppedState())
	{
		// ignore any current selection

		comboBox_[ parent_ ]->ShowWindow(SW_HIDE);
		comboBox_[ parent_ ]->Invalidate();

		selectedItem_ = NULL;
	}
	else
	{
		// accept the selection
		PropertyItem::loseFocus();
	}
}

void ComboPropertyItem::set(const std::string& value)
{
	std::vector<std::string>::iterator it = std::find( possibleValues_.begin(),
											possibleValues_.end(),
											value);
	// validate string
	if (it == possibleValues_.end())
		return;

	stringValue_ = it->c_str();

	if ((selectedItem_ == this) && (parent_) && (comboBox_.find(parent_) != comboBox_.end()))
	{
		comboBox_[ parent_ ]->SelectString(-1, stringValue_);
		// force the list to redraw 
		parent_->Invalidate();
	}
}

void ComboPropertyItem::set(int index)
{
	// validate index
	if (index >= (int)(possibleValues_.size()))
		return;

	stringValue_ = possibleValues_.at(index).c_str();

	if ((selectedItem_ == this) && (parent_) && (comboBox_.find(parent_) != comboBox_.end()))
	{
		comboBox_[ parent_ ]->SelectString(-1, stringValue_);
		// force the list to redraw 
		parent_->Invalidate();
	}
}

std::string ComboPropertyItem::get()
{
	// state already dynamically updated with callback
	return stringValue_.GetBuffer(); 
}

void ComboPropertyItem::comboChange()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	comboBox_[ parent_ ]->GetLBText(comboBox_[ parent_ ]->GetCurSel(), stringValue_);
	parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
}


// BoolPropertyItem
/*static*/ std::map<CWnd*, CComboBox*> BoolPropertyItem::comboBox_;

BoolPropertyItem::BoolPropertyItem(const CString& name, int currentValue)
	: PropertyItem( name )
{
	value_ = currentValue;
}

BoolPropertyItem::~BoolPropertyItem()
{
	delete comboBox_[ parent_ ];
	comboBox_.erase( parent_ );
}

/*virtual*/ CString BoolPropertyItem::name()
{
	if (exposedToScriptName_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + exposedToScriptName_ + ")";
		return result.c_str();
	}
}

void BoolPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (comboBox_.find( parent_ ) == comboBox_.end())
	{
		comboBox_[ parent_ ] = new CComboBox();

		CRect rect(10, 10, 10, 10);
		comboBox_[ parent_ ]->Create(CBS_DROPDOWNLIST | CBS_NOINTEGRALHEIGHT |  
					WS_CHILD | WS_BORDER,
					rect, parent, IDC_PROPERTYLIST_BOOL);

		CFont* pFont = parent->GetParent()->GetFont();
		comboBox_[ parent_ ]->SetFont(pFont);

		// add the possible values
		comboBox_[ parent_ ]->ResetContent();
		comboBox_[ parent_ ]->InsertString(0, "False");
		comboBox_[ parent_ ]->InsertString(1, "True");
	}

	// initialise the stringValue_
	comboBox_[ parent_ ]->SetCurSel(value_);
	comboBox_[ parent_ ]->GetLBText(value_, stringValue_);
}

void BoolPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	
	rect.top -= 2;
	rect.bottom += 256;	// extend the bottom to fit in all the choices
	comboBox_[ parent_ ]->MoveWindow(rect);

	comboBox_[ parent_ ]->SetCurSel(value_);
	comboBox_[ parent_ ]->GetLBText(value_, stringValue_);

	comboBox_[ parent_ ]->SetFocus();
	comboBox_[ parent_ ]->ShowWindow(SW_SHOW);
	if (showDropDown)
		comboBox_[ parent_ ]->ShowDropDown();

	selectedItem_ = this;
}

void BoolPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	if (selectedItem_ != this)
		return;

	// hide the list
	comboBox_[ parent_ ]->ShowWindow(SW_HIDE);
	comboBox_[ parent_ ]->Invalidate();

	selectedItem_ = NULL;
}

void BoolPropertyItem::loseFocus()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	if (selectedItem_ != this)
		return;

	if (comboBox_[ parent_ ]->GetDroppedState())
	{
		// ignore any changes to the selection
		comboBox_[ parent_ ]->ShowWindow(SW_HIDE);
		comboBox_[ parent_ ]->Invalidate();

		selectedItem_ = NULL;
	}
	else
	{
		// accept the selection
		PropertyItem::loseFocus();
	}
}

void BoolPropertyItem::set(bool value)
{
	value_ = (int)value;

	if ((selectedItem_ == this) && (parent_) && (comboBox_.find(parent_) != comboBox_.end()))
	{
		comboBox_[ parent_ ]->SetCurSel(value_);
		comboBox_[ parent_ ]->GetLBText(value_, stringValue_);

		// force the list to redraw 
		parent_->Invalidate();
	}
}

bool BoolPropertyItem::get()
{
	if ((selectedItem_ == this) && (parent_) && (comboBox_.find(parent_) != comboBox_.end()))
		return comboBox_[ parent_ ]->GetCurSel() != 0;
	else
		return value_ != 0;
}

void BoolPropertyItem::comboChange()
{
	if (!parent_) return; 
	if ((comboBox_.find(parent_) == comboBox_.end()))
		return;
	
	// update data if required
	CString newValue;
	comboBox_[ parent_ ]->GetLBText(comboBox_[ parent_ ]->GetCurSel(), newValue);
	if (newValue != stringValue_)
	{
		value_ = comboBox_[ parent_ ]->GetCurSel();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}
}

/*virtual*/ std::string BoolPropertyItem::menuOptions()
{
	if (exposedToScriptName_ == "")
		return L("COMMON/PROPERTY_LIST/PYTHON_OFF");
	else
		return L("COMMON/PROPERTY_LIST/PYTHON_ON");
}


// FloatPropertyItem
/*static*/ std::map<CWnd*, controls::EditNumeric*> FloatPropertyItem::editNumeric_;
/*static*/ std::map<CWnd*, controls::EditNumeric*> FloatPropertyItem::editNumericFormatting_;
/*static*/ std::map<CWnd*, controls::Slider*> FloatPropertyItem::slider_;
/*static*/ std::map<CWnd*, CButton*> FloatPropertyItem::button_;

FloatPropertyItem::FloatPropertyItem(const CString& name, float currentValue)
	: PropertyItem( name )
	, value_( currentValue )
	, min_( -FLT_MAX )
	, max_( FLT_MAX )
	, ranged_( false )
	, changing_( false )
	, hasDef_( false )
{
}

FloatPropertyItem::~FloatPropertyItem()
{
	delete editNumeric_[ parent_ ];
	editNumeric_.erase( parent_ );
	delete editNumericFormatting_[ parent_ ];
	editNumericFormatting_.erase( parent_ );
	delete slider_[ parent_ ];
	slider_.erase( parent_ );
	delete button_[ parent_ ];
	button_.erase( parent_ );
}

/*virtual*/ CString FloatPropertyItem::name()
{
	if (exposedToScriptName_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + exposedToScriptName_ + ")";
		return result.c_str();
	}
}

void FloatPropertyItem::create(CWnd* parent)
{
	parent_ = parent;
	
	if (editNumericFormatting_.find( parent_ ) == editNumericFormatting_.end())
	{
		editNumericFormatting_[ parent_ ] = new controls::EditNumeric();
	}	
	if (editNumeric_.find( parent_ ) == editNumeric_.end())
	{
		editNumeric_[ parent_ ] = new controls::EditNumeric();

		CRect rect(10, 10, 10, 10);
		editNumeric_[ parent_ ]->Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_FLOAT);

		CFont* pFont = parent->GetParent()->GetFont();
		editNumeric_[ parent_ ]->SetFont(pFont);

		editNumericFormatting_[ parent_ ]->Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_FLOAT);
	}
	if( slider_.find( parent_ ) == slider_.end())
	{
		slider_[ parent_ ] = new controls::Slider();

		CRect rect(10, 10, 10, 10);
		slider_[ parent_ ]->Create(
			TBS_HORZ | TBS_NOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect,
			parent, IDC_PROPERTYLIST_SLIDER );
	}
	if(button_.find( parent_ ) == button_.end())
	{
		button_[ parent_ ] = new CButton();

		CRect rect(10, 10, 10, 10);
		DWORD style = BS_PUSHBUTTON | WS_CHILD | WS_BORDER;
		button_[ parent_ ]->Create( "*", style, rect, parent, IDC_PROPERTYLIST_DEFAULT);

		CFont* pFont = parent->GetParent()->GetFont();
		button_[ parent_ ]->SetFont(pFont);
	}

	editNumeric_[ parent_ ]->SetValue(value_);
	stringValue_ = editNumeric_[ parent_ ]->GetStringForm();
}

void FloatPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()) && (button_.find(parent_) == button_.end()))
		return;
	// display edit box
	rect.bottom -= 1;		// keep it inside the selection

	static const int MIN_SLIDER_WIDTH = 60;
	static const int EDIT_WIDTH = 40;
	const int BUTTON_WIDTH = hasDef_ ? 10 : 0;
	const int SLIDER_WIDTH = ( ranged_ == 0 || rect.Width() - BUTTON_WIDTH < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - BUTTON_WIDTH - EDIT_WIDTH;

	rect.right -= BUTTON_WIDTH + SLIDER_WIDTH;
	editNumeric_[ parent_ ]->MoveWindow(rect);

	editNumeric_[ parent_ ]->SetMinimum(min_);
	editNumeric_[ parent_ ]->SetMaximum(max_);

	editNumeric_[ parent_ ]->SetValue(value_);
	stringValue_ = editNumeric_[ parent_ ]->GetStringForm();

	editNumeric_[ parent_ ]->ShowWindow(SW_SHOW);
	editNumeric_[ parent_ ]->SetFocus();
	editNumeric_[ parent_ ]->SetSel(0, -1);

	rect.left = rect.right;
	rect.right += SLIDER_WIDTH;
	if( SLIDER_WIDTH != 0 )
	{
		slider_[ parent_ ]->MoveWindow(rect);
		slider_[ parent_ ]->SetRange( (int)( min_ * pow( 10.f, digits_ ) ), (int)( max_ * pow( 10.f, digits_ ) ) );
		slider_[ parent_ ]->SetPos( (int)( value_ * pow( 10.f, digits_ ) ) );
		slider_[ parent_ ]->ShowWindow(SW_SHOW);
		slider_[ parent_ ]->ClearSel( true );
	}

	rect.left = rect.right;
	rect.right += BUTTON_WIDTH;
	if( BUTTON_WIDTH != 0 )
	{
		button_[ parent_ ]->MoveWindow(rect);
		button_[ parent_ ]->ShowWindow(SW_SHOW);
	}

	selectedItem_ = this;
}

void FloatPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()) && (button_.find(parent_) == button_.end()))
		return;
	if (selectedItem_ != this)
		return;

	// this is called at the end of editing
	// store the value for next time
	CString newValue;
	editNumeric_[ parent_ ]->SetNumericText(TRUE);
	editNumeric_[ parent_ ]->GetWindowText(newValue);
	if (newValue != stringValue_)
	{
		value_ = editNumeric_[ parent_ ]->GetValue();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	// close the edit window
	editNumeric_[ parent_ ]->SendMessage(EN_KILLFOCUS);
	editNumeric_[ parent_ ]->ShowWindow(SW_HIDE);
	editNumeric_[ parent_ ]->Invalidate();

	slider_[ parent_ ]->ShowWindow(SW_HIDE);
	slider_[ parent_ ]->Invalidate();

	button_[ parent_ ]->ShowWindow(SW_HIDE);
	button_[ parent_ ]->Invalidate();

	selectedItem_ = NULL;
}

void FloatPropertyItem::setRange( float min, float max, int digits )
{
	ranged_ = true;
	min_ = min;
	max_ = max;
	digits_ = digits;
}

void FloatPropertyItem::setDefault( float def )
{
	hasDef_ = true;
	def_ = def;
}

void FloatPropertyItem::set(float value)
{
	value_ = value;

	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	if (editNumericFormatting_.find(parent_) == editNumericFormatting_.end())
		return;

	editNumericFormatting_[ parent_ ]->SetValue(value);
	stringValue_ = editNumericFormatting_[ parent_ ]->GetStringForm();
	
	if (selectedItem_ == this)
		editNumeric_[ parent_ ]->SetValue(value);

	// force the list to redraw 
	// TODO: only redraw the particular item
	if (parent_)
		parent_->Invalidate();
}

float FloatPropertyItem::get()
{
	if ((selectedItem_ == this) && (parent_) && (editNumeric_.find(parent_) != editNumeric_.end()))
		return editNumeric_[ parent_ ]->GetValue();
	else
		return value_; 
}

void FloatPropertyItem::sliderChange( int value, bool transient )
{
	if (!parent_) return; 
	if( !changing_ ) 
	{
		changing_ = true;
		set( (float)( value / pow( 10.f, digits_ )));
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, (WPARAM)transient, (LPARAM)changeBuddy_);
		changing_ = false;
	}
}

void FloatPropertyItem::onDefault()
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	if( !changing_ ) 
	{
		changing_ = true;
		editNumeric_[ parent_ ]->SetValue((float)( def_/pow(10.f, digits_) ));
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
		if( ranged_ )
			slider_[ parent_ ]->SetPos( (int)( editNumeric_[ parent_ ]->GetValue() * pow( 10.f, digits_ ) ) );
		changing_ = false;
	}
}

void FloatPropertyItem::editChange()
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	if( !changing_ ) 
	{
		changing_ = true;
		if( ranged_ )
			slider_[ parent_ ]->SetPos( (int)( editNumeric_[ parent_ ]->GetValue() * pow(10.f, digits_) ) );
		changing_ = false;
	}
}

/*virtual*/ std::string FloatPropertyItem::menuOptions()
{
	if (exposedToScriptName_ == "")
		return L("COMMON/PROPERTY_LIST/PYTHON_OFF");
	else
		return L("COMMON/PROPERTY_LIST/PYTHON_ON");
}

/*virtual*/ controls::EditNumeric* FloatPropertyItem::ownEdit()
{
	if (editNumeric_.find( parent_ ) == editNumeric_.end())
		return NULL;
	return editNumeric_[parent_];
}

// IntPropertyItem
/*static*/ std::map<CWnd*, controls::EditNumeric*> IntPropertyItem::editNumeric_;
/*static*/ std::map<CWnd*, controls::EditNumeric*> IntPropertyItem::editNumericFormatting_;
/*static*/ std::map<CWnd*, controls::Slider*> IntPropertyItem::slider_;

IntPropertyItem::IntPropertyItem(const CString& name, int currentValue)
	: PropertyItem( name )
	, value_( currentValue )
	, min_( INT_MIN )
	, max_( INT_MAX )
	, ranged_( false )
	, changing_( false )
{
}

IntPropertyItem::~IntPropertyItem()
{
	delete editNumeric_[ parent_ ];
	editNumeric_.erase( parent_ );
	delete editNumericFormatting_[ parent_ ];
	editNumericFormatting_.erase( parent_ );
	delete slider_[ parent_ ];
	slider_.erase( parent_ );
}

/*virtual*/ CString IntPropertyItem::name()
{
	if (exposedToScriptName_ == "")
	{
		return L(name_);
	}
	else
	{	
		std::string result = L(name_);
		result += " (" + exposedToScriptName_ + ")";
		return result.c_str();
	}
}

void IntPropertyItem::create(CWnd* parent)
{
	parent_ = parent;

	if (editNumericFormatting_.find( parent_ ) == editNumericFormatting_.end())
	{
		editNumericFormatting_[ parent_ ] = new controls::EditNumeric();
	}
	if (editNumeric_.find( parent_ ) == editNumeric_.end())
	{
		editNumeric_[ parent_ ] = new controls::EditNumeric();

		editNumeric_[ parent_ ]->SetNumericType( controls::EditNumeric::ENT_INTEGER );

		CRect rect(10, 10, 10, 10);
		editNumeric_[ parent_ ]->Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_INT);

		CFont* pFont = parent->GetParent()->GetFont();
		editNumeric_[ parent_ ]->SetFont(pFont);

		editNumericFormatting_[ parent_ ]->SetNumericType( controls::EditNumeric::ENT_INTEGER );
		editNumericFormatting_[ parent_ ]->Create(ES_LEFT | ES_AUTOHSCROLL | WS_CHILD | WS_BORDER,
						rect, parent, IDC_PROPERTYLIST_INT);
	}
	if (slider_.find( parent_ ) == slider_.end())
	{
		slider_[ parent_ ] = new controls::Slider();

		CRect rect(10, 10, 10, 10);
		slider_[ parent_ ]->Create(
			TBS_HORZ | TBS_NOTICKS | WS_CHILD | WS_VISIBLE | WS_TABSTOP, rect,
			parent, IDC_PROPERTYLIST_SLIDER );
	}

	editNumeric_[ parent_ ]->SetIntegerValue(value_);
	stringValue_ = editNumeric_[ parent_ ]->GetStringForm();
}

void IntPropertyItem::select( CRect rect, bool showDropDown /* = true */ )
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	// display edit box
	rect.bottom -= 1;		// keep it inside the selection

	static const int MIN_SLIDER_WIDTH = 60;
	static const int EDIT_WIDTH = 40;
	const int SLIDER_WIDTH = ( ranged_ == 0 || rect.Width() < EDIT_WIDTH + MIN_SLIDER_WIDTH ) ? 0 : rect.Width() - EDIT_WIDTH;

	rect.right -= SLIDER_WIDTH;
	editNumeric_[ parent_ ]->MoveWindow(rect);

	editNumeric_[ parent_ ]->SetMinimum((float)min_);
	editNumeric_[ parent_ ]->SetMaximum((float)max_);

	editNumeric_[ parent_ ]->SetIntegerValue(value_);
	stringValue_ = editNumeric_[ parent_ ]->GetStringForm();

	editNumeric_[ parent_ ]->ShowWindow(SW_SHOW);
	editNumeric_[ parent_ ]->SetFocus();
	editNumeric_[ parent_ ]->SetSel(0, -1);

	rect.left = rect.right;
	rect.right += SLIDER_WIDTH;
	if( SLIDER_WIDTH != 0 )
	{
		slider_[ parent_ ]->MoveWindow(rect);
		slider_[ parent_ ]->SetRange( min_, max_ );
		slider_[ parent_ ]->SetPos( value_ );
		slider_[ parent_ ]->ShowWindow(SW_SHOW);
	}
	selectedItem_ = this;
}

void IntPropertyItem::deselect()
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	if (selectedItem_ != this)
		return;

	CString newValue = editNumeric_[ parent_ ]->GetStringForm();
	if (newValue != stringValue_)
	{
		value_ = editNumeric_[ parent_ ]->GetIntegerValue();
		stringValue_ = newValue;
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, 0, (LPARAM)changeBuddy_);
	}

	// kill the edit window
	editNumeric_[ parent_ ]->SendMessage(EN_KILLFOCUS);
	editNumeric_[ parent_ ]->ShowWindow(SW_HIDE);
	editNumeric_[ parent_ ]->Invalidate();

	slider_[ parent_ ]->ShowWindow(SW_HIDE);
	slider_[ parent_ ]->Invalidate();
	selectedItem_ = NULL;
}

void IntPropertyItem::setRange( int min, int max )
{
	ranged_ = true;
	min_ = min;
	max_ = max;
}

void IntPropertyItem::set(int value)
{
	value_ = value;

	if (!parent_) return; 
	if (editNumeric_.find(parent_) == editNumeric_.end())
		return;
	if (editNumericFormatting_.find(parent_) == editNumericFormatting_.end())
		return;

	editNumericFormatting_[ parent_ ]->SetIntegerValue(value);
	stringValue_ = editNumericFormatting_[ parent_ ]->GetStringForm();

	if (selectedItem_ == this)
		editNumeric_[ parent_ ]->SetIntegerValue(value);

	// force the list to redraw 
	parent_->Invalidate();
}

int IntPropertyItem::get()
{
	if ((selectedItem_ == this) && (parent_) && (editNumeric_.find(parent_) != editNumeric_.end()))
		return editNumeric_[ parent_ ]->GetIntegerValue();
	else
		return value_; 
}

void IntPropertyItem::sliderChange( int value, bool transient )
{
	if (!parent_) return; 
	if( !changing_ )
	{
		changing_ = true;
		set( value );
		parent_->SendMessage(WM_CHANGE_PROPERTYITEM, (WPARAM)transient, (LPARAM)changeBuddy_);
		changing_ = false;
	}
}

void IntPropertyItem::editChange()
{
	if (!parent_) return; 
	if ((editNumeric_.find(parent_) == editNumeric_.end()) && (slider_.find(parent_) == slider_.end()))
		return;
	if( !changing_ )
	{
		changing_ = true;
		if( ranged_ )
			slider_[ parent_ ]->SetPos( editNumeric_[ parent_ ]->GetIntegerValue() );
		changing_ = false;
	}
}

/*virtual*/ std::string IntPropertyItem::menuOptions()
{
	if (exposedToScriptName_ == "")
		return L("COMMON/PROPERTY_LIST/PYTHON_OFF");
	else
		return L("COMMON/PROPERTY_LIST/PYTHON_ON");
}

/*virtual*/ controls::EditNumeric* IntPropertyItem::ownEdit()
{
	if (editNumeric_.find( parent_ ) == editNumeric_.end())
		return NULL;
	return editNumeric_[parent_];
}


// PropertyList
/*static*/ CButton PropertyList::s_arrayDeleteButton_;

PropertyList::PropertyList():
	enabled_(true),
	tooltipsEnabled_(false),
	delayRedraw_(false)
{
}

PropertyList::~PropertyList()
{
}

void PropertyList::enable( bool enable )
{
	ModifyStyle( enable ? WS_DISABLED : 0, enable ? 0 : WS_DISABLED );
	enabled_ = enable;
	OnPaint();
}

void PropertyList::OnPaint()
{
	//We need to enable tooltips here since it must be inited first
	//and there is no "OnInitDialog" equivalent for a CListBox.
	if (!tooltipsEnabled_)
	{
		EnableToolTips( TRUE );
		tooltipsEnabled_ = true;
	}

	if (!enabled_)
	{
		CClientDC dc(this);
		CRect clientRect;
		GetClientRect( clientRect );
		CBrush brush( ::GetSysColor( COLOR_BTNFACE ));
		dc.FillRect( clientRect, &brush );
		ValidateRect( clientRect );
	}
	CListBox::OnPaint();
}

BEGIN_MESSAGE_MAP(PropertyList, CListBox)
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
	ON_CONTROL_REFLECT(LBN_SELCHANGE, OnSelchange)
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_MESSAGE(WM_CHANGE_PROPERTYITEM, OnChangePropertyItem)
	ON_WM_VSCROLL()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_CBN_SELCHANGE(IDC_PROPERTYLIST_LIST, OnComboChange)
	ON_CBN_SELCHANGE(IDC_PROPERTYLIST_BOOL, OnComboChange)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_COMMAND( IDC_PROPERTYLIST_BROWSE, OnBrowse )
	ON_COMMAND( IDC_PROPERTYLIST_DEFAULT, OnDefault )
	ON_COMMAND_RANGE( IDC_PROPERTYLIST_CUSTOM_MIN, IDC_PROPERTYLIST_CUSTOM_MAX, OnCustom )
	ON_COMMAND( IDC_PROPERTYLIST_ARRAY_DEL, OnArrayDelete )
	ON_WM_HSCROLL()
	ON_EN_CHANGE( IDC_PROPERTYLIST_FLOAT, OnEditChange )
	ON_EN_CHANGE( IDC_PROPERTYLIST_INT, OnEditChange )

	ON_NOTIFY_EX( TTN_NEEDTEXT, 0, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)

END_MESSAGE_MAP()


// PropertyList message handlers

void PropertyList::PreSubclassWindow() 
{
	dividerPos_ = 0;
	dividerMove_ = false;
	dividerTop_ = 0;
	dividerBottom_ = 0;
	dividerLastX_ = 0;

	selected_ = 0;

	cursorSize_ = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
	cursorArrow_ = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
}


void PropertyList::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	lpMeasureItemStruct->itemHeight = 20;
}

const int childIndent = 16;

void PropertyList::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) 
{
	if (delayRedraw_) return;
		
	UINT index = lpDrawItemStruct->itemID;
	if (index == (UINT)-1)
		return;

	PropertyItem* item = (PropertyItem*)GetItemDataPtr( index );

	// draw two rectangles, one for the property lable, one for the value
	// (unless is a group)
	int nCrBackground, nCrText;
	if( (lpDrawItemStruct->itemAction | ODA_SELECT) && 
		(lpDrawItemStruct->itemState & ODS_SELECTED) &&
		(item->getType() != PropertyItem::Type_Group) )
	{
		nCrBackground = COLOR_HIGHLIGHT;
		nCrText = COLOR_HIGHLIGHTTEXT;
	}
	else
	{
		nCrBackground = COLOR_WINDOW;
		nCrText = COLOR_WINDOWTEXT;
	}
	COLORREF crBackground = ::GetSysColor(nCrBackground);
	COLORREF crText = ::GetSysColor(nCrText);

	CRect rectItem = lpDrawItemStruct->rcItem;

	CDC dc;
	dc.Attach( lpDrawItemStruct->hDC );
	dc.FillSolidRect(rectItem, crBackground);

	int border = 1;
	rectItem.right -= border;
	rectItem.left += border;
	rectItem.top += border;
	rectItem.bottom -= border;

	CRect rectLabel = lpDrawItemStruct->rcItem;
	CRect rectValue = lpDrawItemStruct->rcItem;
	CRect rectColour = lpDrawItemStruct->rcItem;

	rectLabel.left =  item->getGroupDepth() * childIndent;
	if ( item->arrayCallback() != NULL )
	{
		// At the moment, nested, arrays of arrays are not supported
		rectLabel.left += childIndent;
	}
	if ((item->getType() == PropertyItem::Type_Group) ||
		(item->getType() == PropertyItem::Type_Colour) ||
		(item->getType() == PropertyItem::Type_Vector))
	{
		if ((item->getType() == PropertyItem::Type_Group))
		{
			// change the background colour
			nCrBackground = COLOR_INACTIVECAPTIONTEXT;
			crBackground = ::GetSysColor(nCrBackground);
			dc.FillSolidRect(rectLabel, crBackground);
		}

		// + / -
		CRect rcSign(lpDrawItemStruct->rcItem);
		rcSign.top = (int)(0.5f * (rcSign.bottom - rcSign.top)) + rcSign.top;
		rcSign.bottom = rcSign.top;
		rcSign.right = rectLabel.left - (int)(childIndent * 0.5f);
		rcSign.left = rcSign.right;
		rcSign.InflateRect(5, 5, 7, 7);

		dc.DrawEdge( rcSign, EDGE_RAISED, BF_RECT );
		
		CPoint ptCenter( rcSign.CenterPoint() );
		ptCenter.x -= 1;
		ptCenter.y -= 1;

		CPen pen_( PS_SOLID, 1, crText );
		CPen* oldPen = dc.SelectObject( &pen_ );
		
		// minus		
		dc.MoveTo(ptCenter.x - 3, ptCenter.y);
		dc.LineTo(ptCenter.x + 4, ptCenter.y);

		GroupPropertyItem * gItem = (GroupPropertyItem *)(item);
		if (!gItem->getExpanded())
		{
			// plus
			dc.MoveTo(ptCenter.x, ptCenter.y - 3);
			dc.LineTo(ptCenter.x, ptCenter.y + 4);
		}

		dc.SelectObject( oldPen );

		if (item->getType() == PropertyItem::Type_Colour)
		{
			ColourPropertyItem * cItem = (ColourPropertyItem *)(item);
			
			if (dividerPos_ == 0)
				dividerPos_ = rectValue.Width() / 2;
			rectValue.left = dividerPos_;

			rectColour = rectValue;
			rectColour.left -= 1;
			rectColour.right = rectColour.left + rectColour.Height();
			rectColour.bottom -= 2;

			std::string colour = cItem->get();

			if (colour != "") 
			{
				int r,g,b,a;
				sscanf( colour.c_str(), "%d , %d , %d , %d", &r, &g, &b, &a );
				dc.FillSolidRect( rectColour, RGB(r,g,b) );
			}

			rectLabel.right = rectValue.left - 1;
		}
		else if (item->getType() == PropertyItem::Type_Vector)
		{
			if (dividerPos_ == 0)
				dividerPos_ = rectValue.Width() / 2;
			rectValue.left = dividerPos_;

			rectLabel.right = rectValue.left - 1;
		}
	}
	else if (item->getType() == PropertyItem::Type_Label )
	{
		// do nothing
	}
	else if (item->getType() == PropertyItem::Type_Label_Highlight )
	{
		// use highlighting
		// change the background colour
		nCrBackground = COLOR_INACTIVECAPTIONTEXT;
		crBackground = ::GetSysColor(nCrBackground);
		dc.FillSolidRect(rectLabel, crBackground);
	}
	else
	{
		if (dividerPos_ == 0)
			dividerPos_ = rectValue.Width() / 2;
		rectValue.left = dividerPos_;
		rectLabel.right = rectValue.left - 1;
	}


	dc.DrawEdge( rectLabel, EDGE_ETCHED,BF_BOTTOMRIGHT );
	if (item->getType() != PropertyItem::Type_Group &&
			item->getType() != PropertyItem::Type_Label &&
			item->getType() != PropertyItem::Type_Label_Highlight)
		dc.DrawEdge( rectValue, EDGE_ETCHED,BF_BOTTOM );

	if (item->getType() == PropertyItem::Type_Colour )
	{
		rectValue.left = rectColour.right + 4;
	}

	border = 3;
	rectLabel.right -= border;
	rectLabel.left += border;
	rectLabel.top += border;
	rectLabel.bottom -= border;

	rectValue.right -= border;
	rectValue.left += border;
	rectValue.top += border;
	rectValue.bottom -= border;

	// write the property name in the first rectangle
	// value in the second rectangle
	COLORREF crOldBkColor = dc.SetBkColor(crBackground);
	COLORREF crOldTextColor = dc.SetTextColor(crText);

	dc.DrawText(item->name(), rectLabel, DT_LEFT | DT_SINGLELINE);
	if (item->getType() != PropertyItem::Type_Group &&
			item->getType() != PropertyItem::Type_Label &&
			item->getType() != PropertyItem::Type_Label_Highlight)
		dc.DrawText(item->value(), rectValue, DT_LEFT | DT_SINGLELINE);

	dc.SetTextColor(crOldTextColor);
	dc.SetBkColor(crOldBkColor);

	dc.Detach();
}


void PropertyList::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (dividerMove_)
	{
		// redraw columns for new divider position
		dividerMove_ = false;

		//if mouse was captured then release it
		if (GetCapture() == this)
			::ReleaseCapture();

		::ClipCursor(NULL);

		DrawDivider(point.x);	// remove the divider
		
		//set the divider position to the new value
		dividerPos_ = point.x + 2;

		//Do this to ensure that the value field(s) are moved to the new divider position
		selChange( false );
		
		//redraw
		Invalidate();
	}
	else
	{
		// select the item under the cursor
		BOOL out;
		UINT index = ItemFromPoint(point,out);
		if (!out && index != (uint16)-1)
			Select( index );

		CListBox::OnLButtonUp(nFlags, point);
	}
}


void PropertyList::OnLButtonDown(UINT nFlags, CPoint point) 
{


	BOOL out;
	int index = ItemFromPoint(point,out);

	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
		item = (PropertyItem*)GetItemDataPtr( index );

	if ((point.x >= dividerPos_ - 4) && (point.x <= dividerPos_ - 1))
	{
		// get ready to resize the divider
		::SetCursor(cursorSize_);

		// keep mouse inside the control
		CRect windowRect;
		GetWindowRect(windowRect);
		windowRect.left += 10; windowRect.right -= 10;
		::ClipCursor(windowRect);

		//Unselect the item first
		deselectCurrentItem();

		CRect clientRect;
		GetClientRect(clientRect);
		dividerMove_ = true;
		dividerTop_ = clientRect.top;
		dividerBottom_ = clientRect.bottom;
		dividerLastX_ = point.x;

		DrawDivider(dividerLastX_);

		//capture the mouse
		SetCapture();
		SetFocus();

		return;
	}

	// select the item
	CListBox::OnLButtonDown(nFlags, point);

	if (item)
	{
		if (item->getType() == PropertyItem::Type_Group)
		{
			int xBoundUpper = item->getGroupDepth() * childIndent;
			int xBoundLower = xBoundUpper - childIndent;
			if (point.x >= xBoundLower  &&  point.x <= xBoundUpper)
			{
				GroupPropertyItem * gItem = (GroupPropertyItem *)(item);
				if (gItem->getExpanded())
					collapseGroup( gItem, index );
				else
					expandGroup( gItem, index );
			}
		}
		else if ((item->getType() == PropertyItem::Type_Colour) ||
				(item->getType() == PropertyItem::Type_Vector))
		{
			int xBoundUpper = item->getGroupDepth() * childIndent;
			int xBoundLower = xBoundUpper - childIndent;
			if (point.x >= xBoundLower  &&  point.x <= xBoundUpper)
			{
				ColourPropertyItem * cItem = (ColourPropertyItem *)(item);
				if (cItem->getExpanded())
					collapseGroup( cItem, index );
				else
					expandGroup( cItem, index );
			}
		}
	}

	dividerMove_ = false;
}


void PropertyList::OnMouseMove(UINT nFlags, CPoint point) 
{	
	BOOL out;
	UINT index = ItemFromPoint(point,out);
	
	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
		item = (PropertyItem*)GetItemDataPtr( index );

	// handle the resizing divider
	if (dividerMove_)
	{
		//move divider line
		DrawDivider(dividerLastX_);	// remove old
		DrawDivider(point.x);			// draw new
		dividerLastX_ = point.x;
	}
	else if( item && (item->getType() != PropertyItem::Type_Group) &&
			(item->getType() != PropertyItem::Type_Label) &&
			(item->getType() != PropertyItem::Type_Label_Highlight) &&
			(point.x >= dividerPos_ - 4) && (point.x <= dividerPos_ - 1) )
	{
		//set the cursor to a sizing cursor if the cursor is over the row divider
		::SetCursor(cursorSize_);
	}
	else
	{
		CListBox::OnMouseMove(nFlags, point);
	}
}

void PropertyList::DrawDivider(int xpos)
{
	CClientDC dc(this);
	int nOldMode = dc.SetROP2(R2_NOT);	// draw inverse of screen colour
	dc.MoveTo(xpos, dividerTop_);
	dc.LineTo(xpos, dividerBottom_);
	dc.SetROP2(nOldMode);
}

void PropertyList::OnSize( UINT nType, int cx, int cy )
{
	if ( PropertyItem::selectedItem() )
	{
		//Do this to ensure that the value field(s) are resized to the new size
		selChange( false );
		
		//Redraw
		Invalidate();
	}
	CListBox::OnSize( nType, cx, cy );
}

void PropertyList::establishGroup( PropertyItem* item )
{
	std::string group = item->getGroup();

	// build current group
	std::string currentGroup = "";
	for (uint i = 0; i < parentGroupStack_.size(); i++)
	{
		currentGroup += parentGroupStack_[i]->name().GetBuffer();
		currentGroup += "/";
	}
	if (currentGroup != "")
		currentGroup.erase( --currentGroup.end() );

	if (currentGroup == group)
		return;

	if (group.empty())
	{
		parentGroupStack_.clear();
		return;
	}

	if (parentGroupStack_.size() == 0)
	{
		makeSubGroup( group, item );
		return;
	}
	
	// look forward from the root
	uint stackIndex = 0;
	int startIndex = 0;
	int endIndex = group.find( "/" );
	bool stackTooBig = false;
	bool stackTooSmall = false;
	while (!stackTooBig && !stackTooSmall)
	{
		std::string name = group.substr( startIndex, 
								endIndex == -1 ? -1 : endIndex - startIndex );
		if (name != parentGroupStack_[stackIndex]->name().GetBuffer())
		{
			stackTooBig = true;
			stackTooSmall = true;
		}
		else
		{
			if (endIndex == -1)
				stackTooBig = true;

			startIndex = endIndex + 1;
			endIndex = group.find( "/", startIndex );
		}

		if (++stackIndex == parentGroupStack_.size())
		{
			stackTooSmall = true;
		}
	}

	if (stackTooBig)
	{
		// pop things off the stack
		MF_ASSERT( parentGroupStack_.size() > 0 );
		if( stackTooSmall )
			stackIndex--;
		int diff = parentGroupStack_.size() - stackIndex;
		for (int i = 0; i < diff; i++)
			parentGroupStack_.pop_back();
	}

	if (stackTooSmall)
	{
		// make new subgroups
		std::string subGroup = group.substr( startIndex );
		makeSubGroup( subGroup, item );
	}
}

void PropertyList::makeSubGroup( const std::string & subGroup, PropertyItem* item )
{
	int startIndex = 0;
	int endIndex = subGroup.find( "/" );
	while (endIndex != -1)
	{
		addGroupToStack( subGroup.substr( startIndex, endIndex - startIndex ) );
		startIndex = endIndex + 1;
		endIndex = subGroup.find( "/", startIndex );
	} 
	addGroupToStack( subGroup.substr( startIndex ), item );
}

void PropertyList::addGroupToStack( const std::string & label, PropertyItem* item )
{
	int groupDepth = parentGroupStack_.size() + 1;

	GroupPropertyItem * newItem = NULL;
	if (item && item->getType() == PropertyItem::Type_Group)
	{
		MF_ASSERT( label.c_str() == item->name() );
		newItem = (GroupPropertyItem *)item;
		newItem->setGroupDepth( groupDepth );
	}
	else
	{
		newItem = new GroupPropertyItem( label.c_str(), groupDepth );

		int index = InsertString( -1, "" );
		SetItemDataPtr(index, newItem);
		newItem->create(this);
	}

	if (parentGroupStack_.size() > 0)
		parentGroupStack_.back()->addChild( newItem );
	parentGroupStack_.push_back( newItem );
}

int PropertyList::AddPropItem(PropertyItem* item)
{
	establishGroup( item );

	if (parentGroupStack_.size() > 0  &&
		item->getType() != PropertyItem::Type_Group)
	{
		parentGroupStack_.back()->addChild( item );
	}

	int index = InsertString( -1, "" );
	SetItemDataPtr(index, item);
	item->create(this);

	if ( !arrayCallback_.empty() )
		item->arrayData( arrayIndex_.top(), arrayCallback_.top().getObject() );

	if( GetCurSel() == LB_ERR )
		SetCurSel( index );
	return index;
}

void PropertyList::Select(int selected)
{
	selected_ = selected;
}


void PropertyList::OnSelchange() 
{
	selChange( true );
}

void PropertyList::selChange( bool showDropDown )
{
	selected_ = GetCurSel();
	CRect rect;
	GetItemRect( selected_, rect );
	rect.left = dividerPos_;

	deselectCurrentItem();

	if (selected_ != -1)
	{
		PropertyItem* item = (PropertyItem*) GetItemDataPtr( selected_ );
		if ((item) && (item->getSelectable()))
		{
			if ( item->arrayCallback() != NULL )
			{
				rect.right -= rect.Height();
			}
			
			item->select( rect, showDropDown );
			GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, (LPARAM)(item->getChangeBuddy()));

			if ( item->arrayCallback() != NULL )
			{
				CRect butRect( 0, 0, 1, 1 );
				if ( !s_arrayDeleteButton_.GetSafeHwnd() )
				{
					s_arrayDeleteButton_.Create(
						"-",
						BS_PUSHBUTTON | WS_CHILD,
						butRect,
						this,
						IDC_PROPERTYLIST_ARRAY_DEL );
					s_arrayDeleteButton_.SetFont( GetParent()->GetFont() );
				}
				butRect.left = rect.right;
				butRect.right = butRect.left + rect.Height();
				butRect.top = rect.top;
				butRect.bottom = rect.bottom;
				s_arrayDeleteButton_.MoveWindow(butRect);
				s_arrayDeleteButton_.ShowWindow(SW_SHOWNOACTIVATE);
				s_arrayDeleteButton_.SetWindowPos( &CWnd::wndTop, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE );
			}
		}
		else
		{
			GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, 0);
		}
	}
}

void PropertyList::clear()
{
	if (GetSafeHwnd() == NULL)
		return;

	// tell current item it is not selected
	deselectCurrentItem();

	// empty the list box
	ResetContent();
	selected_ = -1;
	parentGroupStack_.clear();

	GetParent()->SendMessage(WM_SELECT_PROPERTYITEM, 0, 0);
}


afx_msg LRESULT PropertyList::OnChangePropertyItem(WPARAM wParam, LPARAM lParam)
{
	GetParent()->SendMessage(WM_CHANGE_PROPERTYITEM, wParam, lParam);
	return 0;
}

bool PropertyList::changeSelectItem( int delta )
{
	if (selected_ == -1)
		return false;

	if (GetCount() == 0)
		return false;

	int newItem = selected_;
	newItem += delta;

	if (newItem < 0)
	{
		deselectCurrentItem();
		Select( -1 );
		return false;
	}

	if (newItem >= GetCount())
	{
		deselectCurrentItem();
		Select( -1 );
		return false;
	}

	Select( newItem );

	SetCurSel(selected_);
	selChange( true );

	return true;
}

bool PropertyList::selectItem( int itemIndex )
{
	if (GetCount() == 0)
		return false;

	if (itemIndex < 0)
	{
		deselectCurrentItem();
		Select( -1 );
		return false;
	}

	if (itemIndex >= GetCount())
	{
		deselectCurrentItem();
		Select( -1 );
		return false;
	}

	Select( itemIndex );

	SetCurSel(selected_);
	selChange( true );

	return true;
}

/*static*/ void PropertyList::deselectCurrentItem()
{
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->deselect();

	if ( s_arrayDeleteButton_.GetSafeHwnd() != NULL )
	{
		s_arrayDeleteButton_.ShowWindow(SW_HIDE);
		s_arrayDeleteButton_.DestroyWindow();
	}
}

void PropertyList::selectPrevItem()
{
	if (GetCount() == 0)
		return;

	int nextItem = selected_;
	if( nextItem == 0)
		nextItem = GetCount() - 1;
	else
		--nextItem;
	Select( nextItem );

	SetCurSel(selected_);
	OnSelchange();
}

void PropertyList::selectNextItem()
{
	if (GetCount() == 0)
		return;

	int nextItem = selected_;
	nextItem += 1;
	if (nextItem >= GetCount())
		nextItem = 0;
	Select( nextItem );

	SetCurSel(selected_);
	OnSelchange();
}

void PropertyList::setDividerPos( int x )
{
	dividerPos_ = x;
}

void PropertyList::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CListBox::OnVScroll(nSBCode, nPos, pScrollBar);

	//Force a redraw
	Invalidate();
}

void PropertyList::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar->GetDlgCtrlID() == IDC_PROPERTYLIST_SLIDER)
	{
		int pos = ((CSliderCtrl*)pScrollBar)->GetPos();
		bool transient = (nSBCode != TB_ENDTRACK);
		PropertyItem::selectedItem()->sliderChange( pos, transient );
		
	}
	else
	{
		CListBox::OnHScroll(nSBCode, nPos, pScrollBar);
	}
}

void PropertyList::OnSetFocus(CWnd* pOldWnd)
{
	if ((selected_ == -1) && (GetAsyncKeyState(VK_TAB) < 0))
	{
		if (GetAsyncKeyState(VK_SHIFT) < 0)
		{
			Select( GetCount() - 1 );
		}
		else
		{
			Select( 0 );
		}
			
		SetCurSel( selected_ );
		selChange( true );
	}

	CListBox::OnSetFocus( pOldWnd );
}

void PropertyList::OnKillFocus(CWnd* pNewWnd)
{
	deselectCurrentItem();

	CListBox::OnKillFocus( pNewWnd );
}

void PropertyList::OnComboChange()
{
	if ( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->comboChange();
}

void PropertyList::OnBrowse()
{
	PropertyItem::selectedItem()->onBrowse();
}

void PropertyList::OnDefault()
{
	PropertyItem::selectedItem()->onDefault();
}


/**
 *	Forwards messages with id between IDC_PROPERTYLIST_CUSTOM_MIN and 
 *	IDC_PROPERTYLIST_CUSTOM_MAX to the property item
 *
 *	@param nID	id of the command, between IDC_PROPERTYLIST_CUSTOM_MIN and MAX
 */
void PropertyList::OnCustom(UINT nID)
{
	PropertyItem::selectedItem()->onCustom(nID);
}


/**
 *	Notifies the array property through the arrayCallback that the array
 *	"Delete Item" button ahs been pressed in one of the array items.
 */
void PropertyList::OnArrayDelete()
{
	PropertyItem* item = PropertyItem::selectedItem();
	if ( item && item->arrayCallback() != NULL )
		(*item->arrayCallback())( item->arrayIndex() );
}


void PropertyList::OnEditChange( )
{
	if( PropertyItem::selectedItem() )
		PropertyItem::selectedItem()->editChange();
}

PropertyItem * PropertyList::getHighlightedItem()
{
	if (selected_ == -1) return NULL;
	void* pItem =  GetItemDataPtr(selected_);

	if (pItem == reinterpret_cast<void*>(-1)) return NULL;

	return static_cast<PropertyItem*>(pItem);
}

void PropertyList::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	BOOL out;
	int index = ItemFromPoint(point,out);
	PropertyItem* item = 0;
	if (!out && index != (uint16)(-1))
	{
		item = (PropertyItem*) GetItemDataPtr(index);
	}
	
	GetParent()->SendMessage(WM_DBLCLK_PROPERTYITEM, 0, (LPARAM)item);
}

void PropertyList::OnRButtonUp( UINT, CPoint point )
{
	BOOL out;
	int index = ItemFromPoint(point,out);
	PropertyItem* item = 0;
	if (!out && index != (uint16)(-1))
	{
		Select( index );
		SetCurSel( selected_ );
		selChange( false );
		item = (PropertyItem*) GetItemDataPtr(index);
	}

	GetParent()->SendMessage(WM_RCLK_PROPERTYITEM, 0, (LPARAM)item);
}

/*afx_msg*/ HBRUSH PropertyList::OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor )
{
	HBRUSH brush = CListBox::OnCtlColor( pDC, pWnd, nCtlColor );

	
	PropertyItem * item = getHighlightedItem();
	if (item)
	{
		controls::EditNumeric* edit = item->ownEdit();

		if (edit)
		{
			if (edit->isRanged())
			{
				edit->SetBoundsColour( pDC, pWnd, edit->GetMinimum(), edit->GetMaximum() );
			}
		}
	}

	return brush;
}


void PropertyList::collapseGroup(GroupPropertyItem* gItem, int index)
{
	if (!gItem->getExpanded())
		return;

	delayRedraw_ = true;
	
	PropertyItemVector & children = gItem->getChildren();

	// hide all the children
	for (PropertyItemVector::iterator it = children.begin();
		it != children.end();
		it++)
	{
		DeleteString(index + 1);

		// remove all of their children
		if ( ( (*it)->getType() == PropertyItem::Type_Group )  ||
			( (*it)->getType() == PropertyItem::Type_Colour ) ||
			( (*it)->getType() == PropertyItem::Type_Vector ) ) 
		{
			GroupPropertyItem * g = (GroupPropertyItem *)(*it);
			collapseGroup( g, index + 1 );
		}
	}

	gItem->setExpanded( false );
	selectItem( index );
	delayRedraw_ = false;
}

void PropertyList::expandGroup(GroupPropertyItem* gItem, int index)
{
	if (gItem->getExpanded())
		return;

	delayRedraw_ = true;

	PropertyItemVector & children = gItem->getChildren();

	// show all the children (one 1 level)
	for (PropertyItemVector::iterator it = children.begin();
		it != children.end();
		it++)
	{
		index++;
		InsertString( index, "" );
		SetItemDataPtr(index, *it);
		(*it)->create(this);
	}

	gItem->setExpanded( true );

	delayRedraw_ = false;
}


/**
 *	This method is called by array properties when they are about to elect
 *	their item's properties, so the property list knows the views added
 *	correspond to the array.
 *
 *	@param callback		callback with one int parameter: index of the array
 *						item whose "Delete Item" button has been pressed.
 */
void PropertyList::startArray( BWBaseFunctor1<int>* callback )
{
	arrayIndex_.push( 0 );
	arrayCallback_.push( callback );
}


/**
 *	This method is called every time an array item's property is elected, so
 *	all the views and PropertyItem objects created have this 'index' associated
 *	as the index in the array.
 *
 *	@param index	desired index to assign to property items added subsequently.
 */
void PropertyList::arrayIndex( int index )
{
	arrayIndex_.top() = index;
}


/**
 *	This method must be called to signal that no more property items will be
 *	added for the array, so other non-array property items can be added properly.
 */
void PropertyList::endArray()
{
	arrayIndex_.pop();
	arrayCallback_.pop();
}


CRect PropertyList::dropTest( CPoint point, const std::string& fileName )
{
	ScreenToClient( &point );
	BOOL out;
	int index = ItemFromPoint( point, out );

	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
		item = (PropertyItem*)GetItemDataPtr( index );

	//Make sure the item is a string
	if ( item  &&  (item->getType() == PropertyItem::Type_String) )
	{
		StringPropertyItem* stringItem = (StringPropertyItem*)item;
		std::string ext = fileName.substr( fileName.rfind(".") );

		//Ensure that the file type is accepted by the file filter
		if ((stringItem->fileFilter() != "") &&
			(stringItem->fileFilter().find(ext) != std::string::npos))
		{
			return rect;
		}
	}

	return CRect(0,0,0,0);
}

bool PropertyList::doDrop( CPoint point, const std::string& fileName )
{
	ScreenToClient( &point );
	BOOL out;
	int index = ItemFromPoint( point, out );

	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
		item = (PropertyItem*)GetItemDataPtr( index );

	//Make sure the item is a string
	if ( item  &&  (item->getType() == PropertyItem::Type_String))
	{
		StringPropertyItem* stringItem = (StringPropertyItem*)item;
		std::string ext = fileName.substr( fileName.rfind(".") );

		//Ensure that the file type is accepted by the file filter
		if ((stringItem->fileFilter() != "") &&
			(stringItem->fileFilter().find(ext) != std::string::npos))
		{
			stringItem->set( fileName );
			return true;
		}
	}

	return false;
}

/*static*/ WCHAR PropertyList::s_tooltipBuffer_[512];

int PropertyList::OnToolHitTest(CPoint point, TOOLINFO * pTI) const
{
	BOOL out;
	UINT index = ItemFromPoint( point, out );
	
	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
	{
		item = (PropertyItem*)GetItemDataPtr( index );
	}
	
	if (!item)
	{
		if (mainFrame_)
			mainFrame_->SetMessageText( "" );	
		return -1;
	}

	if (mainFrame_)
	{
		std::string desc = "";
		if ( item->UIDescL() != "" )
		{
			desc = item->UIDescL();
			if ( item->UIDescExtra() != "" )
			{
				desc = desc + " (" + item->UIDescExtra() + ")";
			}
		}
		mainFrame_->SetMessageText( desc.c_str() );
	}
	
	//Get the client (area occupied by this control
	RECT rcClient;
	GetClientRect( &rcClient );

	//Fill in the TOOLINFO structure
	pTI->hwnd = GetSafeHwnd();
	pTI->uId = (UINT)(item);
	pTI->lpszText = LPSTR_TEXTCALLBACK;
	pTI->rect = rcClient;

	return pTI->uId; //By returning a unique value per listItem,
					//we ensure that when the mouse moves over another list item,
					//the tooltip will change
}

BOOL PropertyList::OnToolTipText(UINT id, NMHDR *pNMHDR, LRESULT *pResult)
{
	//Ensure that we are able to use newlines in the tooltip text
	CToolTipCtrl* pToolTip = AfxGetModuleState()->m_thread.GetDataNA()->m_pToolTip;
	pToolTip->SetMaxTipWidth( SHRT_MAX );
  
	//Handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;

	//Ignore messages from the built in tooltip, we are processing them internally
	if( (pNMHDR->idFrom == (UINT)m_hWnd) &&
		 ( ((pNMHDR->code == TTN_NEEDTEXTA) && (pTTTA->uFlags & TTF_IDISHWND)) ||
         ((pNMHDR->code == TTN_NEEDTEXTW) && (pTTTW->uFlags & TTF_IDISHWND)) ) ){
      return FALSE;
   }

   *pResult = 0;

	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point); 
	BOOL out;
	UINT index = ItemFromPoint( point, out );
	
	CRect rect;
	GetItemRect( index, rect );

	//Determine the item
	PropertyItem* item = NULL;
	if ((!out) && (index != (uint16)(-1)) && (rect.PtInRect( point )))
	{
		item = (PropertyItem*)GetItemDataPtr( index );
	}
	
	if (item)
	{
		CString strTipText = "";
		
		if ( item->UIDescL() != "" )
		{
			if ( item->UIDescExtra() != "" )
			{
				strTipText.Format("%s\n%s", item->UIDescL().c_str(), item->UIDescExtra().c_str() );
			}
			else
			{
				strTipText.Format("%s", item->UIDescL().c_str() );
			}
		}
		else if ( item->UIDescExtra() != "" )
		{
			strTipText.Format("%s", item->UIDescExtra().c_str() );
		}

		pTTTA->lpszText = (LPSTR)(&s_tooltipBuffer_);

		if (pNMHDR->code == TTN_NEEDTEXTA)
 			lstrcpyn(pTTTA->lpszText, strTipText, sizeof(s_tooltipBuffer_));
		else
			_mbstowcsz(pTTTW->lpszText, strTipText, sizeof(s_tooltipBuffer_));
		*pResult = 0;

		return TRUE;    // message was handled
	}

	return FALSE; // message was not handled
}