#!/usr/bin/env python

# This script is used to launch the CellAppMgr running as a back-end service.
# This script is run by xinetd. The server option in xinetd.conf should refer to
# this file.

# xinetd calls this script with stdin and stdout bound to the stream to the
# front-end CellAppMgr process, implemented with service_front_end.py.

# Version 4: Intial version committed and working remotely
# Version 5: Support for shared CellAppData, BaseAppData and GlobalData.
# Version 6: Mercury change to support once-off reliability

VERSION_NUMBERS = (4, 5, 6)

MSG_BWLOG = 9
MSG_INIT = 10

import os
import popen2
import posix

import random

import socket
import struct
import sys
import syslog

import time
import glob
import re

DEBUGGING = True

if DEBUGGING:
	# Simple class so that error messages will be forwarded to /var/log/messages
	class SyslogWriter:
		def __init__( self ):
			self.data = ""

		def write( self, data ):
			self.data += data
			if self.data[-1] == "\n":
				syslog.syslog( self.data )
				self.data = ""

	sys.stderr = SyslogWriter()

# The command line argument is the executable to run as the real CellAppMgr.
if len( sys.argv ) != 2:
	syslog.syslog( "Invalid arguments: " + str( sys.argv ) )
	sys.exit( 0 )

EXE_PATH = sys.argv[1]

# This class is used to handle the initial TCP connection with the local CellAppMgr.
class Connection:
	def __init__( self, fd ):
		self.socket = socket.fromfd( fd, socket.AF_INET, socket.SOCK_STREAM )

	def receive( self, size ):
		data = ""
		while len( data ) < size:
			curr = self.socket.recv( size - len( data ) )
			if not curr:
				raise "Lost connection"
			data += curr
		return data

	def getStruct( self, structFormat ):
		return struct.unpack( structFormat,
				self.receive( struct.calcsize( structFormat ) ) )

	def sendMsg( self, msg ):
		self.socket.send(
				struct.pack( "=Hi", MSG_BWLOG, len( msg ) ) + msg )

# Verify that the token sent back from the local side is valid.
def verifyToken( key ):
	inst = popen2.Popen3( ("/usr/bin/gpg", ), True )
	inst.tochild.write( key )
	inst.tochild.close()

	msg = ""
	data = True
	while data != "":
		data = inst.fromchild.read( 1024 )
		msg += data
	inst.fromchild.close()

	stderrData = ""
	data = True
	while data != "":
		data = inst.childerr.read( 1024 )
		stderrData += data
	inst.childerr.close()

	# Get the return code. A value of 0 indicates that the signature matches the
	# returned token.
	code = inst.wait()

	return code == 0, msg, stderrData

def main():
	# stdin is dup'ed to make sure nothing unexpected is written or read from the
	# stream. CellAppMgr is informed of the appropriate file descriptor via its
	# command line arguments.
	fd = posix.dup( 0 )

	nullFD = posix.open( "/dev/null", posix.O_RDWR )
	posix.dup2( nullFD, 0 )
	posix.dup2( nullFD, 1 )
	posix.dup2( nullFD, 2 )

	connection = Connection( fd )

	peerString = connection.socket.getpeername()[0]

	syslog.syslog( "Got connection from " + peerString )

	versionNumber, = connection.getStruct( "=i" )

	if versionNumber not in VERSION_NUMBERS:
		connection.sendMsg(
				"ERROR: Invalid local CellAppMgr version. " \
				"Expected versions %s. Got %d\n" % \
				(str( VERSION_NUMBERS ), versionNumber) )
		sys.exit( 0 )

	accountNameLen, = connection.getStruct( "=i" )
	accountName = connection.receive( accountNameLen )

	# Make a unique token that will be sent to the front-end, signed and then
	# returned.
	sentToken = str( time.time() ) + str( random.random() ) + peerString

	connection.socket.send(
			struct.pack( "=Hi", MSG_INIT, len( sentToken ) ) + sentToken )

	signedTokenLen, = connection.getStruct( "=i" )
	signedToken = connection.receive( signedTokenLen )

	isVerified, receivedToken, decryptData = verifyToken( signedToken )

	if receivedToken != sentToken:
		syslog.syslog( "Authentication failed '%s' at '%s')" %
				(accountName, peerString) )

	if not isVerified:
		connection.sendMsg(
			"ERROR: Authentication failed for GPG key '%s'.\n" % accountName )
		connection.sendMsg(
			"ERROR: Make sure the public GPG key is registered with BigWorld.\n" )
		connection.sendMsg(
			"ERROR: To register a key, please contact support@bigworldtech.com\n" )
		syslog.syslog( "Authentication failed for '%s' at '%s')" %
				(accountName, peerString) )
		sys.exit( 0 )

	remoteUID, remotePID, remoteViewerPort = \
		connection.getStruct( "=iiH" )

	syslogPrefix = "CellAppMgr:%s:%s:%d:%d" % \
			(accountName, connection.socket.getpeername()[0],
			 remoteUID, remotePID )
	syslog.openlog( syslogPrefix )
	syslog.syslog( "Started" )
	for line in decryptData.split( "\n" ):
		syslog.syslog( line )

	# Read the command line arguments
	args = []
	argLen = 1
	while argLen:
		argLen, = connection.getStruct( "=i" )
		if argLen:
			arg = connection.receive( argLen )
			args.append( arg )

	try:
		# This affects where core files are created.
		os.chdir( os.path.dirname( EXE_PATH ) )
	except Exception, e:
		pass

	# The CellAppMgr
	# find the version of the cellappmgr with this major version number
	# with the highest minor version number
	binaryPaths = glob.glob( EXE_PATH + "." +
		str( versionNumber ) + ".[0-9]*" )
	maxMinorVersion = -1
	cellAppMgrBinaryPath = None
	for binaryPath in binaryPaths:
		matches = re.search( "\.([0-9]+)$", binaryPath )
		if not matches:
			continue
		try:
			minorVersion = int( matches.groups()[0] )
			if minorVersion > maxMinorVersion:
				maxMinorVersion = minorVersion
				cellAppMgrBinaryPath = binaryPath
		except ValueError:
			continue

	if cellAppMgrBinaryPath is None:
		syslog.syslog( "Could not find binary for version %d" % versionNumber )
		sys.exit( 1 )

	syslog.syslog( "Binary chosen: %s" % cellAppMgrBinaryPath )
	posix.execv( cellAppMgrBinaryPath,
			args + ["-remoteService",
			"%d:%d:%d:%d" % ( fd, remoteUID, remotePID, remoteViewerPort ),
			syslogPrefix] )

main()

# End of file
