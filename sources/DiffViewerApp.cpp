#include "pch.h"
#include "resource.h"
#include "DiffViewerApp.h"

using namespace Yaswl;

DiffViewerApp::DiffViewerApp()
{
  m_strAppName = LoadString(IDS_APP_TITLE);
  m_strRegistryRoot = L"SOFTWARE\\LMSoft\\DiffViewer";
}

DiffViewerApp::~DiffViewerApp()
{
  if (m_bComInited)
    ::CoUninitialize();
}

bool DiffViewerApp::Init(HINSTANCE hInstance, const std::wstring& strCmdLine)
{
  if (!WinApp::Init(hInstance, strCmdLine))
  {
    ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }

  INITCOMMONCONTROLSEX iccs = {};
  iccs.dwSize = sizeof(iccs);
  iccs.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
  ::InitCommonControlsEx(&iccs);

  m_bComInited = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
  return true;
}

int DiffViewerApp::Run(Yaswl::Window* pMainWnd)
{
  m_hAccel = ::LoadAccelerators(GetInstance(), MAKEINTRESOURCE(IDR_MAIN_ACCEL));
  if (!m_hAccel)
  {
    ReportSystemError(IDS_SYSTEM_ERROR);
    return 1;
  }
  return WinApp::Run(pMainWnd);
}

std::wstring DiffViewerApp::GetSetting(const wchar_t* name, const std::wstring& def)
{
  std::wstring value;
  if (RegistryW::Value(GetRegistryRoot(), name, value))
    return value;
  return def;
}

void DiffViewerApp::SetSetting(const wchar_t* name, const std::wstring& value)
{
  RegistryW::SetValue(GetRegistryRoot(), name, value);
}

unsigned long DiffViewerApp::GetSettingDword(const wchar_t* name, unsigned long def)
{
  unsigned long value = 0;
  if (RegistryW::Value(GetRegistryRoot(), name, value))
    return value;
  return def;
}

void DiffViewerApp::SetSettingDword(const wchar_t* name, unsigned long value)
{
  RegistryW::SetValue(GetRegistryRoot(), name, value);
}
