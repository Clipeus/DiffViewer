#include "pch.h"
#include "DiffView.h"

using namespace Yaswl;

namespace
{
  constexpr int MAP_W = 14;
  constexpr int GUTTER_PAD = 6;
  constexpr int MARKER_W = 3;
  constexpr int TAB_SIZE = 4;

  const COLORREF CLR_ADDED_BG   = RGB(221, 255, 221);
  const COLORREF CLR_ADDED_HL   = RGB(150, 230, 150);
  const COLORREF CLR_REMOVED_BG = RGB(255, 222, 222);
  const COLORREF CLR_REMOVED_HL = RGB(255, 165, 165);
  const COLORREF CLR_CHANGED_BG = RGB(255, 250, 205);
  const COLORREF CLR_CHANGED_HL = RGB(255, 220, 110);
  const COLORREF CLR_FILLER_BG  = RGB(238, 238, 238);
  const COLORREF CLR_FILLER_FG  = RGB(205, 205, 205);
  const COLORREF CLR_GUTTER_BG  = RGB(246, 246, 246);
  const COLORREF CLR_GUTTER_FG  = RGB(130, 130, 130);
  const COLORREF CLR_ACCENT     = RGB(0, 120, 215);
  const COLORREF CLR_MAP_BG     = RGB(240, 240, 240);
  const COLORREF CLR_SEPARATOR  = RGB(200, 200, 200);

  COLORREF Blend(COLORREF a, COLORREF b, int percentB)
  {
    int pa = 100 - percentB;
    return RGB((GetRValue(a) * pa + GetRValue(b) * percentB) / 100,
               (GetGValue(a) * pa + GetGValue(b) * percentB) / 100,
               (GetBValue(a) * pa + GetBValue(b) * percentB) / 100);
  }

  void FillSolid(HDC hdc, const RECT& rc, COLORREF color)
  {
    COLORREF old = ::SetBkColor(hdc, color);
    ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rc, L"", 0, nullptr);
    ::SetBkColor(hdc, old);
  }

  COLORREF BackgroundFor(const DiffRow& row)
  {
    switch (row.kind)
    {
    case RowKind::Added:   return CLR_ADDED_BG;
    case RowKind::Removed: return CLR_REMOVED_BG;
    case RowKind::Changed: return CLR_CHANGED_BG;
    default:               return ::GetSysColor(COLOR_WINDOW);
    }
  }

  // Shortens text from the start ("...tail") so that the end of a path stays visible.
  std::wstring FitTail(HDC hdc, const std::wstring& text, int width)
  {
    SIZE sz = {};
    ::GetTextExtentPoint32(hdc, text.c_str(), static_cast<int>(text.size()), &sz);
    if (sz.cx <= width || text.empty())
      return text;

    const std::wstring ellipsis = L"...";
    size_t lo = 0, hi = text.size();
    while (lo < hi)
    {
      size_t mid = (lo + hi) / 2;
      std::wstring probe = ellipsis + text.substr(mid);
      ::GetTextExtentPoint32(hdc, probe.c_str(), static_cast<int>(probe.size()), &sz);
      if (sz.cx <= width)
        hi = mid;
      else
        lo = mid + 1;
    }
    return ellipsis + text.substr(lo);
  }

  COLORREF HighlightFor(RowKind kind)
  {
    switch (kind)
    {
    case RowKind::Added:   return CLR_ADDED_HL;
    case RowKind::Removed: return CLR_REMOVED_HL;
    default:               return CLR_CHANGED_HL;
    }
  }
}

DiffView::DiffView() : WindowEx(L"DiffViewer_DiffView_1.0")
{
}

DiffView::~DiffView()
{
}

bool DiffView::Init()
{
  return RegisterClass();
}

void DiffView::OnRegisterClass(WNDCLASSEX& wc)
{
  wc.style = CS_DBLCLKS;
  wc.hbrBackground = nullptr;
}

bool DiffView::Create(HWND hWndParent, int id)
{
  return WindowEx::Create(0, GetClassName(), L"",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS | WS_TABSTOP,
    0, 0, 0, 0, hWndParent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)));
}

// ─── public API ──────────────────────────────────────────────────────────────

void DiffView::SetDocument(std::shared_ptr<DiffDocument> doc)
{
  m_doc = std::move(doc);
  m_placeholder.clear();
  m_top = 0;
  m_hs = 0;
  m_curBlock = -1;
  m_selSide = -1;
  m_selAnchor = m_selEnd = -1;

  m_maxCols = 0;
  if (m_doc)
  {
    auto measure = [&](const std::vector<std::wstring>& lines)
    {
      for (const auto& s : lines)
      {
        int col = 0;
        for (wchar_t c : s)
          col += (c == L'\t') ? (TAB_SIZE - col % TAB_SIZE) : 1;
        m_maxCols = std::max(m_maxCols, col);
      }
    };
    measure(m_doc->leftLines);
    measure(m_doc->rightLines);
  }

  UpdateMetrics();
  UpdateScrollBars();

  if (m_doc && !m_doc->blocks.empty())
    SetCurrentBlock(0, true);
  else
    ::SendMessage(::GetParent(m_hWnd), WM_DV_BLOCKCHANGED, static_cast<WPARAM>(-1), 0);

  ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void DiffView::SetPlaceholder(const std::wstring& text)
{
  SetDocument(nullptr);
  m_placeholder = text;
  ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void DiffView::GotoBlock(int index)
{
  if (!m_doc || index < 0 || index >= static_cast<int>(m_doc->blocks.size()))
    return;
  SetCurrentBlock(index, true);
}

void DiffView::NextBlock()
{
  if (GetBlockCount() == 0)
    return;
  int next = m_curBlock + 1;
  if (next >= GetBlockCount())
    next = GetBlockCount() - 1;
  SetCurrentBlock(next, true);
}

void DiffView::PrevBlock()
{
  if (GetBlockCount() == 0)
    return;
  int prev = m_curBlock - 1;
  if (prev < 0)
    prev = 0;
  SetCurrentBlock(prev, true);
}

void DiffView::FirstBlock()
{
  if (GetBlockCount() > 0)
    SetCurrentBlock(0, true);
}

void DiffView::LastBlock()
{
  if (GetBlockCount() > 0)
    SetCurrentBlock(GetBlockCount() - 1, true);
}

void DiffView::SetCurrentBlock(int index, bool scroll)
{
  if (!m_doc || index < 0 || index >= static_cast<int>(m_doc->blocks.size()))
    return;

  m_curBlock = index;
  if (scroll)
  {
    const DiffBlock& b = m_doc->blocks[index];
    int visible = VisibleRows();
    int blockRows = b.lastRow - b.firstRow + 1;
    int target;
    if (blockRows >= visible - 2)
      target = b.firstRow - 1;
    else
      target = b.firstRow - std::max(1, (visible - blockRows) / 3);
    ScrollToRow(target);
  }
  ::InvalidateRect(m_hWnd, nullptr, FALSE);
  ::SendMessage(::GetParent(m_hWnd), WM_DV_BLOCKCHANGED, static_cast<WPARAM>(m_curBlock), 0);
}

void DiffView::SelectAll()
{
  if (!m_doc || m_doc->rows.empty())
    return;
  if (m_selSide < 0)
    m_selSide = 0;
  m_selAnchor = 0;
  m_selEnd = RowCount() - 1;
  ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void DiffView::CopySelection()
{
  if (!m_doc || m_selSide < 0 || m_selAnchor < 0)
    return;

  int from = std::min(m_selAnchor, m_selEnd);
  int to = std::max(m_selAnchor, m_selEnd);
  const auto& lines = (m_selSide == 0) ? m_doc->leftLines : m_doc->rightLines;

  std::wstring text;
  for (int r = from; r <= to && r < RowCount(); ++r)
  {
    const DiffRow& row = m_doc->rows[r];
    int idx = (m_selSide == 0) ? row.left : row.right;
    if (idx < 0)
      continue;
    text += lines[idx];
    text += L"\r\n";
  }

  if (!::OpenClipboard(m_hWnd))
    return;
  ::EmptyClipboard();
  size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (hMem)
  {
    void* p = ::GlobalLock(hMem);
    if (p)
    {
      memcpy(p, text.c_str(), bytes);
      ::GlobalUnlock(hMem);
      ::SetClipboardData(CF_UNICODETEXT, hMem);
    }
    else
    {
      ::GlobalFree(hMem);
    }
  }
  ::CloseClipboard();
}

// ─── window procedure ────────────────────────────────────────────────────────

LRESULT DiffView::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    HANDLE_MSG(hWnd, WM_CREATE,      OnCreate);
    HANDLE_MSG(hWnd, WM_DESTROY,     OnDestroy);
    HANDLE_MSG(hWnd, WM_SIZE,        OnSize);
    HANDLE_MSG(hWnd, WM_PAINT,       OnPaint);
    HANDLE_MSG(hWnd, WM_VSCROLL,     OnVScroll);
    HANDLE_MSG(hWnd, WM_HSCROLL,     OnHScroll);
    HANDLE_MSG(hWnd, WM_MOUSEWHEEL,  OnMouseWheel);
    HANDLE_MSG(hWnd, WM_LBUTTONDOWN, OnLButtonDown);
    HANDLE_MSG(hWnd, WM_LBUTTONUP,   OnLButtonUp);
    HANDLE_MSG(hWnd, WM_MOUSEMOVE,   OnMouseMove);
    HANDLE_MSG(hWnd, WM_KEYDOWN,     OnKey);

  case WM_MOUSEHWHEEL:
    ScrollHorz(m_hs + GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 8);
    return 0;

  case WM_ERASEBKGND:
    return 1;

  case WM_GETDLGCODE:
    return DLGC_WANTARROWS;

  case WM_SETFOCUS:
  case WM_KILLFOCUS:
    ::InvalidateRect(hWnd, nullptr, FALSE);
    return 0;

  case WM_CONTEXTMENU:
    return ::SendMessage(::GetParent(hWnd), WM_CONTEXTMENU, wParam, lParam);

  case WM_SETTINGCHANGE:
    CreateFonts();
    UpdateMetrics();
    UpdateScrollBars();
    ::InvalidateRect(hWnd, nullptr, FALSE);
    return 0;

  default:
    return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
}

bool DiffView::OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
  m_hWnd = hWnd;
  CreateFonts();
  m_hFillerBrush = ::CreateHatchBrush(HS_BDIAGONAL, CLR_FILLER_FG);
  UpdateMetrics();
  return true;
}

void DiffView::OnDestroy(HWND hWnd)
{
  if (m_hFont) { ::DeleteObject(m_hFont); m_hFont = nullptr; }
  if (m_hHeaderFont) { ::DeleteObject(m_hHeaderFont); m_hHeaderFont = nullptr; }
  if (m_hFillerBrush) { ::DeleteObject(m_hFillerBrush); m_hFillerBrush = nullptr; }
}

void DiffView::CreateFonts()
{
  if (m_hFont) ::DeleteObject(m_hFont);
  if (m_hHeaderFont) ::DeleteObject(m_hHeaderFont);

  HDC hdc = ::GetDC(m_hWnd);
  int dpi = ::GetDeviceCaps(hdc, LOGPIXELSY);
  ::ReleaseDC(m_hWnd, hdc);

  LOGFONT lf = {};
  lf.lfHeight = -::MulDiv(10, dpi, 72);
  lf.lfWeight = FW_NORMAL;
  lf.lfCharSet = DEFAULT_CHARSET;
  lf.lfQuality = CLEARTYPE_QUALITY;
  lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
  ::wcscpy_s(lf.lfFaceName, L"Consolas");
  m_hFont = ::CreateFontIndirect(&lf);

  NONCLIENTMETRICS ncm = {};
  ncm.cbSize = sizeof(ncm);
  ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  m_hHeaderFont = ::CreateFontIndirect(&ncm.lfMessageFont);
}

void DiffView::UpdateMetrics()
{
  RECT rc;
  ::GetClientRect(m_hWnd, &rc);
  m_metrics.clientW = rc.right;
  m_metrics.clientH = rc.bottom;

  HDC hdc = ::GetDC(m_hWnd);
  HFONT old = SelectFont(hdc, m_hFont);
  TEXTMETRIC tm = {};
  ::GetTextMetrics(hdc, &tm);
  m_metrics.charW = std::max<int>(1, tm.tmAveCharWidth);
  m_metrics.lineH = tm.tmHeight + tm.tmExternalLeading + 2;
  m_metrics.textTop = 1;

  SelectFont(hdc, m_hHeaderFont);
  ::GetTextMetrics(hdc, &tm);
  m_metrics.headerH = tm.tmHeight + 8;
  SelectFont(hdc, old);
  ::ReleaseDC(m_hWnd, hdc);

  int digits = 1;
  if (m_doc)
  {
    size_t maxLines = std::max(m_doc->leftLines.size(), m_doc->rightLines.size());
    digits = static_cast<int>(std::to_wstring(std::max<size_t>(1, maxLines)).size());
  }
  digits = std::max(digits, 3);

  m_metrics.mapW = MAP_W;
  m_metrics.gutterW = MARKER_W + GUTTER_PAD + digits * m_metrics.charW + GUTTER_PAD;
  m_metrics.paneW = std::max(0, (m_metrics.clientW - m_metrics.mapW - 1) / 2);
  m_metrics.textCols = std::max(1, (m_metrics.paneW - m_metrics.gutterW) / m_metrics.charW);
}

int DiffView::VisibleRows() const
{
  if (m_metrics.lineH <= 0)
    return 1;
  return std::max(1, (m_metrics.clientH - m_metrics.headerH) / m_metrics.lineH);
}

void DiffView::UpdateScrollBars()
{
  int rows = RowCount();
  int visible = VisibleRows();
  m_top = std::max(0, std::min(m_top, rows - visible));
  m_hs = std::max(0, std::min(m_hs, m_maxCols - m_metrics.textCols));

  SCROLLINFO si = {};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = rows > 0 ? rows - 1 : 0;
  si.nPage = visible;
  si.nPos = m_top;
  ::SetScrollInfo(m_hWnd, SB_VERT, &si, TRUE);

  si.nMax = m_maxCols > 0 ? m_maxCols - 1 : 0;
  si.nPage = m_metrics.textCols;
  si.nPos = m_hs;
  ::SetScrollInfo(m_hWnd, SB_HORZ, &si, TRUE);
}

void DiffView::ScrollToRow(int row)
{
  int rows = RowCount();
  int visible = VisibleRows();
  row = std::max(0, std::min(row, rows - visible));
  if (row == m_top)
    return;
  m_top = row;
  UpdateScrollBars();
  ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void DiffView::ScrollBy(int rows)
{
  ScrollToRow(m_top + rows);
}

void DiffView::ScrollHorz(int col)
{
  col = std::max(0, std::min(col, m_maxCols - m_metrics.textCols));
  if (col == m_hs)
    return;
  m_hs = col;
  UpdateScrollBars();
  ::InvalidateRect(m_hWnd, nullptr, FALSE);
}

void DiffView::OnSize(HWND hWnd, UINT state, int cx, int cy)
{
  UpdateMetrics();
  UpdateScrollBars();
  ::InvalidateRect(hWnd, nullptr, FALSE);
}

void DiffView::OnVScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos)
{
  int visible = VisibleRows();
  switch (code)
  {
  case SB_LINEUP:   ScrollBy(-1); break;
  case SB_LINEDOWN: ScrollBy(1); break;
  case SB_PAGEUP:   ScrollBy(-visible); break;
  case SB_PAGEDOWN: ScrollBy(visible); break;
  case SB_TOP:      ScrollToRow(0); break;
  case SB_BOTTOM:   ScrollToRow(RowCount()); break;
  case SB_THUMBTRACK:
  case SB_THUMBPOSITION:
  {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_TRACKPOS;
    ::GetScrollInfo(hWnd, SB_VERT, &si);
    ScrollToRow(si.nTrackPos);
    break;
  }
  }
}

void DiffView::OnHScroll(HWND hWnd, HWND hWndCtl, UINT code, int pos)
{
  switch (code)
  {
  case SB_LINELEFT:  ScrollHorz(m_hs - 4); break;
  case SB_LINERIGHT: ScrollHorz(m_hs + 4); break;
  case SB_PAGELEFT:  ScrollHorz(m_hs - m_metrics.textCols); break;
  case SB_PAGERIGHT: ScrollHorz(m_hs + m_metrics.textCols); break;
  case SB_LEFT:      ScrollHorz(0); break;
  case SB_RIGHT:     ScrollHorz(m_maxCols); break;
  case SB_THUMBTRACK:
  case SB_THUMBPOSITION:
  {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_TRACKPOS;
    ::GetScrollInfo(hWnd, SB_HORZ, &si);
    ScrollHorz(si.nTrackPos);
    break;
  }
  }
}

void DiffView::OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
  if (fwKeys & MK_SHIFT)
  {
    ScrollHorz(m_hs - zDelta / WHEEL_DELTA * 8);
    return;
  }

  UINT linesPerNotch = 3;
  ::SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &linesPerNotch, 0);
  if (linesPerNotch == WHEEL_PAGESCROLL)
    linesPerNotch = VisibleRows();

  m_wheelRemainder += zDelta;
  int notches = m_wheelRemainder / WHEEL_DELTA;
  m_wheelRemainder -= notches * WHEEL_DELTA;
  if (notches != 0)
    ScrollBy(-notches * static_cast<int>(linesPerNotch));
}

int DiffView::RowFromY(int y) const
{
  if (y < m_metrics.headerH || m_metrics.lineH <= 0)
    return -1;
  int row = m_top + (y - m_metrics.headerH) / m_metrics.lineH;
  if (row >= RowCount())
    return -1;
  return row;
}

int DiffView::SideFromX(int x) const
{
  if (x < m_metrics.paneW)
    return 0;
  if (x < m_metrics.paneW + 1 + m_metrics.paneW)
    return 1;
  return -1;
}

bool DiffView::IsMapPoint(int x, int y) const
{
  return x >= m_metrics.clientW - m_metrics.mapW && y >= m_metrics.headerH;
}

void DiffView::OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
  ::SetFocus(hWnd);
  if (!m_doc)
    return;

  if (IsMapPoint(x, y))
  {
    int mapH = std::max(1, m_metrics.clientH - m_metrics.headerH);
    int row = static_cast<int>(static_cast<long long>(y - m_metrics.headerH) * RowCount() / mapH);
    // jump to the block under the cursor if there is one
    for (size_t i = 0; i < m_doc->blocks.size(); ++i)
    {
      const DiffBlock& b = m_doc->blocks[i];
      int y0 = m_metrics.headerH + static_cast<int>(static_cast<long long>(b.firstRow) * mapH / std::max(1, RowCount()));
      int y1 = m_metrics.headerH + static_cast<int>(static_cast<long long>(b.lastRow + 1) * mapH / std::max(1, RowCount()));
      if (y >= y0 - 2 && y <= std::max(y1, y0 + 2) + 2)
      {
        SetCurrentBlock(static_cast<int>(i), true);
        return;
      }
    }
    ScrollToRow(row - VisibleRows() / 2);
    return;
  }

  int side = SideFromX(x);
  int row = RowFromY(y);
  if (side < 0 || row < 0)
    return;

  m_selSide = side;
  if ((keyFlags & MK_SHIFT) && m_selAnchor >= 0)
  {
    m_selEnd = row;
  }
  else
  {
    m_selAnchor = row;
    m_selEnd = row;
  }
  m_selecting = true;
  ::SetCapture(hWnd);

  if (m_doc->rows[row].block >= 0 && m_doc->rows[row].block != m_curBlock)
    SetCurrentBlock(m_doc->rows[row].block, false);

  ::InvalidateRect(hWnd, nullptr, FALSE);
}

void DiffView::OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
  if (!m_selecting || !m_doc)
    return;

  if (y < m_metrics.headerH)
    ScrollBy(-1);
  else if (y >= m_metrics.clientH)
    ScrollBy(1);

  int row = RowFromY(std::max(y, m_metrics.headerH));
  if (row < 0)
    row = std::min(RowCount() - 1, m_top + VisibleRows());
  if (row != m_selEnd)
  {
    m_selEnd = row;
    ::InvalidateRect(hWnd, nullptr, FALSE);
  }
}

void DiffView::OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
  if (m_selecting)
  {
    m_selecting = false;
    ::ReleaseCapture();
  }
}

void DiffView::OnKey(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
  bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
  switch (vk)
  {
  case VK_UP:    ScrollBy(-1); break;
  case VK_DOWN:  ScrollBy(1); break;
  case VK_PRIOR: ScrollBy(-VisibleRows()); break;
  case VK_NEXT:  ScrollBy(VisibleRows()); break;
  case VK_HOME:  if (ctrl) ScrollToRow(0); else ScrollHorz(0); break;
  case VK_END:   if (ctrl) ScrollToRow(RowCount()); else ScrollHorz(m_maxCols); break;
  case VK_LEFT:  ScrollHorz(m_hs - 4); break;
  case VK_RIGHT: ScrollHorz(m_hs + 4); break;
  case VK_ESCAPE:
    m_selSide = -1;
    m_selAnchor = m_selEnd = -1;
    ::InvalidateRect(hWnd, nullptr, FALSE);
    break;
  }
}

// ─── painting ────────────────────────────────────────────────────────────────

void DiffView::OnPaint(HWND hWnd)
{
  PAINTSTRUCT ps;
  HDC hdc = ::BeginPaint(hWnd, &ps);

  RECT rc;
  ::GetClientRect(hWnd, &rc);
  if (rc.right > 0 && rc.bottom > 0)
  {
    HDC mem = ::CreateCompatibleDC(hdc);
    HBITMAP bmp = ::CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBmp = SelectBitmap(mem, bmp);

    Draw(mem);

    ::BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectBitmap(mem, oldBmp);
    ::DeleteObject(bmp);
    ::DeleteDC(mem);
  }

  ::EndPaint(hWnd, &ps);
}

void DiffView::Draw(HDC hdc)
{
  const Metrics& m = m_metrics;
  RECT all = { 0, 0, m.clientW, m.clientH };
  FillSolid(hdc, all, ::GetSysColor(COLOR_WINDOW));

  if (!m_doc)
  {
    DrawPlaceholder(hdc);
    return;
  }

  HFONT oldFont = SelectFont(hdc, m_hFont);
  ::SetBkMode(hdc, TRANSPARENT);

  int visible = VisibleRows() + 1;
  int rows = RowCount();
  for (int i = 0; i < visible; ++i)
  {
    int r = m_top + i;
    if (r >= rows)
      break;
    int y = m.headerH + i * m.lineH;
    DrawRowSide(hdc, m_doc->rows[r], 0, y);
    DrawRowSide(hdc, m_doc->rows[r], 1, y);
  }

  // separators
  RECT sep = { m.paneW, m.headerH, m.paneW + 1, m.clientH };
  FillSolid(hdc, sep, CLR_SEPARATOR);
  RECT sep2 = { m.clientW - m.mapW - 1, m.headerH, m.clientW - m.mapW, m.clientH };
  FillSolid(hdc, sep2, CLR_SEPARATOR);

  DrawMap(hdc);
  DrawHeader(hdc);

  SelectFont(hdc, oldFont);
}

void DiffView::DrawPlaceholder(HDC hdc)
{
  if (m_placeholder.empty())
    return;
  HFONT oldFont = SelectFont(hdc, m_hHeaderFont);
  ::SetBkMode(hdc, TRANSPARENT);
  ::SetTextColor(hdc, ::GetSysColor(COLOR_GRAYTEXT));
  RECT rc = { 24, 0, m_metrics.clientW - 24, m_metrics.clientH };
  ::DrawText(hdc, m_placeholder.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
  SelectFont(hdc, oldFont);
}

void DiffView::DrawHeader(HDC hdc)
{
  const Metrics& m = m_metrics;
  RECT rc = { 0, 0, m.clientW, m.headerH };
  FillSolid(hdc, rc, ::GetSysColor(COLOR_BTNFACE));
  RECT line = { 0, m.headerH - 1, m.clientW, m.headerH };
  FillSolid(hdc, line, CLR_SEPARATOR);

  HFONT oldFont = SelectFont(hdc, m_hHeaderFont);
  ::SetBkMode(hdc, TRANSPARENT);
  ::SetTextColor(hdc, ::GetSysColor(COLOR_BTNTEXT));

  for (int side = 0; side < 2; ++side)
  {
    const std::wstring& title = side == 0 ? m_doc->leftTitle : m_doc->rightTitle;
    const std::wstring& info = side == 0 ? m_doc->leftInfo : m_doc->rightInfo;

    RECT r = { PaneX(side) + 6, 0, PaneX(side) + m.paneW - 6, m.headerH - 1 };
    if (!info.empty())
    {
      SIZE sz;
      ::GetTextExtentPoint32(hdc, info.c_str(), static_cast<int>(info.size()), &sz);
      RECT ri = { r.right - sz.cx, 0, r.right, m.headerH - 1 };
      ::SetTextColor(hdc, ::GetSysColor(COLOR_GRAYTEXT));
      ::DrawText(hdc, info.c_str(), -1, &ri, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
      ::SetTextColor(hdc, ::GetSysColor(COLOR_BTNTEXT));
      r.right = ri.left - 12;
    }
    std::wstring path = FitTail(hdc, title, r.right - r.left);
    ::DrawText(hdc, path.c_str(), static_cast<int>(path.size()), &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }

  SelectFont(hdc, oldFont);
}

void DiffView::DrawRowSide(HDC hdc, const DiffRow& row, int side, int y)
{
  const Metrics& m = m_metrics;
  const int paneX = PaneX(side);
  const int idx = side == 0 ? row.left : row.right;
  const auto& lines = side == 0 ? m_doc->leftLines : m_doc->rightLines;
  const auto& ranges = side == 0 ? row.leftRanges : row.rightRanges;
  const int rowIndex = static_cast<int>(&row - m_doc->rows.data());

  RECT gutter = { paneX, y, paneX + m.gutterW, y + m.lineH };
  RECT text = { paneX + m.gutterW, y, paneX + m.paneW, y + m.lineH };

  FillSolid(hdc, gutter, CLR_GUTTER_BG);

  if (idx < 0)
  {
    // filler row
    ::SetBkColor(hdc, CLR_FILLER_BG);
    ::SetTextColor(hdc, CLR_FILLER_FG);
    ::SetBkMode(hdc, OPAQUE);
    ::FillRect(hdc, &text, m_hFillerBrush);
    ::SetBkMode(hdc, TRANSPARENT);
  }
  else
  {
    COLORREF bg = BackgroundFor(row);
    bool selected = (m_selSide == side && m_selAnchor >= 0 &&
      rowIndex >= std::min(m_selAnchor, m_selEnd) && rowIndex <= std::max(m_selAnchor, m_selEnd));
    if (selected)
      bg = Blend(bg, ::GetSysColor(COLOR_HIGHLIGHT), ::GetFocus() == m_hWnd ? 35 : 20);
    FillSolid(hdc, text, bg);

    std::vector<int> map;
    const std::wstring& line = lines[idx];
    std::wstring display = DiffEngine::ExpandTabs(line, ranges.empty() ? nullptr : &map, TAB_SIZE);

    // intra-line highlights
    if (!ranges.empty())
    {
      COLORREF hl = HighlightFor(row.kind);
      if (selected)
        hl = Blend(hl, ::GetSysColor(COLOR_HIGHLIGHT), 35);
      for (const TextRange& range : ranges)
      {
        int cs = map[std::min<size_t>(range.start, map.size() - 1)];
        int ce = map[std::min<size_t>(range.end, map.size() - 1)];
        int x0 = text.left + (cs - m_hs) * m.charW;
        int x1 = text.left + (ce - m_hs) * m.charW;
        x0 = std::max<int>(x0, text.left);
        x1 = std::min<int>(x1, text.right);
        if (x1 > x0)
        {
          RECT hr = { x0, y, x1, y + m.lineH };
          FillSolid(hdc, hr, hl);
        }
      }
    }

    // text
    if (static_cast<int>(display.size()) > m_hs)
    {
      ::SetTextColor(hdc, ::GetSysColor(COLOR_WINDOWTEXT));
      int count = std::min<int>(static_cast<int>(display.size()) - m_hs, m.textCols + 2);
      ::ExtTextOut(hdc, text.left, y + m.textTop, ETO_CLIPPED, &text, display.c_str() + m_hs, count, nullptr);
    }

    // line number
    std::wstring num = std::to_wstring(idx + 1);
    int nx = gutter.right - GUTTER_PAD - static_cast<int>(num.size()) * m.charW;
    ::SetTextColor(hdc, CLR_GUTTER_FG);
    ::ExtTextOut(hdc, nx, y + m.textTop, ETO_CLIPPED, &gutter, num.c_str(), static_cast<UINT>(num.size()), nullptr);
  }

  // current block marker
  if (row.block >= 0 && row.block == m_curBlock)
  {
    RECT marker = { paneX, y, paneX + MARKER_W, y + m.lineH };
    FillSolid(hdc, marker, CLR_ACCENT);
  }
}

void DiffView::DrawMap(HDC hdc)
{
  const Metrics& m = m_metrics;
  const int mapX = m.clientW - m.mapW;
  const int mapH = m.clientH - m.headerH;
  RECT rc = { mapX, m.headerH, m.clientW, m.clientH };
  FillSolid(hdc, rc, CLR_MAP_BG);

  const int rows = RowCount();
  if (rows <= 0 || mapH <= 0)
    return;

  auto yFor = [&](int row) { return m.headerH + static_cast<int>(static_cast<long long>(row) * mapH / rows); };

  for (size_t i = 0; i < m_doc->blocks.size(); ++i)
  {
    const DiffBlock& b = m_doc->blocks[i];
    int y0 = yFor(b.firstRow);
    int y1 = std::max(yFor(b.lastRow + 1), y0 + 2);
    RECT r = { mapX + 3, y0, m.clientW - 3, y1 };
    FillSolid(hdc, r, HighlightFor(b.kind));
    if (static_cast<int>(i) == m_curBlock)
    {
      RECT f = { mapX + 1, y0 - 1, m.clientW - 1, y1 + 1 };
      HBRUSH br = ::CreateSolidBrush(RGB(60, 60, 60));
      ::FrameRect(hdc, &f, br);
      ::DeleteObject(br);
    }
  }

  // viewport indicator
  int v0 = yFor(m_top);
  int v1 = std::max(yFor(std::min(rows, m_top + VisibleRows())), v0 + 2);
  RECT vp = { mapX + 1, v0, m.clientW - 1, v1 };
  HBRUSH br = ::CreateSolidBrush(CLR_ACCENT);
  ::FrameRect(hdc, &vp, br);
  ::DeleteObject(br);
}
