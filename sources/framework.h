#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows Header Files
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj_core.h>
#include <shellapi.h>
#include <comdef.h>
#include <commdlg.h>
#include <winhttp.h>
#include <uxtheme.h>
#include <crtdbg.h>

// C RunTime Header Files
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cwctype>

// Yaswl
#include <Misc.h>
#include <WinApp.h>
#include <Window.h>
#include <Dialog.h>
#include <MainWindow.h>
#include <Registry.h>
#include <OwnerDrawMenuImpl.h>
#include <ToolBar.h>
#include <WindowSubclass.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "uxtheme.lib")

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
