/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CONTROLS_EDIT_NUMERIC_HPP
#define CONTROLS_EDIT_NUMERIC_HPP


#include "controls/defs.hpp"
#include "controls/fwd.hpp"


/////////////////////////////////////////////////////////////////////////////
// EditNumeric

namespace controls
{
	class CONTROLS_DLL EditNumeric : public CEdit
	{
	public:
		enum EditNumericType
		{
			ENT_FLOAT = 1,
			ENT_INTEGER = 2
		};

		EditNumeric();
		virtual ~EditNumeric();
		
		void		initInt( int minVal, int maxVal, int val );
		void		initFloat( float minVal, float maxVal, float val );

		void		SetDisplayThousandsSeperator(BOOL displaySeperator);
		void		SetNumericType(EditNumericType type);
		
		void		SetMinimum(float minimum, bool includeMinimum = true);
		void		SetMaximum(float maximum, bool includeMaximum = true);

		float		GetMinimum();
		float		GetMaximum();

		bool		isRanged();
		
		void		SetAllowNegative(bool option) { allowNegative_ = option; }
		void		SetAllowEmpty(bool option) { allowEmpty_ = option; }
		
		void		SetNumDecimals( int num ) { numDecimals_ = num; }

		void		setSilent( bool value ) { silent_ = value; }
		bool		silent() const	{ return silent_; }
		
		// Operations
		float		GetValue() const			{ return value_; }
		int			GetIntegerValue() const		{ return (int)(value_); }
		void		SetValue(float value);
		void		SetIntegerValue(int value)	{ SetValue((float)(value)); }

		float		GetRoundedNumber(float value) const;
		
		void		Clear();
		bool		isEmpty() { return isEmpty_; }
		
		CString	GetStringForm(BOOL useFormatting = true);
		void		SetNumericText(BOOL InsertFormatting);
		
		void		SetText(const CString & text);
		bool		SetBoundsColour(CDC* pDC, CWnd* pWnd,
									float editMinValue, float editMaxValue);
		
		void commitOnFocusLoss( bool state ) { commitOnFocusLoss_ = state; }
		
		bool needsUpdate()	{ return dirty_; }
		void updateDone()	{ dirty_ = false; }
		bool doUpdate()		{ bool temp = dirty_; dirty_ = false; return temp; }
		bool		isValidValue();
		CString GetFormattedString( const float& value, BOOL insertFormatting ); 
		
	private:
		void doCommit( bool focus = false );

		bool				commitOnFocusLoss_;
		
		bool				dirty_;
		
		BOOL				decimalPointPresent_;
		int					decimalPointIndex_;
		BOOL				displayThousandsSeperator_;
		float				value_;
		float				oldValue_;
		EditNumericType		type_;
		CString				previousText_;
		
		float				minimum_;
		bool				includeMinimum_;
		float				maximum_;
		bool				includeMaximum_;
		
		bool				allowNegative_;
		bool				allowEmpty_;
		bool				isEmpty_;
		
		int					numDecimals_;

		bool				silent_;
		
		void		SetNumericValue();
		BOOL		DoesCharacterPass(UINT nChar, int CharacterPosition);
		
		// Generated message map functions
	protected:
		//{{AFX_MSG(EditNumeric)
		afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
		afx_msg BOOL OnChange();
		afx_msg BOOL OnSetfocus();
		afx_msg BOOL OnKillfocus();
		//}}AFX_MSG
		afx_msg LRESULT OnPaste(WPARAM Wparam, LPARAM LParam);
		
		DECLARE_MESSAGE_MAP()
	};
}

#endif