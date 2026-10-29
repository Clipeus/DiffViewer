#include "pch.h"
#include "resource.h"
#include "DiffViewerApp.h"
#include "MainWnd.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  DiffViewerApp app;
  if (!app.Init(hInstance, lpCmdLine))
    return 1;

  MainWnd wnd;
  if (!wnd.Init() || !wnd.Create())
    return 1;

  wnd.RestoreWindowPlacement(nCmdShow);
  wnd.OpenFromCommandLine(lpCmdLine);
  return app.Run(&wnd);
}
