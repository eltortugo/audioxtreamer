#include "stdafx.h"


#include "winusbhelper.h"
#pragma comment(lib, "winusb.lib")
#include "strsafe.h"

#include "UsbBackend.h"
#include "ASIOSettings.h"

HRESULT
RetrieveDevicePath(
  _Out_bytecap_(BufLen) LPTSTR DevicePath,
  _In_                  ULONG  BufLen,
  _Out_opt_             PBOOL  FailureDeviceNotFound
)
/*++

Routine description:

    Retrieve the device path that can be used to open the WinUSB-based device.

    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.

Arguments:

    DevicePath - On successful return, the path of the device (use with CreateFile).

    BufLen - The size of DevicePath's buffer, in bytes

    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.

Return value:

    HRESULT

--*/
{
  BOOL                             bResult = FALSE;
  HDEVINFO                         deviceInfo;
  SP_DEVICE_INTERFACE_DATA         interfaceData;
  PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
  ULONG                            length;
  ULONG                            requiredLength = 0;
  HRESULT                          hr;

  if (NULL != FailureDeviceNotFound) {

    *FailureDeviceNotFound = FALSE;
  }

  //
  // Enumerate all devices exposing the interface
  //
  deviceInfo = SetupDiGetClassDevs(&IID_TORTUGASIO_XTREAMER,
    NULL,
    NULL,
    DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

  if (deviceInfo == INVALID_HANDLE_VALUE) {

    hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
  }

  interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  //
  // Get the first interface (index 0) in the result set
  //
  bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
    NULL,
    &IID_TORTUGASIO_XTREAMER,
    0,
    &interfaceData);

  if (FALSE == bResult) {

    //
    // We would see this error if no devices were found
    //
    if (ERROR_NO_MORE_ITEMS == GetLastError() &&
      NULL != FailureDeviceNotFound) {

      *FailureDeviceNotFound = TRUE;
    }

    hr = HRESULT_FROM_WIN32(GetLastError());
    SetupDiDestroyDeviceInfoList(deviceInfo);
    return hr;
  }

  //
  // Get the size of the path string
  // We expect to get a failure with insufficient buffer
  //
  bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
    &interfaceData,
    NULL,
    0,
    &requiredLength,
    NULL);

  if (FALSE == bResult && ERROR_INSUFFICIENT_BUFFER != GetLastError()) {

    hr = HRESULT_FROM_WIN32(GetLastError());
    SetupDiDestroyDeviceInfoList(deviceInfo);
    return hr;
  }

  //
  // Allocate temporary space for SetupDi structure
  //
  detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)
    LocalAlloc(LMEM_FIXED, requiredLength);

  if (NULL == detailData)
  {
    hr = E_OUTOFMEMORY;
    SetupDiDestroyDeviceInfoList(deviceInfo);
    return hr;
  }

  detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
  length = requiredLength;

  //
  // Get the interface's path string
  //
  bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
    &interfaceData,
    detailData,
    length,
    &requiredLength,
    NULL);

  if (FALSE == bResult)
  {
    hr = HRESULT_FROM_WIN32(GetLastError());
    LocalFree(detailData);
    SetupDiDestroyDeviceInfoList(deviceInfo);
    return hr;
  }

  //
  // Give path to the caller. SetupDiGetDeviceInterfaceDetail ensured
  // DevicePath is NULL-terminated.
  //
  hr = StringCbCopy(DevicePath,
    BufLen,
    detailData->DevicePath);

  LocalFree(detailData);
  SetupDiDestroyDeviceInfoList(deviceInfo);

  return hr;
}


HRESULT
OpenDevice(
  _Out_     PDEVICE_DATA DeviceData,
  _Out_opt_ PBOOL        FailureDeviceNotFound
)
/*++

Routine description:

    Open all needed handles to interact with the device.

    If the device has multiple USB interfaces, this function grants access to
    only the first interface.

    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.

Arguments:

    DeviceData - Struct filled in by this function. The caller should use the
        WinusbHandle to interact with the device, and must pass the struct to
        CloseDevice when finished.

    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.

Return value:

    HRESULT

--*/
{
  HRESULT hr = S_OK;
  BOOL    bResult;

  DeviceData->HandlesOpen = FALSE;

  hr = RetrieveDevicePath(DeviceData->DevicePath,
    sizeof(DeviceData->DevicePath),
    FailureDeviceNotFound);

  if (FAILED(hr)) {

    return hr;
  }

  DeviceData->DeviceHandle = CreateFile(DeviceData->DevicePath,
    GENERIC_WRITE | GENERIC_READ,
    FILE_SHARE_WRITE | FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
    NULL);

  if (INVALID_HANDLE_VALUE == DeviceData->DeviceHandle) {

    hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
  }

  bResult = WinUsb_Initialize(DeviceData->DeviceHandle,
    &DeviceData->WinusbHandle);

  if (FALSE == bResult) {

    hr = HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(DeviceData->DeviceHandle);
    return hr;
  }

  DeviceData->HandlesOpen = TRUE;
  return hr;
}

VOID
CloseDevice(
  _Inout_ PDEVICE_DATA DeviceData
)
/*++

Routine description:

    Perform required cleanup when the device is no longer needed.

    If OpenDevice failed, do nothing.

Arguments:

    DeviceData - Struct filled in by OpenDevice

Return value:

    None

--*/
{
  if (FALSE == DeviceData->HandlesOpen) {

    //
    // Called on an uninitialized DeviceData
    //
    return;
  }

  BOOL st = CloseHandle(DeviceData->DeviceHandle);
  st = WinUsb_Free(DeviceData->WinusbHandle);
  DeviceData->HandlesOpen = FALSE;

  return;
}

static BOOL cont_rx_stream = FALSE;
static BOOL cont_tx_stream = FALSE;

bool winusb_open(HANDLE& dev, HANDLE& file)
{
  DEVICE_DATA devData; ZeroMemory(&devData, sizeof(devData));
  BOOL status = FALSE;

  if (S_OK == OpenDevice(&devData, &status))
  {
    dev = devData.WinusbHandle;
    file = devData.DeviceHandle;

    cont_rx_stream = FALSE;
    cont_tx_stream = FALSE;

    return true;
  }

  return false;
}

bool winusb_close(HANDLE& dev, HANDLE& file)
{
  DEVICE_DATA devData = { TRUE, dev, file, {0} };
  CloseDevice(&devData);
  return true;
}

int64_t winusb_control_transfer(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char* data, uint16_t wLength, unsigned int timeout)
{
  if (wLength > 4096)
    return -1;

  WINUSB_SETUP_PACKET  SetupPacket;
  ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
  SetupPacket.RequestType = bmRequestType;
  SetupPacket.Request = bRequest;
  SetupPacket.Value = wValue;
  SetupPacket.Index = wIndex;
  SetupPacket.Length = wLength;

  ULONG cbSent = 0;

  BOOL bResult = WinUsb_ControlTransfer(handle, SetupPacket, data , wLength, &cbSent, 0);
  if (!bResult)
    return -1;
  else
    return uint64_t(cbSent);
}

struct winusb_iso_info
{
  WINUSB_ISOCH_BUFFER_HANDLE isoBuffHandle;
  USBD_ISO_PACKET_DESCRIPTOR* pkts;
  USBD_ISO_PACKET_DESCRIPTOR* results;
  uint8_t* alloc;
  uint32_t pktCount;
  uint32_t pktSize;
};


bool winusb_init_xfer(HANDLE handle, XferReq* req, const size_t pktCount, const size_t pktSize)
{
  struct winusb_iso_info* bknd_spec = new struct winusb_iso_info;
  if (nullptr != bknd_spec) {
    ZeroMemory(bknd_spec, sizeof(struct winusb_iso_info));
    req->ctx = bknd_spec;
    if (pktCount > 0 && pktSize > 0) {
      bknd_spec->alloc = (uint8_t*)_aligned_malloc(pktCount * pktSize, 4096);
      req->buff = bknd_spec->alloc;
      memset(req->buff, 0xff, pktCount * pktSize);

      bknd_spec->pktCount = (uint32_t)pktCount;
      bknd_spec->pktSize = (uint32_t)pktSize;

      bknd_spec->pkts = new USBD_ISO_PACKET_DESCRIPTOR[pktCount];
      bknd_spec->results = new USBD_ISO_PACKET_DESCRIPTOR[pktCount];
    }
    BOOL res = WinUsb_RegisterIsochBuffer(handle, req->endpoint, (PUCHAR)req->buff, (ULONG)(pktCount * pktSize), &bknd_spec->isoBuffHandle);
    return (TRUE == res) ? true : false;
  }
  return false;
}


bool winusb_iso_read(XferReq* req)
{
  struct winusb_iso_info* bknd_spec = (struct winusb_iso_info*)req->ctx;

  BOOL res = WinUsb_ReadIsochPipeAsap(bknd_spec->isoBuffHandle, 0/*offset*/, bknd_spec->pktCount * bknd_spec->pktSize, cont_rx_stream, bknd_spec->pktCount, bknd_spec->pkts, &req->ovlp);
  cont_rx_stream = TRUE;
  if (TRUE != res)
  {
    DWORD last_err = GetLastError();
    return last_err == ERROR_IO_PENDING;
  }
  return false;
}


bool winusb_iso_write(XferReq * req)
{
  struct winusb_iso_info* bknd_spec = (struct winusb_iso_info*)req->ctx;

  BOOL res = WinUsb_WriteIsochPipeAsap(bknd_spec->isoBuffHandle, 0/*offset*/, bknd_spec->pktCount * bknd_spec->pktSize, cont_tx_stream, &req->ovlp);

  cont_tx_stream = TRUE;

  if (TRUE != res)
  {
    DWORD last_err = GetLastError();
    return last_err == ERROR_IO_PENDING;
  }
  return false;
}

IsoReqResult winusb_iso_result(XferReq* req, uint32_t idx)
{
  USBD_ISO_PACKET_DESCRIPTOR *res = &((struct winusb_iso_info*)req->ctx)->pkts[idx];
  IsoReqResult result = { res->Length, res->Status };
  return result;
}

bool winusb_iso_cleanup(XferReq* req)
{
  struct winusb_iso_info* ctx = (struct winusb_iso_info*)req->ctx;
  if (ctx)
  {
    WinUsb_UnregisterIsochBuffer(ctx->isoBuffHandle);
    if (ctx->alloc) {
      _aligned_free(ctx->alloc);
      req->buff = nullptr;
    }
    if (ctx->pkts)
      delete ctx->pkts;
    if (ctx->results)
      delete ctx->results;

    delete req->ctx;
  }

  return true;
}

bool winusb_abort_pipe(HANDLE handle, uint8_t ep )
{
  return WinUsb_AbortPipe((WINUSB_INTERFACE_HANDLE)handle, ep);
}


const USB_BKND_OPEN_CLOSE bknd_open = winusb_open;
const USB_BKND_OPEN_CLOSE bknd_close = winusb_close;
const USB_BACKEND_CTRL_XFER control_transfer        = winusb_control_transfer;
const USB_BACKEND_INIT_ISO_PRIV bknd_init_read_xfer = winusb_init_xfer;
const USB_BACKEND_INIT_ISO_PRIV bknd_init_write_xfer = winusb_init_xfer;
const USB_BACKEND_XFER bknd_iso_read            = winusb_iso_read;
const USB_BACKEND_XFER bknd_iso_write           = winusb_iso_write;
const USB_BACKEND_ISO_GET_RESULT bknd_iso_get_result= winusb_iso_result;
const USB_BACKEND_XFER bknd_xfer_cleanup = winusb_iso_cleanup;
const USB_BACKEND_ABORT bknd_abort_pipe = winusb_abort_pipe;

