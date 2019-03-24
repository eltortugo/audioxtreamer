#pragma once

#include "SettingsDlg.h"
#include "AudioXtreamerDlg.h"


class PropertySheetDlg : public CPropertySheet
{
public:

  PropertySheetDlg(UsbDevice &usbdev, CWnd *parent);
  ~PropertySheetDlg();
  bool ManualControls();
  virtual BOOL OnInitDialog();
 
protected:
  UsbDevice &mDevice;

  CPropertyPage* pp1;
  CPropertyPage* pp2;

  CMenu Menu;
  HICON mHicon;

public:
  DECLARE_MESSAGE_MAP()
  afx_msg void OnDestroy();
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
  afx_msg void OnApplyNow();
};
