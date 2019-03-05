#pragma once
#include <stdint.h>
#include <ASIOSettings.h>

typedef struct _UsbDeviceStatus
{
  uint32_t st[8];
} UsbDeviceStatus;

class UsbDeviceClient
{
public:
  virtual void Switch(uint32_t rxSampleSize, uint8_t *rxBuff, uint32_t txSampleSize, uint8_t *txBuff) = 0;
  virtual bool GetDeviceStatus(UsbDeviceStatus &var) = 0;
};


class UsbDevice
{
public:
  enum HwType{ UsbDevFX2LP } ;
  static UsbDevice * CreateDevice(HwType type, UsbDeviceClient & client, ASIOSettings::Settings & params);//factory

  virtual bool Open() = 0;
  virtual bool Close() = 0;
  virtual bool Start() = 0;
  virtual bool Stop(bool wait) = 0;
  virtual bool IsRunning() = 0;

  virtual uint32_t GetFifoSize() = 0;

  virtual bool GetStatus(UsbDeviceStatus &var) = 0;
};
