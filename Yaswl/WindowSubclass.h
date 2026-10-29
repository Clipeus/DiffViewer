#pragma once

#include "Window.h"

namespace Yaswl
{
  class WindowSubclass : public Window
  {
  public:
    ~WindowSubclass()
    {
      Subclass(false);
    }

    bool Subclass(bool bSet)
    {
      bool result = false;
      if (bSet && !m_uIdSubclass)
      {
        m_lpWndProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(m_hWnd, GWLP_WNDPROC));
        result = ::SetWindowSubclass(m_hWnd, &WindowSubclass::s_SubclassProc, m_uIdSubclass, (DWORD_PTR)this);
        m_uIdSubclass = 1;
      }
      else if (m_uIdSubclass)
      {
        result = ::RemoveWindowSubclass(m_hWnd, &WindowSubclass::s_SubclassProc, m_uIdSubclass);
        m_lpWndProc = nullptr;
        m_uIdSubclass = 0;
      }

      return result;
    }

  protected:
    virtual LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      if (m_lpWndProc)
        return ::CallWindowProc(m_lpWndProc, hWnd, uMsg, wParam, lParam);
      else
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

    static LRESULT CALLBACK s_SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
      if (dwRefData)
      {
        WindowSubclass* pWnd = pWnd = reinterpret_cast<WindowSubclass*>(dwRefData);
        return pWnd->WndProc(hWnd, uMsg, wParam, lParam);
      }
      return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

  protected:
    int m_uIdSubclass = 0;
    WNDPROC m_lpWndProc = nullptr;
  };

  template <>
  class WindowSubclassEx<WindowEx> : public WindowSubclass
  {
  public:
    WindowSubclassEx(WindowEx* pWnd) : m_pDispWnd(pWnd)
    {
    }

  protected:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
      if (m_pDispWnd)
      {
        WindowEx* p = reinterpret_cast<WindowEx*>(m_pDispWnd);
        (p->WndProc)(hWnd, uMsg, wParam, lParam);
      }

      if (m_lpWndProc)
        return ::CallWindowProc(m_lpWndProc, hWnd, uMsg, wParam, lParam);
      else
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

  protected:
    WindowEx* m_pDispWnd = nullptr;
  };

  template <class T>
  class WindowSubclassEx : public WindowSubclass
  {
    typedef LRESULT(T::*SubWndProc)(HWND, UINT, WPARAM, LPARAM, bool&);

  public:
    WindowSubclassEx(T* pWnd, SubWndProc pSubWndProc = nullptr) : m_pDispWnd(pWnd), m_pSubWndProc(pSubWndProc)
    {
    }

  protected:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
      if (m_pDispWnd)
      {
        if (m_pSubWndProc)
        {
          bool bHandled = false;
          LRESULT lPesult = (m_pDispWnd->*m_pSubWndProc)(hWnd, uMsg, wParam, lParam, bHandled);
          if (bHandled)
            return lPesult;
        }
      }

      if (m_lpWndProc)
        return ::CallWindowProc(m_lpWndProc, hWnd, uMsg, wParam, lParam);
      else
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

  protected:
    T* m_pDispWnd = nullptr;
    SubWndProc m_pSubWndProc = nullptr;
  };
}

