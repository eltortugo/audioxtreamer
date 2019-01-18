#pragma once

#include "UsbDev\UsbDev.h"
#include  <atomic>

class CypressDevice : public UsbDevice
{
public:
  explicit CypressDevice(UsbDeviceClient & client, uint32_t NrOuts, uint32_t NrIns, uint32_t NrSamples);
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

  const uint32_t mNrIns;
  const uint32_t mNrOuts;
  const uint32_t mNrSamples;

  uint32_t mReframes;
  uint8_t mDefOutEP;
  uint8_t mDefInEP;

  HANDLE mDevHandle;
};
