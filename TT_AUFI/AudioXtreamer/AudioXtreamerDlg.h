
// AudioXtreamerDlg.h : header file
//

#pragma once

#include "UsbDev\UsbDev.h"
#include "resource.h"
#include "wxx_dialog.h"


// CAudioXtreamerDlg dialog
class CypressDevice;
class CAudioXtreamerDlg : public CDialog, public UsbDeviceClient
{
// Construction
public:
	CAudioXtreamerDlg(); // standard constructor
  ~CAudioXtreamerDlg();

  void Switch(uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) override;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUDIOXTREAMER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange &DX);	// DDX/DDV support
  INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
  virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

// Implementation
protected:
	HICON m_hIcon;

  UsbDevice * mDevice;


	virtual BOOL OnInitDialog();

  LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
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
};
