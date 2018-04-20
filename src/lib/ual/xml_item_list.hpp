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
 *	XmlItemList: xml item list manager for the Ual Dialog
 */



#ifndef XML_ITEM_LIST_HPP
#define XML_ITEM_LIST_HPP

#include "resmgr/datasection.hpp"
#include "asset_info.hpp"

//XmlItem
class XmlItem
{
public:
	enum Position
	{
		TOP,
		BOTTOM
	};
	XmlItem() :
		position_( TOP )
	{};
	XmlItem(
		const AssetInfo& assetInfo,
		const Position position = TOP ) :
		assetInfo_( assetInfo ),
		position_( position )
	{};

	bool empty() const { return assetInfo_.empty(); };

	const AssetInfo& assetInfo() const { return assetInfo_; };
	Position position() const { return position_; };

private:
	AssetInfo assetInfo_;
	Position position_;
};
typedef std::vector<XmlItem> XmlItemVec;

// XmlItemList
class XmlItemList
{
public:
	XmlItemList();
	virtual ~XmlItemList();

	virtual DataSectionPtr add( const XmlItem& item );
	virtual DataSectionPtr addAt( const XmlItem& item , const XmlItem& atItem );
	virtual void remove( const XmlItem& item );

	virtual void clear();

	virtual void setDataSection( const DataSectionPtr section );
	virtual void setPath( const std::string& path );
	virtual std::string getPath() { return path_; };

	virtual void getItems( XmlItemVec& items );
	virtual DataSectionPtr getItem( const XmlItem& item );

protected:
	std::string path_;
	int sectionLock_;
	DataSectionPtr section_;
	DataSectionPtr rootSection_;

	DataSectionPtr lockSection();
	void unlockSection();

	virtual void dumpItem( DataSectionPtr section,  const XmlItem& item );
};


#endif // XML_ITEM_LIST_HPP
