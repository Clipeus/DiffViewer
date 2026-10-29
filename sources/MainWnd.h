#pragma once

#include "DiffView.h"
#include "CompareSession.h"

class MainWnd : public Yaswl::MainWindow, public Yaswl::OwnerDrawMenuImpl<MainWnd>
{
  IMPLEMENT_ODW(MainWnd, m_wndToolBar);

public:
  MainWnd();
  ~MainWnd();

  bool Init();
  bool Create();
  static const wchar_t* GetClassName();

  // Opens whatever the command line describes: two paths or a pull request URL.
  void OpenFromCommandLine(const std::wstring& cmdLine);
  bool CompareFiles(const std::wstring& left, const std::wstring& right);
  bool CompareFolders(const std::wstring& left, const std::wstring& right);
  bool OpenPullRequest(const std::wstring& url);
  bool OpenPaths(const std::wstring& left, const std::wstring& right);

protected:
  void OnRegisterClass(WNDCLASSEX& wc) override;
  LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
  bool OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
  void OnDestroy(HWND hWnd);
  void OnSize(HWND hWnd, UINT state, int cx, int cy);
  void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify);
  LRESULT OnNotify(HWND hWnd, int idFrom, NMHDR* pNmhdr);
  void OnMenuSelect(HWND hWnd, HMENU hMenu, int item, HMENU hMenuPopup, UINT flags);
  void OnContextMenu(HWND hWnd, HWND hwndContext, UINT x, UINT y);
  bool OnSetCursor(HWND hWnd, HWND hWndCursor, UINT codeHitTest, UINT msg);
  void OnActivate(HWND hWnd, UINT nState, HWND hWndActDeact, bool fMinimized);
  void OnInitMenuPopup(HWND hWnd, HMENU hMenu, UINT item, bool fSystemMenu);
  void OnDropFiles(HWND hWnd, HDROP hDrop);
  void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
  void OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
  void OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags);

  bool StartSession(std::unique_ptr<CompareSession> session);
  void Refresh();
  void PopulateTree();
  void AddTreeItems(HTREEITEM hParent, const CompareItem& item);
  const CompareItem* FirstInterestingItem(const CompareItem& node) const;
  void SelectItem(const CompareItem* item);
  void ShowItem(const CompareItem* item);
  void LayoutChildren();
  void UpdateTitle();
  void UpdateStatusBar();
  void UpdateToolBar();
  bool IsSplitterPoint(int x, int y) const;
  void LoadSettings();
  void SaveSettings();

private:
  Yaswl::ToolBar m_wndToolBar;
  Yaswl::Window m_wndTree;
  DiffView m_wndDiff;
  HWND m_hStatusBar = nullptr;
  HFONT m_hUiFont = nullptr;

  std::unique_ptr<CompareSession> m_session;
  const CompareItem* m_currentItem = nullptr;
  std::wstring m_currentRelPath;
  std::wstring m_encodings;     // "UTF-8  |  UTF-8" for the status bar

  int m_treeWidth = 320;
  bool m_splitterDrag = false;
  bool m_ignoreWhitespace = false;
  bool m_showIdentical = true;
  bool m_populating = false;
};
