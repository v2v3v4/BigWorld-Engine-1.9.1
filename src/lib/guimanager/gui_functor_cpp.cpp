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
#include "gui_functor_cpp.hpp"
#include "gui_item.hpp"

BEGIN_GUI_NAMESPACE

void CppFunctor::set( const std::string& name, Textor* textor )
{
	textors_[ name ] = textor;
}

void CppFunctor::set( const std::string& name, Updater* updater )
{
	updaters_[ name ] = updater;
}

void CppFunctor::set( const std::string& name, Importer* importer )
{
	importers_[ name ] = importer;
}

void CppFunctor::set( const std::string& name, Action* action )
{
	actions_[ name ] = action;
}

void CppFunctor::remove( Textor* textor )
{
	for( std::map< std::string, Textor* >::iterator iter = textors_.begin();
		iter != textors_.end(); ++iter )
		if( iter->second == textor )
		{
			textors_.erase( iter );
			break;
		}
}

void CppFunctor::remove( Updater* updater )
{
	for( std::map< std::string, Updater* >::iterator iter = updaters_.begin();
		iter != updaters_.end(); ++iter )
		if( iter->second == updater )
		{
			updaters_.erase( iter );
			break;
		}
}

void CppFunctor::remove( Importer* importer )
{
	for( std::map< std::string, Importer* >::iterator iter = importers_.begin();
		iter != importers_.end(); ++iter )
		if( iter->second == importer )
		{
			importers_.erase( iter );
			break;
		}
}

void CppFunctor::remove( Action* action )
{
	for( std::map< std::string, Action* >::iterator iter = actions_.begin();
		iter != actions_.end(); ++iter )
		if( iter->second == action )
		{
			actions_.erase( iter );
			break;
		}
}

const std::string& CppFunctor::name() const
{
	static std::string name = "C++";
	return name;
}

bool CppFunctor::text( const std::string& textor, ItemPtr item, std::string& result )
{
	if( textors_.find( textor ) != textors_.end() )
	{
		result = textors_[ textor ]->text( item );
		return true;
	}
	return false;
}

bool CppFunctor::update( const std::string& updater, ItemPtr item, unsigned int& result )
{
	if( updaters_.find( updater ) != updaters_.end() )
	{
		result = updaters_[ updater ]->update( item );
		return true;
	}
	return false;
}

DataSectionPtr CppFunctor::import( const std::string& importer, ItemPtr item )
{
	if( importers_.find( importer ) != importers_.end() )
		return importers_[ importer ]->import( item );
	return NULL;
}

bool CppFunctor::act( const std::string& action, ItemPtr item, bool& result )
{
	if( actions_.find( action ) != actions_.end() )
	{
		result = actions_[ action ]->act( item );
		return true;
	}
	return false;
}


END_GUI_NAMESPACE
