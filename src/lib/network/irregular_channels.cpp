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

#include "irregular_channels.hpp"
#include "channel.hpp"

DECLARE_DEBUG_COMPONENT2( "Network", 0 )

namespace Mercury
{

// -----------------------------------------------------------------------------
// Section: IrregularChannels
// -----------------------------------------------------------------------------

/**
 *	This method remembers this channel for resend checking if it is irregular
 *	and is not already stored.
 */
void IrregularChannels::addIfNecessary( Channel & channel )
{
	if (channel.isIrregular_ &&
		channel.hasUnackedPackets())
	{
		MonitoredChannels::addIfNecessary( channel );
	}
}

IrregularChannels::iterator & IrregularChannels::channelIter(
	Channel & channel ) const
{
	return channel.irregularIter_;
}


float IrregularChannels::defaultPeriod() const
{
	return 1.f;
}


/**
 *	This method handles the timer event. It checks whether irregular channels
 *	need to resend unacked packets.
 */
int IrregularChannels::handleTimeout( TimerID, void * )
{
	iterator iter = channels_.begin();

	while (iter != channels_.end())
	{
		Channel & channel = **iter++;

		if (channel.hasUnackedPackets() && channel.isIrregular())
		{
			// Call send instead of checkResendTimers so that piggybacks will be
			// sent immediately. It is not allowed to have piggybacks on unsent
			// packets. Empty packets are not sent.
			channel.send();

			// checkResendTimers() sets remote failure status
			if (channel.hasRemoteFailed())
			{
				ERROR_MSG( "IrregularChannels::handleTimeout: "
					"Removing dead channel to %s\n",
					channel.c_str() );

				this->delIfNecessary( channel );
			}
		}
		else
		{
			this->delIfNecessary( channel );
		}
	}

	return 0;
}

} // namespace Mercury

// irregular_channels.cpp
