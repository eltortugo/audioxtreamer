#pragma once
#include <afxtoolbarimages.h>
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
  afx_msg LRESULT XtreamerMessage(WPARAM wp, LPARAM lp);

  enum State{Stopped, Started, Active };
  void SetState(State st);

  DECLARE_MESSAGE_MAP()

protected:

  CPngImage m_pngImage;
  CBitmap  m_BitmapTrayIcon;
  UINT_PTR m_nTimerID;

public:
  afx_msg void OnDestroy();  
};

class ProperySheetDlg : public CPropertySheet
{
public:

  ProperySheetDlg(LPCTSTR p) : CPropertySheet(p) {}

  virtual BOOL OnInitDialog();

  CMenu Menu;
  HICON mHicon;
};

class CAboutDlg : public CDialog
{
public:
  CAboutDlg();

  // Dialog Data
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ABOUTBOX };
#endif

protected:
  virtual void DoDataExchange(CDataExchange * DX);    // DDX/DDV support

// Implementation
protected:

public:

};

