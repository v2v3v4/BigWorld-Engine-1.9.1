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
#include "message_box.hpp"
#include "controls/show_cursor_helper.hpp"

static BOOL CALLBACK enumWindowsProc( HWND hwnd, LPARAM lParam )
{
	char className[1024];
	GetClassName( hwnd, className, sizeof( className ) );
	if (strcmp( className, "tooltips_class32" ) &&
		strcmp( className, "#32770" ) &&// #32770 is the general dialog class
		GetWindowLong( hwnd, GWL_STYLE ) & WS_VISIBLE)// only visible window
	{
		DWORD processId;
		GetWindowThreadProcessId( hwnd, &processId );
		if( processId == GetCurrentProcessId() )
		{
			*(HWND*)lParam = hwnd;
			return FALSE;
		}
	}
	return TRUE;
}

static HWND getDefaultParent()
{
	HWND hwnd = NULL;
	EnumWindows( enumWindowsProc, (LPARAM)&hwnd );
	return hwnd;
}

std::map<HWND, MsgBox*> MsgBox::wndMap_;
std::map<const MsgBox*, HWND> MsgBox::msgMap_;

MsgBox::MsgBox( const std::string& caption, const std::string& text,
		const std::string& buttonText1 /*= ""*/, const std::string& buttonText2 /*= ""*/,
		const std::string& buttonText3 /*= ""*/, const std::string& buttonText4 /*= ""*/ )
	:caption_( caption ), text_( text ), result_( TIME_OUT ), timeOut_( INFINITE ),
	font_( NULL ), topmost_( false )
{
	if( !buttonText1.empty() )
		buttons_.push_back( buttonText1 );
	if( !buttonText2.empty() )
		buttons_.push_back( buttonText2 );
	if( !buttonText3.empty() )
		buttons_.push_back( buttonText3 );
	if( !buttonText4.empty() )
		buttons_.push_back( buttonText4 );
}

MsgBox::MsgBox( const std::string& caption, const std::string& text,
	const std::vector<std::string>& buttonTexts )
	:caption_( caption ), text_( text ), buttons_( buttonTexts ), result_( TIME_OUT )
	, timeOut_( INFINITE ), font_( NULL ), topmost_( false )
{}

MsgBox::~MsgBox()
{
	if( font_ )
		DeleteObject( font_ );
}

unsigned int MsgBox::doModal( HWND parent /* = NULL */, bool topmost /*= false*/, unsigned int timeOutTicks /*= INFINITE*/ )
{
	topmost_ = topmost;
	if( parent == NULL )
		parent = getDefaultParent();
	if( buttons_.empty() && timeOutTicks == INFINITE )
	{
		result_ = TIME_OUT;
		return result_;
	}
	model_ = true;

	DLGTEMPLATE dlg[ 2 ] = { { 0 } };
	dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
	dlg->dwExtendedStyle = 0;
	dlg->cdit = 0;
	dlg->x = 0;
	dlg->y = 0;
	dlg->cx = 100;
	dlg->cy = 100;

	timeOut_ = timeOutTicks;

	ShowCursorHelper scopedShowCursor( true );

	result_ = 
        (unsigned int)
        DialogBoxIndirectParam
        ( 
            GetModuleHandle( NULL ), 
            dlg, 
            parent, 
            dialogProc, 
            (LPARAM)this 
        );

	return getResult();
}

void MsgBox::doModalless( HWND parent /* = NULL */, unsigned int timeOutTicks /*= INFINITE*/ )
{
	topmost_ = false;
	if( parent == NULL )
		parent = getDefaultParent();
	if( buttons_.empty() && timeOutTicks == INFINITE )
	{
		result_ = TIME_OUT;
		return;
	}
	model_ = false;

	DLGTEMPLATE dlg[ 2 ] = { { 0 } };
	dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_VISIBLE | WS_SYSMENU;
	dlg->dwExtendedStyle = WS_EX_TOPMOST;
	dlg->cdit = 0;
	dlg->x = 0;
	dlg->y = 0;
	dlg->cx = 100;
	dlg->cy = 100;

	timeOut_ = timeOutTicks;

	CreateDialogIndirectParam( GetModuleHandle( NULL ), dlg, parent, dialogProc, (LPARAM)this );
}

unsigned int MsgBox::getResult() const
{
	if( result_ == IDOK )
		return 0;
	else if( result_ == IDCANCEL )
		return (unsigned int)buttons_.size() - 1;
	return result_ - IDCANCEL;
}

bool MsgBox::stillActive() const
{
	return msgMap_.find( this ) != msgMap_.end();
}

static void centerWindow( HWND hwnd )
{
	RECT parentRect;
	RECT selfRect;
	HWND parent = GetParent( hwnd );
	if( !parent )
		parent = GetDesktopWindow();
	GetWindowRect( parent, &parentRect );
	GetWindowRect( hwnd, &selfRect );
	LONG x = ( parentRect.right + parentRect.left ) / 2 - ( selfRect.right - selfRect.left ) / 2;
	LONG y = ( parentRect.bottom + parentRect.top ) / 2 - ( selfRect.bottom - selfRect.top ) / 2;
	MoveWindow( hwnd, x, y, selfRect.right - selfRect.left, selfRect.bottom - selfRect.top, TRUE );
}

INT_PTR CALLBACK MsgBox::dialogProc( HWND hwnd, UINT msg, WPARAM w, LPARAM l )
{
	switch( msg )
	{
	case WM_INITDIALOG:
		wndMap_[ hwnd ] = (MsgBox*)l;
		msgMap_[ (MsgBox*)l ] = hwnd;
		wndMap_[ hwnd ]->create( hwnd );
		if( wndMap_[ hwnd ]->timeOut_ != INFINITE )
			SetTimer( hwnd, 1, wndMap_[ hwnd ]->timeOut_, NULL );
		centerWindow( hwnd );
		break;
	case WM_TIMER:
		wndMap_[ hwnd ]->result_ = TIME_OUT;
		wndMap_[ hwnd ]->kill( hwnd );
		break;
	case WM_DESTROY:
		msgMap_.erase( msgMap_.find( wndMap_[ hwnd ] ) );
		wndMap_.erase( wndMap_.find( hwnd ) );
		break;
	case WM_COMMAND:
		wndMap_[ hwnd ]->result_ = (unsigned int)w;
		wndMap_[ hwnd ]->kill( hwnd );
		break;
	}
	return FALSE;
}

void MsgBox::create( HWND hwnd )
{
	// this isn't the best layout solution, but should be
	// more than enough for now

	// 0. preparation
	SetWindowText( hwnd, caption_.c_str() );
	font_ = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	// 1. const definition
	static const int VERTICAL_MARGIN = 10, HORIZONTAL_MARGIN = 10;
	static const int MIN_BUTTON_WIDTH = 81, BUTTON_HEIGHT = 21;
	static const int MIN_BUTTON_MARGIN = 10;
	static const int MIN_BUTTON_SPACE = 10;
	static const int MIN_DIALOG_WIDTH = 324;

	const double PHI = 1.618;

	// 2. static size
	HWND wndStatic = CreateWindow( "STATIC", text_.c_str(), WS_CHILD | WS_VISIBLE,
		0, 0, 10, 10, hwnd, NULL, GetModuleHandle( NULL ), NULL );
	SendMessage( wndStatic, WM_SETFONT, (WPARAM)font_, FALSE );

	int staticHeight = 0, staticWidth = MIN_DIALOG_WIDTH - 2 * HORIZONTAL_MARGIN;
	HDC clientDC = GetDC( wndStatic );

	std::string s = text_;
	if( s.find( '\n' ) == s.npos )
		s += "\n.";

	for(;;)
	{
		RECT rect = { 0, 0, staticWidth, staticHeight };
		DrawText( clientDC, s.c_str(), -1, &rect, DT_CALCRECT );
		if( rect.bottom < staticHeight )
			break;
		staticHeight += 20;
		staticWidth = int( staticHeight * PHI );
		if( staticWidth < MIN_DIALOG_WIDTH - 2 * HORIZONTAL_MARGIN )
			staticWidth = MIN_DIALOG_WIDTH - 2 * HORIZONTAL_MARGIN;
	}

	/*
	std::string temp = s;
	
	char* resToken = strtok( (char*)temp.c_str(), "\n" );
	while ( resToken != 0 )
	{
		RECT rect = { 0, 0, 0, 0 };
		DrawText( clientDC, resToken, -1, &rect, DT_CALCRECT );
		if (staticWidth < rect.right - rect.left)
			staticWidth = rect.right - rect.left;
		resToken = strtok( 0, "\n" );
	}
	{
		RECT rect = { 0, 0, staticWidth, 512 };
		DrawText( clientDC, s.c_str(), -1, &rect, DT_CALCRECT );
		staticHeight = rect.bottom - rect.top;
	}
	*/

	ReleaseDC( wndStatic, clientDC );

	// 3. button size
	HWND wndButton = NULL;
	int buttonWidth = MIN_BUTTON_WIDTH;
	if( !buttons_.empty() )
	{
		wndButton = CreateWindow( "BUTTON", buttons_[0].c_str(), WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
			0, 0, 10, 10, hwnd, (HMENU)IDOK, GetModuleHandle( NULL ), NULL );
		SendMessage( wndButton, WM_SETFONT, (WPARAM)font_, FALSE );

		HDC clientDC = GetDC( wndButton );
		SelectObject( clientDC, font_ );

		for( std::vector<std::string>::iterator iter = buttons_.begin();
			iter != buttons_.end(); ++iter )
		{
			RECT rect = { 0, 0, buttonWidth, BUTTON_HEIGHT };
			DrawText( clientDC, iter->c_str(), -1, &rect, DT_CALCRECT );
			if( buttonWidth < rect.right + 2 * MIN_BUTTON_MARGIN )
				buttonWidth = rect.right + 2 * MIN_BUTTON_MARGIN;
		}

		ReleaseDC( wndButton, clientDC );
	}

	// 4. dialog size
	int dialogWidth = MIN_DIALOG_WIDTH, dialogHeight;
	int buttonSpace = MIN_BUTTON_SPACE;

	if( dialogWidth < staticWidth + 2 * HORIZONTAL_MARGIN )
		dialogWidth = staticWidth + 2 * HORIZONTAL_MARGIN;

	if( buttonSpace * ( buttons_.size() + 1 ) + buttonWidth * buttons_.size()
		< unsigned( dialogWidth - HORIZONTAL_MARGIN * 2 ) )
		buttonSpace = ( dialogWidth - HORIZONTAL_MARGIN * 2 - buttonWidth * (int)buttons_.size() )
			/ ( (int)buttons_.size() + 1 );
	else
		dialogWidth = buttonSpace * ( (int)buttons_.size() + 1 ) + buttonWidth * (int)buttons_.size()
			+ HORIZONTAL_MARGIN * 2;

	if( dialogWidth > staticWidth + 2 * HORIZONTAL_MARGIN )
		staticWidth = dialogWidth - 2 * HORIZONTAL_MARGIN;

	if( buttons_.empty() )
		dialogHeight = 2 * VERTICAL_MARGIN + staticHeight;
	else
		dialogHeight = 3 * VERTICAL_MARGIN + staticHeight + BUTTON_HEIGHT;

	// 5. go
	RECT rect = { 0, 0, dialogWidth, dialogHeight};
	AdjustWindowRect( &rect, GetWindowLong( hwnd, GWL_STYLE ), FALSE );
	rect.right -= rect.left;
	rect.bottom -= rect.top;

	HWND parentWnd = GetParent( hwnd );
	if( GetWindowLong( parentWnd, GWL_STYLE ) & WS_MINIMIZE )
		ShowWindow( parentWnd, SW_RESTORE );
	RECT parentRect;
	GetWindowRect( parentWnd, &parentRect );

	rect.left = ( parentRect.right - parentRect.left - rect.right ) / 2;
	rect.top = ( parentRect.bottom - parentRect.top - rect.bottom ) / 2;

	MoveWindow( wndStatic, HORIZONTAL_MARGIN, VERTICAL_MARGIN,
		staticWidth, staticHeight, FALSE );

	if( buttons_.size() )
	{
		int buttonY = VERTICAL_MARGIN + staticHeight + VERTICAL_MARGIN;
		int buttonX = HORIZONTAL_MARGIN + buttonSpace;
		int i = 0;
		for( std::vector<std::string>::iterator iter = buttons_.begin();
			iter != buttons_.end(); ++iter )
		{
			if( iter != buttons_.begin() )
			{
				wndButton = CreateWindow( "BUTTON", iter->c_str(), WS_CHILD | WS_VISIBLE,
					buttonX, buttonY, buttonWidth, BUTTON_HEIGHT, hwnd, 
					(HMENU)( iter + 1 == buttons_.end() ? IDCANCEL : std::distance( buttons_.begin(), iter ) + IDCANCEL ),
					GetModuleHandle( NULL ), NULL );
				SendMessage( wndButton, WM_SETFONT, (WPARAM)font_, FALSE );
			}
			else
			{
				MoveWindow( wndButton, buttonX, buttonY,
					buttonWidth, BUTTON_HEIGHT, FALSE );
			}
			buttonX += buttonSpace + buttonWidth;
		}
	}

	MoveWindow( hwnd, rect.left, rect.top, rect.right, rect.bottom, TRUE );
	if( topmost_ )
		SetWindowPos( hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
	
	return;
}

void MsgBox::kill( HWND hwnd )
{
	if( model_ )
		EndDialog( hwnd, result_ );
	else
		DestroyWindow( hwnd );
}
