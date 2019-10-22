
// AudioXtreamer.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols
#include "usbdev\usbdev.h"


// CAudioXtreamerApp:
// See AudioXtreamer.cpp for the implementation of this class
//
class MainFrame;
class CAudioXtreamerApp : public CWinAppEx, public UsbDeviceClient
{
public:
	CAudioXtreamerApp();


public:
  BOOL InitInstance() override;
  bool Switch(uint32_t timeout, uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) override;
  HANDLE GetSwitchHandle() override { return hAsioEvent; };
  bool ClientPresent() override;
  void AllocBuffers(uint32_t rxSize, uint8_t *&rxBuff, uint32_t txSize, uint8_t *&txBuff) override;
  void FreeBuffers(uint8_t *&rxBuff, uint8_t *&txBuff) override;
  void DeviceStopped(bool error) override;
  void SampleRateChanged() override;

  bool IsClientActive() { return mClientActive; }

protected:

  HANDLE hMapFile;
  uint8_t* pBuf;
  HANDLE hAsioEvent;
  HANDLE hXtreamerEvent;
  UsbDevice * mDevice;
  MainFrame * pMainFrame;
  bool mClientActive;
  bool mSwitchWait;

  int ExitInstance() override;
};


extern CAudioXtreamerApp theApp;


#define WM_TRAYNOTIFY WM_USER + 100

