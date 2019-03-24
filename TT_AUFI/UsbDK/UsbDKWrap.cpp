#include "stdafx.h"
#include "UsbDKWrap.h"

#include "UsbBackend.h"



usbdk_lib UsbDk;

static FARPROC get_usbdk_proc_addr(LPCSTR api_name)
{
  FARPROC api_ptr = GetProcAddress(UsbDk.module, api_name);

  if (api_ptr == NULL) {
    LOGN("UsbDkHelper API %s not found: 0x%08X", api_name, GetLastError());
  }

  return api_ptr;
}

void unload_usbdk(void)
{
  if (UsbDk.module != NULL) {
    FreeLibrary(UsbDk.module);
    UsbDk.module = NULL;
  }
}

bool load_usbdk()
{
  UsbDk.module = LoadLibraryA("UsbDkHelper");
  if (UsbDk.module == NULL) {
    LOGN("Failed to load UsbDkHelper.dll: 0x%08X", GetLastError());
    return false;
  }

  UsbDk.GetDevicesList = (USBDK_GET_DEVICES_LIST)get_usbdk_proc_addr( "UsbDk_GetDevicesList");
  if (UsbDk.GetDevicesList == NULL)
    goto error_unload;

  UsbDk.ReleaseDevicesList = (USBDK_RELEASE_DEVICES_LIST)get_usbdk_proc_addr( "UsbDk_ReleaseDevicesList");
  if (UsbDk.ReleaseDevicesList == NULL)
    goto error_unload;

  UsbDk.StartRedirect = (USBDK_START_REDIRECT)get_usbdk_proc_addr( "UsbDk_StartRedirect");
  if (UsbDk.StartRedirect == NULL)
    goto error_unload;

  UsbDk.StopRedirect = (USBDK_STOP_REDIRECT)get_usbdk_proc_addr( "UsbDk_StopRedirect");
  if (UsbDk.StopRedirect == NULL)
    goto error_unload;

  UsbDk.GetConfigurationDescriptor = (USBDK_GET_CONFIGURATION_DESCRIPTOR)get_usbdk_proc_addr( "UsbDk_GetConfigurationDescriptor");
  if (UsbDk.GetConfigurationDescriptor == NULL)
    goto error_unload;

  UsbDk.ReleaseConfigurationDescriptor = (USBDK_RELEASE_CONFIGURATION_DESCRIPTOR)get_usbdk_proc_addr( "UsbDk_ReleaseConfigurationDescriptor");
  if (UsbDk.ReleaseConfigurationDescriptor == NULL)
    goto error_unload;

  UsbDk.ReadPipe = (USBDK_READ_PIPE)get_usbdk_proc_addr( "UsbDk_ReadPipe");
  if (UsbDk.ReadPipe == NULL)
    goto error_unload;

  UsbDk.WritePipe = (USBDK_WRITE_PIPE)get_usbdk_proc_addr( "UsbDk_WritePipe");
  if (UsbDk.WritePipe == NULL)
    goto error_unload;

  UsbDk.AbortPipe = (USBDK_ABORT_PIPE)get_usbdk_proc_addr( "UsbDk_AbortPipe");
  if (UsbDk.AbortPipe == NULL)
    goto error_unload;

  UsbDk.ResetPipe = (USBDK_RESET_PIPE)get_usbdk_proc_addr( "UsbDk_ResetPipe");
  if (UsbDk.ResetPipe == NULL)
    goto error_unload;

  UsbDk.SetAltsetting = (USBDK_SET_ALTSETTING)get_usbdk_proc_addr( "UsbDk_SetAltsetting");
  if (UsbDk.SetAltsetting == NULL)
    goto error_unload;

  UsbDk.ResetDevice = (USBDK_RESET_DEVICE)get_usbdk_proc_addr( "UsbDk_ResetDevice");
  if (UsbDk.ResetDevice == NULL)
    goto error_unload;

  UsbDk.GetRedirectorSystemHandle = (USBDK_GET_REDIRECTOR_SYSTEM_HANDLE)get_usbdk_proc_addr( "UsbDk_GetRedirectorSystemHandle");
  if (UsbDk.GetRedirectorSystemHandle == NULL)
    goto error_unload;

  return true;

error_unload:
  FreeLibrary(UsbDk.module);
  UsbDk.module = NULL;
  return false;
}



struct libusb_control_setup {
  /** Request type. Bits 0:4 determine recipient, see
  * \ref libusb_request_recipient. Bits 5:6 determine type, see
  * \ref libusb_request_type. Bit 7 determines data transfer direction, see
  * \ref libusb_endpoint_direction.
  */
  uint8_t  bmRequestType;

  /** Request. If the type bits of bmRequestType are equal to
  * \ref libusb_request_type::LIBUSB_REQUEST_TYPE_STANDARD
  * "LIBUSB_REQUEST_TYPE_STANDARD" then this field refers to
  * \ref libusb_standard_request. For other cases, use of this field is
  * application-specific. */
  uint8_t  bRequest;

  /** Value. Varies according to request */
  uint16_t wValue;

  /** Index. Varies according to request, typically used to pass an index
  * or offset */
  uint16_t wIndex;

  /** Number of bytes to transfer */
  uint16_t wLength;
};


int GetDeviceID(USB_DK_DEVICE_ID* ret)
{
  const int id_vendor = 0x221A;  	// ZTEX vendor ID
  const int id_product = 0x100; 	// default product ID for ZTEX firmware
  //const int id_vendor = 0x04B4;  	// 
  //const int id_product = 0x00F1; 	// default product ID for ZTEX firmware

  PUSB_DK_DEVICE_INFO DevInfo = nullptr;
  ULONG DevCount = 0;
  int dev_idx = -1;

  BOOL r = UsbDk.GetDevicesList(&DevInfo, &DevCount);
  if (r == FALSE)
    goto exit;

  // find device

  for (ULONG i = 0; i < DevCount; i++) {
    USB_DEVICE_DESCRIPTOR* desc = &DevInfo[i].DeviceDescriptor;
    if ((desc->idVendor == id_vendor) && (desc->idProduct == id_product)) {
      dev_idx = i;
      break;
    }
  }

  if (dev_idx >= 0 && ret)
    * ret = DevInfo[dev_idx].ID;

exit:

  if (DevInfo)
    UsbDk.ReleaseDevicesList(DevInfo);

  return dev_idx;
}

bool usbdk_open(HANDLE& dev, HANDLE& file)
{
  if (!load_usbdk())
    return false;

  USB_DK_DEVICE_ID devid;
  if (GetDeviceID(&devid) < 0)
    return false;

  dev = UsbDk.StartRedirect(&devid);

  if (INVALID_HANDLE_VALUE == dev)
    return false;

  BOOL status = UsbDk.ResetDevice(dev);
  if (status == FALSE) {
    fprintf(stderr, "Error: Unable to reset device\n");
    return false;
  }

  return true;

  /*status = UsbDk.SetAltsetting(handle, 0, 6);
  if (status == FALSE) {
    fprintf(stderr, "Error: Unable to SetAltsetting\n");
    goto err;
  }
  else
  {
    mDefInEP = 0x82;
    mDefOutEP = 0x6;
  }*/
}

bool usbdk_close(HANDLE& dev, HANDLE& file)
{
  return UsbDk.StopRedirect(dev) == TRUE;
}

int64_t usbdk_control_transfer(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char *data, uint16_t wLength, unsigned int timeout)
{
  uint32_t sendlen = sizeof(libusb_control_setup) + wLength;
  if (sendlen > 4096)
    return -1;
  uint8_t sendbuff[4096];
  struct libusb_control_setup &setup = *(struct libusb_control_setup *)sendbuff;
  setup.bmRequestType = bmRequestType;
  setup.bRequest = bRequest;
  setup.wValue = wValue;
  setup.wIndex = wIndex;
  setup.wLength = wLength;

  USB_DK_TRANSFER_REQUEST xfer;
  ZeroMemory(&xfer, sizeof(USB_DK_TRANSFER_REQUEST));
  xfer.EndpointAddress = 0;
  xfer.TransferType = ControlTransferType;
  xfer.Buffer = sendbuff;
  xfer.BufferLength = sendlen;

  TransferResult result;

  if (bmRequestType & 0x80)
    result = UsbDk.ReadPipe(handle, &xfer, nullptr);// &ovlp);
  else {
    memcpy(sendbuff + sizeof(libusb_control_setup), data, wLength);
    result = UsbDk.WritePipe(handle, &xfer, nullptr);// &ovlp);
  }

  if (result != TransferFailure && xfer.Result.GenResult.UsbdStatus == USBD_STATUS_SUCCESS) {

    if (bmRequestType & 0x80)
      memcpy(data, sendbuff + sizeof(libusb_control_setup), (size_t)xfer.Result.GenResult.BytesTransferred);

    return xfer.Result.GenResult.BytesTransferred;
  } else {
    return -1;
  }
}

//int64_t bulk_transfer(HANDLE handle,
//  uint8_t endpoint, uint8_t *data, int length,
//  int *actual_length, OVERLAPPED * ovlp, unsigned int timeout)
//{
//  USB_DK_TRANSFER_REQUEST xfer;
//  ZeroMemory(&xfer, sizeof(USB_DK_TRANSFER_REQUEST));
//  xfer.EndpointAddress = endpoint;
//  xfer.TransferType = BulkTransferType;
//  xfer.Buffer = data;
//  xfer.BufferLength = length;
//
//  TransferResult result;
//  if (endpoint & 0x80)
//    result = UsbDk.ReadPipe(handle, &xfer, ovlp);
//  else
//    result = UsbDk.WritePipe(handle, &xfer, ovlp);
//
//  if(ovlp)
//    WaitForSingleObject(ovlp->hEvent, timeout);
//
//  USBD_STATUS status = (USBD_STATUS)xfer.Result.GenResult.UsbdStatus;
//  *actual_length = (int)xfer.Result.GenResult.BytesTransferred;
//
//  switch (result)
//  {
//  case TransferFailure: return -1;
//  default:
//    return status;
//  }
//}

struct usbdk_req
{
  USB_DK_TRANSFER_REQUEST req;
  PVOID64 * pkts;
  PVOID alloc;
  USB_DK_ISO_TRANSFER_RESULT *results;
};

bool usbdk_init_read_xfer(HANDLE handle, XferReq* req, size_t pktCount, size_t pktSize )
{
  struct usbdk_req* ctx = new struct usbdk_req;
  USB_DK_TRANSFER_TYPE type = BulkTransferType;

  if (nullptr != ctx) {
    ZeroMemory(ctx, sizeof(struct usbdk_req));
    req->ctx = ctx;
    if (pktCount > 0 && pktSize > 0) {
      ctx->alloc = _aligned_malloc(pktCount * pktSize, 4096);
      req->buff = ctx->alloc;

      ctx->pkts = new PVOID64[pktCount];
      ctx->results = new USB_DK_ISO_TRANSFER_RESULT[pktCount];

      for (uint32_t c = 0; c < pktCount; ++c)
        ctx->pkts[c] = (PVOID64)(uint64_t)pktSize;

      type = IsochronousTransferType;
    }

    ctx->req = { req->endpoint, req->buff, req->bufflen, (ULONG64)type, pktCount, ctx->pkts };
    ctx->req.Result.IsochronousResultsArray = ctx->results;
    return true;
  }
  return false;
}

bool usbdk_iso_read(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;
  return UsbDk.ReadPipe(req->handle, &ctx->req, &req->ovlp) == TransferSuccessAsync;
}


bool usbdk_init_write_xfer(HANDLE handle, XferReq* req, size_t pktCount, size_t pktSize)
{
  struct usbdk_req* ctx = new struct usbdk_req;
  USB_DK_TRANSFER_TYPE type = BulkTransferType;
  if (nullptr != ctx) {
    ZeroMemory(ctx, sizeof(struct usbdk_req));
    req->ctx = ctx;

    if (pktCount > 0 && pktSize > 0) {
      ctx->alloc = _aligned_malloc(pktCount * pktSize, 4096);
      req->buff = ctx->alloc;
      ZeroMemory(req->buff, pktCount * pktSize);

      ctx->pkts = new PVOID64[pktCount];
      ctx->results = new USB_DK_ISO_TRANSFER_RESULT[pktCount];

      for (uint32_t c = 0; c < pktCount; ++c)
        ctx->pkts[c] = (PVOID64)(uint64_t)pktSize;

      type = IsochronousTransferType;
    }

    ctx->req = { req->endpoint, req->buff, req->bufflen, (ULONG64)type, pktCount, ctx->pkts };
    ctx->req.Result.IsochronousResultsArray = ctx->results;

    return true;
  }

  return false;
}

bool usbdk_iso_write(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;

  return UsbDk.WritePipe(req->handle, &ctx->req, &req->ovlp) == TransferSuccessAsync;
}

IsoReqResult usbdk_iso_result(XferReq* req, uint32_t idx)
{
  USB_DK_ISO_TRANSFER_RESULT* res = nullptr;
  if (req->endpoint & 0x80) //in
   res = &((struct usbdk_req*)req->ctx)->results[idx];
  else
   res = &((struct usbdk_req*)req->ctx)->results[idx];
 
  IsoReqResult result = { res->ActualLength , (USBD_STATUS)res->TransferResult };
  return result;
}

bool usbdk_xfer_cleanup(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;
  if (ctx)
  {
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

bool usbdk_bulk_read(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;
  if (ctx == nullptr)
    return false;

  ctx->req.BufferLength = req->bufflen;
  ctx->req.Buffer = req->buff;
  return UsbDk.ReadPipe(req->handle, &ctx->req, req->ovlp.hEvent ? &req->ovlp : nullptr) == TransferSuccessAsync;
}

bool usbdk_bulk_write(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;
  if (ctx == nullptr)
    return false;

  ctx->req.BufferLength = req->bufflen;
  ctx->req.Buffer = req->buff;
  return UsbDk.WritePipe(req->handle, &ctx->req, req->ovlp.hEvent? &req->ovlp : nullptr) == TransferSuccessAsync;
}



const USB_BKND_OPEN_CLOSE bknd_open          = usbdk_open;
const USB_BKND_OPEN_CLOSE bknd_close         = usbdk_close;
const USB_BACKEND_CTRL_XFER control_transfer        = usbdk_control_transfer;
const USB_BACKEND_INIT_ISO_PRIV bknd_init_read_xfer  = usbdk_init_read_xfer;
const USB_BACKEND_INIT_ISO_PRIV bknd_init_write_xfer = usbdk_init_write_xfer;
const USB_BACKEND_XFER bknd_iso_read            = usbdk_iso_read;
const USB_BACKEND_XFER bknd_iso_write           = usbdk_iso_write;
const USB_BACKEND_ISO_GET_RESULT bknd_iso_get_result = usbdk_iso_result;
const USB_BACKEND_XFER bknd_xfer_cleanup = usbdk_xfer_cleanup;

const USB_BACKEND_XFER bknd_bulk_read = usbdk_bulk_read;
const USB_BACKEND_XFER bknd_bulk_write = usbdk_bulk_write;


