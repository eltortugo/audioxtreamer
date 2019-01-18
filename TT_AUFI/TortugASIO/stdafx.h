// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

// Windows Header Files:
#define _CRT_SECURE_NO_WARNINGS
#include "Winsock2.h"
#include <windows.h>

#include <stdint.h>

#include <cfgmgr32.h>
#include <usb.h>

//#define USBDK_LIB
#include "usbdkhelper.h"

#include <crtdbg.h>

#define LOG0(x) _RPT0(_CRT_WARN, x##"\n")
#define LOGN(x, ...) _RPTN(_CRT_WARN, x, __VA_ARGS__ )

