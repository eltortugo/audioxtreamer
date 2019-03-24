#pragma once
#include <afxtoolbarimages.h>
#include "ASIOSettingsFile.h"
#include "PropertySheetDlg.h"


enum State;
enum IconState;
class UsbDevice;
class MainFrame :
  public CFrameWnd
{
public:
  MainFrame(UsbDevice & dev);
  ~MainFrame();

  afx_msg void OnTimer(UINT_PTR nIDEvent);
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
  afx_msg LRESULT XtreamerMessage(WPARAM wp, LPARAM lp);
  afx_msg void OnAudioxtreamerOpen();
  afx_msg void OnUpdateAudioxtreamerOpen(CCmdUI *pCmdUI);
  afx_msg void OnAudioxtreamerQuit();
  afx_msg void OnUpdateAudioxtreamerQuit(CCmdUI *pCmdUI);

  DECLARE_MESSAGE_MAP()

protected:
  INT_PTR OpenControlPanel(bool pause);

  void NextState(enum State newState);
  void SetIconState(enum IconState st);

  ASIOSettingsFile mIniFile;
  UsbDevice & mDevice;
  enum State mState;

  PropertySheetDlg mPropertySheet;
  CPngImage m_pngImage;
  CBitmap  m_BitmapTrayIcon;
  UINT_PTR m_nTimerID;

public:
  afx_msg void OnDestroy();
};

