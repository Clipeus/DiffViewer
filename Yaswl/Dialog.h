#pragma once

#include "Window.h"

namespace Yaswl
{
  class Dialog : public Window
  {
  public:
    Dialog(const wchar_t* lpTemplateName = nullptr) : m_lpTemplateName(lpTemplateName)
    {
    }
    virtual ~Dialog() = default;

  public:
    virtual INT_PTR DoModal(HWND hWnd, const wchar_t* lpTemplateName = nullptr)
    {
      return ::DialogBoxParam(GetApp()->GetInstance(),
        lpTemplateName != nullptr ? lpTemplateName : m_lpTemplateName, hWnd, s_DlgProc, reinterpret_cast<LPARAM>(this));
    }

  protected:
    virtual bool DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      switch (uMsg)
      {
        HANDLE_DLGMSG(hWnd, WM_DESTROY, OnDestroy);
        HANDLE_DLGMSG(hWnd, WM_INITDIALOG, OnInitDialog);
        HANDLE_DLGMSG(hWnd, WM_COMMAND, OnCommand);
      }
      return false;
    }

    static INT_PTR CALLBACK s_DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      Dialog* dlg = reinterpret_cast<Dialog*>(::GetWindowLongPtr(hWnd, DWLP_USER));

      if (uMsg == WM_INITDIALOG)
      {
        dlg = reinterpret_cast<Dialog*>(lParam);
        _ASSERTE(dlg);
        if (dlg)
          dlg->m_hWnd = hWnd;
        ::SetWindowLongPtr(hWnd, DWLP_USER, reinterpret_cast<LONG_PTR>(dlg));
      }

      if (dlg)
        return dlg->DlgProc(hWnd, uMsg, wParam, lParam);

      return false;
    }

    bool OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
    {
      return true;
    }

    void OnDestroy(HWND hWnd)
    {
      ::DestroyIcon((HICON)::SendMessage(hWnd, WM_GETICON, ICON_BIG, 0));
      ::DestroyIcon((HICON)::SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0));
      ::DestroyIcon((HICON)::SendMessage(hWnd, WM_GETICON, ICON_SMALL2, 0));
    }

    void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
    {
      switch (id)
      {
        case IDOK:
        case IDCANCEL:
        {
          ::EndDialog(hWnd, id);
          return;
        }
      }
    }

  protected:
    const wchar_t* m_lpTemplateName;
  };
}