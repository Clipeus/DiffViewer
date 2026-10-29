#pragma once

#include "Misc.h"
#include "Window.h"

namespace Yaswl
{
  class MainWindow;

  class WinApp
  {
  public:
    class WaitCursor
    {
    public:
      WaitCursor()
      {
        m_hPrevCursor = ::SetCursor(GetApp()->GetWaitCursor());
        GetApp()->SetWaitCursor(true);
      }

      ~WaitCursor()
      {
        ::SetCursor(m_hPrevCursor ? m_hPrevCursor : GetApp()->GetStdCursor());
        GetApp()->SetWaitCursor(false);
      }

    protected:
      HCURSOR m_hPrevCursor = nullptr;
    };

  public:
    WinApp(const std::wstring& strAppName = L"")
    {
      _ASSERTE(s_pWinApp == nullptr);

      s_pWinApp = this;
      m_strAppName = strAppName;
    }

    virtual ~WinApp()
    {
      s_pWinApp = nullptr;
    }

  public:
    static WinApp* GetApp()
    {
      _ASSERTE(s_pWinApp);
      return s_pWinApp;
    }

  public:
    virtual bool Init(HINSTANCE hInstance, const std::wstring& strCmdLine)
    {
      m_hInstance = hInstance;

      SetGlobalHandlers();

      INITCOMMONCONTROLSEX iccs = { 0 };
      iccs.dwSize = sizeof(iccs);
      iccs.dwICC = ICC_WIN95_CLASSES;
      ::InitCommonControlsEx(&iccs);

      m_hWaitCursor = ::LoadCursor(nullptr, IDC_WAIT);
      m_hStdCursor = ::LoadCursor(nullptr, IDC_ARROW);

      return true;
    }

    virtual int Run(Window* pMainWnd)
    {
      _ASSERTE(pMainWnd);
      m_pMainWindow = pMainWnd;

      MSG msg;
      while (::GetMessage(&msg, nullptr, 0, 0))
      {
        if (PreTranslateMessage(&msg))
          continue;

        if (!m_hAccel || !::TranslateAccelerator(m_pMainWindow->GetHandle(), m_hAccel, &msg))
        {
          ::TranslateMessage(&msg);
          ::DispatchMessage(&msg);
        }
      }
      return static_cast<int>(msg.wParam);
    }

  protected:
    virtual bool PreTranslateMessage(MSG* pMsg)
    {
      return false;
    }

  public:
    HINSTANCE GetInstance()
    {
      return m_hInstance;
    }
    std::wstring GetAppName()
    {
      return m_strAppName;
    }
    void SetAppName(const std::wstring& value)
    {
      m_strAppName = value;
    }
    std::wstring GetRegistryRoot()
    {
      return m_strRegistryRoot;
    }
    void SetRegistryRoot(const std::wstring& value)
    {
      m_strRegistryRoot = value;
    }
    bool IsWaitCursor()
    {
      return m_bWaitCursor;
    }
    void SetWaitCursor(bool bWait)
    {
      m_bWaitCursor = bWait;
    }
    HCURSOR GetWaitCursor()
    {
      return m_hWaitCursor;
    }
    HCURSOR GetStdCursor()
    {
      return m_hStdCursor;
    }

    template <class T = Window>
    T* GetMainWnd()
    {
      return static_cast<T*>(m_pMainWindow);
    }

  public:
    std::wstring LoadString(UINT uID)
    {
      std::wstring result;
      LPWSTR lpszText = nullptr;

      int nLen = ::LoadString(GetInstance(), uID, reinterpret_cast<LPWSTR>(&lpszText), 0);

      if (lpszText && nLen > 0)
        result.assign(lpszText, nLen);

      return result;
    }

    int ReportBox(const std::wstring& strMessage, int nMode = MB_OK)
    {
      return ::MessageBox(m_pMainWindow ? m_pMainWindow->GetHandle() : nullptr, strMessage.c_str(), GetAppName().c_str(), nMode);
    }

    int ReportBox(UINT uMesID, int nMode = MB_OK)
    {
      return ReportBox(LoadString(uMesID).c_str(), nMode);
    }

    int ReportSystemError(UINT uMesID, DWORD dwError = 0, int nMode = MB_OK|MB_ICONERROR)
    {
      std::wstring strText = LoadString(uMesID);
      strText += GetSystemErrorMessage(dwError);
      return ReportBox(strText.c_str(), nMode);
    }

  protected:
    static LONG WINAPI _unexception_filter(EXCEPTION_POINTERS* pException)
    {
      ::FatalAppExit(0, GetSystemErrorMessage(ERROR_PROCESS_ABORTED).c_str());
      _RPTF1(_CRT_ERROR, "_unexception_filter -> Exception code : %X\n", pException->ExceptionRecord->ExceptionCode);
      return EXCEPTION_CONTINUE_SEARCH;
    }

    virtual void SetGlobalHandlers()
    {
      ::SetUnhandledExceptionFilter(GetApp()->_unexception_filter);	// For Win32 SEH
    }

  protected:
    inline static WinApp* s_pWinApp = nullptr;
    Window* m_pMainWindow = nullptr;
    std::wstring m_strAppName;
    std::wstring m_strRegistryRoot;
    HINSTANCE m_hInstance = nullptr;
    HACCEL m_hAccel = nullptr;
    bool m_bWaitCursor = false;
    HCURSOR m_hWaitCursor = nullptr;
    HCURSOR m_hStdCursor = nullptr;
  };

  template <class T = WinApp>
  WinApp* GetApp()
  {
    return static_cast<T*>(WinApp::GetApp());
  }
}
