;#ifndef MACHINED_MESSAGES_H
;#define MACHINED_MESSAGES_H

LanguageNames =
(
	English = 0x0409:Messages_ENU
)

;// event log categories

MessageID = 1
SymbolicName = LOG_DAEMON
Severity = Success
Language = English
Daemon: %1
.

MessageID = +1
SymbolicName = LOG_CRIT
Severity = Error
Language = English
Critical: %1
.

MessageID = +1
SymbolicName = LOG_INFO
Severity = Warning
Language = English
Info: %1
.

MessageID = +1
SymbolicName = LOG_ERR
Severity = Error
Language = English
Error: %1
.

;#endif
