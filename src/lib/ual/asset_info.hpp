/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

/**
 *	AssetInfo: generic asset info class
 */

#ifndef ASSET_INFO_HPP
#define ASSET_INFO_HPP

#include "cstdmf/smartpointer.hpp"
#include "resmgr/datasection.hpp"

//XmlItem
class AssetInfo : public ReferenceCount
{
public:
	AssetInfo()
	{};
	AssetInfo(
		const std::string& type,
		const std::string& text,
		const std::string& longText,
		const std::string& thumbnail = "",
		const std::string& description = "" ) :
		type_( type ),
		text_( text ),
		longText_( longText ),
		thumbnail_( thumbnail ),
		description_( description )
	{};
	AssetInfo( DataSectionPtr sec )
	{
		if ( sec )
		{
			type_ = sec->readString( "type" );
			text_ = sec->asString();
			longText_ = sec->readString( "longText" );
			thumbnail_ = sec->readString( "thumbnail" );
			description_ = sec->readString( "description" );
		}
	}

	AssetInfo operator=( const AssetInfo& other )
	{
		type_ = other.type_;
		text_ = other.text_;
		longText_ = other.longText_;
		thumbnail_ = other.thumbnail_;
		description_ = other.description_;
		return *this;
	};

	bool empty() const { return text_.empty(); };
	bool equalTo( const AssetInfo& other ) const
	{
		return
			type_ == other.type_ &&
			text_ == other.text_ &&
			longText_ == other.longText_;
	};
	const std::string& type() const { return type_; };
	const std::string& text() const { return text_; };
	const std::string& longText() const { return longText_; };
	const std::string& thumbnail() const { return thumbnail_; };
	const std::string& description() const { return description_; };

	void type( const std::string& val ) { type_ = val; };
	void text( const std::string& val ) { text_ = val; };
	void longText( const std::string& val ) { longText_ = val; };
	void thumbnail( const std::string& val ) { thumbnail_ = val; };
	void description( const std::string& val ) { description_ = val; };
private:
	std::string type_;
	std::string text_;
	std::string longText_;
	std::string thumbnail_;
	std::string description_;
};
typedef SmartPointer<AssetInfo> AssetInfoPtr;

#endif // ASSET_INFO
