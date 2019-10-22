
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

#define _CRT_SECURE_NO_WARNINGS
#include <afxwin.h>
#include <afxext.h>         // MFC extensions

#include <afxcontrolbars.h>     // MFC support for ribbons and control bars

#include <atlbase.h>        // ATL support
extern ATL::CComModule _Module;
#include <atlwin.h>

#include <stdint.h>
#include <cfgmgr32.h>

#include "setupapi.h"
#include <usb.h>


#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include <crtdbg.h>
#define LOG0(x) _RPT0(_CRT_WARN, x##"\n")
#define LOGN(x, ...) _RPTN(_CRT_WARN, x, __VA_ARGS__ )


