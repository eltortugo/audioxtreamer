#pragma once

#include "..\UsbDev\UsbDev.h"
#include "ASIOSettings.h"

class ASIOSettingsDlg :
  public CDialog
{
public:
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_DLG_ASIO };
#endif

  explicit ASIOSettingsDlg(UsbDevice &dev, ASIOSettings::Settings &info);
  ~ASIOSettingsDlg();

  afx_msg void OnTimer(UINT_PTR nIDEvent);

  UsbDevice &mDev;
  ASIOSettings::Settings &mInfo;
  afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
  afx_msg void OnDestroy();

private:
  uint16_t mSRvals[16];
  uint32_t mSRacc;
  uint8_t mSRidx;
 

protected:
  DECLARE_MESSAGE_MAP()
public:
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);
  CComboBox mListIns;
  CComboBox mListOuts;
  CComboBox mListNrSamples;
  CComboBox mListFifoDepth;
};

