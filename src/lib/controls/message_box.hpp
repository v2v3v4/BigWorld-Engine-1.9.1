/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef MESSAGE_BOX_HPP
#define MESSAGE_BOX_HPP

#include <map>
#include <vector>
#include <string>

class MsgBox
{
	static std::map<HWND, MsgBox*> wndMap_;
	static std::map<const MsgBox*, HWND> msgMap_;
	static INT_PTR CALLBACK dialogProc( HWND hwnd, UINT msg, WPARAM w, LPARAM l );

	std::string caption_;
	std::string text_;
	std::vector<std::string> buttons_;
	unsigned int result_;
	bool model_;
	bool topmost_;
	unsigned int timeOut_;

	void create( HWND hwnd );
	void kill( HWND hwnd );

	HFONT font_;
public:
	MsgBox( const std::string& caption, const std::string& text,
		const std::string& buttonText1 = "", const std::string& buttonText2 = "",
		const std::string& buttonText3 = "", const std::string& buttonText4 = "" );
	MsgBox( const std::string& caption, const std::string& text,
		const std::vector<std::string>& buttonTexts );
	~MsgBox();
	unsigned int doModal( HWND parent = NULL, bool topmost = false, unsigned int timeOutTicks = INFINITE );
	void doModalless( HWND parent = NULL, unsigned int timeOutTicks = INFINITE );
	unsigned int getResult() const;
	bool stillActive() const;

	static const unsigned int TIME_OUT = INFINITE;
};

#endif//MESSAGE_BOX_HPP
