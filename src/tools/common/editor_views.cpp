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
#include "editor_views.hpp"
#include "resmgr/string_provider.hpp"

PropertyTable* PropTable::s_propTable_ = NULL;	

BaseView::BaseView( )
{
}
	
void __stdcall BaseView::expel()
{
	propTable_->clear();

	for (size_t i = 0; i < propertyItems_.size(); ++i)
	{
		delete propertyItems_[i];
		propertyItems_[i] = NULL;
	}

	propertyItems_.clear();
}

//---------------------------------

void __stdcall TextView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = getCurrentValue();

	StringPropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->fileFilter( property_.fileFilter() );
	newItem->defaultDir( property_.defaultDir() );
	newItem->canTextureFeed( property_.canTextureFeed() );
	newItem->textureFeed( property_.textureFeed() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall StaticTextView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = getCurrentValue();

	PropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str(), true);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall TextLabelView::elect()
{
	propTable_ = PropTable::table();
	
	PropertyItem* newItem = new LabelPropertyItem(property_.name(), property_.highlight());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall IDView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = getCurrentValue();

	PropertyItem* newItem = new IDPropertyItem(property_.name(), oldValue_.c_str());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall GroupView::elect()
{
	propTable_ = PropTable::table();
	
	PropertyItem* newItem = new GroupPropertyItem(property_.name(), -1);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall ListTextView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = getCurrentValue();

	PropertyItem* newItem = new ComboPropertyItem(property_.name(), 
												oldValue_.c_str(),
												property_.possibleValues());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall ChoiceView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = property_.pInt()->get();
	
	std::vector<std::string> possibleValues;
	std::map<int,std::string> possibleValuesMap;

	std::string oldStringValue = "";

	DataSectionPtr choices = property_.pChoices();
	for (DataSectionIterator iter = choices->begin();
		iter != choices->end();
		iter++)
	{
		DataSectionPtr pDS = *iter;
		std::string name = property_.getName(pDS->sectionName(), pDS);
		choices_[ name ] = pDS->asInt( 0 );
		possibleValuesMap[ pDS->asInt( 0 ) ] = name;
		if (pDS->asInt( 0 ) == oldValue_)
		{
			oldStringValue = name;
		}
	}

	std::map<int,std::string>::iterator it = possibleValuesMap.begin();
	std::map<int,std::string>::iterator end = possibleValuesMap.end();
	for (; it!=end; ++it )
	{
			possibleValues.push_back( it->second );
	}

	// make sure the old string value is valid, otherwise select the first valid value
	bool setDefault = oldStringValue.empty();
	if (setDefault)
	{
		oldStringValue = possibleValues.front();
		//INFO_MSG( L("COMMON/EDITOR_VIEWS/CHOICEVIEW_ELECT", oldStringValue, property_.name() ) );
	}

	PropertyItem* newItem = new ComboPropertyItem(property_.name(), 
													oldStringValue.c_str(),
													possibleValues);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );

	if (setDefault)
		onChange( true );		// update the actual object property
}

//---------------------------------

void __stdcall GenBoolView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = property_.pBool()->get();

	PropertyItem* newItem = new BoolPropertyItem(property_.name(), oldValue_);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall GenFloatView::elect()
{
	propTable_ = PropTable::table();
	
	newValue_ = property_.pFloat()->get();
	oldValue_ = newValue_;
	lastValue_ = newValue_;
	lastTimeStamp_ = 0;

	PropertyItem* newItem = new FloatPropertyItem(property_.name(), property_.pFloat()->get());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	float min, max;
	int digits;
	if(property_.pFloat()->getRange( min, max, digits ))
		((FloatPropertyItem*)newItem)->setRange( min, max, digits );
	float def;
	if(property_.pFloat()->getDefault( def ))
		((FloatPropertyItem*)newItem)->setDefault( def );
	if( property_.name() == std::string("multiplier") )
		((FloatPropertyItem*)newItem)->setRange( 0, 3, 1 );
	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}


//---------------------------------

void __stdcall GenIntView::elect()
{
	propTable_ = PropTable::table();
	
	newValue_ = property_.pInt()->get();
	oldValue_ = newValue_;
	lastValue_ = newValue_;
	lastTimeStamp_ = 0;

	PropertyItem* newItem = new IntPropertyItem(property_.name(), property_.pInt()->get());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	int min, max;
	if(property_.pInt()->getRange( min, max ))
		((IntPropertyItem*)newItem)->setRange( min, max );
	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall GenPositionView::elect()
{
	propTable_ = PropTable::table();
	
	Matrix matrix;
	property_.pMatrix()->getMatrix( matrix );
	oldValue_ = matrix.applyToOrigin();

	CString name = property_.name();
	propertyItems_.reserve(3);
	FloatPropertyItem* newItem = new FloatPropertyItem( L("COMMON/EDITOR_VIEWS/X_NAME", L(name) ), oldValue_.x);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Y_NAME", L(name) ), oldValue_.y);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Z_NAME", L(name) ), oldValue_.z);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);

	propTable_->addView( this );
}

//---------------------------------

void __stdcall GenRotationView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = rotation();

	CString name = property_.name();
	propertyItems_.reserve(3);
	FloatPropertyItem* newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/PITCH", L(name) ), oldValue_.x);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/YAW", L(name) ), oldValue_.y);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/ROLL", L(name) ), oldValue_.z);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);

	propTable_->addView( this );
}

//---------------------------------

void __stdcall GenScaleView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = scale();

	CString name = property_.name();
	propertyItems_.reserve(3);
	FloatPropertyItem* newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/X_NAME", L(name) ), oldValue_.x);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Y_NAME", L(name) ), oldValue_.y);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	newItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Z_NAME", L(name) ), oldValue_.z);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);

	propTable_->addView( this );
}
//---------------------------------

void __stdcall GenLinkView::elect()
{
	propTable_ = PropTable::table();
	CString name = property_.name();
	CString linkValue = property_.link()->linkValue().c_str();
	oldValue_ = linkValue;
	StringPropertyItem* newItem = new StringPropertyItem(name, linkValue, true);
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);
	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

ColourView::ColourView( ColourProperty & property ) 
	: property_( property ),
	transient_( true )
{
}

ColourView::~ColourView()
{
}

void __stdcall ColourView::elect()
{
	
	propTable_ = PropTable::table();
	
	Moo::Colour c;
	if (!getCurrentValue( c ))
		return;

	newValue_ = c;
	oldValue_ = newValue_;
	lastValue_ = newValue_;
	lastTimeStamp_ = 0;

	char buf[256];
	bw_snprintf( buf, sizeof(buf), "%d , %d , %d , %d", (int)(c.r), (int)(c.g), (int)(c.b), (int)(c.a) );
	ColourPropertyItem* newItem = new ColourPropertyItem(property_.name(), buf, 1 );
	newItem->setGroup( property_.getGroup() );
	newItem->setGroupDepth( newItem->getGroupDepth() + 1 );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	propertyItems_.push_back(newItem);

	// we need to add the view here otherwise the children will be added in the view as well ( we don't want that. Trust me )
	int listLocation = propTable_->addView( this );

	IntPropertyItem* newColItem = new IntPropertyItem(L("COMMON/EDITOR_VIEWS/RED"), (int)(c.r));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	if ( property_.UIDesc() != "" )
		newColItem->UIDesc( L("COMMON/EDITOR_VIEWS/RED_END", property_.UIDesc() ) );

	newColItem->setRange( 0, 255 );
	propertyItems_.push_back(newColItem);

	newColItem = new IntPropertyItem(L("COMMON/EDITOR_VIEWS/GREEN"), (int)(c.g));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	if ( property_.UIDesc() != "" )
		newColItem->UIDesc( L("COMMON/EDITOR_VIEWS/GREEN_END", property_.UIDesc() ) );

	newColItem->setRange( 0, 255 );
	propertyItems_.push_back(newColItem);

	newColItem = new IntPropertyItem(L("COMMON/EDITOR_VIEWS/BLUE"), (int)(c.b));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	if ( property_.UIDesc() != "" )
		newColItem->UIDesc( L("COMMON/EDITOR_VIEWS/BLUE_END", property_.UIDesc() ) );

	newColItem->setRange( 0, 255 );
	propertyItems_.push_back(newColItem);

	newColItem = new IntPropertyItem(L("COMMON/EDITOR_VIEWS/ALPHA"), (int)(c.a));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	if ( property_.UIDesc() != "" )
		newColItem->UIDesc( L("COMMON/EDITOR_VIEWS/ALPHA_END", property_.UIDesc() ) );

	newColItem->setRange( 0, 255 );
	propertyItems_.push_back(newColItem);

	propTable_->propertyList()->collapseGroup( newItem,
		listLocation);
}

void ColourView::onChange( bool transient )
{
	int r,g,b,a;

	sscanf( item()->get().c_str(), "%d , %d , %d , %d", &r, &g, &b, &a );
	Moo::Colour c1( (FLOAT)r, (FLOAT)g, (FLOAT)b, (FLOAT)a );

	r = item(0)->get();
	g = item(1)->get();
	b = item(2)->get();
	a = item(3)->get();

	Moo::Colour c2( (FLOAT)r, (FLOAT)g, (FLOAT)b, (FLOAT)a );

	if (!equal( c1, oldValue_ ))
	{
		item(0)->set( (int)(c1.r) );
		item(1)->set( (int)(c1.g) );
		item(2)->set( (int)(c1.b) );
		item(3)->set( (int)(c1.a) );

		newValue_ = c1;
	}
	else if (!equal( c2, oldValue_ ))
	{
		char buf[256];
		bw_snprintf( buf, sizeof(buf), "%d , %d , %d , %d", (int)(c2.r), (int)(c2.g), (int)(c2.b), (int)(c2.a) );
		item()->set( buf );

		newValue_ = c2;
	}

	transient_ = transient;
}

void ColourView::updateGUI()
{
	Moo::Colour c;
	getCurrentValue( c );

	if (c != oldValue_)
	{
		newValue_ = c;
		oldValue_ = c;
		item(0)->set( (int)(c.r) );
		item(1)->set( (int)(c.g) );
		item(2)->set( (int)(c.b) );
		item(3)->set( (int)(c.a) );
		char buf[64];
		bw_snprintf( buf, sizeof(buf), "%d , %d , %d , %d", (int)(c.r), (int)(c.g), (int)(c.b), (int)(c.a) );
		item()->set( buf );
	}
		
	if ((newValue_ != oldValue_) || (!transient_))
	{
		float lastUpdateMilliseconds = (float) (((int64)(timestamp() - lastTimeStamp_)) / stampsPerSecondD()) * 1000.f;
		if (lastUpdateMilliseconds > 100.f)
		{
			if (!transient_)
			{
				setCurrentValue( lastValue_, true);
				lastValue_ = newValue_;
			}
			setCurrentValue( newValue_, transient_ );
			oldValue_ = newValue_;
			lastTimeStamp_ = timestamp();
			transient_ = true;
		}
	}
}

//---------------------------------

MultiplierFloatView::MultiplierFloatView( GenFloatProperty & property )
	: property_( property )
{
	isMultiplier_ = property.name() == std::string("multiplier");
}

MultiplierFloatView::~MultiplierFloatView()
{
}

void __stdcall MultiplierFloatView::elect()
{
	propTable_ = PropTable::table();
	
	if (!isMultiplier_)
		return;

	oldValue_ = property_.pFloat()->get();

	propTable_->addView( this );

	lastSeenSliderValue_ = oldValue_;

	propTable_->addView( this );
}

void MultiplierFloatView::onChange( bool transient )
{
}

void MultiplierFloatView::updateGUI()
{
}

//---------------------------------

void __stdcall PythonView::elect()
{
	propTable_ = PropTable::table();
	
	oldValue_ = getCurrentValue();

	StringPropertyItem* newItem = new StringPropertyItem(property_.name(), oldValue_.c_str());
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	// newItem->fileFilter( property_.fileFilter() );
	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

void __stdcall Vector4View::elect()
{
	propTable_ = PropTable::table();

	Vector4 c = property_.pVector4()->get();
	newValue_ = c;
	oldValue_ = newValue_;
	lastValue_ = newValue_;
	lastTimeStamp_ = 0;

	char buf[256];
	bw_snprintf( buf, sizeof(buf), "%.2f , %.2f , %.2f , %.2f", (float)(c.x), (float)(c.y), (float)(c.z), (float)(c.w) );
	ColourPropertyItem* newItem = new ColourPropertyItem(property_.name(), buf, 1, false /*property_.getGroupDepth() + 1*/ );
	newItem->setGroup( property_.getGroup() );
	newItem->setGroupDepth( newItem->getGroupDepth() + 1 );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );
	newItem->exposedToScriptName( property_.exposedToScriptName() );
	newItem->canExposeToScript( property_.canExposeToScript() );

	propertyItems_.push_back(newItem);

	// we need to add the view here otherwise the children will be added in the view as well ( we don't want that. Trust me )
	int listLocation = propTable_->addView( this );

	FloatPropertyItem* newColItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/X"), (float)(c.x));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	newColItem->setRange( 0.f, 1.f, 2 );
	propertyItems_.push_back(newColItem);

	newColItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Y"), (float)(c.y));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	newColItem->setRange( 0.f, 1.f, 2 );
	propertyItems_.push_back(newColItem);

	newColItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/Z"), (float)(c.z));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	newColItem->setRange( 0.f, 1.f, 2 );
	propertyItems_.push_back(newColItem);

	newColItem = new FloatPropertyItem(L("COMMON/EDITOR_VIEWS/W"), (float)(c.w));
	newItem->addChild( newColItem );
	newColItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	newColItem->setRange( 0.f, 1.f, 2 );
	propertyItems_.push_back(newColItem);

	propTable_->propertyList()->collapseGroup( newItem,
		listLocation);
}

void Vector4View::onChange( bool transient )
{
	float x,y,z,w;

	sscanf( item()->get().c_str(), "%f , %f , %f , %f", &x, &y, &z, &w );
	Vector4 c1( x, y, z, w );

	x = item(0)->get();
	y = item(1)->get();
	z = item(2)->get();
	w = item(3)->get();

	Vector4 c2( x, y, z, w );

	if (c1 != oldValue_)
	{
		item(0)->set( c1.x );
		item(1)->set( c1.y );
		item(2)->set( c1.z );
		item(3)->set( c1.w );

		newValue_ = c1;
	}
	else if (c2 != oldValue_)
	{
		char buf[256];
		bw_snprintf( buf, sizeof(buf), "%.2f , %.2f , %.2f , %.2f", c2.x, c2.y, c2.z, c2.w );
		item()->set( buf );

		newValue_ = c2;
	}

	transient_ = transient;
}

void Vector4View::updateGUI()
{
	Vector4 v; // = property_.pVector4()->get();
	getCurrentValue( v );

	if (v != oldValue_)
	{
		newValue_ = v;
		oldValue_ = v;
		item(0)->set( v.x );
		item(1)->set( v.y );
		item(2)->set( v.z );
		item(3)->set( v.w );
		char buf[256];
		bw_snprintf( buf, sizeof(buf), "%.2f , %.2f , %.2f , %.2f", v.x, v.y, v.z, v.w );
		item()->set( buf );
	}
		
	if ((newValue_ != oldValue_) || (!transient_))
	{
		float lastUpdateMilliseconds = (float) (((int64)(timestamp() - lastTimeStamp_)) / stampsPerSecondD()) * 1000.f;
		if (lastUpdateMilliseconds > 100.f)
		{
			if (!transient_)
			{
				setCurrentValue( lastValue_, true );
				lastValue_ = newValue_;
			}
			setCurrentValue( newValue_, transient_ );
			oldValue_ = newValue_;
			lastTimeStamp_ = timestamp();
			transient_ = true;
		}
	}
}

//---------------------------------

void __stdcall MatrixView::elect()
{
	propTable_ = PropTable::table();
	
	property_.pMatrix()->getMatrix( oldValue_, true );

	PropertyItem* newItem = new StringPropertyItem(property_.name(), NumVecToStr( (float*)&oldValue_, 16 ).c_str() );
	newItem->setGroup( property_.getGroup() );
	newItem->setChangeBuddy(this);

	newItem->descName( property_.descName() );
	newItem->UIDesc( property_.UIDesc() );

	propertyItems_.push_back(newItem);
	propTable_->addView( this );
}

//---------------------------------

TextView::Enroller TextView::Enroller::s_instance;
StaticTextView::Enroller StaticTextView::Enroller::s_instance;
TextLabelView::Enroller TextLabelView::Enroller::s_instance;
IDView::Enroller IDView::Enroller::s_instance;
GroupView::Enroller GroupView::Enroller::s_instance;
ListTextView::Enroller ListTextView::Enroller::s_instance;
ChoiceView::Enroller ChoiceView::Enroller::s_instance;
GenBoolView::Enroller GenBoolView::Enroller::s_instance;
GenFloatView::Enroller GenFloatView::Enroller::s_instance;
GenIntView::Enroller GenIntView::Enroller::s_instance;
GenPositionView::Enroller GenPositionView::Enroller::s_instance;
GenRotationView::Enroller GenRotationView::Enroller::s_instance;
GenScaleView::Enroller GenScaleView::Enroller::s_instance;
GenLinkView::Enroller GenLinkView::Enroller::s_instance;
ColourView::Enroller ColourView::Enroller::s_instance;
MultiplierFloatView::Enroller MultiplierFloatView::Enroller::s_instance;
PythonView::Enroller PythonView::Enroller::s_instance;
Vector4View::Enroller Vector4View::Enroller::s_instance;
MatrixView::Enroller MatrixView::Enroller::s_instance;