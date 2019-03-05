#include "stdafx.h"
#include "MainFrame.h"
#include "..\..\libs\ntray\NTray.h"
#include "resource.h"



IMPLEMENT_DYNCREATE(MainFrame, CFrameWnd)

#define WM_TRAYNOTIFY WM_USER + 100
#pragma warning(suppress: 26433 26440)
BEGIN_MESSAGE_MAP(MainFrame, CFrameWnd)
  ON_WM_CREATE()
  ON_WM_TIMER()  
  ON_MESSAGE(WM_TRAYNOTIFY, &MainFrame::OnTrayNotification)
  ON_WM_DESTROY()
END_MESSAGE_MAP()


MainFrame::MainFrame()
{
}


MainFrame::~MainFrame()
{
}


CTrayNotifyIcon g_TrayIcon;

int MainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;

  //Create the dynamic systray
  if (!CTrayNotifyIcon::GetDynamicDCAndBitmap(&m_TrayIconDC, &m_BitmapTrayIcon))
  {
    AfxMessageBox(_T("Failed to create tray icon 3"), MB_OK | MB_ICONSTOP);
    return -1;
  }
  if (!g_TrayIcon.Create(this, IDR_TRAYMENU, _T("TortugASIO"), _T("The TortugASIO driver is active and will try to connect to the USB slave"), _T("TortugASIO"), 10, CTrayNotifyIcon::Warning, &m_BitmapTrayIcon, WM_TRAYNOTIFY, IDR_TRAYMENU))
  {
    AfxMessageBox(_T("Failed to create tray icon 3"), MB_OK | MB_ICONSTOP);
    return -1;
  }

  m_nTimerID = SetTimer(1, 100, nullptr);
  return 0;
}

void MainFrame::OnTimer(UINT_PTR nIDEvent)
{
  if (m_nTimerID == nIDEvent)
  {
    //Draw into the tray icon bitmap
    CString sNum;
    sNum.Format(_T("%2.1f"), 44.1f);
    m_TrayIconDC.SetBkMode(TRANSPARENT);
    m_TrayIconDC.SetTextColor(RGB(255, 255, 255));
    const int w = m_TrayIconDC.GetDeviceCaps(LOGPIXELSX);
    const int h = m_TrayIconDC.GetDeviceCaps(LOGPIXELSY);
    CRect r;
    r.top = 0;
    r.left = 0;
    r.right = w;
    r.bottom = h;
    CBrush blackBrush;
    blackBrush.CreateStockObject(BLACK_BRUSH);
    m_TrayIconDC.FillRect(&r, &blackBrush);

    CFont font;
    VERIFY(font.CreateFont(
      12,                        // nHeight
      0,                         // nWidth
      0,                         // nEscapement
      0,                         // nOrientation
      FW_BOLD,                 // nWeight
      FALSE,                     // bItalic
      FALSE,                     // bUnderline
      0,                         // cStrikeOut
      ANSI_CHARSET,              // nCharSet
      OUT_DEFAULT_PRECIS,        // nOutPrecision
      CLIP_DEFAULT_PRECIS,       // nClipPrecision
      DEFAULT_QUALITY,           // nQuality
      DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
      _T("Arial")));
    
    CFont* def_font = m_TrayIconDC.SelectObject(&font);
    m_TrayIconDC.TextOut(0, 0, "44");
    m_TrayIconDC.SelectObject(def_font);

    //Update it
    g_TrayIcon.SetWindowPos(NULL, 0, 0, 32, 32, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER),
    g_TrayIcon.SetIcon(&m_BitmapTrayIcon);
  }

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
