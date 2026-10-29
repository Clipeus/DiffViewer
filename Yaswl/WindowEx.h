#pragma once

#include "Window.h"

namespace Yaswl
{
  template <class T> class WindowSubclassEx;

  class WindowEx : public Window
  {
    friend class WindowSubclassEx<WindowEx>;

  public:
    explicit WindowEx(const wchar_t* pClassName = nullptr) : m_pClassName(pClassName)
    {
    }

  public:
    bool Create(DWORD dwExStyle, const std::wstring& strClassName, const std::wstring& strWindowName = L"", DWORD dwStyle = WS_OVERLAPPEDWINDOW,
      int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT, HWND hWndParent = nullptr, HMENU hMenu = nullptr)
    {
      return Window::Create(dwExStyle, strClassName.c_str(), strWindowName.c_str(), dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, GetApp()->GetInstance(), reinterpret_cast<LPVOID>(this));
    }

    const wchar_t* GetClassName()
    {
      return m_pClassName;
    }

  protected:
    virtual void OnRegisterClass(WNDCLASSEX& wc)
    {
    }

    bool RegisterClass(const WNDCLASSEXW* pWndClass = nullptr)
    {
      if (pWndClass)
      {
        return ::RegisterClassEx(pWndClass);
      }
      else
      {
        _ASSERTE(GetClassName());

        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.hInstance = GetApp()->GetInstance();
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpfnWndProc = WindowEx::s_WndProc;
        wc.lpszClassName = GetClassName();

        OnRegisterClass(wc);
        return ::RegisterClassEx(&wc);
      }
    }

  protected:
    virtual LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    static LRESULT WINAPI s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      WindowEx* pWnd = nullptr;
      if (WM_NCCREATE == uMsg)
      {
        LPCREATESTRUCTA lpCS = reinterpret_cast<LPCREATESTRUCTA>(lParam);
        pWnd = reinterpret_cast<WindowEx*>(lpCS->lpCreateParams);
        _ASSERTE(pWnd);
        if (pWnd)
          pWnd->m_hWnd = hWnd;
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));
      }
      else
      {
        pWnd = reinterpret_cast<WindowEx*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
      }

      if (pWnd)
        return pWnd->WndProc(hWnd, uMsg, wParam, lParam);

      return -1;
    }

  protected:
    const wchar_t* m_pClassName = nullptr;
  };
}

