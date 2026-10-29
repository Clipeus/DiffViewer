#pragma once

#include "WinApp.h"

namespace Yaswl
{
  class Window
  {
  public:
    Window() = default;
    virtual ~Window() = default;

  public:
    bool Create(DWORD dwExStyle, const std::wstring& strClassName, const std::wstring& strWindowName = L"", DWORD dwStyle = WS_OVERLAPPEDWINDOW,
      int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT, HWND hWndParent = nullptr, HMENU hMenu = nullptr, HINSTANCE hInstance = nullptr, LPVOID lpParam = nullptr)
    {
      m_hWnd = ::CreateWindowEx(dwExStyle, strClassName.c_str(), strWindowName.c_str(), dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
      _ASSERTE(m_hWnd);
      return m_hWnd != nullptr;
    }

    HWND Attach(HWND hWnd)
    {
      HWND hOld = m_hWnd;
      m_hWnd = hWnd;
      return hOld;
    }

    HWND GetHandle() const
    {
      return m_hWnd;
    }

    operator HWND() const
    {
      return GetHandle();
    }

  protected:
    HWND m_hWnd = nullptr;
  };
}

