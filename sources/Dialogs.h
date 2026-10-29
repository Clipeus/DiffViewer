#pragma once

class AboutDlg : public Yaswl::Dialog
{
public:
  AboutDlg();

protected:
  bool DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
  bool OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam);
};

// "Compare Files" / "Compare Folders" dialog with two paths.
class CompareDlg : public Yaswl::Dialog
{
public:
  explicit CompareDlg(bool folders);

  bool m_folders;
  std::wstring m_left;
  std::wstring m_right;

protected:
  bool DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
  bool OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam);
  void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify);
  void OnDropFiles(HWND hWnd, HDROP hDrop);
  void Browse(int editId);
};

class PullRequestDlg : public Yaswl::Dialog
{
public:
  PullRequestDlg();

  std::wstring m_url;

protected:
  bool DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
  bool OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam);
  void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify);
};

// Shows the standard file / folder picker. Returns an empty string when cancelled.
std::wstring BrowseForPath(HWND hWndOwner, bool folder, const std::wstring& initial);
