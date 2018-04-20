/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LINEEDITOR_HPP
#define LINEEDITOR_HPP

#include "input/input.hpp"
#include <map>

// Forward declarations
class XConsole;

/**
 *	This class handles key and joystick events to edit a line of text
 */
class LineEditor
{
public:
	typedef std::vector<std::string> StringVector;

	enum ProcessState
	{
		NOT_HANDLED,
		PROCESSED,
		RESULT_SET,
	};


	LineEditor(XConsole * console);
	~LineEditor();

	ProcessState processKeyEvent( KeyEvent event, std::string & resultString );

	const std::string& editString() const			{ return editString_; }
	void editString( const std::string & s );

	int cursorPosition() const						{ return cx_; }
	void cursorPosition( int pos );

	bool advancedEditing() const					{ return advancedEditing_; }
	void advancedEditing( bool enable )				{ advancedEditing_ = enable; }
	
	void tick( float dTime );
	void deactivate();

	const StringVector history() const;
	void setHistory(const StringVector & history);

	void lineLength( int length ) { lineLength_ = length; }	
	int lineLength() { return lineLength_; }

private:
	LineEditor(const LineEditor&);
	LineEditor& operator=(const LineEditor&);

	bool processAdvanceEditKeys( KeyEvent event );
	
	void processJoystickStates( int joyADir, int joyBDir,
		bool joyADown, bool joyBDown );

	int insertChar( int pos, char c );
	void deleteChar( int pos );
	int curWordStart( int pos ) const;
	int curWordEnd( int pos ) const;
	
	std::string cutText( int start, int end );
	int pasteText( int pos, const std::string & text );
	
	void showHistory();

	std::string editString_;		// the currently edited string
	std::string clipBoard_;			// clip-board string

	int		cx_;
	bool	inOverwriteMode_;
	bool	advancedEditing_;
	char	lastChar_;

	std::vector<std::string>	 history_;
	int	historyShown_;
	
	typedef std::pair< KeyEvent, float > KeyTime;
	KeyTime keyRepeat_;
	float time_;
	int lineLength_;
	
	XConsole * console_;
};

#ifdef CODE_INLINE
#include "lineeditor.ipp"
#endif


#endif // LINEEDITOR_HPP
