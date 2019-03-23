
// AudioXtreamer.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols
#include "ASIOSettingsFile.h"
#include "usbdev\usbdev.h"
#include "MainFrame.h"

// CAudioXtreamerApp:
// See AudioXtreamer.cpp for the implementation of this class
//

enum State;
class CAudioXtreamerApp : public CWinAppEx, public UsbDeviceClient
{
public:
	CAudioXtreamerApp();


public:
  BOOL InitInstance() override;
  void Switch(uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) override;
  void AllocBuffers(uint32_t rxSize, uint8_t *&rxBuff, uint32_t txSize, uint8_t *&txBuff) override;
  void FreeBuffers(uint8_t *&rxBuff, uint8_t *&txBuff) override;
  void DeviceStopped(bool error) override;

  bool IsClientActive() { return mClientActive; }

  LRESULT XtreamerMessage(WPARAM wp, LPARAM lp);


protected:
  ASIOSettingsFile mIniFile;
  INT_PTR OpenControlPanel(bool pause); 

  void NextState(enum State newState);

  HANDLE hMapFile;
  uint8_t* pBuf;
  HANDLE hAsioMutex;
  UsbDevice * mDevice;
  MainFrame * pMainFrame;
  volatile bool mClientActive;
  volatile bool mClientCleanup;

  ProperySheetDlg mPropertySheetDlg;
  enum State mState;

  int ExitInstance() override;
  DECLARE_MESSAGE_MAP()
  afx_msg void OnAudioxtreamerOpen();
  afx_msg void OnUpdateAudioxtreamerOpen(CCmdUI *pCmdUI);
  afx_msg void OnAudioxtreamerQuit();
  afx_msg void OnUpdateAudioxtreamerQuit(CCmdUI *pCmdUI);
};


extern CAudioXtreamerApp theApp;


#define WM_TRAYNOTIFY WM_USER + 100

