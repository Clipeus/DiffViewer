#pragma once

class DiffViewerApp : public Yaswl::WinApp
{
public:
  DiffViewerApp();
  ~DiffViewerApp();

public:
  bool Init(HINSTANCE hInstance, const std::wstring& strCmdLine) override;
  int Run(Yaswl::Window* pMainWnd) override;

  // settings helpers
  std::wstring GetSetting(const wchar_t* name, const std::wstring& def = L"");
  void SetSetting(const wchar_t* name, const std::wstring& value);
  unsigned long GetSettingDword(const wchar_t* name, unsigned long def);
  void SetSettingDword(const wchar_t* name, unsigned long value);

private:
  bool m_bComInited = false;
};
