#pragma once

#define IMPLEMENT_ODW(tclass, member) \
friend class Yaswl::OwnerDrawMenuImpl<tclass>; \
private: \
  HWND GetToolBarWnd() \
  {\
    return member;\
  }

namespace Yaswl
{
  template <class T>
  class OwnerDrawMenuImpl
  {
  protected:
    void OnInitMenuPopup(HWND hWnd, HMENU hMenu, UINT item, bool fSystemMenu)
    {
      if (!fSystemMenu)
      {
        HIMAGELIST hImageList = (HIMAGELIST)::SendMessage(static_cast<T*>(this)->GetToolBarWnd(), TB_GETIMAGELIST, 0, 0);
        if (hImageList)
        {
          int nCount = ::GetMenuItemCount(hMenu);
          for (int i = 0; i < nCount; i++)
          {
            MENUITEMINFO mii = { 0 };
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_FTYPE | MIIM_DATA;
            ::GetMenuItemInfo(hMenu, i, true, &mii);

            if (!mii.dwItemData)
            {
              mii.fMask = MIIM_FTYPE | MIIM_DATA;
              mii.fType |= MFT_OWNERDRAW;
              mii.dwItemData = reinterpret_cast<ULONG_PTR>(hMenu);
            }
            ::SetMenuItemInfo(hMenu, i, true, &mii);
          }
        }
      }
    }

    void OnMeasureItem(HWND hWnd, MEASUREITEMSTRUCT* lpMeasureItem)
    {
      if (lpMeasureItem->CtlType == ODT_MENU)
      {
        std::wstring srtCaption(255, 0);
        int nSize = ::GetMenuString(reinterpret_cast<HMENU>(lpMeasureItem->itemData), lpMeasureItem->itemID, &srtCaption.front(), static_cast<int>(srtCaption.size()), MF_BYCOMMAND);
        if (nSize)
          srtCaption.resize(nSize);
        else
          srtCaption = L"-";

        HDC hDC = ::GetWindowDC(static_cast<T*>(this)->GetHandle());

        SIZE Size;
        ::GetTextExtentPoint32(hDC, srtCaption.c_str(), static_cast<int>(srtCaption.size()), &Size);

        ::ReleaseDC(static_cast<T*>(this)->GetHandle(), hDC);

        if (srtCaption != L"-")
          lpMeasureItem->itemHeight = (Size.cy < MENU_PIC_PLACE_CY ? MENU_PIC_PLACE_CY : Size.cy) + MENU_PIC_INDENT_CY;
        else
          lpMeasureItem->itemHeight = Size.cy / 2;

        lpMeasureItem->itemWidth = Size.cx + MENU_PIC_PLACE_CX + MENU_PIC_INDENT_RIGH + MENU_PIC_INDENT_LEFT;
      }
    }
    
    void OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* lpDrawItem)
    {
      if (lpDrawItem->CtlType == ODT_MENU)
      {
        std::wstring srtCaption(255, 0);
        int nSize = ::GetMenuString(reinterpret_cast<HMENU>(lpDrawItem->itemData), lpDrawItem->itemID, &srtCaption.front(), static_cast<int>(srtCaption.size()), MF_BYCOMMAND);
        if (nSize)
          srtCaption.resize(nSize);
        else
          srtCaption.clear();

        int nImageIndex = -1;
        HIMAGELIST hImageList = (HIMAGELIST)::SendMessage(static_cast<T*>(this)->GetToolBarWnd(), TB_GETIMAGELIST, 0, 0);
        if (hImageList && !srtCaption.empty())
        {
          TBBUTTONINFO tbi = { 0 };
          tbi.cbSize = sizeof(tbi);
          tbi.dwMask = TBIF_IMAGE;
          if (::SendMessage(static_cast<T*>(this)->GetToolBarWnd(), TB_GETBUTTONINFO, lpDrawItem->itemID, (LPARAM)&tbi) != -1)
            nImageIndex = tbi.iImage;
        }

        RECT Rect;
        ::CopyRect(&Rect, &lpDrawItem->rcItem);

        if (!srtCaption.empty())
        {
          std::wstring strShortKey;
          size_t pos = srtCaption.find(L'\t');
          if (std::wstring::npos != pos)
          {
            strShortKey = srtCaption.substr(pos + 1);
            srtCaption = srtCaption.substr(0, pos);
          }

          if ((lpDrawItem->itemState & ODS_GRAYED) && (lpDrawItem->itemState & ODS_SELECTED))
          {
            ::FillRect(lpDrawItem->hDC, &Rect, ::GetSysColorBrush(COLOR_3DLIGHT));
          }
          else if (lpDrawItem->itemState & ODS_SELECTED)
          {
            ::FillRect(lpDrawItem->hDC, &Rect, ::GetSysColorBrush(COLOR_MENUHILIGHT));
          }
          else
          {
            ::FillRect(lpDrawItem->hDC, &Rect, ::GetSysColorBrush(COLOR_MENUBAR));
          }

          Rect.left += MENU_PIC_PLACE_CX + MENU_PIC_INDENT_LEFT;
          Rect.right -= MENU_PIC_PLACE_CX;
          ::OffsetRect(&Rect, MENU_PIC_INDENT_RIGH, 0);

          ::SetBkMode(lpDrawItem->hDC, TRANSPARENT);

          if (lpDrawItem->itemState & ODS_GRAYED)
          {
            if (!(lpDrawItem->itemState & ODS_SELECTED))
            {
              ::SetTextColor(lpDrawItem->hDC, ::GetSysColor(COLOR_HIGHLIGHTTEXT));
              ::DrawText(lpDrawItem->hDC, srtCaption.c_str(), static_cast<int>(srtCaption.size()), &Rect, DT_LEFT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
              if (!strShortKey.empty())
                ::DrawText(lpDrawItem->hDC, strShortKey.c_str(), static_cast<int>(strShortKey.size()), &Rect, DT_RIGHT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
            }

            ::SetTextColor(lpDrawItem->hDC, ::GetSysColor(COLOR_GRAYTEXT));
            ::OffsetRect(&Rect, -1, -1);
            ::DrawText(lpDrawItem->hDC, srtCaption.c_str(), static_cast<int>(srtCaption.size()), &Rect, DT_LEFT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
            if (!strShortKey.empty())
              ::DrawText(lpDrawItem->hDC, strShortKey.c_str(), static_cast<int>(strShortKey.size()), &Rect, DT_RIGHT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
          }
          else
          {
            if (lpDrawItem->itemState & ODS_SELECTED)
              ::SetTextColor(lpDrawItem->hDC, ::GetSysColor(COLOR_HIGHLIGHTTEXT));

            ::DrawText(lpDrawItem->hDC, srtCaption.c_str(), static_cast<int>(srtCaption.size()), &Rect, DT_LEFT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
            if (!strShortKey.empty())
              ::DrawText(lpDrawItem->hDC, strShortKey.c_str(), static_cast<int>(strShortKey.size()), &Rect, DT_RIGHT | DT_VCENTER | DT_EXPANDTABS | DT_SINGLELINE);
          }

          if (hImageList && nImageIndex != -1)
          {
            ::ImageList_Draw(hImageList, nImageIndex, lpDrawItem->hDC,
              lpDrawItem->rcItem.left + (MENU_PIC_PLACE_CX - MENU_PIC_CX) / 2 + MENU_PIC_INDENT_LEFT,
              lpDrawItem->rcItem.top + (MENU_PIC_PLACE_CY - MENU_PIC_CY) / 2 + MENU_PIC_INDENT_CY / 2,
              ILD_NORMAL /*| (lpDrawItem->itemState & ODS_GRAYED ? ILD_OVERLAYMASK : 0)*/);

            if (lpDrawItem->itemState & ODS_SELECTED)
            {
              RECT Rect;
              ::CopyRect(&Rect, &lpDrawItem->rcItem);
              Rect.right = Rect.left + MENU_PIC_PLACE_CX;
              Rect.bottom = Rect.top + MENU_PIC_PLACE_CY;
            }
          }
        }
        else
        {
          ::FillRect(lpDrawItem->hDC, &Rect, ::GetSysColorBrush(COLOR_MENU));
          Rect.bottom -= 3;
          ::DrawEdge(lpDrawItem->hDC, &Rect, EDGE_ETCHED, BF_BOTTOM);
        }
      }
    }

  protected:
    inline static int MENU_PIC_CX = 16;
    inline static int MENU_PIC_CY = 16;
    inline static int MENU_PIC_PLACE_CX = MENU_PIC_CX + (MENU_PIC_CX * 25 / 100); // + 25%
    inline static int MENU_PIC_PLACE_CY = MENU_PIC_CY + (MENU_PIC_CY * 25 / 100); // + 25%
    inline static int MENU_PIC_INDENT_LEFT = 4;
    inline static int MENU_PIC_INDENT_RIGH = MENU_PIC_CX - 2;
    inline static int MENU_PIC_INDENT_CY = 4;
  };
}

