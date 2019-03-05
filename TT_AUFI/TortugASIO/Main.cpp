#include "stdafx.h"
#include <stdio.h>
#include <inttypes.h>
#include "MainFrame.h"

extern BOOL WINAPI DllEntryPoint(HINSTANCE hInstance, ULONG ulReason, LPVOID pv);


#if 0
BOOL WINAPI DllMain(HINSTANCE hInstance, ULONG dwReason, LPVOID pv)
{
  //DebugBreak();
  return DllEntryPoint(hInstance, dwReason, pv);
}

#pragma comment(lib, "comctl32.lib")
#else
class TortugASIOApp : public CWinApp
{
public:
  TortugASIOApp();

  // Overrides
public:
  virtual BOOL InitInstance();

  DECLARE_MESSAGE_MAP()
  virtual int ExitInstance();

  MainFrame * pMainFrame;
};


BEGIN_MESSAGE_MAP(TortugASIOApp, CWinApp)
END_MESSAGE_MAP()


// CMFCDllsampleApp construction

TortugASIOApp::TortugASIOApp()
: pMainFrame(nullptr)
{
}


// The one and only CMFCDllsampleApp object

TortugASIOApp theApp;


// CMFCDllsampleApp initialization

BOOL TortugASIOApp::InitInstance()
{
  CWinApp::InitInstance();

  pMainFrame = new MainFrame;
  //m_pMainWnd = pMainFrame;
  //do not set this frame as main window, otherwise the ASIO dialog wont be able to be modal to the parent program
  if (!pMainFrame->Create(nullptr, _T("Traytest")))
    return FALSE;

  pMainFrame->ShowWindow(SW_HIDE);
  pMainFrame->UpdateWindow();

  return DllEntryPoint(m_hInstance, DLL_PROCESS_ATTACH, nullptr);
}



int TortugASIOApp::ExitInstance()
{
  
  DllEntryPoint(m_hInstance, DLL_PROCESS_DETACH, nullptr);


  pMainFrame->DestroyWindow();//will magically delete itself
  

  return CWinApp::ExitInstance();
}
#endif