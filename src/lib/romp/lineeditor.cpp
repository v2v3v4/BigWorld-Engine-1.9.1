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

#include <windows.h>

#include "lineeditor.hpp"
#include "xconsole.hpp"

#include "cstdmf/debug.hpp"

DECLARE_DEBUG_COMPONENT2( "UI", 0 )


#ifndef CODE_INLINE
#include "lineeditor.ipp"
#endif


namespace { // anonymous

// Named constants
const float	KEY_REPEAT_START_SEC	= 0.400f;
const float	KEY_REPEAT_DELAY_SEC	= 0.065f;
const float	KEY_REPEAT_MAX_DTIME    = 1/30.f;
const int   MAX_HISTORY_ENTRIES		= 50;
const int   MAX_LINES_OFFSET        = 5;
const std::string SEPARATORS		= " `~!@#$%^&*()_+-=[]{}\\|;:'\",./<>?";

// The joystick text entry maps
const char	JOYSTICKCHARMAPS[18][9] =
{
// stick up
	{	 0	,'8','9','7',0	,'5',0	,'6',0	 },	// numB
	{	 0	,'x','q','g',0	,'k',0	,'c',-25 },	// gutt
	{	 0	,'z','_','y',0	,'w',0	,'s',0	 },	// semivowel/spirant
	{	 0	,'r',0	,'n',0	,'m',0	,'l',0	 },	// nasal/liquid
	{	 '^','o','u','i',0	,'a','`','e',-76 },	// vowel
	{	 0	,'v',0	,'b',0	,'p',0	,'f',0	 },	// lab
	{	 0	,'3','4','2',0	,'0',0	,'1',0	 },	// numA
	{	 0	,'j',0	,'d',0	,'t',0	,'h',0	 },	// dent
	{	 ')','?','"',',',0	,'.','(','!','\''},	// punct

// stick down
	{	 '>','@',-79,-93,0	,'$','<','%','#' },	// numB
	{	 0	,'X','Q','G',0	,'K',0	,'C',-57 },	// gutt
	{	 0	,'Z','_','Y',0	,'W',0	,'S',0	 },	// semivowel/spirant
	{	 0	,'R',0	,'N',0	,'M',0	,'L',0	 },	// nasal/liquid
	{	 '~','O','U','I',0	,'A',-70,'E',-88 },	// vowel
	{	 0	,'V',0	,'B',0	,'P',0	,'F',0	 },	// lab
	{	'\\','/','=','-',0	,'+','|','*','&' },	// numA
	{	 0	,'J',0	,'D',0	,'T',0	,'H',0	 },	// dent
	{	 ']','}',-108,';',0	,':','[','}',-110 }	// punct
};

// Helper functions
bool isSeparator( char character );

} // namespace anonymous

// -----------------------------------------------------------------------------
// section: LineEditor
// -----------------------------------------------------------------------------

LineEditor::LineEditor(XConsole * console)
:cx_( 0 ),
 inOverwriteMode_( false ),
 advancedEditing_( false ),
 lastChar_( 0 ),
 historyShown_( -1 ),
 keyRepeat_(KeyEvent(), FLT_MAX),
 time_( 0 ),
 console_(console)
{
	lineLength_ = console_->visibleWidth();
}

LineEditor::~LineEditor()
{
}



/**
 *	This method processes key down events
 */
LineEditor::ProcessState LineEditor::processKeyEvent(
	KeyEvent event, std::string& resultString )
{
	#define EMPTY_STR(str) \
		(str.empty() || str.find_first_not_of(32) == std::string::npos)

	const KeyEvent::Key eventKey = event.key();

	bool isResultSet = false;
	bool isHandled = false;

	if (event.isKeyDown())
	{
		char keyChar = event.character();

		isHandled = this->processAdvanceEditKeys( event );
		if (!isHandled)
		{
			isHandled = true;
			switch (eventKey)
			{
			case KeyEvent::KEY_RETURN:
			case KeyEvent::KEY_JOY8:	// 'A' key
				if (!event.isAltDown())
				{
					resultString = editString_;
					isResultSet = true;
					editString_ = "";
					cx_ = 0;
					lastChar_ = 0;
				}
				else
				{
					isHandled = false;
				}
				break;

			case KeyEvent::KEY_DELETE:
				if ( cx_ < (int)editString_.length( ) )
					deleteChar( cx_ );
				break;

			case KeyEvent::KEY_BACKSPACE:
			case KeyEvent::KEY_JOY14:	// left trigger
				if ( cx_ )
				{
					cx_--;
					deleteChar( cx_ );
				}
				break;

			case KeyEvent::KEY_INSERT:
				inOverwriteMode_ = !inOverwriteMode_;
				break;

			case KeyEvent::KEY_LEFTARROW:
			case KeyEvent::KEY_JOY2:	// dpad left
				if ( cx_ > 0 )
					cx_--;
				break;

			case KeyEvent::KEY_RIGHTARROW:
			case KeyEvent::KEY_JOY3:	// dpad right
				if ( cx_ < (int)editString_.length() )
					cx_++;
				break;

			case KeyEvent::KEY_UPARROW:
			case KeyEvent::KEY_JOY0:	// dpad up
				if (history_.size() > 0) 
				{
					if (historyShown_ == -1)
					{
						history_.insert( history_.begin(), editString_ );
						historyShown_ = 1;
					}
					else 
					{
						if (!EMPTY_STR(editString_))
						{
							history_[historyShown_] = editString_;
						}
						++historyShown_;
					}
					showHistory();
				}
				break;

			case KeyEvent::KEY_DOWNARROW:
			case KeyEvent::KEY_JOY1:	// dpad down
				if (history_.size() > 0) 
				{
					if (historyShown_ == -1)
					{
						history_.insert( history_.begin(), editString_ );
						historyShown_ = history_.size() - 1;
					}
					else 
					{
						if (!EMPTY_STR(editString_))
						{
							history_[historyShown_] = editString_;
						}
						--historyShown_;
					}
					showHistory();
				}
				break;

			case KeyEvent::KEY_HOME:
				cx_ = 0;
				break;

			case KeyEvent::KEY_END:
				cx_ = editString_.length();
				break;

			// joystick space
			case KeyEvent::KEY_JOY15:
				keyChar = ' ';
				isHandled = false;
				break;

			default:
				isHandled = false;
				break;
			}
		}

		if (!isHandled && keyChar != 0)
		{
			isHandled = true;

			cx_ += this->insertChar( cx_, keyChar );

			lastChar_ = 0;
		}
		else if (event.isCtrlDown())
		{
			if (event.isKeyDown() && eventKey == KeyEvent::KEY_U)
			{
				int assignmentLocation = editString_.find_last_of( "=" );

				if (assignmentLocation > 0)
				{
					editString_ = editString_.substr( 0, assignmentLocation + 1 );
					cx_ = min( cx_, (int)editString_.length() );

					isHandled = true;
				}
			}
		}

		// if key is relevant (i.e. was handled) and 
		// it isn't the RETURN key, insert it into 
		// list of currently pressed-down keys
		if (isHandled && !isResultSet && this->keyRepeat_.first.key() != eventKey )
		{
			this->keyRepeat_.first  = event;
			this->keyRepeat_.second = this->time_ + KEY_REPEAT_START_SEC;
		}
		if( this->keyRepeat_.first.modifiers() != event.modifiers() )
		{
			this->keyRepeat_.first = KeyEvent( keyRepeat_.first.type(), 
				keyRepeat_.first.key(), event.modifiers() );
		}
	}
	else
	{
		// this is a key-up event. 
		// Stop key from repeating
		if (eventKey == this->keyRepeat_.first.key() ||
			event.isCtrlDown() || event.isAltDown() )
		{
			this->keyRepeat_.first = KeyEvent();
			this->keyRepeat_.second = FLT_MAX;
			this->time_ = 0;
		}
		else if( this->keyRepeat_.first.modifiers() != event.modifiers() )
		{
			this->keyRepeat_.first = KeyEvent( keyRepeat_.first.type(), 
				keyRepeat_.first.key(), event.modifiers() );
		}
	}

	// these are key up and key downs
	if (!isHandled) switch (eventKey)
	{
		// any joystick button or quantized direction 
		// change and we go and update our joystick state
		case KeyEvent::KEY_JOYALPUSH:
		case KeyEvent::KEY_JOYARPUSH:
		case KeyEvent::KEY_JOYALUP:
		case KeyEvent::KEY_JOYALDOWN:
		case KeyEvent::KEY_JOYALLEFT:
		case KeyEvent::KEY_JOYALRIGHT:
		case KeyEvent::KEY_JOYARUP:
		case KeyEvent::KEY_JOYARDOWN:
		case KeyEvent::KEY_JOYARLEFT:
		case KeyEvent::KEY_JOYARRIGHT:
			this->processJoystickStates(
				InputDevices::joystick().stickDirection( 1 ),
				InputDevices::joystick().stickDirection( 0 ),
				InputDevices::isKeyDown( KeyEvent::KEY_JOYARPUSH ),
				InputDevices::isKeyDown( KeyEvent::KEY_JOYALPUSH ) );
			isHandled = true;
			break;
	}

	// end of line, request processing
	if (isResultSet)
	{
		if (!EMPTY_STR(resultString))
		{
			if (history_.size() > 0 && historyShown_ != -1)
			{
				history_[ 0 ] = resultString;
			}
			else
			{
				history_.insert( history_.begin(), resultString );
			}
		}
		else
		{
			if (history_.size() > 0 && EMPTY_STR(history_[ 0 ]))
			{
				history_.erase(history_.begin());
			}
		}
		// clamp history 
		if (history_.size() > MAX_HISTORY_ENTRIES)
		{
			history_.erase( history_.end()-1 );
		}
		historyShown_ = -1;
		return RESULT_SET;
	}

	if (isHandled)
	{
		return PROCESSED;
	}

	return NOT_HANDLED;
}


/**
 * Sets contents of edit string.
 */
void LineEditor::editString( const std::string & s )
{
	editString_ = s.length() < uint(this->lineLength()) - MAX_LINES_OFFSET
		? s
		: s.substr(0, this->lineLength() - MAX_LINES_OFFSET);

	cx_ = min( cx_, int(editString_.size()) );
}


/**
 * Sets current cursor position.
 */
void LineEditor::cursorPosition( int pos )					
{ 
	cx_ = min( pos, int(editString_.size()) );
}


/**
 *	Processes key events, handling advanced editing commands. Advance editing
 *	commands are ignored if the advancedEditing flag is set to false. Bellow
 *	are the supported commands:
 *
 *	CTRL + <-		Go to beginning of word
 *	CTRL + ->		Go to end of word
 *	CTRL + A		Go to beginning of line (same as HOME)
 *	CTRL + E		Go to end word (same as END)
 *	CTRL + D		Delete character to the right (same as DELETE)
 *	CTRL + H		Delete character to the left (same as BACKSPACE)
 *	CTRL + K		Delete line to right of cursor (copies to clipboard)
 *	CTRL + U		Delete line to left of cursor (copies to clipboard)
 *	CTRL + [R|DEL]	Delete word to right of cursor (copies to clipboard)
 *	CTRL + [W|BS]	Delete word to left of cursor (copies to clipboard)
 *	CTRL + [Y|INS]	Paste clipboard contents
 */
bool LineEditor::processAdvanceEditKeys( KeyEvent event )
{
	if (!this->advancedEditing_) 
	{
		return false;
	}

	bool isHandled = false;
	if (event.isCtrlDown()) 
	{
		isHandled = true;
		switch (event.key())
		{
		case KeyEvent::KEY_LEFTARROW:
		case KeyEvent::KEY_JOY2:	// dpad left
			cx_ = curWordStart( cx_ );
			break;

		case KeyEvent::KEY_RIGHTARROW:
		case KeyEvent::KEY_JOY3:	// dpad right
			cx_ = curWordEnd( cx_ );
			break;

		case KeyEvent::KEY_A:
			cx_ = 0;
			break;

		case KeyEvent::KEY_E:
			cx_ = editString_.length();
			break;

		case KeyEvent::KEY_D:
			if ( cx_ < (int)editString_.length( ) )
			{
				deleteChar( cx_ );
			}
			break;

		case KeyEvent::KEY_H:
			if ( cx_ > 0 )
			{
				cx_--;
				deleteChar( cx_ );
			}
			break;

		case KeyEvent::KEY_K:
			clipBoard_ = cutText( cx_, editString_.length() );
			break;

		case KeyEvent::KEY_U:
			clipBoard_ = cutText( 0, cx_ );
			cx_ = 0;
			break;

		case KeyEvent::KEY_W:
		case KeyEvent::KEY_BACKSPACE:
		{
			int wordStart = curWordStart( cx_ );
			clipBoard_ = cutText( wordStart, cx_ );
			cx_ = wordStart;
			break;
		}

		case KeyEvent::KEY_R:
		case KeyEvent::KEY_DELETE:
		{
			clipBoard_ = cutText( cx_, curWordEnd( cx_ ) );
			break;
		}

		case KeyEvent::KEY_Y:
		case KeyEvent::KEY_INSERT:
			cx_ = pasteText( cx_, clipBoard_ );
			break;

		default:
			isHandled = false;
			break;
		}
	}	
	return isHandled;
}



/**
 *	Gives the line editor a chance to process time 
 *	based operations, like key repeat an the like. 
 */
void LineEditor::tick( float dTime )
{
	this->time_ += std::min(dTime, KEY_REPEAT_MAX_DTIME);
	if (this->keyRepeat_.second < this->time_)
	{
		std::string resultString;
		this->processKeyEvent( this->keyRepeat_.first, resultString );
		this->keyRepeat_.second += KEY_REPEAT_DELAY_SEC;
	}
}


/**
 *	Deactivates the line editor. Clears the down keys map.
 */
void LineEditor::deactivate()
{
	this->keyRepeat_.second = FLT_MAX;
}


/**
 *	Retrieves the current command history list.
 */
const LineEditor::StringVector LineEditor::history() const
{
	struct encodeSpaces
	{
		static std::string doit(const std::string &input)
		{
			std::string output = input;
			std::string::size_type pos = 0;
			while ((pos = output.find("\\", pos)) != std::string::npos)
			{
				output.replace(pos, 1, "\\c");
				pos += 2;
			}
			pos = 0;
			while ((pos = output.find(" ", pos)) != std::string::npos)
			{
				output.replace(pos, 1, "\\s");
				pos += 2;
			}			
			return output;
		}
	};

	// reverse history to make it more 
	// readable when output to text format
	StringVector result(this->history_.size());
	std::reverse_copy(this->history_.begin(), this->history_.end(), result.begin());
	std::transform(result.begin(), result.end(), result.begin(), &encodeSpaces::doit);
	return result;
}


/**
 *	Replaces the command history list.
 */
void LineEditor::setHistory(const StringVector & history)
{
	struct decodeSpaces
	{
		static std::string doit(const std::string &input)
		{
			std::string output = input;
			std::string::size_type pos = 0;
			while ((pos = output.find("\\s", pos)) != std::string::npos)
			{
				output.replace(pos, 2, " ");
				++pos;
			}
			pos = 0;
			while ((pos = output.find("\\c", pos)) != std::string::npos)
			{
				output.replace(pos, 2, "\\");
				++pos;
			}			
			return output;
		}
	};

	this->history_.resize(history.size());
	std::reverse_copy(history.begin(), history.end(), this->history_.begin());
	std::transform(
		this->history_.begin(), this->history_.end(), 
		this->history_.begin(), &decodeSpaces::doit);
		
	this->historyShown_ = -1;
}


/**
 *	This method processes joystick states. It assumes if it's getting
 *	called then it should handle the joystick movement.
 */
void LineEditor::processJoystickStates( int joyADir, int joyBDir,
	bool joyADown, bool joyBDown )
{
	// calculate the current character
	char curChar = JOYSTICKCHARMAPS[joyADir+9*int(joyADown)][joyBDir];
	if (curChar == 0 && joyBDir != 4) curChar = lastChar_;

	// if there are current and last characters, replace the last one
	if (curChar != 0 && lastChar_ != 0 && cx_ > 0)
	{
		editString_[cx_-1] = curChar;
	}
	// otherwise add the current character if there is one
	else if (curChar != 0)
	{
		cx_ += this->insertChar( cx_, curChar );
	}

	lastChar_ = curChar;
}


/**
 *	Insert the character at pos
 *
 *	@return amount to increase pos by
 */
int LineEditor::insertChar( int pos, char c )
{
	if (editString_.length() >= uint(this->lineLength()) - MAX_LINES_OFFSET) 
	{
		return 0;
	}

	if (pos < (int)editString_.length())
	{
		if (inOverwriteMode_)
		{
			editString_[pos] = c;
		}
		else
		{
			editString_.insert( pos, 1, c );
		}
	}
	else	
	{
		editString_ += c;
	}

	return 1;
}


/**
 *	Delete the character at pos
 */
void LineEditor::deleteChar( int pos )
{
	if ( pos == editString_.length( ) )
		editString_ = editString_.substr( 0, pos - 1 );
	else
	{
		std::string preStr = editString_.substr( 0, pos );
		std::string postStr =
			editString_.substr( pos + 1, editString_.length( ) );

		editString_ = preStr + postStr;
	}
}


/**
 *	Returns index of start of current word
 */
int LineEditor::curWordStart( int pos ) const
{
	// go to start of separating block
	while (pos > 0 && isSeparator( editString_[ pos - 1 ] ))
	{
		--pos;
	}

	// go to start of word	
	while (pos > 0 && !isSeparator( editString_[ pos - 1 ] ))
	{
		--pos;
	}

	return pos;
}


/**
 *	Returns index of end of current word
 */
int LineEditor::curWordEnd( int pos ) const
{
	// go to end of word	
	while (uint( pos ) < editString_.length() && 
			!isSeparator( editString_[ pos ] ))
	{
		++pos;
	}

	// go to end of separating block
	while (uint( pos ) < editString_.length() && 
			isSeparator( editString_[ pos ] ))
	{
		++pos;
	}

	return pos;
}


/**
 *	Deletes the substring defined by the given indices and returns it.
 */
std::string LineEditor::cutText( int start, int end )
{
	std::string text = editString_.substr( start, end - start );
	editString_ = 
			editString_.substr( 0, start ) + 
			editString_.substr( end, editString_.length() - end ); 
	return text;
}


/**
 *	Pastes the given text into the editing string. Returns new cursor pos.
 */
int LineEditor::pasteText( int pos, const std::string & text )
{
	editString_ = 
			editString_.substr( 0, pos ) + text +
			editString_.substr( pos, editString_.length() - pos ); 
	return pos + text.length();
}

/**
 *	Show the line historyShown_ from history_
 */
void LineEditor::showHistory()
{
	if (historyShown_ < 0)
	{
		 historyShown_ = history_.size() - 1;
	}
	else if (historyShown_ == history_.size())
	{
		 historyShown_ = 0;
	}

	this->editString( history_[ historyShown_ ] );
	cx_ = editString_.length();
	lastChar_ = 0;
}


namespace { // anonymous

// Helper functions
bool isSeparator( char character )
{
	return std::find( 
		SEPARATORS.begin(), 
		SEPARATORS.end(), 
		character ) != SEPARATORS.end();
}

} // namespace anonymous

// lineeditor.cpp
