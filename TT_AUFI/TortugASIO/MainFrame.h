#pragma once
#include <afxwin.h>
class MainFrame :
  public CFrameWnd
{
public:
  MainFrame();
  ~MainFrame();  
 
  DECLARE_DYNCREATE(MainFrame)
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);

  DECLARE_MESSAGE_MAP()

protected:
  CDC      m_TrayIconDC;
  CBitmap  m_BitmapTrayIcon;
  UINT_PTR m_nTimerID;
public:
  afx_msg void OnDestroy();
};

