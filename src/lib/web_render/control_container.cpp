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
#include "control_container.hpp"
#include <map>
#include <tchar.h>

std::map<HWND, ControlContainer*>* s_windowMap = NULL;
ControlContainer* s_containerCreating = NULL;

namespace
{

const TCHAR WINDOW_CLASS_NAME[] = _T( "{DC3B23EA-CDEC-49e7-B4F1-6AB2A737CB18}" );

struct Initialiser
{
	Initialiser()
	{
		OleInitialize( NULL );

		WNDCLASS wndClass = { sizeof( WNDCLASS ) };

		wndClass.hInstance = GetModuleHandle( NULL );;
		wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wndClass.lpfnWndProc = ControlContainer::windowProc;
		wndClass.lpszClassName = WINDOW_CLASS_NAME;

		RegisterClass( &wndClass );
	}
	~Initialiser()
	{
		UnregisterClass( WINDOW_CLASS_NAME, GetModuleHandle( NULL ) );

		OleUninitialize();
	}
}
initialiser;

}

ControlContainer::ControlContainer()
	: hwnd_( NULL ),
	embededUnknown_( NULL ),
	embededOleObject_( NULL ),
	embededOleInPlaceObject_( NULL )
{
}

ControlContainer::~ControlContainer()
{
	if (hwnd_)
	{
		DestroyWindow( hwnd_ );
		( *s_windowMap ).erase( ( *s_windowMap ).find( hwnd_ ) );
	}
}

BOOL ControlContainer::create( int width, int height, REFCLSID clsid )
{
	s_containerCreating = this;

	hwnd_ = CreateWindowEx( WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WINDOW_CLASS_NAME, NULL, WS_POPUP | WS_DISABLED,
		0, 0, width, height, NULL, NULL, GetModuleHandle( NULL ), (LPVOID)&clsid );

//	hwnd_ = CreateWindowEx( WS_EX_TOOLWINDOW, WINDOW_CLASS_NAME, NULL, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
//		100, 100, width, height, NULL, NULL, GetModuleHandle( NULL ), (LPVOID)&clsid );

	if (hwnd_)
	{
		if (!s_windowMap)
		{
			s_windowMap = new std::map<HWND, ControlContainer*>;
		}
		( *s_windowMap )[ hwnd_ ] = this;
	}

	s_containerCreating = NULL;

	return !!hwnd_;
}

LRESULT ControlContainer::windowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	ControlContainer* This = NULL;
	if (s_containerCreating)
	{
		This = s_containerCreating;
		This->hwnd_ = hwnd;
	}
	else
	{
		This = ( *s_windowMap )[ hwnd ];
	}
	return This->windowProc( uMsg, wParam, lParam );
}

LRESULT ControlContainer::windowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch (uMsg)
	{
	case WM_CREATE:
		if (SUCCEEDED( CoCreateInstance( *( CLSID* )( (CREATESTRUCT*)lParam )->lpCreateParams,
			NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, IID_IUnknown, (LPVOID*)&embededUnknown_ )))
		{
			embededUnknown_->QueryInterface( IID_IOleObject, (LPVOID*)&embededOleObject_ );
			embededUnknown_->QueryInterface( IID_IOleInPlaceObject, (LPVOID*)&embededOleInPlaceObject_ );

			if (embededOleObject_)
			{
				CREATESTRUCT* cs = (CREATESTRUCT*)lParam;

				embededOleObject_->SetClientSite( this );
				OleSetContainedObject( embededOleObject_, TRUE );

				RECT rect = { 0, 0, cs->cx, cs->cy };
				embededOleObject_->DoVerb( OLEIVERB_SHOW, NULL, this, -1, hwnd_, &rect );
			}
		}

		if (!embededUnknown_ || !embededOleObject_ || !embededOleInPlaceObject_ )
		{
			if (embededUnknown_)
			{
				embededUnknown_->Release();
				embededUnknown_ = NULL;
			}
			if (embededOleObject_)
			{
				embededOleObject_->Release();
				embededOleObject_ = NULL;
			}
			if (embededOleInPlaceObject_)
			{
				embededOleInPlaceObject_->Release();
				embededOleInPlaceObject_ = NULL;
			}
			return -1;
		}
		break;
	case WM_SIZE:
		if (embededOleInPlaceObject_)
		{
			RECT rect = { 0, 0, LOWORD( lParam ), HIWORD( lParam ) };
			embededOleInPlaceObject_->SetObjectRects( &rect, &rect );
		}
		break;
	case WM_DESTROY:
		if (embededUnknown_)
		{
			embededUnknown_->Release();
			embededOleObject_->Close( OLECLOSE_NOSAVE );
			embededOleObject_->Release();
			embededOleInPlaceObject_->Release();
		}
		break;
	}
	return DefWindowProc( hwnd_, uMsg, wParam, lParam );
}

HRESULT ControlContainer::QueryInterface( REFIID iid, LPVOID* ppvObject )
{
	*ppvObject = NULL;
	if (iid == IID_IOleClientSite)
	{
		*ppvObject = (IOleClientSite*)this;
	}
	else if(iid == IID_IOleInPlaceFrame)
	{
		*ppvObject = (IOleInPlaceFrame*)this;
	}
	else if(iid == IID_IOleInPlaceSite)
	{
		*ppvObject = (IOleInPlaceSite*)this;
	}
	else if(iid == IID_IOleWindow)
	{
		*ppvObject = (IOleWindow*)(IOleInPlaceFrame*)this;
	}
	else if(iid == IID_IOleInPlaceUIWindow)
	{
		*ppvObject = (IOleInPlaceUIWindow*)this;
	}
	else if(iid == IID_IUnknown)
	{
		*ppvObject = (IUnknown*)(IOleClientSite*)this;
	}
	return *ppvObject ? S_OK : E_NOINTERFACE;
}

ULONG ControlContainer::AddRef()
{
	return 1;
}

ULONG ControlContainer::Release()
{
	return 1;
}

HRESULT ControlContainer::SaveObject()
{
	return S_OK;
}

HRESULT ControlContainer::GetMoniker( DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk )
{
	return E_NOTIMPL;
}

HRESULT ControlContainer::GetContainer( LPOLECONTAINER FAR* ppContainer )
{
	*ppContainer = NULL;
	return E_NOINTERFACE;
}

HRESULT ControlContainer::ShowObject()
{
	return S_OK;
}

HRESULT ControlContainer::OnShowWindow( BOOL fShow )
{
	return S_OK;
}

HRESULT ControlContainer::RequestNewObjectLayout()
{
	return E_NOTIMPL;
}

HRESULT ControlContainer::GetWindow( HWND* phwnd )
{
	*phwnd = hwnd_;
	return S_OK;
}

HRESULT ControlContainer::ContextSensitiveHelp( BOOL fEnterMode )
{
	return S_OK;
}

HRESULT ControlContainer::GetBorder( LPRECT lprectBorder )
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT ControlContainer::RequestBorderSpace( LPCBORDERWIDTHS pborderwidths )
{
	return INPLACE_E_NOTOOLSPACE;
}

HRESULT ControlContainer::SetBorderSpace( LPCBORDERWIDTHS pborderwidths )
{
	return S_OK;
}

HRESULT ControlContainer::SetActiveObject( IOleInPlaceActiveObject *pActiveObject, LPCOLESTR pszObjName )
{
	return S_OK;
}

HRESULT ControlContainer::InsertMenus( HMENU hmenuShared, LPOLEMENUGROUPWIDTHS lpMenuWidths )
{
	return S_OK;
}

HRESULT ControlContainer::SetMenu( HMENU hmenuShared, HOLEMENU holemenu, HWND hwndActiveObject )
{
	return S_OK;
}

HRESULT ControlContainer::RemoveMenus( HMENU hmenuShared )
{
	return S_OK;
}

HRESULT ControlContainer::SetStatusText( LPCOLESTR pszStatusText )
{
	return S_OK;
}

HRESULT ControlContainer::EnableModeless( BOOL fEnable )
{
	return S_OK;
}

HRESULT ControlContainer::TranslateAccelerator( LPMSG lpmsg, WORD wID )
{
	return S_FALSE;
}

HRESULT ControlContainer::CanInPlaceActivate()
{
	return S_OK;
}

HRESULT ControlContainer::OnInPlaceActivate()
{
	return S_OK;
}

HRESULT ControlContainer::OnUIActivate()
{
	return S_OK;
}

HRESULT ControlContainer::GetWindowContext( IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
	LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo )
{
	*ppFrame = this;
	*ppDoc = NULL;

	RECT rect;
	GetClientRect( hwnd_, &rect );

	*lprcPosRect = rect;
	*lprcClipRect = rect;

	lpFrameInfo->fMDIApp = FALSE;
	lpFrameInfo->hwndFrame = hwnd_;
	lpFrameInfo->haccel = 0;
	lpFrameInfo->cAccelEntries = 0;

	return S_OK;
}

HRESULT ControlContainer::Scroll( SIZE scrollExtent )
{
	return S_OK;
}

HRESULT ControlContainer::OnUIDeactivate( BOOL fUndoable )
{
	return S_OK;
}

HRESULT ControlContainer::OnInPlaceDeactivate()
{
	return S_OK;
}

HRESULT ControlContainer::DiscardUndoState()
{
	return S_OK;
}

HRESULT ControlContainer::DeactivateAndUndo()
{
	return S_OK;
}

HRESULT ControlContainer::OnPosRectChange( LPCRECT lprcPosRect )
{
	return embededOleInPlaceObject_->SetObjectRects( lprcPosRect, lprcPosRect );
}
