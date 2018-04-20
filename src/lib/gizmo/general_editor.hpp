/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef GENERAL_EDITOR_HPP
#define GENERAL_EDITOR_HPP


#include "cstdmf/smartpointer.hpp"

#include "pyscript/pyobject_plus.hpp"
#include "pyscript/script.hpp"

class GeneralEditor;
typedef SmartPointer<GeneralEditor> GeneralEditorPtr;

class GeneralProperty;


/**
 *	This class controls and defines the editing operations which
 *	can be performed on a general object.
 */
class GeneralEditor : public PyObjectPlus
{
	Py_Header( GeneralEditor, PyObjectPlus )
public:
	GeneralEditor( PyTypePlus * pType = &s_type_ );
	virtual ~GeneralEditor();

	virtual void addProperty( GeneralProperty * pProp );
	// desired views:
	//  gizmos (can be left to item itself 'tho... maybe all should be?)
	//  text (I guess)
	//  Borland controls
	//  python (combine with text...?)

	void elect();
	void expel();

	typedef std::vector< GeneralEditorPtr > Editors;

	static const Editors & currentEditors( void );
	static void currentEditors( const Editors & editors );

	PyObject *		pyGetAttribute( const char * attr );
	int				pySetAttribute( const char * attr, PyObject * value );

	PyObject *		pyAdditionalMembers( PyObject * pBaseSeq );

	PY_MODULE_STATIC_METHOD_DECLARE( py_getCurrentEditors )
	PY_MODULE_STATIC_METHOD_DECLARE( py_setCurrentEditors )

protected:
	typedef std::vector< GeneralProperty * > PropList;
	PropList		properties_;

	bool			constructorOver_;

	// this keeps track of the last item edited
	std::string lastItemName_;

private:
	GeneralEditor( const GeneralEditor& );
	GeneralEditor& operator=( const GeneralEditor& );

	static Editors	s_currentEditors_;
};

// move this down.... theres already a propertymanager...
namespace PropManager
{
	typedef void (*PropFini)( void );

	void registerFini(PropFini fn);
	void fini();
};

/**
 *	This macro declares the view factory stuff for a property class
 */
#define GENPROPERTY_VIEW_FACTORY_DECLARE( PROPCLASS )						\
	public:																	\
		typedef View * (*ViewFactory)( PROPCLASS & prop );					\
		static void registerViewFactory( int vkid, ViewFactory fn );		\
		static void fini();													\
																			\
	private:																\
		static std::vector<ViewFactory>	* viewFactories_;					\
};																			\
extern "C" WORLDEDITORDLL_API void PROPCLASS##_registerViewFactory(			\
				int vkid, PROPCLASS::ViewFactory fn );						\
namespace DodgyNamespace {													\


/**
 *	This macro implements the view factory stuff for a property class
 */
#define GENPROPERTY_VIEW_FACTORY( PROPCLASS )								\
	std::vector<PROPCLASS::ViewFactory> * PROPCLASS::viewFactories_ = NULL;	\
																			\
	void PROPCLASS::registerViewFactory( int vkid, ViewFactory fn )			\
	{																		\
		if (viewFactories_ == NULL)											\
			viewFactories_ = new std::vector<ViewFactory>;					\
		while (int(viewFactories_->size()) <= vkid)							\
			viewFactories_->push_back( NULL );								\
		(*viewFactories_)[ vkid ] = fn;										\
		PropManager::registerFini( &fini );									\
	}																		\
	void PROPCLASS::fini( )													\
	{																		\
		delete viewFactories_;												\
		viewFactories_ = NULL;												\
	}																		\
																			\
	void PROPCLASS##_registerViewFactory(									\
							int vkid, PROPCLASS::ViewFactory fn )			\
	{																		\
		PROPCLASS::registerViewFactory( vkid, fn );							\
	}																		\


class PropertyManager : public ReferenceCount
{
public:
	virtual bool canRemoveItem() { return false; }
	virtual void removeItem() = 0;

	virtual bool canAddItem() { return false; }
	virtual void addItem() = 0;
};
typedef SmartPointer<PropertyManager> PropertyManagerPtr;



//TODO: just revert back and add the vectors to a global list to be destructed at some stage....

class GeneralProperty
{
public:
	GeneralProperty( const std::string & name );

    void WBEditable( bool editable );
    bool WBEditable() const;

    void descName( const std::string& descName );
	const std::string& descName();
	
	void UIName( const std::string& name );
    const std::string& UIName();

	void UIDesc( const std::string& name );
    const std::string& UIDesc();

	void exposedToScriptName( const std::string& exposedToScriptName );
	const std::string& exposedToScriptName();

	void canExposeToScript( bool canExposeToScript );
	bool canExposeToScript() const;

	virtual void deleteSelf() { delete this; }

	virtual void elect();
	virtual void expel();
	virtual void select();

	virtual PyObject * EDCALL pyGet();
	virtual int EDCALL pySet( PyObject * value, bool transient = false );

	const char * EDCALL name() const		{ return name_; }

	void setGroup( const std::string & groupName ) { group_ = groupName; }
	std::string getGroup() { return group_; }

	class View
	{
	public:
		// Make the object responsible for deleting itself so that it is deleted
		// from the correct heap.
		virtual void EDCALL deleteSelf()	{ delete this; }

		virtual void EDCALL elect() = 0;
		virtual void EDCALL expel() = 0;

		virtual void EDCALL select() = 0;
	protected:
		// Always use deleteSelf. This is because the object may not have been
		// created in the DLL.
		virtual ~View() {}
	};

	static int nextViewKindID();

	void setPropertyManager( PropertyManagerPtr management ) { propManager_ = management; }
	PropertyManagerPtr getPropertyManager() const { return propManager_; }

protected:
	virtual ~GeneralProperty();
	static int nextViewKindID_;

	class Views
	{
	public:
		Views();
		~Views();

		void set( int i, View * v );
		View * operator[]( int i );

	private:
		View **		e_;

		Views( const Views & oth );
		Views & operator=( const Views & oth );
	};

	Views				views_;
	char *				name_;
	std::string			group_;

private:
	GeneralProperty( const GeneralProperty & oth );
	GeneralProperty & operator=( const GeneralProperty & oth );

	friend class Views;

	PropertyManagerPtr propManager_;
	uint32 flags_;

    bool            WBEditable_;
    std::string     descName_;
	std::string     UIName_;
    std::string     UIDesc_;
	std::string     exposedToScriptName_;
	bool			canExposeToScript_;

	GENPROPERTY_VIEW_FACTORY_DECLARE( GeneralProperty )
};


// Exported C style function for Borland to call.
extern "C" WORLDEDITORDLL_API int GeneralProperty_nextViewKindID();


/**
 *	This macro sets up the views for a general property type.
 *
 *	It should be used in the constructor of all classes that derive from
 *	GeneralProperty.
 *
 *	Note that if a view kind has implementations for both derived and base
 *	classes of a property (which would not be not unusual), then the base
 *	class view will get created for a short time before it is deleted and
 *	replaced by the derived class view. If this turns out to be a problem
 *	it could be avoided, but I will leave it for now.
 */
#define GENPROPERTY_MAKE_VIEWS()												\
	if (viewFactories_ != NULL)												\
	{																		\
		MF_ASSERT( int(viewFactories_->size()) <= nextViewKindID_ );				\
																			\
		for (uint i = 0; i < viewFactories_->size(); i++)					\
		{																	\
			ViewFactory vf = (*viewFactories_)[i];							\
			if (vf != NULL)													\
			{																\
				View * v = (*vf)( *this );									\
				if (v != NULL)												\
				{															\
					views_.set( i, v );										\
				}															\
			}																\
		}																	\
	}																		\




/**
 *	This is the base class for all read only properties
 */
class GeneralROProperty : public GeneralProperty
{
public:
	GeneralROProperty( const std::string & name );

	virtual int EDCALL pySet( PyObject * value, bool transient = false );

private:

	GENPROPERTY_VIEW_FACTORY_DECLARE( GeneralROProperty )
};



#endif // GENERAL_EDITOR_HPP
