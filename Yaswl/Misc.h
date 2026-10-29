#pragma once

#ifndef UNICODE
#error UNICODE define required
#endif // !UNICODE

#include <windows.h>
#include <Windowsx.h>
#include <CommCtrl.h>
#include <string>
#include <memory>

namespace Yaswl
{
// The normal HANDLE_MSG macro in WINDOWSX.H does not work properly for dialog
// boxes because DlgProc's return a bool instead of an LRESULT (like
// WndProcs). This HANDLE_DLGMSG macro corrects the problem:
#define HANDLE_DLGMSG(hWnd, message, fn)                          \
		 case (message): return (SetDlgMsgResult(hWnd, uMsg,               \
				HANDLE_##message((hWnd), (wParam), (lParam), (fn))))

#define UNIQUE_VALUE(NAME, TYPE, DELETER) \
	using unique_##NAME = std::unique_ptr<std::remove_pointer<TYPE>::type, decltype(&DELETER)>; \
	inline unique_##NAME make_unique_##NAME(TYPE value = nullptr) \
	{ \
			return unique_##NAME(value, DELETER); \
	}

#define UNIQUE_PTR(NAME, PTR, DELETER) \
	using unique_##NAME = std::unique_ptr<PTR, decltype(&DELETER)>; \
	inline unique_##NAME make_unique_##NAME(PTR* ptr = nullptr) \
	{ \
			return unique_##NAME(ptr, DELETER); \
	}

inline void CloseHandleAndSaveLastError(HANDLE hObject)
{
  DWORD le = ::GetLastError();
  ::CloseHandle(hObject);
  ::SetLastError(le);
}

UNIQUE_VALUE(handle, HANDLE, CloseHandleAndSaveLastError)

inline void RegCloseKeyAndSaveLastError(HKEY hKey)
{
  DWORD le = ::GetLastError();
  ::RegCloseKey(hKey);
  ::SetLastError(le);
}

UNIQUE_VALUE(regkey, HKEY, RegCloseKeyAndSaveLastError)

inline void ScreenToClient(HWND hParentWnd, LPRECT lpRect)
{
  ::ScreenToClient(hParentWnd, (LPPOINT)lpRect);
  ::ScreenToClient(hParentWnd, ((LPPOINT)lpRect) + 1);
}

inline std::wstring a2w(const std::string& str)
{
  std::wstring wstr;
  int size = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size() * sizeof(std::string::traits_type)), nullptr, 0);
  if (size > 0)
  {
    wstr.resize(size);
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size() * sizeof(std::string::traits_type)), &wstr.front(), static_cast<int>(wstr.size()));
  }
  return wstr;
}

inline std::string w2a(const std::wstring& wstr)
{
  std::string str;
  int size = ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
  if (size > 0)
  {
    str.resize(size);
    ::WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str.front(), static_cast<int>(str.size()), nullptr, nullptr);
  }
  return str;
}

inline std::wstring StrFormat(LPCWSTR lpszFormat, ...)
{
  va_list args;
  va_start(args, lpszFormat);

  size_t size = _vscwprintf(lpszFormat, args);

  std::wstring str;
  str.resize(size);

  vswprintf_s(&str.front(), size + 1, lpszFormat, args);

  va_end(args);

  return str;
}

inline std::wstring GetSystemErrorMessage(DWORD dwError = 0)
{
  wchar_t* szErrorText = nullptr;

  if (!dwError)
    dwError = ::GetLastError();

  if (dwError)
    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&szErrorText), 0, nullptr);

  std::wstring result;

  if (szErrorText)
  {
    result = szErrorText;
    ::LocalFree(szErrorText);
  }

  return result;
}

inline bool WaitWithMessageLoop(HANDLE hEvent)
{
  DWORD dwRet;
  MSG msg;

  while (true)
  {
    dwRet = ::MsgWaitForMultipleObjects(1, &hEvent, FALSE, INFINITE, QS_ALLINPUT);

    if (dwRet == WAIT_OBJECT_0)
      return TRUE; // The event was signaled

    if (dwRet != WAIT_OBJECT_0 + 1)
      break; // Something else happened

    // There is one or more window message available. Dispatch them
    while (::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
    {
      if (::GetMessage(&msg, nullptr, 0, 0) > 0)
      {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
      }

      if (::WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0)
        return true; // Event is now signaled.
    }
  }
  return false;
}

inline bool IsWow64Process()
{
  typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
  static LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(::GetModuleHandleA("kernel32"), "IsWow64Process");

  BOOL bIsWow64 = false;

  if (nullptr != fnIsWow64Process)
  {
    if (!fnIsWow64Process(::GetCurrentProcess(), &bIsWow64))
    {
      return false;
    }
  }
  return bIsWow64 != false;
}

}
