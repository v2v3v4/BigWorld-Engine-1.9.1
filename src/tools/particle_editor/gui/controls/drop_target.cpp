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
#include "gui/controls/drop_target.hpp"

using namespace std;

DropTarget::DropTarget()
:
m_dropTarget(NULL)
{
}

void DropTarget::Register(IDropTargetObj *target, CWnd *window)
{
    m_dropTarget = target;
    COleDropTarget::Register(window);
}

/*virtual*/ 
DROPEFFECT
DropTarget::OnDragEnter
(
    CWnd                *window,
    COleDataObject      *dataObject,
    DWORD               keyState,
    CPoint              point
)
{
    if (m_dropTarget != NULL)
        return m_dropTarget->OnDragEnter(window, dataObject, keyState, point);
    else
        return DROPEFFECT_NONE;
}

/*virtual*/ void DropTarget::OnDragLeave(CWnd *window)
{
    if (m_dropTarget != NULL)
        m_dropTarget->OnDragLeave(window);
}

/*virtual*/ 
DROPEFFECT
DropTarget::OnDragOver
(
    CWnd                *window,
    COleDataObject      *dataObject,
    DWORD               keyState,
    CPoint              point
)
{
    if (m_dropTarget != NULL)
        return m_dropTarget->OnDragOver(window, dataObject, keyState, point);
    else
        return DROPEFFECT_NONE;
}

/*virtual*/ 
DROPEFFECT 
DropTarget::OnDragScroll
(
    CWnd                *window, 
    DWORD               keyState, 
    CPoint              point
)
{
    if (m_dropTarget != NULL)
        return m_dropTarget->OnDragScroll(window, keyState, point);
    else
        return DROPEFFECT_NONE;
}

/*virtual*/
BOOL
DropTarget::OnDrop
(
    CWnd                *window,
    COleDataObject      *dataObject,
    DROPEFFECT          dropEffect,
    CPoint              point
)
{
    if (m_dropTarget != NULL)
        return m_dropTarget->OnDrop(window, dataObject, dropEffect, point);
    else
        return FALSE;
}

/*static*/ string DropTarget::GetText(COleDataObject *dataObject)
{
    if (dataObject == NULL)
        return string();

    HGLOBAL hglobal = dataObject->GetGlobalData(CF_TEXT);
    if (hglobal == NULL)
        return string();

    char *text = (char *)::GlobalLock(hglobal);
    string result = text;
    ::GlobalUnlock(hglobal);
    return result;

}