
// AudioXtreamerDlg.h : header file
//

#pragma once

#include "UsbDev\UsbDev.h"
#include "resource.h"
#include "ASIOSettings.h"



// CAudioXtreamerDlg dialog

class CAudioXtreamerDlg : public CPropertyPage
{
// Construction
public:
	CAudioXtreamerDlg(UsbDevice & device, ASIOSettings::Settings &info); // standard constructor
  ~CAudioXtreamerDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOXTREAMER_DIALOG };
#endif
  DECLARE_MESSAGE_MAP()

	protected:
	virtual void DoDataExchange(CDataExchange *DX)override;	// DDX/DDV support

  virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

// Implementation
protected:
	HICON m_hIcon;
  UsbDevice & mDevice;

	virtual BOOL OnInitDialog();

	HCURSOR OnQueryDragIcon();

public:
  void OnBnClickedCheck1();
protected:
  BOOL mCheckOpenDevice;
public:
  void OnOpen();
  void OnClose();
  void OnSendAsync();
  void OnRecvAsync();
  void OnStartStream();
  void OnStopStream();
  afx_msg void OnPaint();
};
