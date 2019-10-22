#include "stdafx.h"
#include "MainFrame.h"
#include "ntray\NTray.h"
#include "resource.h"
#include "AudioXtreamer.h"
#include "PropertySheetDlg.h"


// CAboutDlg dialog used for App About


BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_TIMER()
  ON_MESSAGE(WM_TRAYNOTIFY, &MainFrame::OnTrayNotification)
  ON_WM_DESTROY()
  ON_MESSAGE(WM_XTREAMER, &MainFrame::XtreamerMessage)
  ON_COMMAND(ID_AUDIOXTREAMER_QUIT, &MainFrame::OnAudioxtreamerQuit)
  ON_UPDATE_COMMAND_UI(ID_AUDIOXTREAMER_QUIT, &MainFrame::OnUpdateAudioxtreamerQuit)
  ON_COMMAND(ID_AUDIOXTREAMER_OPEN, &MainFrame::OnAudioxtreamerOpen)
  ON_UPDATE_COMMAND_UI(ID_AUDIOXTREAMER_OPEN, &MainFrame::OnUpdateAudioxtreamerOpen)
END_MESSAGE_MAP()


enum State { stClosed, stOpen, stReady, stActive };
enum IconState { icstStopped, icstStarted, icstActive };


MainFrame::MainFrame(UsbDevice & dev)
: mIniFile(theSettings)
, mDevice(dev)
, mPropertySheet(mDevice, this)
, mState(stClosed)
{
  WNDCLASSEX wndc;
  ZeroMemory(&wndc, sizeof(wndc));
  wndc.cbSize = sizeof(wndc);
  wndc.hInstance = AfxGetInstanceHandle();
  wndc.lpszClassName = szNameClass;
  wndc.lpfnWndProc = AfxWndProc;
  RegisterClassEx(&wndc);

  mIniFile.Load();
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

  SetIconState(icstStopped);

  SetTimer(100, 100, NULL);

  return 1;
}

void MainFrame::SetIconState(enum IconState st)
{
  UINT res = 0;
  switch (st)
  {
  case icstStopped:  res = IDI_AXR; break;
  case icstStarted:  res = IDI_AXY; break;
  case icstActive:   res = IDI_AXG; break;
  default: break;
  }
  g_TrayIcon.SetIcon(res);
}

void MainFrame::OnTimer(UINT_PTR nIDEvent)
{
  if (nIDEvent == 100)
    NextState(mState);
  CFrameWnd::OnTimer(nIDEvent);
}

LRESULT MainFrame::OnTrayNotification(WPARAM wParam, LPARAM lParam)
{
  //pass to tray icon
  g_TrayIcon.OnTrayNotification(wParam, lParam);

  return LRESULT(0);
}


void MainFrame::OnDestroy()
{
  CFrameWnd::OnDestroy();
  g_TrayIcon.DestroyWindow();
}

LRESULT MainFrame::XtreamerMessage(WPARAM wp, LPARAM lp)
{
  switch (wp)
  {
  case 1: return ( mDevice.IsPresent()) ? LRESULT(1) : LRESULT(0);
  case 2: return ( mDevice.IsRunning()) ? LRESULT(1) : LRESULT(0);
  case 3: return OpenControlPanel(true) == IDOK ? LRESULT(1) : LRESULT(0);
  case 4: return (LRESULT)(mDevice.GetSampleRate());
  }
  return LRESULT(0);
}

void MainFrame::NextState(enum State newState)
{
  switch (mState)
  {
  case stClosed:
    if (mDevice.Open())
      mState = stOpen;
    break;

  case stOpen:
    if (mPropertySheet.GetSafeHwnd() && mPropertySheet.ManualControls()) //dont try to start while editing
      break;
    if (mDevice.Start()) {
      mState = stReady;
      SetIconState(icstStarted);
    }
    else {
      mDevice.Close();
      mState = stClosed;
      break;
    }

  case stReady:
    if (mDevice.IsRunning()) {
      if (theApp.IsClientActive()) {
        SetIconState(icstActive);
        mState = stActive;
      }
    }
    else {
      SetIconState(icstStopped);
      mState = stOpen;
    }
    break;

  case stActive:
    if (mDevice.IsRunning()) {
      if (!theApp.IsClientActive()) {
        SetIconState(icstStarted);
        mState = stReady;
      }
    }
    else {
      SetIconState(icstStopped);
      mState = stOpen;
    }
    break;
  default: break;
  }
}


INT_PTR MainFrame::OpenControlPanel(bool pause)
{
  //the window exists so bring it to the front and leave
  if (mPropertySheet.GetSafeHwnd() != NULL) {
    mPropertySheet.SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    mPropertySheet.SetWindowPos(&CWnd::wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return 0;
  }

  bool restart = false;
  if (pause && mDevice.IsRunning())
  {
    KillTimer(100);
    mDevice.Stop(true);
    restart = true;
  }

  INT_PTR result = mPropertySheet.DoModal();


  if (result == IDOK)
    SaveSettings();

  if (restart)
  {
    mDevice.Start();
    SetTimer(100, 100, NULL);
  }

  return result;
}

void MainFrame::SaveSettings()
{
  mIniFile.Save();
}



void MainFrame::OnAudioxtreamerQuit()
{
  //OnAudioxtreamerStop();
  PostQuitMessage(0);
}


void MainFrame::OnUpdateAudioxtreamerQuit(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mPropertySheet.GetSafeHwnd() == NULL);
}


void MainFrame::OnAudioxtreamerOpen()
{
  OpenControlPanel(false);
}


void MainFrame::OnUpdateAudioxtreamerOpen(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mPropertySheet.GetSafeHwnd() == NULL);
}


