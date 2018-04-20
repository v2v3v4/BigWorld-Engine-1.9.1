/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#pragma once

#include "property_list.hpp"
#include "property_table.hpp"

#include "gizmo/link_property.hpp"
#include "gizmo/general_editor.hpp"
#include "gizmo/general_properties.hpp"

#include <map>

class PropertyTable;

class PropTable
{
public:
	static void table( PropertyTable* table ) { s_propTable_ = table; }
	static PropertyTable* table() { return s_propTable_; }
private:
	static PropertyTable* s_propTable_;
};

class BaseView : public GeneralProperty::View
{
public:
	BaseView();

	typedef std::vector<PropertyItem*> PropertyItems;
	PropertyItems& propertyItems() { return propertyItems_; }

	virtual void onChange( bool transient ) = 0;
	virtual void updateGUI() = 0;

	virtual void __stdcall deleteSelf()
	{
		for (PropertyItems::iterator it = propertyItems_.begin();
			it != propertyItems_.end();
			it++)
		{
			delete *it;
		}
		propertyItems_.clear();

		GeneralProperty::View::deleteSelf();
	}

	virtual void __stdcall expel();

	virtual void __stdcall select() {};

	virtual void onSelect() {};

	virtual PropertyManagerPtr getPropertyManger() const { return NULL; }
	virtual void setToDefault(){}
	virtual bool isDefault(){	return false;	}

protected:

	PropertyItems propertyItems_;
	PropertyTable* propTable_;

};


// text
class TextView : public BaseView
{
public:
	TextView( TextProperty & property ):
		property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static TextProperty::View * create( TextProperty & property )
	{
		return new TextView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }


	TextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			TextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};



// static text
class StaticTextView : public BaseView
{
public:
	StaticTextView( StaticTextProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static StaticTextProperty::View * create( StaticTextProperty & property )
	{
		return new StaticTextView( property );
	}

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

	StaticTextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			StaticTextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};



// static text
class TextLabelView : public BaseView
{
public:
	TextLabelView( TextLabelProperty & property )
		: property_( property )
	{
	}

	LabelPropertyItem* item() { return (LabelPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
	}

	virtual void updateGUI()
	{
	}

	static TextLabelProperty::View * create( TextLabelProperty & property )
	{
		return new TextLabelView( property );
	}

    void setCurrentValue(std::string s)
    {
    }

	void * getUserObject()
	{
		return property_.getUserObject();
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	TextLabelProperty & property_;

	class Enroller {
		Enroller() {
			TextLabelProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// id view
class IDView : public BaseView
{
public:
	IDView( IDProperty & property )
		: property_( property )
	{
	}

	IDPropertyItem* item() { return (IDPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static IDProperty::View * create( IDProperty & property )
	{
		return new IDView( property );
	}

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

	IDProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			IDProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};



// group view
class GroupView : public BaseView
{
public:
	GroupView( GroupProperty & property )
		: property_( property )
	{
	}

	GroupPropertyItem* item() { return (GroupPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
	}

	virtual void updateGUI()
	{
	}

	static GroupProperty::View * create( GroupProperty & property )
	{
		return new GroupView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GroupProperty & property_;

	class Enroller {
		Enroller() {
			GroupProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

// list of text items
class ListTextView : public BaseView
{
public:
	ListTextView( ListTextProperty & property )
		: property_( property )
	{
	}

	ComboPropertyItem* item() { return (ComboPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void __stdcall expel()
	{
		item()->setChangeBuddy(NULL);
	}

	virtual void onChange( bool transient )
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            setCurrentValue( s );
            oldValue_ = s;
        }
	}

	virtual void updateGUI()
	{
        std::string s = getCurrentValue();

        if (s != oldValue_)
        {
            oldValue_ = s;
			item()->set(s);
        }
	}

	static ListTextProperty::View * create( ListTextProperty & property )
	{
		return new ListTextView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
            return "";

        std::string s = "";

        PyObject * pAsString = PyObject_Str( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    void setCurrentValue(std::string s)
    {
        property_.pySet( Py_BuildValue( "s", s.c_str() ) );
    }


	ListTextProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			ListTextProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// list of text items associated to an int field
class ChoiceView : public BaseView
{
public:
	ChoiceView( ChoiceProperty & property )
		: property_( property )
	{
	}

	ComboPropertyItem* item() { return (ComboPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		std::string s = item()->get();
		int v = choices_.find( s )->second;

		if (v != oldValue_)
		{
			property_.pInt()->set( v, false );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		const int newValue = property_.pInt()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			// set the combobox based on string as the int values may be disparate
			std::string strValue = "";
			for ( StringHashMap<int>::iterator it = choices_.begin();
				it != choices_.end();
				it++ )
			{
				if (it->second == newValue)
				{
					strValue = it->first;
					break;
				}
			}

			item()->set(strValue);
		}
	}

	static ChoiceProperty::View * create( ChoiceProperty & property )
	{
		return new ChoiceView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	ChoiceProperty & property_;
	int oldValue_;
	StringHashMap<int> choices_;

	class Enroller {
		Enroller() {
			ChoiceProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};



// bool
class GenBoolView : public BaseView
{
public:
	GenBoolView( GenBoolProperty & property ) : property_( property )
	{
	}

	BoolPropertyItem* item() { return (BoolPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		bool newValue = item()->get();
		property_.pBool()->set( newValue, false );
		oldValue_ = newValue;
	}

	virtual void updateGUI()
	{
		const bool newValue = property_.pBool()->get();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set(newValue);
		}
	}

	static GenFloatProperty::View * create( GenBoolProperty & property )
	{
		return new GenBoolView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GenBoolProperty & property_;
	bool oldValue_;

	class Enroller {
		Enroller() {
			GenBoolProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// float
class GenFloatView : public BaseView
{
public:
	GenFloatView( GenFloatProperty & property )
		: property_( property ),
		transient_( true )
	{
	}

	FloatPropertyItem* item() { return (FloatPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void setToDefault()
	{
		property_.pFloat()->setToDefault();
	}

	virtual bool isDefault()
	{
		return property_.pFloat()->isDefault();
	}

	virtual void onChange( bool transient )
	{
		newValue_ = item()->get();
		transient_ = transient;
	}

	virtual void updateGUI()
	{
		float v = property_.pFloat()->get();

		if (v != oldValue_)
		{
			newValue_ = v;
			oldValue_ = v;
			item()->set( v );
		}

		if ((newValue_ != oldValue_) || (!transient_))
		{
			float lastUpdateMilliseconds = (float) (((int64)(timestamp() - lastTimeStamp_)) / stampsPerSecondD()) * 1000.f;
			if (lastUpdateMilliseconds > 100.f)
			{
				if (!transient_)
				{
					property_.pFloat()->set( lastValue_, true);
					lastValue_ = newValue_;
				}
				property_.pFloat()->set( newValue_, transient_ );
				oldValue_ = newValue_;
				lastTimeStamp_ = timestamp();
				transient_ = true;
			}
		}
	}

	static GenFloatProperty::View * create( GenFloatProperty & property )
	{
		return new GenFloatView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenFloatProperty & property_;
	float oldValue_;
	float newValue_;
	float lastValue_;
	uint64 lastTimeStamp_;
	bool transient_;


	class Enroller {
		Enroller() {
			GenFloatProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// int
class GenIntView : public BaseView
{
public:
	GenIntView( GenIntProperty & property )
		: property_( property ),
		transient_( true )
	{
	}

	IntPropertyItem* item() { return (IntPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		newValue_ = item()->get();
		transient_ = transient;
	}

	virtual void updateGUI()
	{
		int v = property_.pInt()->get();

		if (v != oldValue_)
		{
			newValue_ = v;
			oldValue_ = v;
			item()->set( v );
		}

		if ((newValue_ != oldValue_) || (!transient_))
		{
			float lastUpdateMilliseconds = (float) (((int64)(timestamp() - lastTimeStamp_)) / stampsPerSecondD()) * 1000.f;
			if (lastUpdateMilliseconds > 100.f)
			{
				if (!transient_)
				{
					property_.pInt()->set( lastValue_, true);
					lastValue_ = newValue_;
				}
				property_.pInt()->set( newValue_, transient_ );
				oldValue_ = newValue_;
				lastTimeStamp_ = timestamp();
				transient_ = true;
			}
		}
	}

	static GenIntProperty::View * create( GenIntProperty & property )
	{
		return new GenIntView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenIntProperty & property_;
	int newValue_;
	int oldValue_;
	int lastValue_;
	uint64 lastTimeStamp_;
	bool transient_;

	class Enroller {
		Enroller() {
			GenIntProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// vector3 position
class GenPositionView : public BaseView
{
public:
	GenPositionView( GenPositionProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemX() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemY() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemZ() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		Vector3 newValue;
		newValue.x = itemX()->get();
		newValue.y = itemY()->get();
		newValue.z = itemZ()->get();

		Matrix matrix, ctxInv;
		property_.pMatrix()->recordState();
		property_.pMatrix()->getMatrix( matrix, false );
		property_.pMatrix()->getMatrixContextInverse( ctxInv );
		matrix.translation( ctxInv.applyPoint( newValue ) );
		if( !property_.pMatrix()->setMatrix( matrix ) )
		{
			oldValue_ = newValue;
			updateGUI();
		}
		property_.pMatrix()->commitState();
	}

	virtual void updateGUI()
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );
		const Vector3 & newValue = matrix.applyToOrigin();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			itemX()->set(newValue.x);
			itemY()->set(newValue.y);
			itemZ()->set(newValue.z);
		}
	}

	static GenPositionProperty::View * create( GenPositionProperty & property )
	{
		return new GenPositionView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	GenPositionProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenPositionProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// rotation
class GenRotationView : public BaseView
{
public:
	GenRotationView( GenRotationProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemPitch() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemYaw() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemRoll() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		Matrix prevMatrix;
		property_.pMatrix()->recordState();
		property_.pMatrix()->getMatrix( prevMatrix, false );

		Matrix newMatrix, temp;
		newMatrix.setScale(
			prevMatrix.applyToUnitAxisVector( X_AXIS ).length(),
			prevMatrix.applyToUnitAxisVector( Y_AXIS ).length(),
			prevMatrix.applyToUnitAxisVector( Z_AXIS ).length() );

		// If pitch = 90deg, we add a small epsilon to get meaningful yaw and
		// roll values.
		float xPitch = itemPitch()->get();
		if (almostEqual( fabs(xPitch), 90.f ))
		{
			// Add the epsilon in the same direction of the sign of the pitch.
			xPitch += xPitch >= 0.f ? -0.04f : 0.04f;
		}
		float yYaw = itemYaw()->get();
		float zRoll = itemRoll()->get();

		temp.setRotate(
			DEG_TO_RAD( yYaw ),
			DEG_TO_RAD( xPitch ),
			DEG_TO_RAD( zRoll ) );
		newMatrix.postMultiply( temp );

		temp.setTranslate( prevMatrix.applyToOrigin() );

		newMatrix.postMultiply( temp );

		if( !property_.pMatrix()->setMatrix( newMatrix ) )
		{
			oldValue_ = Vector3( itemPitch()->get(), itemYaw()->get(), itemRoll()->get() );
			updateGUI();
		}
		property_.pMatrix()->commitState();
	}

	virtual void updateGUI()
	{
		const Vector3 newValue = rotation();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			// We need to round to 1 decimal only. More than that makes it
			// difficult/confusing for the user to edit the angles.
			itemPitch()->set( roundTo( newValue.x, 10, 1 ) );
			itemYaw()->set( roundTo( newValue.y, 10, 1 ) );
			itemRoll()->set( roundTo( newValue.z, 10, 1 ) );
		}
	}

	static GenRotationProperty::View * create( GenRotationProperty & property )
	{
		return new GenRotationView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:

	/**
	 *  This method rounds a float value to the nearest number that could be divided by 
	 *  a epsilon( e.g. 0.05 ) without remainder.
	 *  A few examples:
	 *	- roundTo( 13.9352, 100, 1 ) = 13.94 (it's rounded to 0.01)
	 *	- roundTo( 13.9352, 100, 3 ) = 13.95 (it's rounded to 0.03)
	 *
	 *	@param value	number to be rounded
	 *	@param base		a power of 10 to express how many digits (10 = 1, 100 = 2, etc)
	 *	@param multiple this param and base build the epsilon
	 *	@return			rounded number
	 */
	float roundTo( float value, int base, int multiple )
	{
		float temp = floorf( value * base + 0.5f );
		temp = floorf( temp / multiple + 0.5f );
		temp = floorf( temp * multiple );
		return temp / base;
	}


	/**
	 *  This method format the yaw, pitch and roll, if both yaw and roll 
	 *  are > 90deg, change them back to a value <= 90deg by adjusting 
	 *  pitch at same time
	 */
	Vector3 formatRotation( float xPitch, float yYaw, float zRoll ) const
	{
		if (almostEqual( xPitch, -180.f, 0.02f ))
		{
			xPitch = -xPitch;
		}

		if (almostEqual( yYaw, -180.f, 0.02f ))
		{
			yYaw = -yYaw;
		}

		if (almostEqual( zRoll, -180.f, 0.02f ))
		{
			zRoll = -zRoll;
		}

		if ((yYaw < -90.f || yYaw > 90.f) && (zRoll < -90.f || zRoll > 90.f))
		{
			float adj = (almostEqual( xPitch, 0.f ) || xPitch > 0.f) ? 180.f : -180.f;
			xPitch = adj - xPitch;
			yYaw = fmodf(yYaw - adj, 360.f);
			
			if (yYaw < -180.f)
			{
				yYaw += 360.f;
			}
			else if (yYaw > 180.f)
			{
				yYaw -= 360.f;
			}

			zRoll = fmodf(zRoll - adj, 360.f);

			if (zRoll < -180.f)
			{
				zRoll += 360.f;
			}
			else if (zRoll > 180.f)
			{
				zRoll -= 360.f;
			}

		}

		return Vector3( xPitch, yYaw, zRoll );

	}

	Vector3 rotation() const
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );

		return formatRotation(
			RAD_TO_DEG( matrix.pitch() ),
			RAD_TO_DEG( matrix.yaw() ) ,
			RAD_TO_DEG( matrix.roll() ) );
	}

	GenRotationProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenRotationProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// scale
class GenScaleView : public BaseView
{
public:
	GenScaleView( GenScaleProperty & property )
		: property_( property )
	{
	}

	FloatPropertyItem* itemX() { return (FloatPropertyItem*)&*(propertyItems_[0]); }
	FloatPropertyItem* itemY() { return (FloatPropertyItem*)&*(propertyItems_[1]); }
	FloatPropertyItem* itemZ() { return (FloatPropertyItem*)&*(propertyItems_[2]); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		Vector3 newValue;
		newValue.x = itemX()->get();
		newValue.y = itemY()->get();
		newValue.z = itemZ()->get();

		if (oldValue_.x != 0.f &&
			oldValue_.y != 0.f &&
			oldValue_.z != 0.f &&
			newValue.x != 0.f &&
			newValue.y != 0.f &&
			newValue.z != 0.f)
		{
			Matrix matrix;
			property_.pMatrix()->recordState();
			property_.pMatrix()->getMatrix( matrix, false );
			Matrix scaleMatrix;
			scaleMatrix.setScale(
				newValue.x/oldValue_.x,
				newValue.y/oldValue_.y,
				newValue.z/oldValue_.z );
			matrix.preMultiply( scaleMatrix );

			if( !property_.pMatrix()->setMatrix( matrix ) )
			{
				oldValue_ = newValue;
				updateGUI();
			}
			property_.pMatrix()->commitState();
		}
	}

	virtual void updateGUI()
	{
		const Vector3 newValue = this->scale();

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;

			itemX()->set(newValue.x);
			itemY()->set(newValue.y);
			itemZ()->set(newValue.z);
		}
	}

	static GenScaleProperty::View * create( GenScaleProperty & property )
	{
		return new GenScaleView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	Vector3 scale() const
	{
		Matrix matrix;
		property_.pMatrix()->getMatrix( matrix );
		return Vector3(
			matrix.applyToUnitAxisVector( X_AXIS ).length(),
			matrix.applyToUnitAxisVector( Y_AXIS ).length(),
			matrix.applyToUnitAxisVector( Z_AXIS ).length() );
	}

	GenScaleProperty & property_;
	Vector3 oldValue_;

	class Enroller {
		Enroller() {
			GenScaleProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

//link

class GenLinkView : public BaseView
{
public:
	GenLinkView( LinkProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_[0]); }
	
	virtual void __stdcall elect();

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange(bool transient)
	{
			
	}

	virtual void updateGUI()
	{
		const CString newValue = property_.link()->linkValue().c_str();
		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set(newValue.GetString());
		}		
	}

	static LinkProperty::View * create(LinkProperty & property )
	{
		return new GenLinkView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	LinkProperty & property_;
	CString oldValue_;
	class Enroller {
		Enroller() {
			LinkProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// colour
class ColourView : public BaseView
{
public:
	ColourView( ColourProperty & property );

	~ColourView();

	ColourPropertyItem* item() { return (ColourPropertyItem*)&*(propertyItems_[0]); }

	IntPropertyItem* item( int index ) { return (IntPropertyItem*)&*(propertyItems_[index+1]); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient );

	virtual void updateGUI();

	static ColourProperty::View * create( ColourProperty & property )
	{
		return new ColourView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
	Vector4 toVector( Moo::Colour c )
	{
		return Vector4(c.r, c.g, c.b, c.a);
	}

	Moo::Colour toColour( Vector4& v )
	{
		Moo::Colour c;
		c.r = v.x;
		c.g = v.y;
		c.b = v.z;
		c.a = v.w;
	}

	bool fromString( const char* s, Moo::Colour& c )
	{
		if (*s == '#')
			s++;

		if (strlen(s) != 8)
			return false;

		char* endptr;

		char buf[3];
		buf[2] = '\0';

		buf[0] = s[0];
		buf[1] = s[1];
		c.r = (float)strtol(buf, &endptr, 16);

		buf[0] = s[2];
		buf[1] = s[3];
		c.g = (float)strtol(buf, &endptr, 16);

		buf[0] = s[4];
		buf[1] = s[5];
		c.b = (float)strtol(buf, &endptr, 16);

		buf[0] = s[6];
		buf[1] = s[7];
		c.a = (float)strtol(buf, &endptr, 16);

		return true;
	}

	void setCurrentValue( Moo::Colour& c, bool transient )
	{
		property_.pySet( Py_BuildValue( "ffff", c.r, c.g, c.b, c.a ), transient );
	}

	bool getCurrentValue(Moo::Colour& c)
	{

		PyObject* pValue = property_.pyGet();
		if (!pValue) {
			PyErr_Clear();
			return false;
		}

		// Using !PyVector<Vector4>::Check( pValue ) doesn't work so we do it
		// this dodgy way
		if (std::string(pValue->ob_type->tp_name) != "Math.Vector4")
		{
			PyErr_SetString( PyExc_TypeError, "ColourView::getCurrentValue() "
				"expects a PyVector<Vector4>" );

    		Py_DECREF( pValue );
			return false;
		}

		PyVector<Vector4>* pv = static_cast<PyVector<Vector4>*>( pValue );
		float* v = (float*) &pv->getVector();

		c.r = v[0];
		c.g = v[1];
		c.b = v[2];
		c.a = v[3];

		Py_DECREF( pValue );

		return true;
	}

	bool equal( Moo::Colour c1, Moo::Colour c2 )
	{
		return ((int) c1.r == (int) c2.r &&
			(int) c1.g == (int) c2.g &&
			(int) c1.b == (int) c2.b &&
			(int) c1.a == (int) c2.a);
	}

	ColourProperty & property_;
	Moo::Colour oldValue_;
	Moo::Colour newValue_;
	Moo::Colour lastValue_;
	uint64 lastTimeStamp_;
	bool transient_;

	class Enroller {
		Enroller() {
			ColourProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


class MultiplierFloatView : public BaseView
{
public:
	MultiplierFloatView( GenFloatProperty & property );

	~MultiplierFloatView();

	//FloatPropertyItem* item() { return (FloatPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient );

	virtual void updateGUI();

	static GenFloatProperty::View * create( GenFloatProperty & property )
	{
		GenFloatProperty::View* view = new MultiplierFloatView( property );
		return view;
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	bool isMultiplier_;
	GenFloatProperty & property_;
	float oldValue_;

	float lastSeenSliderValue_;

	class Enroller {
		Enroller() {
			GenFloatProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};


// Python
class PythonView : public BaseView
{
public:
	PythonView( PythonProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();
	
	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
        std::string s = item()->get();

        if (s != oldValue_)
        {
            if (this->setCurrentValue( s ))
			{
				oldValue_ = s;
			}
        }
	}

	virtual void updateGUI()
	{
        std::string s = this->getCurrentValue();

        if (s != oldValue_)
        {
			if (this->setCurrentValue( s ))
			{
				oldValue_ = s;
			}
        }
	}

	static PythonProperty::View * create( PythonProperty & property )
	{
		return new PythonView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

private:
    std::string getCurrentValue()
    {
		PyObject* pValue = property_.pyGet();
        if (!pValue)
		{
			PyErr_Clear();
            return "";
		}

        std::string s = "";

        PyObject * pAsString = PyObject_Repr( pValue );

		if (pAsString)
		{
			s = PyString_AsString( pAsString );

			Py_DECREF( pAsString );
		}

		Py_DECREF( pValue );

        return s;
    }

    bool setCurrentValue( std::string s )
    {
		PyObject * pNew = Script::runString( s.c_str(), false );

		if (pNew)
		{
			property_.pySet( pNew );
			// This may be slightly differnt to s
			std::string newStr =  this->getCurrentValue();
			this->item()->set( newStr );
			oldValue_ = newStr;
			Py_DECREF( pNew );
		}
		else
		{
			PyErr_Clear();
			return false;
		}

		return true;
    }


	PythonProperty & property_;
	std::string oldValue_;

	class Enroller {
		Enroller() {
			PythonProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

// help method for vector4 and matrix
#include <sstream>

template<typename EntryType>
static std::string NumVecToStr( const EntryType* vec, int size )
{
	std::ostringstream oss;
	for( int i = 0; i < size; ++i )
	{
		oss << vec[ i ];
		if( i != size - 1 )
			oss << ',';
	}
	return oss.str();
}

template<typename EntryType>
static bool StrToNumVec( std::string str, EntryType* vec, int size )
{
	bool result = false;
	std::istringstream iss( str );

	for( int i = 0; i < size; ++i )
	{
		char ch;
		iss >> vec[ i ];
		if( i != size - 1 )
		{
			iss >> ch;
			if( ch != ',' )
				break;
		}
		else
			result = iss != NULL;
	}
	return result;
}

// vector4
class Vector4View : public BaseView
{
public:
	Vector4View( Vector4Property & property )
		: property_( property ),
		transient_( true )
	{}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	FloatPropertyItem* item( int index ) { return (FloatPropertyItem*)&*(propertyItems_[index+1]); }

	virtual void __stdcall elect();

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient );

	virtual void updateGUI();

	static Vector4Property::View * create( Vector4Property & property )
	{
		return new Vector4View( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	void setCurrentValue( Vector4& v, bool transient )
	{
		property_.pySet( Py_BuildValue( "ffff", v.x, v.y, v.z, v.w ), transient );
	}

	bool getCurrentValue( Vector4& v )
	{

		PyObject* pValue = property_.pyGet();
		if (!pValue) {
			PyErr_Clear();
			return false;
		}

		// Using !PyVector<Vector4>::Check( pValue ) doesn't work so we do it
		// this dodgy way
		if (std::string(pValue->ob_type->tp_name) != "Math.Vector4")
		{
			PyErr_SetString( PyExc_TypeError, "Vector4View::getCurrentValue() "
				"expects a PyVector<Vector4>" );

    		Py_DECREF( pValue );
			return false;
		}

		PyVector<Vector4>* pv = static_cast<PyVector<Vector4>*>( pValue );
		float* tv = (float*) &pv->getVector();

		v.x = tv[0];
		v.y = tv[1];
		v.z = tv[2];
		v.w = tv[3];

		Py_DECREF( pValue );

		return true;
	}

	Vector4Property & property_;

	Vector4 newValue_;
	Vector4 oldValue_;
	Vector4 lastValue_;
	uint64 lastTimeStamp_;
	bool transient_;

	class Enroller {
		Enroller() {
			Vector4Property_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};

// matrix
class MatrixView : public BaseView
{
public:
	MatrixView( GenMatrixProperty & property )
		: property_( property )
	{
	}

	StringPropertyItem* item() { return (StringPropertyItem*)&*(propertyItems_.front()); }

	virtual void __stdcall elect();

	virtual void onSelect()
	{
		property_.select();
	}

	virtual void onChange( bool transient )
	{
		std::string newValue = item()->get();
		Matrix v;
		if( StrToNumVec( newValue, (float*)v, 16 ) )
		{
			property_.pMatrix()->setMatrix( v );
			oldValue_ = v;
		}
	}

	virtual void updateGUI()
	{
		Matrix newValue;
		property_.pMatrix()->getMatrix( newValue, true );

		if (newValue != oldValue_)
		{
			oldValue_ = newValue;
			item()->set( NumVecToStr( (float*)&newValue, 16 ) );
		}
	}

	static GenMatrixProperty::View * create( GenMatrixProperty & property )
	{
		return new MatrixView( property );
	}

	virtual PropertyManagerPtr getPropertyManger() const { return property_.getPropertyManager(); }

protected:

private:
	GenMatrixProperty & property_;
	Matrix oldValue_;

	class Enroller {
		Enroller() {
			GenMatrixProperty_registerViewFactory(
				GeneralProperty::nextViewKindID(), &create );
		}
		static Enroller s_instance;
	};
};
