#include "pch.h"
#include "resource.h"
#include "GitHub.h"
#include "Dialogs.h"

using namespace Yaswl;

namespace
{
  std::wstring GetDlgItemString(HWND hWnd, int id)
  {
    HWND hCtl = ::GetDlgItem(hWnd, id);
    int len = ::GetWindowTextLength(hCtl);
    std::wstring s(len, L'\0');
    if (len > 0)
      ::GetWindowText(hCtl, &s[0], len + 1);
    // trim quotes and spaces
    size_t b = 0, e = s.size();
    while (b < e && (iswspace(s[b]) || s[b] == L'"')) ++b;
    while (e > b && (iswspace(s[e - 1]) || s[e - 1] == L'"')) --e;
    return s.substr(b, e - b);
  }

  std::wstring GetClipboardString()
  {
    std::wstring result;
    if (!::OpenClipboard(nullptr))
      return result;
    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (h)
    {
      const wchar_t* p = static_cast<const wchar_t*>(::GlobalLock(h));
      if (p)
      {
        result = p;
        ::GlobalUnlock(h);
      }
    }
    ::CloseClipboard();
    return result;
  }
}

std::wstring BrowseForPath(HWND hWndOwner, bool folder, const std::wstring& initial)
{
  std::wstring path;

  _COM_SMARTPTR_TYPEDEF(IFileOpenDialog, __uuidof(IFileOpenDialog));
  _COM_SMARTPTR_TYPEDEF(IShellItem, __uuidof(IShellItem));

  IFileOpenDialogPtr pfd;
  if (FAILED(pfd.CreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER)))
    return path;

  DWORD dwOptions = 0;
  if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
  {
    dwOptions |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM;
    if (folder)
      dwOptions |= FOS_PICKFOLDERS;
    pfd->SetOptions(dwOptions);
  }

  if (!folder)
  {
    COMDLG_FILTERSPEC filter[1] = { { L"All files (*.*)", L"*.*" } };
    pfd->SetFileTypes(1, filter);
  }

  if (!initial.empty())
  {
    std::error_code ec;
    std::filesystem::path p(initial);
    std::filesystem::path dir = std::filesystem::is_directory(p, ec) ? p : p.parent_path();
    if (!dir.empty() && std::filesystem::exists(dir, ec))
    {
      IShellItemPtr psiFolder;
      if (SUCCEEDED(::SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&psiFolder))))
        pfd->SetFolder(psiFolder);
    }
    if (!folder && !std::filesystem::is_directory(p, ec))
      pfd->SetFileName(p.filename().c_str());
  }

  if (SUCCEEDED(pfd->Show(hWndOwner)))
  {
    IShellItemPtr psi;
    if (SUCCEEDED(pfd->GetResult(&psi)))
    {
      LPWSTR lpPath = nullptr;
      if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &lpPath)))
      {
        path = lpPath;
        ::CoTaskMemFree(lpPath);
      }
    }
  }
  return path;
}

// ─── AboutDlg ────────────────────────────────────────────────────────────────

AboutDlg::AboutDlg() : Dialog(MAKEINTRESOURCE(IDD_ABOUTBOX))
{
}

bool AboutDlg::OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  ::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)::LoadIcon(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN)));
  ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)::LoadIcon(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN)));
  return Dialog::OnInitDialog(hWnd, hWndFocus, lParam);
}

bool AboutDlg::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    HANDLE_DLGMSG(hWnd, WM_INITDIALOG, OnInitDialog);
  }
  return Dialog::DlgProc(hWnd, uMsg, wParam, lParam);
}

// ─── CompareDlg ──────────────────────────────────────────────────────────────

CompareDlg::CompareDlg(bool folders) : Dialog(MAKEINTRESOURCE(IDD_COMPARE)), m_folders(folders)
{
}

bool CompareDlg::OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  ::SetWindowText(hWnd, GetApp()->LoadString(m_folders ? IDS_COMPARE_FOLDERS_TITLE : IDS_COMPARE_FILES_TITLE).c_str());
  ::SetDlgItemText(hWnd, IDC_HINT, GetApp()->LoadString(m_folders ? IDS_DROP_HINT_FOLDERS : IDS_DROP_HINT_FILES).c_str());
  ::SetDlgItemText(hWnd, IDC_LEFT_PATH, m_left.c_str());
  ::SetDlgItemText(hWnd, IDC_RIGHT_PATH, m_right.c_str());
  ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)::LoadIcon(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN)));
  ::DragAcceptFiles(hWnd, TRUE);
  ::SetFocus(::GetDlgItem(hWnd, m_left.empty() ? IDC_LEFT_PATH : IDC_RIGHT_PATH));
  return false;
}

void CompareDlg::Browse(int editId)
{
  std::wstring current = GetDlgItemString(m_hWnd, editId);
  std::wstring path = BrowseForPath(m_hWnd, m_folders, current);
  if (!path.empty())
    ::SetDlgItemText(m_hWnd, editId, path.c_str());
}

void CompareDlg::OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  switch (id)
  {
  case IDC_BROWSE_LEFT:
    Browse(IDC_LEFT_PATH);
    break;

  case IDC_BROWSE_RIGHT:
    Browse(IDC_RIGHT_PATH);
    break;

  case IDC_SWAP:
  {
    std::wstring l = GetDlgItemString(hWnd, IDC_LEFT_PATH);
    std::wstring r = GetDlgItemString(hWnd, IDC_RIGHT_PATH);
    ::SetDlgItemText(hWnd, IDC_LEFT_PATH, r.c_str());
    ::SetDlgItemText(hWnd, IDC_RIGHT_PATH, l.c_str());
    break;
  }

  case IDOK:
  {
    m_left = GetDlgItemString(hWnd, IDC_LEFT_PATH);
    m_right = GetDlgItemString(hWnd, IDC_RIGHT_PATH);
    if (m_left.empty() || m_right.empty())
    {
      ::MessageBox(hWnd, GetApp()->LoadString(IDS_ERR_BOTH_PATHS).c_str(), GetApp()->GetAppName().c_str(), MB_OK | MB_ICONWARNING);
      ::SetFocus(::GetDlgItem(hWnd, m_left.empty() ? IDC_LEFT_PATH : IDC_RIGHT_PATH));
      return;
    }
    std::error_code ec;
    for (const std::wstring& p : { m_left, m_right })
    {
      bool ok = m_folders ? std::filesystem::is_directory(p, ec) : std::filesystem::is_regular_file(p, ec);
      if (!ok)
      {
        ::MessageBox(hWnd, StrFormat(GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), p.c_str()).c_str(), GetApp()->GetAppName().c_str(), MB_OK | MB_ICONWARNING);
        ::SetFocus(::GetDlgItem(hWnd, p == m_left ? IDC_LEFT_PATH : IDC_RIGHT_PATH));
        return;
      }
    }
    ::EndDialog(hWnd, IDOK);
    break;
  }

  case IDCANCEL:
    ::EndDialog(hWnd, IDCANCEL);
    break;
  }
}

void CompareDlg::OnDropFiles(HWND hWnd, HDROP hDrop)
{
  UINT count = ::DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
  std::vector<std::wstring> paths;
  for (UINT i = 0; i < count && i < 2; ++i)
  {
    wchar_t buf[MAX_PATH * 2];
    if (::DragQueryFile(hDrop, i, buf, static_cast<UINT>(std::size(buf))))
      paths.push_back(buf);
  }
  ::DragFinish(hDrop);

  if (paths.size() >= 2)
  {
    ::SetDlgItemText(hWnd, IDC_LEFT_PATH, paths[0].c_str());
    ::SetDlgItemText(hWnd, IDC_RIGHT_PATH, paths[1].c_str());
  }
  else if (paths.size() == 1)
  {
    // fill the first empty field
    int target = GetDlgItemString(hWnd, IDC_LEFT_PATH).empty() ? IDC_LEFT_PATH : IDC_RIGHT_PATH;
    ::SetDlgItemText(hWnd, target, paths[0].c_str());
  }
}

bool CompareDlg::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    HANDLE_DLGMSG(hWnd, WM_INITDIALOG, OnInitDialog);
    HANDLE_DLGMSG(hWnd, WM_COMMAND, OnCommand);
  case WM_DROPFILES:
    OnDropFiles(hWnd, reinterpret_cast<HDROP>(wParam));
    return true;
  }
  return Dialog::DlgProc(hWnd, uMsg, wParam, lParam);
}

// ─── PullRequestDlg ──────────────────────────────────────────────────────────

PullRequestDlg::PullRequestDlg() : Dialog(MAKEINTRESOURCE(IDD_PULLREQUEST))
{
}

bool PullRequestDlg::OnInitDialog(HWND hWnd, HWND hWndFocus, LPARAM lParam)
{
  ::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)::LoadIcon(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN)));

  // Prefer a pull request URL found in the clipboard over the last used one.
  std::wstring clip = GetClipboardString();
  PullRequestInfo probe;
  if (clip.size() < 1024 && clip.find(L"/pull/") != std::wstring::npos && GitHubClient::ParsePullRequestUrl(clip, probe))
    m_url = clip;

  ::SetDlgItemText(hWnd, IDC_PR_URL, m_url.c_str());
  ::SendDlgItemMessage(hWnd, IDC_PR_URL, EM_SETSEL, 0, -1);
  ::SetFocus(::GetDlgItem(hWnd, IDC_PR_URL));
  return false;
}

void PullRequestDlg::OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  switch (id)
  {
  case IDOK:
  {
    m_url = GetDlgItemString(hWnd, IDC_PR_URL);
    PullRequestInfo probe;
    if (!GitHubClient::ParsePullRequestUrl(m_url, probe))
    {
      ::MessageBox(hWnd, GetApp()->LoadString(IDS_ERR_PR_URL).c_str(), GetApp()->GetAppName().c_str(), MB_OK | MB_ICONWARNING);
      ::SetFocus(::GetDlgItem(hWnd, IDC_PR_URL));
      return;
    }
    ::EndDialog(hWnd, IDOK);
    break;
  }
  case IDCANCEL:
    ::EndDialog(hWnd, IDCANCEL);
    break;
  }
}

bool PullRequestDlg::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    HANDLE_DLGMSG(hWnd, WM_INITDIALOG, OnInitDialog);
    HANDLE_DLGMSG(hWnd, WM_COMMAND, OnCommand);
  }
  return Dialog::DlgProc(hWnd, uMsg, wParam, lParam);
}
