/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#ifndef CONTROL_CONTAINER_HPP__
#define CONTROL_CONTAINER_HPP__

#include <oleidl.h>

class ControlContainer
	: public IOleClientSite, public IOleInPlaceFrame, public IOleInPlaceSite
{
public:
	ControlContainer();
	virtual ~ControlContainer();
	BOOL create( int width, int height, REFCLSID clsid );
	static LRESULT CALLBACK windowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	IUnknown* embededUnknown()	{	return embededUnknown_;	}
	IOleObject* embededOleObject()	{	return embededOleObject_;	}
	IOleInPlaceObject* embededOleInPlaceObject()	{	return embededOleInPlaceObject_;	}

private:
	LRESULT windowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );

	// IUnknown methods
	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, LPVOID* ppvObject );
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	// IOleClientSite methods
	HRESULT STDMETHODCALLTYPE SaveObject();
	HRESULT STDMETHODCALLTYPE GetMoniker( DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk );
	HRESULT STDMETHODCALLTYPE GetContainer( LPOLECONTAINER FAR* ppContainer );
	HRESULT STDMETHODCALLTYPE ShowObject();
	HRESULT STDMETHODCALLTYPE OnShowWindow( BOOL fShow );
	HRESULT STDMETHODCALLTYPE RequestNewObjectLayout();

	// IOleWindow methods ==> used by IOleInPlaceFrame and IOleInPlaceSite
	HRESULT STDMETHODCALLTYPE GetWindow( HWND * phwnd );
	HRESULT STDMETHODCALLTYPE ContextSensitiveHelp( BOOL fEnterMode );

	// IOleInPlaceUIWindow methods
	HRESULT STDMETHODCALLTYPE GetBorder( LPRECT lprectBorder );
	HRESULT STDMETHODCALLTYPE RequestBorderSpace( LPCBORDERWIDTHS pborderwidths );
	HRESULT STDMETHODCALLTYPE SetBorderSpace( LPCBORDERWIDTHS pborderwidths );
	HRESULT STDMETHODCALLTYPE SetActiveObject( IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName );

	// IOleInPlaceFrame methods
	HRESULT STDMETHODCALLTYPE InsertMenus( HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths );
	HRESULT STDMETHODCALLTYPE SetMenu( HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject );
	HRESULT STDMETHODCALLTYPE RemoveMenus( HMENU hmenuShared );
	HRESULT STDMETHODCALLTYPE SetStatusText( LPCOLESTR pszStatusText );
	HRESULT STDMETHODCALLTYPE EnableModeless( BOOL fEnable );
	HRESULT STDMETHODCALLTYPE TranslateAccelerator( LPMSG lpmsg, WORD wID );

	// IOleInPlaceSite
	HRESULT STDMETHODCALLTYPE CanInPlaceActivate();
	HRESULT STDMETHODCALLTYPE OnInPlaceActivate();
	HRESULT STDMETHODCALLTYPE OnUIActivate();
	HRESULT STDMETHODCALLTYPE GetWindowContext( IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
		LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo );
	HRESULT STDMETHODCALLTYPE Scroll( SIZE scrollExtent );
	HRESULT STDMETHODCALLTYPE OnUIDeactivate( BOOL fUndoable );
	HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate();
	HRESULT STDMETHODCALLTYPE DiscardUndoState();
	HRESULT STDMETHODCALLTYPE DeactivateAndUndo();
	HRESULT STDMETHODCALLTYPE OnPosRectChange( LPCRECT lprcPosRect );

	HWND hwnd_;
	IUnknown* embededUnknown_;
	IOleObject* embededOleObject_;
	IOleInPlaceObject* embededOleInPlaceObject_;
};

#endif//CONTROL_CONTAINER_HPP__
