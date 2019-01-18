
// AudioXtreamerDlg.h : header file
//

#pragma once

#include "UsbDev\UsbDev.h"
// CAudioXtreamerDlg dialog
class CypressDevice;
class CAudioXtreamerDlg : public CDialogEx, public UsbDeviceClient
{
// Construction
public:
	CAudioXtreamerDlg(CWnd* pParent = NULL);	// standard constructor
  ~CAudioXtreamerDlg();

  void Switch(uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) override;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOXTREAMER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

  UsbDevice * mDevice;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
  afx_msg void OnBnClickedCheck1();
protected:
  BOOL mCheckOpenDevice;
public:
  afx_msg void OnOpen();
  afx_msg void OnClose();
  afx_msg void OnSendAsync();
  afx_msg void OnRecvAsync();
  afx_msg void OnStartStream();
  afx_msg void OnStopStream();
};
