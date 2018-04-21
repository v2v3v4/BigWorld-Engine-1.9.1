bigworld/src/common directory
=============================

This directory contains code that is shared between the server and the client.












// -----------------------------------------------------------------------------
// Section: Mercury
// -----------------------------------------------------------------------------

Nub		- This is the central object that is responsible for sending and
			receiving information.

Bundle	- A bundle is the type of object that the application deals with. It
			fills the bundle with messages and sends the bundle to a
			destination.

Packet - A bundle contains a number of packets. Packets are the lower level
			object that is sent. Often a bundle is made of just one packet but
			it may be made of many.

Message - Represents an individual message that is to be handled by the
			application.



Packet
------
Byte 0 - Flags

From the back
	if FLAG_HAS_ACKS is set
		Byte for number of ACKs
		Byte for number of NACKs
		uint32 for each ACK and NACK

	if FLAG_HAS_SEQUENCE_NUMBER set
		int for sequence number
