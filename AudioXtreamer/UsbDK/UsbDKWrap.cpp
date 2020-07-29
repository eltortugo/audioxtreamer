#include "stdafx.h"
#include "UsbDKWrap.h"

#include "UsbBackend.h"


struct usbdk_lib {
  HMODULE module;

  USBDK_GET_DEVICES_LIST			GetDevicesList;
  USBDK_RELEASE_DEVICES_LIST		ReleaseDevicesList;
  USBDK_START_REDIRECT			StartRedirect;
  USBDK_STOP_REDIRECT			StopRedirect;
  USBDK_GET_CONFIGURATION_DESCRIPTOR	GetConfigurationDescriptor;
  USBDK_RELEASE_CONFIGURATION_DESCRIPTOR	ReleaseConfigurationDescriptor;
  USBDK_READ_PIPE				ReadPipe;
  USBDK_WRITE_PIPE			WritePipe;
  USBDK_ABORT_PIPE			AbortPipe;
  USBDK_RESET_PIPE			ResetPipe;
  USBDK_SET_ALTSETTING			SetAltsetting;
  USBDK_RESET_DEVICE			ResetDevice;
  USBDK_GET_REDIRECTOR_SYSTEM_HANDLE	GetRedirectorSystemHandle;
} UsbDk;


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
  uint32_t sendlen = sizeof(usb_control_setup) + wLength;
  if (sendlen > 4096)
    return -1;
  uint8_t sendbuff[4096];
  struct usb_control_setup &setup = *(struct usb_control_setup *)sendbuff;
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
    memcpy(sendbuff + sizeof(usb_control_setup), data, wLength);
    result = UsbDk.WritePipe(handle, &xfer, nullptr);// &ovlp);
  }

  if (result != TransferFailure && xfer.Result.GenResult.UsbdStatus == USBD_STATUS_SUCCESS) {

    if (bmRequestType & 0x80)
      memcpy(data, sendbuff + sizeof(usb_control_setup), (size_t)xfer.Result.GenResult.BytesTransferred);

    return xfer.Result.GenResult.BytesTransferred;
  } else {
    return -1;
  }
}

struct usbdk_req
{
  USB_DK_TRANSFER_REQUEST req;
  uint32_t pktCount;
  uint32_t pktSize;
  PVOID64 * pkts;
  uint8_t* alloc;
  USB_DK_ISO_TRANSFER_RESULT *results;
};

bool usbdk_init_xfer(HANDLE handle, XferReq* req, const size_t pktCount, const size_t pktSize )
{
  struct usbdk_req* ctx = new struct usbdk_req;

  if (nullptr != ctx) {
    ZeroMemory(ctx, sizeof(struct usbdk_req));
    req->ctx = ctx;

    switch (req->xtype)
    {
    case eControl:
      ctx->alloc = (uint8_t*)_aligned_malloc(req->bufflen + sizeof(usb_control_setup) , 4096);
      req->buff = ctx->alloc + sizeof(usb_control_setup);
      break;
    case eInterrupt:  break;
    case eBulk:       break;
    case eIsochronous:
      if (pktCount > 0 && pktSize > 0) {
        ctx->alloc = (uint8_t*)_aligned_malloc(pktCount * pktSize, 4096);
        req->buff = ctx->alloc;

        ctx->pktCount = (uint32_t)pktCount;
        ctx->pktSize = (uint32_t)pktSize;

        ctx->pkts = new PVOID64[pktCount];
        ctx->results = new USB_DK_ISO_TRANSFER_RESULT[pktCount];

        for (uint32_t c = 0; c < pktCount; ++c)
          ctx->pkts[c] = (PVOID64)(uint64_t)pktSize;

        ctx->req = { req->endpoint, req->buff, req->bufflen, IsochronousTransferType, pktCount, ctx->pkts };
        ctx->req.Result.IsochronousResultsArray = ctx->results;
      }
      break;
    default:break;
    };


    
    return true;
  }

  
  return false;
}

BOOL usbdk_control_xfer(XferReq & req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req.ctx;

  struct usb_control_setup& setup = *(struct usb_control_setup*)ctx->alloc;
  setup.bmRequestType = req.stp.bmRequestType;
  setup.bRequest = req.stp.bRequest;
  setup.wValue = req.stp.wValue;
  setup.wIndex = req.stp.wIndex;
  setup.wLength = req.stp.wLength;

  ZeroMemory(&ctx->req, sizeof(USB_DK_TRANSFER_REQUEST));
  ctx->req.EndpointAddress = req.endpoint;
  ctx->req.TransferType = ControlTransferType;
  ctx->req.Buffer = ctx->alloc;
  ctx->req.BufferLength = sizeof(usb_control_setup) + req.stp.wLength;


  if (req.stp.bmRequestType & 0x80)
    return UsbDk.ReadPipe(req.handle, &ctx->req,  &req.ovlp);
  else
    return UsbDk.WritePipe(req.handle, &ctx->req, &req.ovlp);
}

bool usbdk_iso_read(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;
  return UsbDk.ReadPipe(req->handle, &ctx->req, &req->ovlp) == TransferSuccessAsync;
}


bool usbdk_iso_write(XferReq* req)
{
  struct usbdk_req* ctx = (struct usbdk_req*)req->ctx;

  uint32_t nrpkts = req->bufflen / ctx->pktSize;
  uint32_t nrbytes = req->bufflen % ctx->pktSize;
  uint32_t c = 0;
  for (; c < ctx->pktCount && c < nrpkts ; ++c)
    ctx->pkts[c] = (PVOID64)(uint64_t)ctx->pktSize;

  if (c < ctx->pktCount)
    ctx->pkts[c++] = (PVOID64)(uint64_t)nrbytes;

  for (; c < ctx->pktCount ; ++c)
    ctx->pkts[c] = (PVOID64)(uint64_t)0;

  ctx->req.BufferLength = req->bufflen;

  return UsbDk.WritePipe(req->handle, &ctx->req, &req->ovlp) == TransferSuccessAsync;
}

IsoReqResult usbdk_iso_result(XferReq* req, uint32_t idx)
{
  USB_DK_ISO_TRANSFER_RESULT & res = ((struct usbdk_req*)req->ctx)->results[idx]; 
  IsoReqResult result = { res.ActualLength , (USBD_STATUS)res.TransferResult };
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

bool usbdk_abort_pipe(HANDLE handle, uint8_t ep)
{
  return UsbDk.AbortPipe(handle, ep);
}
bool usbdk_select_alt_ifc(HANDLE handle, uint8_t ai)
{
  return UsbDk.SetAltsetting(handle, 0/*must be*/, ai);
}

/*
const USB_BKND_OPEN_CLOSE bknd_open          = usbdk_open;
const USB_BKND_OPEN_CLOSE bknd_close         = usbdk_close;
const USB_BACKEND_INIT_XFER bknd_init_xfer   = usbdk_init_xfer;
const USB_BACKEND_XFER bknd_xfer_cleanup     = usbdk_xfer_cleanup;

const USB_BACKEND_CTRL_XFER control_transfer        = usbdk_control_transfer;
const USB_BACKEND_CTRL_XFER_ASYNCH control_xfer = usbdk_control_xfer;
const USB_BACKEND_XFER bknd_iso_read            = usbdk_iso_read;
const USB_BACKEND_XFER bknd_iso_write           = usbdk_iso_write;
const USB_BACKEND_ISO_GET_RESULT bknd_iso_get_result = usbdk_iso_result;

const USB_BACKEND_HI bknd_abort_pipe = usbdk_abort_pipe;
const USB_BACKEND_HI bknd_select_alt_ifc = usbdk_select_alt_ifc;
*/
