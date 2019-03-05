#include "stdafx.h"
#include "UsbDev.h"
#include <FX2LP\CypressDevice.h>

UsbDevice * UsbDevice::CreateDevice(HwType type, UsbDeviceClient & client, ASIOSettings::Settings & params)
{
  switch (type)
  {
  case UsbDevFX2LP: return new CypressDevice(client, params);
  }
  return nullptr;
}
