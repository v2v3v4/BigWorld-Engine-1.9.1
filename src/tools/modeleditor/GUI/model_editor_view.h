// ModelEditorView.h : interface of the CModelEditorView class
//


#pragma once


class CModelEditorView : public CView
{
protected: // create from serialization only
	CModelEditorView();
	DECLARE_DYNCREATE(CModelEditorView)

	CRect lastRect_;

// Attributes
public:
	CModelEditorDoc* GetDocument() const;

	void OnSize(UINT nType, int cx, int cy);

	void OnPaint();

// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~CModelEditorView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};

#ifndef _DEBUG  // debug version in ModelEditorView.cpp
inline CModelEditorDoc* CModelEditorView::GetDocument() const
   { return reinterpret_cast<CModelEditorDoc*>(m_pDocument); }
#endif
