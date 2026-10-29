#pragma once

#include "Window.h"

namespace Yaswl
{
  class ToolBar : public Window
  {
  public:
    explicit ToolBar(bool bAutoDestroyImageList = true) : m_bAutoDestroyImageList(bAutoDestroyImageList)
    {
    }

    ~ToolBar()
    {
      if (m_bAutoDestroyImageList)
      {
        HIMAGELIST hImageList = GetImageList();
        if (hImageList)
          ::ImageList_Destroy(hImageList);
      }
    }

  public:
    bool Create(HWND hWnd, int id, DWORD dwStyle = WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_WRAPABLE)
    {
      m_hWnd = ::CreateWindowEx(0, TOOLBARCLASSNAME, nullptr, dwStyle, 0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(static_cast<long long>(id)), GetApp()->GetInstance(), nullptr);
      return m_hWnd != nullptr;
    }

    HIMAGELIST SetImageList(HIMAGELIST hImageList)
    {
      return (HIMAGELIST)::SendMessage(m_hWnd, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
    }

    HIMAGELIST GetImageList()
    {
      return (HIMAGELIST)::SendMessage(m_hWnd, TB_GETIMAGELIST, 0, 0);
    }

    bool Init(LPTBBUTTON pButtons, int nSize, HIMAGELIST hImageList = nullptr)
    {
      if (hImageList)
      {
        hImageList = SetImageList(hImageList);
        if (hImageList && m_bAutoDestroyImageList)
          ::ImageList_Destroy(hImageList);
      }
      else
      {
        hImageList = GetImageList();
        if (!hImageList)
        {
          hImageList = ::ImageList_Create(TOOLBAR_BUTTON_CX, TOOLBAR_BUTTON_CY, ILC_COLOR32 | ILC_MASK, 9, 0);
          SetImageList(hImageList);
        }
      }

      hImageList = GetImageList();
      if (!hImageList)
        return false;

      for (int i = 0, p = 0; i < nSize; i++)
      {
        if (pButtons[i].iBitmap)
        {
          ::ImageList_AddIcon(hImageList, ::LoadIcon(GetApp()->GetInstance(), MAKEINTRESOURCE(pButtons[i].iBitmap)));
          pButtons[i].iBitmap = MAKELONG(p++, 0);
        }
      }

      ::SendMessage(m_hWnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
      ::SendMessage(m_hWnd, TB_ADDBUTTONS, nSize, (LPARAM)pButtons);
      ::SendMessage(m_hWnd, TB_AUTOSIZE, 0, 0);

      return true;
    }

  protected:
    inline static int TOOLBAR_BUTTON_CX = 16;
    inline static int TOOLBAR_BUTTON_CY = 16;
    bool m_bAutoDestroyImageList;
  };
}

