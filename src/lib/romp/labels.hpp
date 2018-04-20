/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef LABELS_HPP
#define LABELS_HPP

#include "romp/font.hpp"
#include "moo/visual_channels.hpp"

/** This class allows labels to easily be drawn on the screen.
  * Just add the labels and their world positions to the list.
  * then call Moo::SortedChannel::addDrawItem( ChannelDrawItem labels )
  * and it will take care of the rest.
  */

class Labels : public Moo::ChannelDrawItem
	{
	public:
		Labels()
		{
			this->distance_ = 0;
		}

		virtual void draw()
		{
			FontPtr font = FontManager::instance().get( "system_small.font" );
			FontManager::instance().setMaterialActive( font );

			StrVecVector::const_iterator it  = this->labels_.begin();
			StrVecVector::const_iterator end = this->labels_.end();
			while (it != end)
			{
				font->draw3DString(it->first, it->second);
				++it;
			}
		}

		virtual void fini()
		{
			delete this;
		}

		void add(const std::string & id, const Vector3 & position)
		{
			this->labels_.push_back(std::make_pair(id, position));
		}

		typedef std::pair<std::string, Vector3> StrVecPair;
		typedef std::vector<StrVecPair> StrVecVector;
		StrVecVector labels_;
	};

#endif // LABELS_HPP
