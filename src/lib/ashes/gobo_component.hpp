/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GOBO_COMPONENT_HPP
#define GOBO_COMPONENT_HPP

#pragma warning( disable:4786 )

#include "ashes/simple_gui_component.hpp"
#include "moo/effect_constant_value.hpp"

/*~ class GUI.GoboComponent
 *
 *	This GoboComponent accesses the render target used by blooming, and blends
 *	in that texture based on the alpha channel of the gobo component's texture.
 *	By turning off bloom + blur, the blooming render target becomes a blurred
 *	version of the scene, meaning that by using the gobo component, you can
 *	selectively display a blurred version of the scene.
 *
 *	This is for example perfect for binoculars and sniper scopes.
 *
 *	For example, in the following code:
 *
 *	@{
 *	comp = GUI.Gobo( "gui/maps/gobo_binoculars.dds" )
 *	comp.materialFX="SOLID"
 *	GUI.addRoot( comp )
 *	BigWorld.selectBloomPreset(1)
 *	@}
 *
 *	This example will display a binocular gobo, and where the alpha channel is
 *	relatively opaque in the binocular texture map, a blurred version of the scene
 *	is drawn. 
 */
/**
 *	This class is a GUI component that blends between the given texture and the
 *	render target used by blooming.
 */
class GoboComponent : public SimpleGUIComponent
{
	Py_Header( GoboComponent, SimpleGUIComponent )

public:
	GoboComponent( const std::string& textureName,
		PyTypePlus * pType = &s_type_ );
	virtual ~GoboComponent();

	//-------------------------------------------------
	//Simple GUI component methods
	//-------------------------------------------------
	virtual void		draw( bool overlay = true );
	void				freeze( void );
	void				unfreeze( void );

	//-------------------------------------------------
	//Python Interface
	//-------------------------------------------------
	PyObject *			pyGetAttribute( const char * attr );
	int					pySetAttribute( const char * attr, PyObject * value );	

	PY_AUTO_METHOD_DECLARE( RETVOID, freeze, END )
	PY_AUTO_METHOD_DECLARE( RETVOID, unfreeze, END )
	PY_FACTORY_DECLARE()

	/**
	 * Texture setter is an effect constant binding that also holds a reference
	 * to a texture.
	 */
	class TextureSetter : public Moo::EffectConstantValue
	{
	public:
		bool operator()(ID3DXEffect* pEffect, D3DXHANDLE constantHandle);
		void map(Moo::BaseTexturePtr pTexture );
		Moo::BaseTexturePtr map();
	private:
		Moo::BaseTexturePtr map_;
	};

protected:

	///setup the material.
	virtual bool	buildMaterial();
	void			setConstants();

private:
	GoboComponent(const GoboComponent&);
	GoboComponent& operator=(const GoboComponent&);

	Moo::EffectConstantValuePtr* pDiffuseMap_;
	Moo::EffectConstantValuePtr* pBlurMap_;
	Moo::EffectConstantValuePtr* pBackBuffer_;
	Moo::BaseTexturePtr	blurTexture_;

	SmartPointer<GoboComponent::TextureSetter> diffuseMapSetterPtr_;
	SmartPointer<GoboComponent::TextureSetter> blurMapSetterPtr_;
	SmartPointer<GoboComponent::TextureSetter> backBufferSetterPtr_;

	COMPONENT_FACTORY_DECLARE( GoboComponent("") )
};


#ifdef CODE_INLINE
#include "gobo_component.ipp"
#endif




#endif // GOBO_COMPONENT_HPP
