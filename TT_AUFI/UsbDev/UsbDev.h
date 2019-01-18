#pragma once
#include <stdint.h>

class UsbDeviceClient
{
public:
  virtual void Switch(uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) = 0;
};

class UsbDevice
{
public:
  static UsbDevice * CreateDevice(UsbDeviceClient & client, uint32_t NrOuts, uint32_t NrIns, uint32_t NrSamples);//factory

  virtual bool Open() = 0;
  virtual bool Close() = 0;
  virtual bool Start() = 0;
  virtual bool Stop(bool wait) = 0;
  virtual bool IsRunning() = 0;

  virtual uint32_t GetFifoSize() = 0;
};