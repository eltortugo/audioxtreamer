#pragma once

#include "..\UsbDev\UsbDev.h"
#include "ASIOSettings.h"

template <class T>
class ASIOSettingsDlg :  public T
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
  uint32_t mSRLast;
  uint8_t mSRidx; 

protected:
  DECLARE_MESSAGE_MAP()
  afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
  afx_msg void OnShowWindow( BOOL bShow, UINT nStatus );

  typedef enum { SCSamples, SCFifo, SCBoth } SCType;
  void UpdateRanges(SCType type);
public:
  virtual BOOL OnInitDialog();
  virtual void DoDataExchange(CDataExchange* pDX);

  CComboBox mListIns;
  CComboBox mListOuts;

  CSliderCtrl mSliderSamples;
  CSliderCtrl mSliderFifo;
private:
  CProgressCtrl mProgressFifo;
};

#include "SettingsDlg.cpp"

