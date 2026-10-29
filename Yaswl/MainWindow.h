#pragma once

#include "WinApp.h"
#include "WindowEx.h"
#include "Registry.h"

namespace Yaswl
{
  class WinApp;

  class MainWindow : public WindowEx
  {
  public:
    explicit MainWindow(const wchar_t* pClassName = L"Yaswl_MainWindow_1.0") : WindowEx(pClassName)
    {
    }

  public:
    virtual bool Init()
    {
      if (!RegisterClass())
        return false;

      return true;
    }

    virtual bool Create()
    {
      if (!WindowEx::Create(0, GetClassName(), GetApp()->GetAppName()))
        return false;

      _ASSERTE(m_hWnd);

      ::UpdateWindow(m_hWnd);

      return true;
    }

    void RestoreWindowPlacement(int nCmdShow, const wchar_t* lpName = L"WindowPos")
    {
      if ((nCmdShow == SW_SHOWNORMAL) || (nCmdShow == SW_SHOWDEFAULT))
      {
        std::vector<unsigned char> buf(sizeof(WINDOWPLACEMENT));
        if (Yaswl::RegistryW::Value(GetApp()->GetRegistryRoot(), lpName, buf))
          ::SetWindowPlacement(m_hWnd, reinterpret_cast<LPWINDOWPLACEMENT>(buf.data()));
        else
          ::ShowWindow(m_hWnd, nCmdShow);
      }
      else
      {
        ::ShowWindow(m_hWnd, nCmdShow);
      }
    }

    void SaveWindowPlacement(const wchar_t* lpName = L"WindowPos")
    {
      std::vector<unsigned char> buf(sizeof(WINDOWPLACEMENT), 0);
      LPWINDOWPLACEMENT pWinPos = reinterpret_cast<LPWINDOWPLACEMENT>(buf.data());
      pWinPos->length = sizeof(WINDOWPLACEMENT);
      if (::GetWindowPlacement(m_hWnd, pWinPos))
        Yaswl::RegistryW::SetValue(GetApp()->GetRegistryRoot(), lpName, buf);
    }

  public:
    virtual LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      switch (uMsg)
      {
      case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
      default:
        return WindowEx::WndProc(hWnd, uMsg, wParam, lParam);
      }
    }
  };
}

