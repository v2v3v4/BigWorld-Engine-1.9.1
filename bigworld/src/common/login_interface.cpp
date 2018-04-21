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

#include "login_interface.hpp"
#include "cstdmf/debug.hpp"
#include "cstdmf/memory_stream.hpp"
#include "network/public_key_cipher.hpp"

/**
 *  This method writes the login parameters to a stream.  If pRSA is non-NULL it
 *  will be used to encrypt the stream.
 */
bool LogOnParams::addToStream( BinaryOStream & data, Flags flags,
	Mercury::PublicKeyCipher * pKey )
{
	// Use the stored flags if none were passed.
	if (flags == PASS_THRU)
	{
		flags = flags_;
	}

	// This is an intermediate stream we use for assembling the plaintext if
	// we're encrypting.
	MemoryOStream clearText;

	BinaryOStream * pRawStream = pKey ? &clearText : &data;

	// Stream the parameters into whichever stream is appropriate
	*pRawStream << flags << username_ << password_ << encryptionKey_;

	if (flags & HAS_DIGEST)
	{
		*pRawStream << digest_;
	}

	*pRawStream << nonce_;

	*pRawStream << nonce_;

#ifdef USE_OPENSSL
	// Encrypt the intermediate stream if necessary
	if (pKey && (pKey->publicEncrypt( clearText, data ) == -1))
	{
		return false;
	}
#else
	MF_ASSERT_DEV( !pKey );
#endif

	return true;
}


/**
 *  This method reads the login parameters from a stream.  If pKey is
 *  non-NULL, the stream will be decrypted.
 */
bool LogOnParams::readFromStream( BinaryIStream & data,
	Mercury::PublicKeyCipher * pKey )
{
	// Intermediate stream used for writing the plaintext for encrypted streams.
	MemoryOStream clearText;

#ifdef USE_OPENSSL
	if (pKey && (pKey->privateDecrypt( data, clearText ) == -1))
	{
		return false;
	}
#else
	MF_ASSERT_DEV( !pKey );
#endif

	BinaryIStream * pRawStream = pKey ? &clearText : &data;

	*pRawStream >> flags_ >> username_ >> password_ >> encryptionKey_;

	if (flags_ & HAS_DIGEST)
	{
		*pRawStream >> digest_;
	}

	if (pRawStream->remainingLength())
	{
		*pRawStream >> nonce_;
	} 
	else // CMM: Get rid of this case next time the login version is bumped.
	{
		nonce_ = 0;
	}

	if (pRawStream->remainingLength())
	{
		*pRawStream >> nonce_;
	} 
	else // CMM: Get rid of this case next time the login version is bumped.
	{
		nonce_ = 0;
	}

	clearText.finish();

	return !data.error();
}

// login_interface.cpp
