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
 *	UalVFolderLoader: manages VFolder parsing from the config file
 */


#ifndef UAL_VFOLDER_LOADER
#define UAL_VFOLDER_LOADER


#include "cstdmf/smartpointer.hpp"
#include <string>
#include <vector>


// forward
class UalDialog;

class UalFolderData;
typedef SmartPointer<UalFolderData> UalFolderDataPtr;

class VFolder;
typedef SmartPointer<VFolder> VFolderPtr;

class VFolderProvider;
typedef SmartPointer<VFolderProvider> VFolderProviderPtr;

class DataSection;
typedef SmartPointer<DataSection> DataSectionPtr;


///////////////////////////////////////////////////////////////////////////////
//	UalVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalVFolderLoader : public ReferenceCount
{
public:
	virtual bool test( const std::string& sectionName ) = 0;
	virtual bool subVFolders() { return false; }
	virtual VFolderPtr load( UalDialog* dlg, DataSectionPtr section,
		VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree ) = 0;

protected:
	UalVFolderLoader() :
		icon_( 0 ),
		iconSel_( 0 ),
		show_( true )
	{
	}
	~UalVFolderLoader()
	{
		if( icon_ )	DestroyIcon( icon_ );
		if( iconSel_ )	DestroyIcon( iconSel_ );
	}

	std::string displayName_;
	HICON icon_;
	HICON iconSel_;
	bool show_;
	UalFolderDataPtr folderData_;

	void error( UalDialog* dlg, const std::string& msg );

	void beginLoad( UalDialog* dlg, DataSectionPtr section,
		DataSectionPtr customData, int defaultThumbSize );

	VFolderPtr endLoad( UalDialog* dlg, VFolderProviderPtr provider,
		VFolderPtr parent, bool expandable,
		bool addToFolderTree = true, bool subVFolders = false );

};
typedef SmartPointer<UalVFolderLoader> UalVFolderLoaderPtr;


///////////////////////////////////////////////////////////////////////////////
//	LoaderRegistry: VFolder loaders vector singleton class
///////////////////////////////////////////////////////////////////////////////
typedef std::vector<UalVFolderLoaderPtr> VFolderLoaders;
class LoaderRegistry
{
public:
	static VFolderLoaders& LoaderRegistry::loaders()
	{
		static LoaderRegistry instance;
		return instance.vfolderLoaders_;
	}
	static UalVFolderLoaderPtr loader( const std::string& sectionName );
private:
	VFolderLoaders vfolderLoaders_;
};


///////////////////////////////////////////////////////////////////////////////
//	UalVFolderLoaderFactory
///////////////////////////////////////////////////////////////////////////////

class UalVFolderLoaderFactory
{
public:
	UalVFolderLoaderFactory( UalVFolderLoaderPtr loader );
};


///////////////////////////////////////////////////////////////////////////////
//	UalFilesVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalFilesVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "Files"; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );

protected:
	bool pathIsGood( const std::string& path );
};


///////////////////////////////////////////////////////////////////////////////
//	UalXmlVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalXmlVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "XmlList"; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


///////////////////////////////////////////////////////////////////////////////
//	UalHistoryVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalHistoryVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "History"; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


///////////////////////////////////////////////////////////////////////////////
//	UalFavouritesVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalFavouritesVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "Favourites"; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


///////////////////////////////////////////////////////////////////////////////
//	UalMultiVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalMultiVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "MultiVFolder"; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


///////////////////////////////////////////////////////////////////////////////
//	UalPlainVFolderLoader
///////////////////////////////////////////////////////////////////////////////

class UalPlainVFolderLoader : public UalVFolderLoader
{
public:
	virtual bool test( const std::string& sectionName )
		{ return sectionName == "VFolder"; }
	virtual bool subVFolders() { return true; }
	virtual VFolderPtr load( UalDialog* dlg,
		DataSectionPtr section, VFolderPtr parent, DataSectionPtr customData,
		bool addToFolderTree );
};


#endif // UAL_VFOLDER_LOADER
