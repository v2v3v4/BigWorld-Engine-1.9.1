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
#include "genprop_gizmoviews.hpp"

#include "general_properties.hpp"

#include "position_gizmo.hpp"
#include "rotation_gizmo.hpp"
#include "scale_gizmo.hpp"
#include "radius_gizmo.hpp"
#include "angle_gizmo.hpp"
#include "link_gizmo.hpp"
#include "link_property.hpp"
#include "input/input.hpp"
#include "cstdmf/debug.hpp"
#include "current_general_properties.hpp"

DECLARE_DEBUG_COMPONENT2( "Gizmo", 0 )


int genprop_gizmoviews_token;

/**
 *	This is a demonstration view class that puts a gizmo in the world
 *	whenever a position property is elected
 */
class GenPosPropAxes : public GeneralProperty::View
{
public:
	GenPosPropAxes( GenPositionProperty & prop ) : prop_( prop )
	{		
	}

	~GenPosPropAxes()
	{	
	}

	virtual void EDCALL elect()
	{
		if (s_count_++ == 0)
		{
			s_pGizmo_ = new PositionGizmo( MODIFIER_SHIFT | MODIFIER_CTRL |MODIFIER_ALT );
			GizmoManager::instance().addGizmo( s_pGizmo_ );
		}
	}

	virtual void EDCALL expel()
	{
		if (--s_count_ == 0)
		{
			GizmoManager::instance().removeGizmo( s_pGizmo_ );
			s_pGizmo_ = NULL;
		}
	}

	virtual void EDCALL select()
	{
		if (s_pGizmo_ != NULL)
		{
			GizmoSetPtr set = new GizmoSet();
			set->add( s_pGizmo_ );
			GizmoManager::instance().forceGizmoSet( set );
		}
	}

	static GeneralProperty::View * create( GenPositionProperty & prop )
	{
		return new GenPosPropAxes( prop );
	}

private:
	GenPositionProperty	& prop_;
	static GizmoPtr		s_pGizmo_;
	static int			s_count_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			GenPositionProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

GenPosPropAxes::ViewEnroller GenPosPropAxes::s_viewEnroller;
int GenPosPropAxes::s_count_ = 0;
GizmoPtr GenPosPropAxes::s_pGizmo_ = NULL;


/**
 *	This creates LinkGizmos.
 */
class LinkPropertyView : public GeneralProperty::View
{
public:
	LinkPropertyView( LinkProperty & prop ) : prop_( &prop )
	{		
		pGizmo_ = new LinkGizmo(prop_->link(), prop_->matrix());
	}

	~LinkPropertyView()
	{	
        prop_ = NULL;
	}

	virtual void EDCALL elect()
	{
		if ( prop_->alwaysShow() )
			GizmoManager::instance().addGizmo( pGizmo_ );
	}

	virtual void EDCALL expel()
	{
		if ( prop_->alwaysShow() )
			GizmoManager::instance().removeGizmo( pGizmo_ );
	}

	virtual void EDCALL select()
	{		
		GizmoSetPtr set = new GizmoSet();
		set->add( pGizmo_ );
		GizmoManager::instance().forceGizmoSet( set );
	}

	static GeneralProperty::View * create( LinkProperty & prop )
	{
		return new LinkPropertyView( prop );
	}

private:
	LinkProperty	    *prop_;
	GizmoPtr		    pGizmo_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			LinkProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

LinkPropertyView::ViewEnroller LinkPropertyView::s_viewEnroller;



/**
 *	This is a demonstration view class that puts a gizmo in the world
 *	whenever a rotation property is elected
 */
class GenRotPropDiscs : public GeneralProperty::View
{
public:
	GenRotPropDiscs( GenRotationProperty & prop ) : prop_( prop )
	{		
	}

	~GenRotPropDiscs()
	{		
	}

	virtual void EDCALL elect()
	{
		if (s_count_++ == 0)
		{
			if ( prop_.allowGizmo() )
			{
				s_pGizmo_ = new RotationGizmo( prop_.pMatrix(), MODIFIER_SHIFT );
				GizmoManager::instance().addGizmo( s_pGizmo_ );
			}
		}
	}

	virtual void EDCALL expel()
	{
		if (--s_count_ == 0)
		{
			GizmoManager::instance().removeGizmo( s_pGizmo_ );
			s_pGizmo_ = NULL;
		}
	}

	virtual void EDCALL select()
	{
		if (s_pGizmo_ != NULL)
		{
			GizmoSetPtr set = new GizmoSet();
			if (s_pGizmo_ != NULL)
				set->add( s_pGizmo_ );
			if (set->vector().size() != 0)
				GizmoManager::instance().forceGizmoSet( set );
		}
	}

	static GeneralProperty::View * create( GenRotationProperty & prop )
	{
		return new GenRotPropDiscs( prop );
	}

private:
	GenRotationProperty	& prop_;
	static GizmoPtr		s_pGizmo_;
	static int			s_count_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			GenRotationProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

GenRotPropDiscs::ViewEnroller GenRotPropDiscs::s_viewEnroller;
int GenRotPropDiscs::s_count_ = 0;
GizmoPtr GenRotPropDiscs::s_pGizmo_ = NULL;



/**
 * Applies a uniform scale to all current scale properties.
 *
 * Doesn't try to extract the current uniform scale, simply adds another one
 * on top.
 */
class UniformScaleFloatProxy : public FloatProxy
{
public:
	UniformScaleFloatProxy()
		: curScale_( 1.f )
		, newTransformOnSet_( true )
	{
	}

	virtual float EDCALL get() const
	{
		return curScale_;
	}

	virtual void EDCALL set( float f, bool transient )
	{
		// Reset after a commit
		if (newTransformOnSet_)
		{
			std::vector<GenScaleProperty*> props = CurrentScaleProperties::properties();
			for (std::vector<GenScaleProperty*>::iterator i = props.begin(); i != props.end(); ++i)
				(*i)->pMatrix()->recordState();

			newTransformOnSet_ = false;
		}		

		// Don't allow 0 or negative scales
		if (f < 0.001f)
			f = 0.001f;

//		curScale_ = f;
		bool overallSuccess = true;

		std::vector<GenScaleProperty*> props = CurrentScaleProperties::properties();
		for (std::vector<GenScaleProperty*>::iterator i = props.begin(); i != props.end(); ++i)
		{
			Matrix m;

			(*i)->pMatrix()->commitState( true, false );	// revert to the orig
			(*i)->pMatrix()->getMatrix( m, false );

			Matrix s;
			s.setScale( f, f, f );
			m.preMultiply(s);

			bool success = (*i)->pMatrix()->setMatrix( m );

			overallSuccess = overallSuccess && success;

			if( ! success )
			{
				(*i)->pMatrix()->commitState( true, false );	// revert to the orig
				(*i)->pMatrix()->getMatrix( m, false );
				s.setScale( curScale_, curScale_, curScale_ );
				m.preMultiply(s);
				(*i)->pMatrix()->setMatrix( m );
			}

			if (!transient)
			{
				(*i)->pMatrix()->commitState( false, false );
			}
		}

		if( overallSuccess )
			curScale_ = f;

		if (!transient)
		{
			curScale_ = 1.f;
			newTransformOnSet_ = true;
		}
	}

private:
	MatrixProxyPtr matrixProxy_;
	float curScale_;
	bool newTransformOnSet_;
};



/** Exposes the current centre position of all the position properties */
class CurrentPositionMatrixProxy : public MatrixProxy
{
public:
	virtual void EDCALL getMatrix( Matrix & m, bool world = true )
	{
		MF_ASSERT( world );

		m.setTranslate( CurrentPositionProperties::centrePosition() );
	}

	virtual void EDCALL getMatrixContext( Matrix & m )
	{
		MF_ASSERT( 0 );
	}

	virtual void EDCALL getMatrixContextInverse( Matrix & m )
	{
		MF_ASSERT( 0 );
	}

	virtual bool EDCALL setMatrix( const Matrix & m )
	{
		MF_ASSERT( 0 );
		return false;
	}

	virtual void EDCALL recordState()
	{
		MF_ASSERT( 0 );
	}

	virtual bool EDCALL commitState( bool revertToRecord = false, bool addUndoBarrier = true )
	{
		MF_ASSERT( 0 );
		return true;
	}

	virtual bool EDCALL hasChanged()
	{
		MF_ASSERT( 0 );
		return false;
	}
};


/**
 *	This is a demonstration view class that puts a gizmo in the world
 *	whenever a scale property is elected
 */
class GenScalePropAxes : public GeneralProperty::View
{
public:
	GenScalePropAxes( GenScaleProperty & prop ) : prop_( prop )
	{		
	}

	~GenScalePropAxes()
	{		
	}

	virtual void EDCALL elect()
	{
		if (s_count_++ == 0)
		{
			static bool addedWatcher = false;
			static float scaleSpeedFactor = 0.25f;
			static float uniformScaleSpeedFactor = 0.1f;

			if ( !addedWatcher )
			{
				MF_WATCH( "App/scaleSpeed", scaleSpeedFactor, Watcher::WT_READ_WRITE, "Speed multiplier for using the non-uniform scale gizmo." );
				MF_WATCH( "App/uniformScaleSpeed", uniformScaleSpeedFactor, Watcher::WT_READ_WRITE, "Speed multiplier for using the uniform scale gizmo." );
				addedWatcher = true;
			}			

			if (prop_.allowNonUniformScale())
			{
				s_pGizmo_ = new ScaleGizmo(NULL,MODIFIER_ALT,scaleSpeedFactor);
				GizmoManager::instance().addGizmo( s_pGizmo_ );
			}
			if (prop_.allowUniformScale())
			{
				s_pUniformScaleGizmo_ = new RadiusGizmo( new UniformScaleFloatProxy(),
					new CurrentPositionMatrixProxy(),
					"Uniform Scale",
					0xffffffff,
					8.f,
					MODIFIER_ALT,
					uniformScaleSpeedFactor,
					true,
					NULL,
					NULL,
					RadiusGizmo::SHOW_SPHERE_NEVER,
					&SimpleFormatter::s_def );	
				GizmoManager::instance().addGizmo( s_pUniformScaleGizmo_ );
			}			
		}
	}

	virtual void EDCALL expel()
	{
		if (--s_count_ == 0)
		{
			GizmoManager::instance().removeGizmo( s_pUniformScaleGizmo_ );
			GizmoManager::instance().removeGizmo( s_pGizmo_ );
			s_pGizmo_ = NULL;
			s_pUniformScaleGizmo_ = NULL;
		}
	}

	virtual void EDCALL select()
	{		
		GizmoSetPtr set = new GizmoSet();
		if (s_pGizmo_ != NULL)
			set->add( s_pGizmo_ );
		if (s_pUniformScaleGizmo_ != NULL)
			set->add( s_pUniformScaleGizmo_ );
		if (set->vector().size() != 0)
			GizmoManager::instance().forceGizmoSet( set );
	}

	static GeneralProperty::View * create( GenScaleProperty & prop )
	{
		return new GenScalePropAxes( prop );
	}

private:
	GenScaleProperty	& prop_;
	static GizmoPtr		s_pGizmo_;
	static GizmoPtr		s_pUniformScaleGizmo_;
	static int			s_count_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			GenScaleProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

GenScalePropAxes::ViewEnroller GenScalePropAxes::s_viewEnroller;
int GenScalePropAxes::s_count_ = 0;
GizmoPtr GenScalePropAxes::s_pGizmo_ = NULL;
GizmoPtr GenScalePropAxes::s_pUniformScaleGizmo_ = NULL;



/**
 *	This is a view class that puts a sphere gizmo in the world
 *	whenever a radius property is elected
 */
class GenRadiusPropDisc : public GeneralProperty::View
{
public:
	GenRadiusPropDisc( GenRadiusProperty & prop,
		uint32 editableColour,
		float editableRadius ) :
	prop_( prop )
	{
		pGizmo_ = new RadiusGizmo(
			prop.pFloat(),
			prop.pCenter(),
			prop.name(),
			editableColour,
			editableRadius,
			MODIFIER_ALT );
	}

	~GenRadiusPropDisc()
	{
		pGizmo_ = NULL;
	}

	virtual void EDCALL elect()
	{
		GizmoManager::instance().addGizmo( pGizmo_ );
	}

	virtual void EDCALL expel()
	{
		GizmoManager::instance().removeGizmo( pGizmo_ );
	}

	virtual void EDCALL select()
	{
		if (pGizmo_ != NULL)
		{
			GizmoSetPtr set = new GizmoSet();
			set->add( pGizmo_ );
			GizmoManager::instance().forceGizmoSet( set );
		}
	}

	static GeneralProperty::View * create( GenRadiusProperty & prop )
	{
		return new GenRadiusPropDisc( prop, prop.widgetColour(), prop.widgetRadius() );
	}

private:
	GenRadiusProperty	& prop_;
	GizmoPtr			pGizmo_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			GenRadiusProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

GenRadiusPropDisc::ViewEnroller GenRadiusPropDisc::s_viewEnroller;


/**
 *	This is a view class that puts a cone gizmo in the world
 *	whenever an angle property is elected
 */
class GenAnglePropCone : public GeneralProperty::View
{
public:
	GenAnglePropCone( AngleProperty & prop,
		uint32 editableColour,
		float editableRadius ) :
	prop_( prop )
	{
		pGizmo_ = new AngleGizmo(
			prop.pCenter(),
			prop.pFloat(),
			MODIFIER_ALT | MODIFIER_SHIFT );
	}

	~GenAnglePropCone()
	{
		pGizmo_ = NULL;
	}

	virtual void EDCALL elect()
	{
		GizmoManager::instance().addGizmo( pGizmo_ );
	}

	virtual void EDCALL expel()
	{
		GizmoManager::instance().removeGizmo( pGizmo_ );
	}

	virtual void EDCALL select()
	{
		if (pGizmo_ != NULL)
		{
			GizmoSetPtr set = new GizmoSet();
			set->add( pGizmo_ );
			GizmoManager::instance().forceGizmoSet( set );
		}
	}

	static GeneralProperty::View * create( AngleProperty & prop )
	{
		return new GenAnglePropCone( prop, 0xffffff00, 2.f );
	}

private:
	AngleProperty&		prop_;
	GizmoPtr			pGizmo_;

	static struct ViewEnroller
	{
		ViewEnroller()
		{
			AngleProperty_registerViewFactory(
				GizmoViewKind::kindID(), &create );
		}
	}	s_viewEnroller;
};

GenAnglePropCone::ViewEnroller GenAnglePropCone::s_viewEnroller;

// genprop_gizmoviews.cpp
