#include "stdafx.h"
#include <stdio.h>
#include <inttypes.h>

extern BOOL WINAPI DllEntryPoint(HINSTANCE hInstance, ULONG ulReason, LPVOID pv);

__declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE hInstance, ULONG dwReason, LPVOID pv)//;

//BOOL WINAPI DllMain(HINSTANCE hInstance, ULONG dwReason, LPVOID pv)
{
  //DebugBreak();
  return DllEntryPoint(hInstance, dwReason, pv);
}

#pragma comment(lib, "comctl32.lib")

