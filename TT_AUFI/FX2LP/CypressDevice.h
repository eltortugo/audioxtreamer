#pragma once

#include "UsbDev\UsbDev.h"
#include  <atomic>

class CypressDevice : public UsbDevice
{
public:
  explicit CypressDevice(UsbDeviceClient & client, ASIOSettings::Settings & params);
  ~CypressDevice();

  bool Open() override;
  bool Start() override;
  bool Stop(bool wait) override;
  bool Close() override;
  uint32_t GetFifoSize() override;
  bool IsRunning() override;

  bool SendAsync(unsigned int size, unsigned int timeout);
  bool RecvAsync(unsigned int size, unsigned int timeout);

  uint32_t GetReframes() { return mReframes; }
  bool GetStatus(UsbDeviceStatus &param) override;

private:

  void WorkerThread();
  volatile HANDLE hth_Worker;
  volatile HANDLE mExitHandle;

  static void StaticWorkerThread(void* arg)
  {
    CypressDevice *inst = static_cast<CypressDevice*>(arg);
    if (inst != nullptr)
      inst->WorkerThread();
  }

  UsbDeviceClient & devClient;
  ASIOSettings::Settings & devParams;

  uint32_t mReframes;
  uint8_t mDefOutEP;
  uint8_t mDefInEP;

  HANDLE mDevHandle;
  HANDLE hSem;
  UsbDeviceStatus mDevStatus;
};
