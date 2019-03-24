#include "stdafx.h"
#include "ASIOSettings.h"

// class id
// {25CBA31C - 951A - 48C6 - B513 - 012E1E2D09D8}
CLSID IID_TORTUGASIO_XTREAMER = { 0x25CBA31C, 0x951A, 0x48C6,{ 0xB5, 0x13, 0x01, 0x2E, 0x1E, 0x2D, 0x09, 0xD8 } };

LPCTSTR const szNameShMem = _T("AudioXtreamer_{25CBA31C-951A-48C6-B513-012E1E2D09D8}_Mem");
LPCTSTR const szNameAsioEvent = _T("AudioXtreamer_{25CBA31C-951A-48C6-B513-012E1E2D09D8}_AsioEvent");
LPCTSTR const szNameXtreamerEvent = _T("AudioXtreamer_{25CBA31C-951A-48C6-B513-012E1E2D09D8}_XtreamerEvent");
LPCTSTR const szNameClass = _T("AudioXtreamer_{25CBA31C-951A-48C6-B513-012E1E2D09D8}_Class");
LPCTSTR const szNameApp   = _T("TortugASIO Xtreamer");

ASIOSettings::Settings theSettings =
{
  { 15, 15, 15,_T("NrIns"),     _T("; zero index based number of pcm LR lines")},
  { 15, 15, 15,_T("NrOuts"),    _T("; zero index based number of pcm LR lines")},
  { 64, 64, 255,_T("NrSamples"), _T("; Whats necesary to run smooth using the least 512b usb packets")},
  { 64, 64, 255,_T("FifoSize"),  _T("; Size of the hardware Out FIFO , multiple of 16")}
};