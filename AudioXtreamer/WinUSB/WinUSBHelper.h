#pragma once

#include "winusb.h"


typedef struct _DEVICE_DATA {

  BOOL                    HandlesOpen;
  WINUSB_INTERFACE_HANDLE WinusbHandle;
  HANDLE                  DeviceHandle;
  TCHAR                   DevicePath[MAX_PATH];

} DEVICE_DATA, * PDEVICE_DATA;

HRESULT
RetrieveDevicePath(
  _Out_bytecap_(BufLen) LPTSTR DevicePath,
  _In_                  ULONG  BufLen,
  _Out_opt_             PBOOL  FailureDeviceNotFound
);

HRESULT
OpenDevice(
  _Out_     PDEVICE_DATA DeviceData,
  _Out_opt_ PBOOL        FailureDeviceNotFound
);

VOID
CloseDevice(
  _Inout_ PDEVICE_DATA DeviceData
);