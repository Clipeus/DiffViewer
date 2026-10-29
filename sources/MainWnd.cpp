#include "pch.h"
#include "resource.h"
#include "DiffViewerApp.h"
#include "MainWnd.h"
#include "Dialogs.h"

constexpr int IDC_MAIN_STATUSBAR = 1;
constexpr int IDC_MAIN_TOOLBAR   = 2;
constexpr int IDC_TREE           = 3;
constexpr int IDC_DIFF_VIEW      = 4;

constexpr int SPLITTER_W           = 5;
constexpr int STATUSBAR_PART1_WIDTH = 200;
constexpr int STATUSBAR_PART2_WIDTH = 240;

using namespace Yaswl;

namespace
{
  DiffViewerApp* App()
  {
    return static_cast<DiffViewerApp*>(GetApp());
  }

  COLORREF ColorForStatus(ItemStatus status)
  {
    switch (status)
    {
    case ItemStatus::Different: return RGB(190, 95, 0);
    case ItemStatus::LeftOnly:  return RGB(200, 30, 30);
    case ItemStatus::RightOnly: return RGB(20, 130, 20);
    default:                    return RGB(120, 120, 120);
    }
  }

  std::wstring ItemLabel(const CompareItem& item)
  {
    std::wstring label = item.name;
    if (!item.prStatus.empty())
    {
      if (item.additions || item.deletions)
        label += StrFormat(L"  (+%d -%d)", item.additions, item.deletions);
      if (!item.prPreviousPath.empty())
        label += L"  (" + StrFormat(GetApp()->LoadString(IDS_RENAMED_FROM).c_str(), item.prPreviousPath.c_str()) + L")";
    }
    else if (item.status == ItemStatus::LeftOnly)
    {
      label += L"  (" + GetApp()->LoadString(IDS_LEFT_ONLY) + L")";
    }
    else if (item.status == ItemStatus::RightOnly)
    {
      label += L"  (" + GetApp()->LoadString(IDS_RIGHT_ONLY) + L")";
    }
    return label;
  }

  HTREEITEM FindTreeItem(HWND hTree, HTREEITEM hStart, const CompareItem* target)
  {
    for (HTREEITEM h = hStart; h; h = TreeView_GetNextSibling(hTree, h))
    {
      TVITEM tvi = {};
      tvi.mask = TVIF_PARAM;
      tvi.hItem = h;
      if (TreeView_GetItem(hTree, &tvi) && reinterpret_cast<const CompareItem*>(tvi.lParam) == target)
        return h;
      HTREEITEM child = TreeView_GetChild(hTree, h);
      if (child)
      {
        HTREEITEM found = FindTreeItem(hTree, child, target);
        if (found)
          return found;
      }
    }
    return nullptr;
  }

  std::wstring TrimQuotes(const std::wstring& s)
  {
    size_t b = 0, e = s.size();
    while (b < e && (iswspace(s[b]) || s[b] == L'"')) ++b;
    while (e > b && (iswspace(s[e - 1]) || s[e - 1] == L'"')) --e;
    return s.substr(b, e - b);
  }
}

// ─── construction ────────────────────────────────────────────────────────────

MainWnd::MainWnd() : MainWindow(MainWnd::GetClassName())
{
}

MainWnd::~MainWnd()
{
}

const wchar_t* MainWnd::GetClassName()
{
  return L"DiffViewer_MainWindow_1.0";
}

void MainWnd::OnRegisterClass(WNDCLASSEX& wc)
{
  wc.hIcon = (HICON)::LoadImage(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 32, 32, 0);
  wc.hIconSm = (HICON)::LoadImage(GetApp()->GetInstance(), MAKEINTRESOURCE(IDI_MAIN), IMAGE_ICON, 16, 16, 0);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN);
}

bool MainWnd::Init()
{
  if (!MainWindow::Init() || !m_wndDiff.Init())
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }
  return true;
}

bool MainWnd::Create()
{
  if (!WindowEx::Create(WS_EX_ACCEPTFILES, GetClassName(), GetApp()->GetAppName()))
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }
  _ASSERTE(m_hWnd);
  ::UpdateWindow(m_hWnd);
  return true;
}

// ─── settings ────────────────────────────────────────────────────────────────

void MainWnd::LoadSettings()
{
  m_treeWidth = static_cast<int>(App()->GetSettingDword(L"TreeWidth", 320));
  m_ignoreWhitespace = App()->GetSettingDword(L"IgnoreWhitespace", 0) != 0;
  m_showIdentical = App()->GetSettingDword(L"ShowIdentical", 1) != 0;
}

void MainWnd::SaveSettings()
{
  App()->SetSettingDword(L"TreeWidth", static_cast<unsigned long>(m_treeWidth));
  App()->SetSettingDword(L"IgnoreWhitespace", m_ignoreWhitespace ? 1 : 0);
  App()->SetSettingDword(L"ShowIdentical", m_showIdentical ? 1 : 0);
}

// ─── window procedure ────────────────────────────────────────────────────────

LRESULT MainWnd::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    HANDLE_MSG(hWnd, WM_CREATE,        OnCreate);
    HANDLE_MSG(hWnd, WM_SIZE,          OnSize);
    HANDLE_MSG(hWnd, WM_DESTROY,       OnDestroy);
    HANDLE_MSG(hWnd, WM_COMMAND,       OnCommand);
    HANDLE_MSG(hWnd, WM_NOTIFY,        OnNotify);
    HANDLE_MSG(hWnd, WM_MENUSELECT,    OnMenuSelect);
    HANDLE_MSG(hWnd, WM_CONTEXTMENU,   OnContextMenu);
    HANDLE_MSG(hWnd, WM_SETCURSOR,     OnSetCursor);
    HANDLE_MSG(hWnd, WM_ACTIVATE,      OnActivate);
    HANDLE_MSG(hWnd, WM_INITMENUPOPUP, OnInitMenuPopup);
    HANDLE_MSG(hWnd, WM_LBUTTONDOWN,   OnLButtonDown);
    HANDLE_MSG(hWnd, WM_LBUTTONUP,     OnLButtonUp);
    HANDLE_MSG(hWnd, WM_MOUSEMOVE,     OnMouseMove);

  case WM_DROPFILES:
    OnDropFiles(hWnd, reinterpret_cast<HDROP>(wParam));
    return 0;

  case WM_MEASUREITEM:
    OnMeasureItem(hWnd, reinterpret_cast<MEASUREITEMSTRUCT*>(lParam));
    return TRUE;

  case WM_DRAWITEM:
    OnDrawItem(hWnd, reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
    return TRUE;

  case DiffView::WM_DV_BLOCKCHANGED:
    UpdateStatusBar();
    UpdateToolBar();
    return 0;

  default:
    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
}

bool MainWnd::OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
  m_hWnd = hWnd;
  LoadSettings();

  NONCLIENTMETRICS ncm = {};
  ncm.cbSize = sizeof(ncm);
  ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  m_hUiFont = ::CreateFontIndirect(&ncm.lfMessageFont);

  TBBUTTON tbButtons[] =
  {
    {IDI_FILES,      ID_FILE_COMPARE_FILES,   TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_FOLDER,     ID_FILE_COMPARE_FOLDERS, TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_PULLREQ,    ID_FILE_OPEN_PR,         TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_REFRESH,    ID_FILE_REFRESH,         TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
    {IDI_FIRST,      ID_VIEW_FIRST_DIFF,      TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_PREV,       ID_VIEW_PREV_DIFF,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_NEXT,       ID_VIEW_NEXT_DIFF,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_LAST,       ID_VIEW_LAST_DIFF,       TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
    {IDI_SWAP,       ID_VIEW_SWAP,            TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
    {IDI_WHITESPACE, ID_VIEW_IGNORE_WS,       TBSTATE_ENABLED, TBSTYLE_CHECK,  {0}, 0, 0},
    {0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, 0},
    {IDI_HELP,       IDM_ABOUT,               TBSTATE_ENABLED, TBSTYLE_BUTTON, {0}, 0, 0},
  };

  if (!m_wndToolBar.Create(m_hWnd, IDC_MAIN_TOOLBAR) ||
      !m_wndToolBar.Init(tbButtons, static_cast<int>(std::size(tbButtons))))
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }

  m_hStatusBar = ::CreateWindowEx(0, STATUSCLASSNAME, nullptr,
    WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
    0, 0, 0, 0, m_hWnd,
    reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MAIN_STATUSBAR)),
    GetApp()->GetInstance(), nullptr);
  if (!m_hStatusBar)
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }

  if (!m_wndTree.Create(WS_EX_CLIENTEDGE, WC_TREEVIEW, L"",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
    0, 0, 0, 0, m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TREE)), GetApp()->GetInstance()))
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }
  ::SetWindowTheme(m_wndTree, L"Explorer", nullptr);
  SetWindowFont(m_wndTree, m_hUiFont, FALSE);

  if (!m_wndDiff.Create(m_hWnd, IDC_DIFF_VIEW))
  {
    GetApp()->ReportSystemError(IDS_SYSTEM_ERROR);
    return false;
  }
  m_wndDiff.SetPlaceholder(GetApp()->LoadString(IDS_NO_SESSION));

  UpdateTitle();
  UpdateStatusBar();
  UpdateToolBar();
  return true;
}

void MainWnd::OnDestroy(HWND hWnd)
{
  SaveWindowPlacement();
  SaveSettings();

  if (m_hUiFont)
  {
    ::DeleteObject(m_hUiFont);
    m_hUiFont = nullptr;
  }
  ::PostQuitMessage(0);
}

void MainWnd::OnSize(HWND hWnd, UINT state, int cx, int cy)
{
  LayoutChildren();
}

void MainWnd::LayoutChildren()
{
  RECT rc;
  ::GetClientRect(m_hWnd, &rc);
  const int cx = rc.right;
  const int cy = rc.bottom;

  ::SendMessage(m_wndToolBar, WM_SIZE, 0, 0);
  ::SendMessage(m_hStatusBar, WM_SIZE, 0, 0);

  int parts[3];
  parts[0] = std::max(50, cx - (STATUSBAR_PART1_WIDTH + STATUSBAR_PART2_WIDTH));
  parts[1] = std::max(parts[0] + 10, cx - STATUSBAR_PART2_WIDTH);
  parts[2] = -1;
  ::SendMessage(m_hStatusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

  RECT rectTB, rectSB;
  ::GetWindowRect(m_wndToolBar, &rectTB);
  ::GetWindowRect(m_hStatusBar, &rectSB);
  const int top = rectTB.bottom - rectTB.top;
  const int bottom = rectSB.bottom - rectSB.top;
  const int h = std::max(0, cy - top - bottom);

  const int maxTree = std::max(120, cx - 240);
  m_treeWidth = std::max(120, std::min(m_treeWidth, maxTree));
  const int treeW = std::min(m_treeWidth, std::max(0, cx - SPLITTER_W));

  ::MoveWindow(m_wndTree, 0, top, treeW, h, TRUE);
  ::MoveWindow(m_wndDiff, treeW + SPLITTER_W, top, std::max(0, cx - treeW - SPLITTER_W), h, TRUE);
}

// ─── commands ────────────────────────────────────────────────────────────────

void MainWnd::OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
  switch (id)
  {
  case IDM_EXIT:
    ::PostMessage(hWnd, WM_CLOSE, 0, 0);
    break;

  case ID_FILE_COMPARE_FILES:
  {
    CompareDlg dlg(false);
    dlg.m_left = App()->GetSetting(L"LastFileLeft");
    dlg.m_right = App()->GetSetting(L"LastFileRight");
    if (dlg.DoModal(m_hWnd) == IDOK)
      CompareFiles(dlg.m_left, dlg.m_right);
    break;
  }

  case ID_FILE_COMPARE_FOLDERS:
  {
    CompareDlg dlg(true);
    dlg.m_left = App()->GetSetting(L"LastFolderLeft");
    dlg.m_right = App()->GetSetting(L"LastFolderRight");
    if (dlg.DoModal(m_hWnd) == IDOK)
      CompareFolders(dlg.m_left, dlg.m_right);
    break;
  }

  case ID_FILE_OPEN_PR:
  {
    PullRequestDlg dlg;
    dlg.m_url = App()->GetSetting(L"LastPullRequest");
    if (dlg.DoModal(m_hWnd) == IDOK)
      OpenPullRequest(dlg.m_url);
    break;
  }

  case ID_FILE_REFRESH:
    Refresh();
    break;

  case ID_EDIT_COPY:
    if (::GetFocus() == m_wndTree.GetHandle())
    {
      if (m_currentItem && ::OpenClipboard(m_hWnd))
      {
        std::wstring text = m_currentItem->relPath;
        ::EmptyClipboard();
        size_t bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem)
        {
          memcpy(::GlobalLock(hMem), text.c_str(), bytes);
          ::GlobalUnlock(hMem);
          ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        ::CloseClipboard();
      }
    }
    else
    {
      m_wndDiff.CopySelection();
    }
    break;

  case ID_EDIT_SELECTALL:
    m_wndDiff.SelectAll();
    break;

  case ID_VIEW_FIRST_DIFF: m_wndDiff.FirstBlock(); break;
  case ID_VIEW_PREV_DIFF:  m_wndDiff.PrevBlock();  break;
  case ID_VIEW_NEXT_DIFF:  m_wndDiff.NextBlock();  break;
  case ID_VIEW_LAST_DIFF:  m_wndDiff.LastBlock();  break;

  case ID_VIEW_IGNORE_WS:
    m_ignoreWhitespace = !m_ignoreWhitespace;
    SaveSettings();
    UpdateToolBar();
    ShowItem(m_currentItem);
    break;

  case ID_VIEW_SHOW_IDENTICAL:
    m_showIdentical = !m_showIdentical;
    SaveSettings();
    if (m_session)
    {
      const CompareItem* keep = m_currentItem;
      PopulateTree();
      if (keep && (m_showIdentical || keep->status != ItemStatus::Identical))
        SelectItem(keep);
      else
        SelectItem(FirstInterestingItem(m_session->Root()));
    }
    break;

  case ID_VIEW_SWAP:
    if (m_session && m_session->CanSwap())
    {
      m_session->Swap();
      Refresh();
    }
    break;

  case IDM_ABOUT:
  {
    AboutDlg dlg;
    dlg.DoModal(m_hWnd);
    break;
  }
  }
}

// ─── sessions ────────────────────────────────────────────────────────────────

bool MainWnd::StartSession(std::unique_ptr<CompareSession> session)
{
  std::wstring error;
  if (!session->Load(error))
  {
    GetApp()->ReportBox(error, MB_OK | MB_ICONERROR);
    return false;
  }

  m_session = std::move(session);
  m_currentItem = nullptr;
  m_currentRelPath.clear();

  PopulateTree();
  UpdateTitle();

  const CompareItem* first = FirstInterestingItem(m_session->Root());
  if (first)
    SelectItem(first);
  else
    ShowItem(nullptr);

  UpdateToolBar();
  UpdateStatusBar();
  return true;
}

void MainWnd::Refresh()
{
  if (!m_session)
    return;

  std::wstring keep = m_currentRelPath;
  std::wstring error;
  if (!m_session->Load(error))
  {
    GetApp()->ReportBox(error, MB_OK | MB_ICONERROR);
    m_session.reset();
    m_currentItem = nullptr;
    PopulateTree();
    UpdateTitle();
    ShowItem(nullptr);
    UpdateToolBar();
    return;
  }

  m_currentItem = nullptr;
  PopulateTree();
  UpdateTitle();

  const CompareItem* item = keep.empty() ? nullptr : m_session->FindItem(keep);
  if (!item)
    item = FirstInterestingItem(m_session->Root());
  if (item)
    SelectItem(item);
  else
    ShowItem(nullptr);
  UpdateToolBar();
}

bool MainWnd::CompareFiles(const std::wstring& left, const std::wstring& right)
{
  if (!StartSession(std::make_unique<FilePairSession>(left, right)))
    return false;
  App()->SetSetting(L"LastFileLeft", left);
  App()->SetSetting(L"LastFileRight", right);
  return true;
}

bool MainWnd::CompareFolders(const std::wstring& left, const std::wstring& right)
{
  if (!StartSession(std::make_unique<FolderSession>(left, right)))
    return false;
  App()->SetSetting(L"LastFolderLeft", left);
  App()->SetSetting(L"LastFolderRight", right);
  return true;
}

bool MainWnd::OpenPullRequest(const std::wstring& url)
{
  if (!StartSession(std::make_unique<PullRequestSession>(url)))
    return false;
  App()->SetSetting(L"LastPullRequest", url);
  return true;
}

bool MainWnd::OpenPaths(const std::wstring& left, const std::wstring& right)
{
  std::error_code ec;
  bool leftDir = std::filesystem::is_directory(left, ec);
  bool rightDir = std::filesystem::is_directory(right, ec);
  if (leftDir && rightDir)
    return CompareFolders(left, right);
  if (!leftDir && !rightDir)
    return CompareFiles(left, right);

  GetApp()->ReportBox(StrFormat(GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), (leftDir ? right : left).c_str()), MB_OK | MB_ICONERROR);
  return false;
}

void MainWnd::OpenFromCommandLine(const std::wstring& cmdLine)
{
  if (TrimQuotes(cmdLine).empty())
    return;

  int argc = 0;
  LPWSTR* argv = ::CommandLineToArgvW(cmdLine.c_str(), &argc);
  if (!argv)
    return;

  std::vector<std::wstring> args;
  for (int i = 0; i < argc; ++i)
    args.push_back(TrimQuotes(argv[i]));
  ::LocalFree(argv);

  if (args.size() == 1)
  {
    PullRequestInfo probe;
    if (GitHubClient::ParsePullRequestUrl(args[0], probe))
      OpenPullRequest(args[0]);
    else
      GetApp()->ReportBox(GetApp()->LoadString(IDS_ERR_PR_URL), MB_OK | MB_ICONWARNING);
  }
  else if (args.size() >= 2)
  {
    OpenPaths(args[0], args[1]);
  }
}

// ─── tree ────────────────────────────────────────────────────────────────────

void MainWnd::PopulateTree()
{
  m_populating = true;
  SetWindowRedraw(m_wndTree, FALSE);
  TreeView_DeleteAllItems(m_wndTree);
  if (m_session)
    AddTreeItems(TVI_ROOT, m_session->Root());
  SetWindowRedraw(m_wndTree, TRUE);
  ::InvalidateRect(m_wndTree, nullptr, TRUE);
  m_populating = false;
}

void MainWnd::AddTreeItems(HTREEITEM hParent, const CompareItem& node)
{
  for (const auto& child : node.children)
  {
    if (!m_showIdentical && m_session->IsTree() && child->status == ItemStatus::Identical)
      continue;

    std::wstring label = ItemLabel(*child);
    TVINSERTSTRUCT tvis = {};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = &label[0];
    tvis.item.lParam = reinterpret_cast<LPARAM>(child.get());
    HTREEITEM h = TreeView_InsertItem(m_wndTree.GetHandle(), &tvis);

    if (child->isDir)
    {
      AddTreeItems(h, *child);
      TreeView_Expand(m_wndTree.GetHandle(), h, TVE_EXPAND);
    }
  }
}

const CompareItem* MainWnd::FirstInterestingItem(const CompareItem& node) const
{
  const CompareItem* firstFile = nullptr;
  std::function<const CompareItem*(const CompareItem&)> visit = [&](const CompareItem& n) -> const CompareItem*
  {
    for (const auto& c : n.children)
    {
      if (c->isDir)
      {
        const CompareItem* r = visit(*c);
        if (r)
          return r;
      }
      else
      {
        if (!firstFile && (m_showIdentical || c->status != ItemStatus::Identical))
          firstFile = c.get();
        if (c->status != ItemStatus::Identical)
          return c.get();
      }
    }
    return nullptr;
  };
  const CompareItem* found = visit(node);
  return found ? found : firstFile;
}

void MainWnd::SelectItem(const CompareItem* item)
{
  if (!item)
  {
    ShowItem(nullptr);
    return;
  }
  HTREEITEM h = FindTreeItem(m_wndTree, TreeView_GetRoot(m_wndTree.GetHandle()), item);
  if (h)
  {
    TreeView_EnsureVisible(m_wndTree.GetHandle(), h);
    TreeView_SelectItem(m_wndTree.GetHandle(), h);   // triggers TVN_SELCHANGED -> ShowItem
    ::SendMessage(m_wndTree, WM_HSCROLL, SB_LEFT, 0); // EnsureVisible may scroll deep items sideways
  }
  else
  {
    ShowItem(item);
  }
}

void MainWnd::ShowItem(const CompareItem* item)
{
  m_currentItem = item;
  m_currentRelPath = item ? item->relPath : L"";

  if (!item || !m_session)
  {
    m_wndDiff.SetPlaceholder(GetApp()->LoadString(m_session ? IDS_SELECT_FILE : IDS_NO_SESSION));
    UpdateStatusBar();
    return;
  }
  if (item->isDir)
  {
    m_wndDiff.SetPlaceholder(GetApp()->LoadString(IDS_SELECT_FILE));
    UpdateStatusBar();
    return;
  }

  TextContent left, right;
  std::wstring leftTitle, rightTitle, error;
  if (!m_session->LoadItem(*item, left, right, leftTitle, rightTitle, error))
  {
    m_wndDiff.SetPlaceholder(error);
    UpdateStatusBar();
    GetApp()->ReportBox(StrFormat(GetApp()->LoadString(IDS_ERR_LOAD).c_str(), error.c_str()), MB_OK | MB_ICONERROR);
    return;
  }

  WinApp::WaitCursor wc;
  auto doc = std::make_shared<DiffDocument>();
  if (left.exists)
    doc->leftLines = DiffEngine::SplitLines(left.text);
  if (right.exists)
    doc->rightLines = DiffEngine::SplitLines(right.text);
  doc->leftTitle = leftTitle;
  doc->rightTitle = rightTitle;
  auto info = [](const TextContent& c)
  {
    std::wstring s = c.source;
    if (c.exists && !c.encoding.empty())
      s += (s.empty() ? L"" : L"  |  ") + c.encoding;
    return s;
  };
  doc->leftInfo = info(left);
  doc->rightInfo = info(right);
  m_encodings = (left.exists ? left.encoding : L"-") + L"  |  " + (right.exists ? right.encoding : L"-");
  DiffEngine::BuildDocument(*doc, m_ignoreWhitespace);

  m_wndDiff.SetDocument(doc);
  UpdateStatusBar();
}

// ─── UI state ────────────────────────────────────────────────────────────────

void MainWnd::UpdateTitle()
{
  std::wstring title;
  if (m_session)
    title = m_session->GetTitle() + L" - ";
  title += GetApp()->GetAppName();
  ::SetWindowText(m_hWnd, title.c_str());
}

void MainWnd::UpdateStatusBar()
{
  std::wstring part0 = m_session ? m_session->GetSummary() : L"";
  if (part0.empty())
    part0 = GetApp()->LoadString(IDS_READY);
  std::wstring part1, part2;

  const DiffDocument* doc = m_wndDiff.GetDocument();
  if (doc)
  {
    if (doc->blocks.empty())
      part1 = GetApp()->LoadString(doc->leftLines == doc->rightLines ? IDS_IDENTICAL : IDS_NO_DIFFS);
    else if (m_wndDiff.GetCurrentBlock() >= 0)
      part1 = StrFormat(GetApp()->LoadString(IDS_DIFF_OF).c_str(), m_wndDiff.GetCurrentBlock() + 1, static_cast<int>(doc->blocks.size()));
    else
      part1 = StrFormat(GetApp()->LoadString(IDS_DIFFS_COUNT).c_str(), static_cast<int>(doc->blocks.size()));

    part2 = m_encodings;
  }

  ::SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(part0.c_str()));
  ::SendMessage(m_hStatusBar, SB_SETTEXT, 1, reinterpret_cast<LPARAM>(part1.c_str()));
  ::SendMessage(m_hStatusBar, SB_SETTEXT, 2, reinterpret_cast<LPARAM>(part2.c_str()));
}

void MainWnd::UpdateToolBar()
{
  const int blocks = m_wndDiff.GetBlockCount();
  const int current = m_wndDiff.GetCurrentBlock();
  const bool hasSession = m_session != nullptr;

  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_FILE_REFRESH,    MAKELONG(hasSession, 0));
  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_VIEW_FIRST_DIFF, MAKELONG(blocks > 0 && current > 0, 0));
  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_VIEW_PREV_DIFF,  MAKELONG(blocks > 0 && current > 0, 0));
  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_VIEW_NEXT_DIFF,  MAKELONG(blocks > 0 && current < blocks - 1, 0));
  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_VIEW_LAST_DIFF,  MAKELONG(blocks > 0 && current < blocks - 1, 0));
  ::SendMessage(m_wndToolBar, TB_ENABLEBUTTON, ID_VIEW_SWAP,       MAKELONG(hasSession && m_session->CanSwap(), 0));
  ::SendMessage(m_wndToolBar, TB_CHECKBUTTON,  ID_VIEW_IGNORE_WS,  MAKELONG(m_ignoreWhitespace, 0));
}

void MainWnd::OnInitMenuPopup(HWND hWnd, HMENU hMenu, UINT item, bool fSystemMenu)
{
  if (fSystemMenu)
    return;

  const int blocks = m_wndDiff.GetBlockCount();
  const int current = m_wndDiff.GetCurrentBlock();
  const bool hasSession = m_session != nullptr;

  int nCount = ::GetMenuItemCount(hMenu);
  for (int i = 0; i < nCount; i++)
  {
    MENUITEMINFO mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID;
    ::GetMenuItemInfo(hMenu, i, TRUE, &mii);

    switch (mii.wID)
    {
    case ID_FILE_REFRESH:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | (hasSession ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_VIEW_FIRST_DIFF:
    case ID_VIEW_PREV_DIFF:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | ((blocks > 0 && current > 0) ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_VIEW_NEXT_DIFF:
    case ID_VIEW_LAST_DIFF:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | ((blocks > 0 && current < blocks - 1) ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_VIEW_SWAP:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | ((hasSession && m_session->CanSwap()) ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_VIEW_IGNORE_WS:
      ::CheckMenuItem(hMenu, mii.wID, MF_BYCOMMAND | (m_ignoreWhitespace ? MF_CHECKED : MF_UNCHECKED));
      break;
    case ID_VIEW_SHOW_IDENTICAL:
      ::CheckMenuItem(hMenu, mii.wID, MF_BYCOMMAND | (m_showIdentical ? MF_CHECKED : MF_UNCHECKED));
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | ((hasSession && m_session->IsTree()) ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_EDIT_COPY:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | ((m_wndDiff.HasSelection() || m_currentItem) ? MFS_ENABLED : MFS_DISABLED));
      break;
    case ID_EDIT_SELECTALL:
      ::EnableMenuItem(hMenu, mii.wID, MF_BYCOMMAND | (m_wndDiff.GetDocument() ? MFS_ENABLED : MFS_DISABLED));
      break;
    }
  }

  Yaswl::OwnerDrawMenuImpl<MainWnd>::OnInitMenuPopup(hWnd, hMenu, item, fSystemMenu);
}

void MainWnd::OnMenuSelect(HWND hWnd, HMENU hMenu, int item, HMENU hMenuPopup, UINT flags)
{
  if ((flags == 0xFFFFFFFF) && (hMenu == nullptr))
  {
    UpdateStatusBar();
  }
  else
  {
    std::wstring strText;
    if (!((flags & MF_POPUP) || (flags & MFT_SEPARATOR)))
    {
      strText = GetApp()->LoadString(item);
      size_t pos = strText.find(L'\n');
      if (std::wstring::npos != pos)
        strText = strText.substr(0, pos);
    }
    ::SendMessage(m_hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(strText.c_str()));
  }
}

void MainWnd::OnContextMenu(HWND hWnd, HWND hwndContext, UINT x, UINT y)
{
  if (hwndContext != m_wndDiff.GetHandle())
    return;

  POINT pt;
  if (x == static_cast<UINT>(-1) || y == static_cast<UINT>(-1))
  {
    RECT rect;
    ::GetClientRect(m_wndDiff, &rect);
    pt.x = rect.right / 2;
    pt.y = rect.bottom / 2;
    ::ClientToScreen(m_wndDiff, &pt);
  }
  else
  {
    pt.x = static_cast<int>(static_cast<short>(LOWORD(MAKELONG(x, 0))));
    pt.y = static_cast<int>(static_cast<short>(LOWORD(MAKELONG(y, 0))));
  }

  HMENU hMenuMain = ::LoadMenu(GetApp()->GetInstance(), MAKEINTRESOURCE(IDR_VIEW_CONTEXT_MENU));
  HMENU hMenuContext = ::GetSubMenu(hMenuMain, 0);
  ::TrackPopupMenu(hMenuContext, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_LEFTBUTTON, pt.x, pt.y, 0, m_hWnd, nullptr);
  ::DestroyMenu(hMenuMain);
}

LRESULT MainWnd::OnNotify(HWND hWnd, int idFrom, NMHDR* pNmhdr)
{
  if (pNmhdr->hwndFrom == m_wndTree.GetHandle())
  {
    switch (pNmhdr->code)
    {
    case TVN_SELCHANGED:
    {
      if (m_populating)
        return 0;
      const NMTREEVIEW* tv = reinterpret_cast<const NMTREEVIEW*>(pNmhdr);
      ShowItem(reinterpret_cast<const CompareItem*>(tv->itemNew.lParam));
      return 0;
    }

    case NM_CUSTOMDRAW:
    {
      NMTVCUSTOMDRAW* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(pNmhdr);
      switch (cd->nmcd.dwDrawStage)
      {
      case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
      case CDDS_ITEMPREPAINT:
      {
        const CompareItem* item = reinterpret_cast<const CompareItem*>(cd->nmcd.lItemlParam);
        if (item && !(cd->nmcd.uItemState & CDIS_SELECTED))
          cd->clrText = ColorForStatus(item->status);
        return CDRF_DODEFAULT;
      }
      }
      return CDRF_DODEFAULT;
    }

    case NM_SETFOCUS:
      UpdateToolBar();
      return 0;
    }
    return 0;
  }

  switch (pNmhdr->code)
  {
  case TTN_GETDISPINFO:
  {
    LPTOOLTIPTEXT lpTTT = reinterpret_cast<LPTOOLTIPTEXT>(pNmhdr);
    std::wstring strText = GetApp()->LoadString(idFrom);
    size_t pos = strText.find(L'\n');
    if (std::wstring::npos != pos)
      strText = strText.substr(pos + 1);
    ::wcsncpy_s(lpTTT->szText, strText.c_str(), _TRUNCATE);
    return TRUE;
  }
  }
  return 0;
}

// ─── mouse / splitter ────────────────────────────────────────────────────────

bool MainWnd::IsSplitterPoint(int x, int y) const
{
  RECT rc;
  ::GetWindowRect(m_wndTree, &rc);
  Yaswl::ScreenToClient(m_hWnd, &rc);
  return x >= rc.right && x < rc.right + SPLITTER_W + 2 && y >= rc.top && y < rc.bottom;
}

bool MainWnd::OnSetCursor(HWND hWnd, HWND hWndCursor, UINT codeHitTest, UINT msg)
{
  if (GetApp()->IsWaitCursor())
  {
    ::SetCursor(GetApp()->GetWaitCursor());
    return true;
  }
  if (hWndCursor == m_hWnd && codeHitTest == HTCLIENT)
  {
    POINT pt;
    ::GetCursorPos(&pt);
    ::ScreenToClient(m_hWnd, &pt);
    if (m_splitterDrag || IsSplitterPoint(pt.x, pt.y))
    {
      ::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
      return true;
    }
  }
  return FORWARD_WM_SETCURSOR(hWnd, hWndCursor, codeHitTest, msg, ::DefWindowProc);
}

void MainWnd::OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
  if (IsSplitterPoint(x, y))
  {
    m_splitterDrag = true;
    ::SetCapture(hWnd);
  }
}

void MainWnd::OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
  if (!m_splitterDrag)
    return;
  m_treeWidth = x - SPLITTER_W / 2;
  LayoutChildren();
}

void MainWnd::OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
  if (m_splitterDrag)
  {
    m_splitterDrag = false;
    ::ReleaseCapture();
    SaveSettings();
  }
}

void MainWnd::OnActivate(HWND hWnd, UINT nState, HWND hWndActDeact, bool fMinimized)
{
  if (nState != WA_INACTIVE && !fMinimized)
    ::SetFocus(m_wndDiff);
}

void MainWnd::OnDropFiles(HWND hWnd, HDROP hDrop)
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
  ::SetForegroundWindow(m_hWnd);

  if (paths.size() >= 2)
  {
    OpenPaths(paths[0], paths[1]);
  }
  else if (paths.size() == 1)
  {
    std::error_code ec;
    CompareDlg dlg(std::filesystem::is_directory(paths[0], ec));
    dlg.m_left = paths[0];
    if (dlg.DoModal(m_hWnd) == IDOK)
    {
      if (dlg.m_folders)
        CompareFolders(dlg.m_left, dlg.m_right);
      else
        CompareFiles(dlg.m_left, dlg.m_right);
    }
  }
}
