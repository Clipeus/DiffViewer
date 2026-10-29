#pragma once

#include "DiffEngine.h"

// Owner-drawn side-by-side diff control: two synchronized panes with line numbers,
// colored rows, intra-line highlights and an overview map on the right edge.
class DiffView : public Yaswl::WindowEx
{
public:
  static constexpr UINT WM_DV_BLOCKCHANGED = WM_APP + 10;   // sent to the parent, wParam = current block index

  DiffView();
  ~DiffView();

  bool Init();
  bool Create(HWND hWndParent, int id);

  void SetDocument(std::shared_ptr<DiffDocument> doc);
  void SetPlaceholder(const std::wstring& text);
  const DiffDocument* GetDocument() const { return m_doc.get(); }

  int GetBlockCount() const { return m_doc ? static_cast<int>(m_doc->blocks.size()) : 0; }
  int GetCurrentBlock() const { return m_curBlock; }
  void GotoBlock(int index);
  void NextBlock();
  void PrevBlock();
  void FirstBlock();
  void LastBlock();

  bool HasSelection() const { return m_selSide >= 0; }
  void CopySelection();
  void SelectAll();

protected:
  void OnRegisterClass(WNDCLASSEX& wc) override;
  LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
  struct Metrics
  {
    int headerH = 0;
    int lineH = 0;
    int charW = 0;
    int textTop = 0;     // text baseline offset inside a row
    int gutterW = 0;
    int mapW = 0;
    int paneW = 0;       // width of one pane including its gutter
    int textCols = 0;    // visible columns per pane
    int clientW = 0;
    int clientH = 0;
  };

  bool OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
  void OnDestroy(HWND hWnd);
  void OnSize(HWND hWnd, UINT state, int cx, int cy);
  void OnPaint(HWND hWnd);
  void OnVScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos);
  void OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos);
  void OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys);
  void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
  void OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
  void OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags);
  void OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);

  void CreateFonts();
  void UpdateMetrics();
  void UpdateScrollBars();
  int RowCount() const { return m_doc ? static_cast<int>(m_doc->rows.size()) : 0; }
  int VisibleRows() const;
  void ScrollToRow(int row);
  void ScrollBy(int rows);
  void ScrollHorz(int col);
  void SetCurrentBlock(int index, bool scroll);

  void Draw(HDC hdc);
  void DrawHeader(HDC hdc);
  void DrawRowSide(HDC hdc, const DiffRow& row, int side, int y);
  void DrawMap(HDC hdc);
  void DrawPlaceholder(HDC hdc);

  int RowFromY(int y) const;
  int SideFromX(int x) const;
  bool IsMapPoint(int x, int y) const;
  int PaneX(int side) const { return side == 0 ? 0 : m_metrics.paneW + 1; }

private:
  std::shared_ptr<DiffDocument> m_doc;
  std::wstring m_placeholder;
  HFONT m_hFont = nullptr;
  HFONT m_hHeaderFont = nullptr;
  HBRUSH m_hFillerBrush = nullptr;
  Metrics m_metrics;
  int m_top = 0;          // first visible row
  int m_hs = 0;           // first visible column
  int m_maxCols = 0;
  int m_curBlock = -1;
  int m_selSide = -1;
  int m_selAnchor = -1;
  int m_selEnd = -1;
  bool m_selecting = false;
  int m_wheelRemainder = 0;
};
