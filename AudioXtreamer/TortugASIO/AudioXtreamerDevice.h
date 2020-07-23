#pragma once
#include "UsbDev\UsbDev.h"
class AudioXtreamerDevice : public UsbDevice
{
public:
  explicit AudioXtreamerDevice(UsbDeviceClient & client);
  ~AudioXtreamerDevice();

  bool Open() override;
  bool Close() override;
  bool Start() override;
  bool Stop(bool wait) override;
  bool IsRunning() override;
  bool IsPresent() override;
  bool GetStatus(UsbDeviceStatus &status) override;
  uint32_t GetSampleRate() override;
  bool ConfigureDevice() override;
  ASIOSettings::StreamInfo AudioXtreamerDevice::GetStreamInfo() override;

private:

  void main();
  volatile HANDLE hth_Worker;
  volatile HANDLE mExitHandle;

  static void StaticWorkerThread(void* arg)
  {
    AudioXtreamerDevice *inst = static_cast<AudioXtreamerDevice*>(arg);
    if (inst != nullptr)
      inst->main();
  }

  HANDLE hMapFile;
  HANDLE hAsioEvent;
  HANDLE hExtreamerEvent;
  HWND   hWnd;
  uint8_t * pStreamParams;
  uint8_t * pTxBuf;
  uint8_t * pRxBuf;
};


