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
#include "controls/edit_numeric.hpp"
#include "controls/user_messages.hpp"
#include "resmgr/string_provider.hpp"
#include <afxole.h>
#include <math.h>
#include <float.h>

using namespace controls;

static const int numberDecimalsAllowed = 3;

/////////////////////////////////////////////////////////////////////////////
// EditNumeric

EditNumeric::EditNumeric()
	: type_(ENT_FLOAT)
	, decimalPointPresent_(FALSE)
	, displayThousandsSeperator_(FALSE)
	, value_(0.f)
	, minimum_(-FLT_MAX)
	, includeMinimum_(true)
	, maximum_(FLT_MAX)
	, includeMaximum_(true)
	, allowNegative_(true)
	, allowEmpty_(false)
	, isEmpty_(true)
	, previousText_("")
	, dirty_(false)
	, commitOnFocusLoss_(true)
	, numDecimals_(3)
	, silent_( true )
{
}

EditNumeric::~EditNumeric()
{
}


void EditNumeric::initInt( int minVal, int maxVal, int val )
{
	SetNumericType( controls::EditNumeric::ENT_INTEGER );
	SetAllowNegative( minVal < 0 );
	SetMinimum( float( minVal ) );
	SetMaximum( float( maxVal ) );
	SetIntegerValue( val );
}


void EditNumeric::initFloat( float minVal, float maxVal, float val )
{
	SetNumericType( controls::EditNumeric::ENT_FLOAT );
	SetAllowNegative( minVal < 0 );
	SetMinimum( minVal );
	SetMaximum( maxVal );
	SetValue( val );
}


BEGIN_MESSAGE_MAP(EditNumeric, CEdit)
	//{{AFX_MSG_MAP(EditNumeric)
	ON_WM_CHAR()
	ON_CONTROL_REFLECT_EX(EN_CHANGE, OnChange)
	ON_CONTROL_REFLECT_EX(EN_SETFOCUS, OnSetfocus)
	ON_CONTROL_REFLECT_EX(EN_KILLFOCUS, OnKillfocus)
	//}}AFX_MSG_MAP
   ON_MESSAGE(WM_PASTE, OnPaste)
END_MESSAGE_MAP()

//ON_CONTROL_REFLECT


/////////////////////////////////////////////////////////////////////////////
// EditNumeric message handlers

void EditNumeric::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	int start, end;
	GetSel(start, end);

	if (nChar == 13) // Catch enter press...
	{
		OnKillfocus();  // commit the contents
		return;			// Ignore the enter
	}
	
	// if character suitable, continue
	if(DoesCharacterPass(nChar, start))
	{
		CEdit::OnChar(nChar, nRepCnt, nFlags);
	}
}


BOOL EditNumeric::DoesCharacterPass(UINT nChar, int index)
{ 
	BOOL allowed = TRUE;

	// if the character is not allowed, return without doing anything
	// check isprint to ensure that unprinted characters are passed to cedit (ctrl-v,etc.)
	if(!isdigit(nChar) && isprint(nChar))
	{
		allowed = FALSE;
		switch(nChar)
		{
			case '.':
				{
					int start, end;
					GetSel(start, end);
					CString tempString;
					GetWindowText(tempString);
					int decimalPlace = tempString.Find(".");

					allowed =  ((type_ == ENT_FLOAT) && ((decimalPlace == -1) || ((decimalPlace >= start) && (decimalPlace < end))));

					break;
				}
			case '-':      // allow '-' only as first character
				{
					if (allowNegative_ && (index == 0))
						allowed = TRUE;
					break;
				}
			default:
				break;
		} 
	}
    
	// do not allow more than numDecimals_ decimal places
	if (type_ == ENT_FLOAT)
	{
		int start, end;
		GetSel(start, end);
		bool multiSelected = start != end;
		CString tempString;
		GetWindowText(tempString);
		int decimalPlace = tempString.Find(".");
		int numberDecimals = tempString.GetLength() - decimalPlace;
		if (!multiSelected &&
			(decimalPlace != -1) && (index > decimalPlace) && 
			(numberDecimals > numDecimals_) && 
			(nChar != 0x8)) // 0x8 == backspace
		{
			allowed = false;
		}
	}

	return allowed;
}


BOOL EditNumeric::OnChange()
{
	// update the current value
	SetNumericValue();

	// update the real object
	// advise only if there was a change
	CString windowText;
	GetWindowText(windowText);

	isEmpty_ = ((allowEmpty_) && (windowText == ""));

	if (windowText != previousText_)
	{
		int start, end;
		GetSel(start, end);

		// tell our parent and ourselves of a change
		SendMessage(WM_EDITNUMERIC_CHANGE);
		GetParent()->SendMessage(WM_EDITNUMERIC_CHANGE);

		// the change might fail in the SendMessage
		// also, previousText_ may change, do not check against this
		CString finalText;
		GetWindowText(finalText);
		if (finalText == windowText)
		{
			SetSel(start, start);
			previousText_ = windowText;
		}
	}

	// update whether the decimal has been used or not
	if (type_ == ENT_INTEGER)
		return FALSE;

	CString tempString;
	GetWindowText(tempString);

	int decimalPlace = tempString.Find(".");
	if (decimalPlace != -1)
	{
		decimalPointPresent_ = TRUE;
	}
	else
	{
		decimalPointPresent_ = FALSE;
	}
	return FALSE;
}

void EditNumeric::SetNumericText(BOOL useFormatting)
{
	SetText(GetStringForm(useFormatting));
}


CString EditNumeric::GetFormattedString( const float& value, BOOL insertFormatting )
{
	if (value == 0.0f)
	{
		CString finalString;
		if (type_ == ENT_INTEGER)
			finalString = "0";
		else
			finalString = "0.0";
	
		return finalString;
	}

	// Round to the appropriate number of decimal points
	double roundEpsilon = (value > 0.0f) ?
			5.0 / pow( 10.0, numDecimals_ + 1 ) :
			-5.0 / pow( 10.0, numDecimals_ + 1 );
	double dValue = (
			BW_ROUND( double( value ) * pow( 10.0, numDecimals_ ) ) + roundEpsilon
					) / ( pow( 10.0, numDecimals_ ) );

	// take dValue from edit control's float dValue
	const int maxStringLength = 50;	// pretty big!
	char charString[maxStringLength];
	bw_snprintf( charString, sizeof(charString), "%.6f", dValue );
	CString initialString = charString;

	int	signNegative = 0;
	if (initialString.Find('-') != -1)
	{
		// must be at the beginning (ensured elsewhere)
		signNegative = 1;
		CString trimCharacters("-");
		initialString.TrimLeft(trimCharacters);
	}

	int decimalPlace = initialString.Find('.');

	if (decimalPlace != -1)
	{
		// remove trailing '0'
		CString trimCharacters("0");
		initialString.TrimRight(trimCharacters);

		// remove the decimal (put back later)
		initialString.Delete(decimalPlace);

		// keep at least one zero after the decimal
		if (initialString.GetLength() == decimalPlace)
			initialString.AppendChar('0');
	}

	int string_length = initialString.GetLength();

	// restrict number of decimals to numDecimals_ (float inaccuracies may occur soon after this )
	string_length = min(string_length, decimalPlace + numDecimals_);

	// put together the final rendered string
	CString finalString = _T("");

	if(signNegative == 0)
	{
		finalString = _T("");
	}
	else
	{
		finalString = _T("-");
	}

	for(int i = 0; i < string_length; i++)
	{
		int wholenum_place = decimalPlace - i;
		if(insertFormatting && displayThousandsSeperator_ && i > 0 && wholenum_place > 2)
		{
			if((wholenum_place % 3) == 0)
			{
				finalString = finalString + _T(",");
			}
		}
		if(decimalPlace == i)
		{
			if (type_ == ENT_INTEGER)
				break;

			finalString = finalString + _T(".");
		}

		finalString = finalString + initialString[i];
	}

	return finalString;
}


CString EditNumeric::GetStringForm(BOOL useFormatting)
{
	// special case if the field is empty (and is allowed to be)
	if ((allowEmpty_) && (isEmpty_))
	{
		return "";
	}
		
	return this->GetFormattedString( value_, useFormatting );
}

void EditNumeric::SetText(const CString & text)
{
	previousText_ = text;
	SetWindowText( text );
}

/**
 *  Checks if the value is beyond the passed range.  Sets the text colour to
 *	red if it is.
 *
 *	@param pDC	Contains a pointer to the display context for the child window.
 *	@param pWnd	Contains a pointer to the control asking for the color.
 *	@param editMinValue	The lower bound of the specified range.
 *	@param editMaxValue	The upper bound of the specified range.
 *	@return	True if this edit numeric was to be checked, false otherwise.
 */
bool EditNumeric::SetBoundsColour(
		CDC* pDC, CWnd* pWnd, float editMinValue, float editMaxValue )
{
	bool checked = false;
	if (this->m_hWnd == pWnd->m_hWnd)
	{
		checked = true;

		CString text;
		this->GetWindowText( text );
		if (atof( text ) > (editMaxValue + std::numeric_limits<float>::epsilon()) ||
			atof( text ) < (editMinValue - std::numeric_limits<float>::epsilon()))
		{
			pDC->SetTextColor( 0x0000ff );
		}
	}
	return checked;
}

void EditNumeric::SetNumericValue()
{
	CString tempString;
	GetWindowText(tempString);

	// remove the commas
	tempString.Remove(',');

	float newValue = static_cast<float>(atof(tempString));

	// restrict the range of values
	if (newValue <= minimum_)
	{
		newValue = minimum_;
		if (!includeMinimum_)
			newValue += (float)pow( 0.1, numDecimals_ );
	}

	if (newValue >= maximum_)
	{
		newValue = maximum_;
		if (!includeMaximum_)
			newValue -= (float)pow( 0.1, numDecimals_ );
	}

	if ( GetStringForm( false ) != tempString )
		value_ = newValue;
}


void EditNumeric::SetValue( float value ) 
{
	isEmpty_ = false;

	// restrict the range of values
	if (value <= minimum_)
	{
		value = minimum_;
		if (!includeMinimum_)
			value += (float)pow( 0.1, numDecimals_ );
	}

	if (value >= maximum_)
	{
		value = maximum_;
		if (!includeMaximum_)
			value -= (float)pow( 0.1, numDecimals_ );
	}

	// round off the value correctly
	value_ = GetRoundedNumber( value );

	//Render with formating
	SetNumericText( TRUE );

	//Avoid a superfluous update
	oldValue_ = value_;
}

float EditNumeric::GetRoundedNumber( float value ) const
{
	// Round to the appropriate number of decimal points
	return float( BW_ROUND(double( value ) * pow( 10.0, numDecimals_ )) /
									pow( 10.0, numDecimals_ ) );
}

void EditNumeric::Clear() 
{
	if (!allowEmpty_) return;
	SetWindowText("");
	value_ = 0.f;
	isEmpty_ = true;
}


BOOL EditNumeric::OnSetfocus() 
{
	// set text without formatting, since going into user edit mode
	SetNumericText(FALSE);
	oldValue_ = value_;
	SetSel(0,-1);

	return FALSE; // Pass on this event
}

void EditNumeric::doCommit( bool focusLost /* = false */ )
{
	SetNumericValue();

	// Render
	SetNumericText(TRUE);

	// advise of change
	if (value_ != oldValue_)
	{
		if ((!focusLost) || (commitOnFocusLoss_))
		{
			dirty_ = true;
		}
		oldValue_ = value_;
		SendMessage(WM_EDITNUMERIC_FINAL_CHANGE);
		GetParent()->SendMessage(WM_EDITNUMERIC_FINAL_CHANGE);
	}
}

BOOL EditNumeric::OnKillfocus() 
{
	if (silent_ || isValidValue())
	{
		doCommit( true );

		return FALSE; // Pass on this event
	}
	else
	{
		this->SetFocus();
		return TRUE;
	}
}


bool EditNumeric::isValidValue()
{
	CString tempString;
	GetWindowText(tempString);

	// remove the commas
	tempString.Remove(',');

	float newValue = static_cast<float>(atof(tempString));

	// restrict the range of values
	if (newValue < minimum_)
	{
		CString warningTooSmall = L(
			"CONTROLS/EDIT_NUMERIC/VALUE_TOO_SMALL",
			(LPCTSTR)this->GetFormattedString( minimum_, TRUE ) );

		std::string warningTooSmallTitle = L( "CONTROLS/EDIT_NUMERIC/VALUE_TOO_SMALL_TITLE" );
		
		this->silent_ = true;
		bool ret = MessageBox( warningTooSmall, warningTooSmallTitle.c_str(), 
				MB_OKCANCEL | MB_DEFBUTTON1 | MB_ICONWARNING ) == IDOK;

		this->silent_ = false;

		return ret;

	}
	else if (newValue > maximum_)
	{
		CString warningTooBig = L(
			"CONTROLS/EDIT_NUMERIC/VALUE_TOO_BIG",
			(LPCTSTR)this->GetFormattedString( maximum_, TRUE ) );
		std::string warningTooBigTitle = L( "CONTROLS/EDIT_NUMERIC/VALUE_TOO_BIG_TITLE" );

		this->silent_ = true;
		bool ret = MessageBox( warningTooBig, warningTooBigTitle.c_str(),
			MB_OKCANCEL | MB_DEFBUTTON1 | MB_ICONWARNING ) == IDOK;
		this->silent_ = false;

		return ret;

	}

	return true;

}


void EditNumeric::SetDisplayThousandsSeperator(BOOL displaySeperator)
{
	displayThousandsSeperator_ = displaySeperator;

	// Render
	SetNumericText(TRUE);
}


void EditNumeric::SetNumericType(EditNumericType type)
{
	type_ = type;
}


void EditNumeric::SetMinimum(float minimum, bool includeMinimum)
{
	minimum_ = minimum;
	includeMinimum_ = includeMinimum;

	maximum_ = max(maximum_, minimum_);

	allowNegative_ = minimum_ < 0.f;
}

void EditNumeric::SetMaximum(float maximum, bool includeMaximum)
{
	maximum_ = maximum;
	includeMaximum_ = includeMaximum;

	minimum_ = min(maximum_, minimum_);
}

float EditNumeric::GetMinimum()
{
	return minimum_;
}

float EditNumeric::GetMaximum()
{
	return maximum_;
}

bool EditNumeric::isRanged()
{
	return ( minimum_ != -FLT_MAX ) || ( maximum_ != FLT_MAX );
}

// this signature used for pasting function in order to get around
// bug mentioned in Article ID: Q195032 of the MS knowledge base 
LRESULT EditNumeric::OnPaste(WPARAM Wparam, LPARAM LParam) 
{
	UINT this_char;
	int start_sel, end_sel, i;
	BOOL does_clipstring_pass = TRUE, pre_was_decimal_used;
	CString buffer, workstr;
	COleDataObject	obj;

	if (obj.AttachClipboard()) 
	{
		if (obj.IsDataAvailable(CF_TEXT)) 
		{
			HGLOBAL hmem = obj.GetGlobalData(CF_TEXT);
			CMemFile sf((BYTE*) ::GlobalLock(hmem), (UINT) ::GlobalSize(hmem));
			CString buffer;

			LPSTR str = buffer.GetBufferSetLength((UINT) ::GlobalSize(hmem));
			sf.Read(str, (UINT) ::GlobalSize(hmem));
			::GlobalUnlock(hmem);

			// pass characters one at a time to control, allowing the control
			// to decide whether to paste it or not (this could be funky for mixed
			// alpha and digit strings)
			buffer.ReleaseBuffer();
			buffer.FreeExtra();

			//  Check buffer for validity- if any character invalid, don't paste
			GetSel(start_sel, end_sel);

			// if decimal is within selection, allow a decimal to be pasted
			// save decimal use flag
			pre_was_decimal_used = decimalPointPresent_;
			if(pre_was_decimal_used == TRUE)
			{
				CString temp_str;
				GetWindowText(temp_str);
				for(i = start_sel; i < end_sel; i++)
					if(temp_str[i] == '.')
						decimalPointPresent_ = FALSE;
			}

			for(i = 0; i < buffer.GetLength(); i++)
			{
				this_char = buffer[i];
				if(!DoesCharacterPass(this_char, start_sel+i))
				{
					does_clipstring_pass = FALSE;
					break;
				}
			}

			// reset decimal use flag
			decimalPointPresent_ = pre_was_decimal_used;

			// continue default windows processing if string okay
			if(does_clipstring_pass)
			{
				Default();
			}
			else   // let user know that paste wasn't allowed
			{
				workstr = L("CONTROLS/EDIT_NUMERIC/CANNOT_PASTE_TEXT");
				std::string title = L("CONTROLS/EDIT_NUMERIC/CANNOT_PASTE_TITLE");
				MessageBox( workstr, title.c_str(), MB_OK | MB_ICONINFORMATION);
			}
		}
	}

	return 0;  
}
