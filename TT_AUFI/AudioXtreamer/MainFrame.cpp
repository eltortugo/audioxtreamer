#include "stdafx.h"
#include "MainFrame.h"
#include "ntray\NTray.h"
#include "resource.h"
#include "AudioXtreamer.h"

// CAboutDlg dialog used for App About



CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* DX)
{
  CDialog::DoDataExchange(DX);
}

// CAudioXtreamerDlg dialog



IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)


#pragma warning(suppress: 26433 26440)
BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_TIMER()
  ON_MESSAGE(WM_TRAYNOTIFY, &MainFrame::OnTrayNotification)
  ON_WM_DESTROY()
  ON_MESSAGE(WM_XTREAMER, &MainFrame::XtreamerMessage)
END_MESSAGE_MAP()


MainFrame::MainFrame()
{
  WNDCLASSEX wndc;
  ZeroMemory(&wndc, sizeof(wndc));
  wndc.cbSize = sizeof(wndc);
  wndc.hInstance = AfxGetInstanceHandle();
  wndc.lpszClassName = szNameClass;
  wndc.lpfnWndProc = AfxWndProc;


  RegisterClassEx(&wndc);
}


MainFrame::~MainFrame()
{
  UnregisterClass(szNameClass, AfxGetInstanceHandle());
}


CTrayNotifyIcon g_TrayIcon;


int MainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
 
  bool result = g_TrayIcon.Create(
         this,
         IDR_TRAYMENU,
         _T("AudioXtreamer"),
         _T("The AudioXtreamer driver is active and will try to connect to the USB Device"),
         _T("AudioXtreamer"),
         10,
         CTrayNotifyIcon::Info,
         (HICON)0,
         WM_TRAYNOTIFY,
    IDR_TRAYMENU, true, true);

  SetState(Stopped);

  SetTimer(100, 100, NULL);

  return 1;
}

void MainFrame::SetState(State st)
{
  UINT res = 0;
  switch (st)
  {
  case Stopped:  res = IDI_AXR; break;
  case Started:  res = IDI_AXY; break;
  case Active:   res = IDI_AXG; break;
  default: break;
  }
  g_TrayIcon.SetIcon(res);
}

void MainFrame::OnTimer(UINT_PTR nIDEvent)
{
  if (nIDEvent == 100)
    XtreamerMessage(0, 0);
  CFrameWnd::OnTimer(nIDEvent);
}

LRESULT MainFrame::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
  //pass to tray icon
  g_TrayIcon.OnTrayNotification(wParam, lParam);

  return LRESULT(0);
}

LRESULT MainFrame::XtreamerMessage(WPARAM wp, LPARAM lp)
{
  return theApp.XtreamerMessage(wp,lp);
}


void MainFrame::OnDestroy()
{
  CFrameWnd::OnDestroy();
  g_TrayIcon.DestroyWindow();
}



BOOL ProperySheetDlg::OnInitDialog()
{
  BOOL bResult = CPropertySheet::OnInitDialog();

  //Menu.LoadMenu(IDR_MENU);
  mHicon = AfxGetApp()->LoadIcon(IDI_AXG);

  CMenu * sysMenu = GetSystemMenu(FALSE);
  sysMenu->InsertMenu(1, MF_BYPOSITION, 1234, _T("&About"));
  sysMenu->InsertMenu(3, MF_BYPOSITION, 1234, _T("&Exit"));
  SetIcon(mHicon, TRUE);
  SetIcon(mHicon, FALSE);

  return bResult;
}
