#pragma once


typedef struct _IsoReq
{
  HANDLE handle;
  uint8_t endpoint;
  OVERLAPPED ovlp;
  uint32_t bufflen;
  PVOID64 buff;
  void* ctx;
} XferReq;

typedef struct _IsoReqResult
{
  ULONG64 length;
  USBD_STATUS status;
} IsoReqResult;

typedef int64_t(*USB_BACKEND_CTRL_XFER)(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char* data, uint16_t wLength, unsigned int timeout);

typedef bool(*USB_BACKEND_INIT_ISO_PRIV)(HANDLE handle, XferReq* req, size_t pktCount, size_t pktSize);
typedef bool(*USB_BACKEND_XFER)(XferReq* req);
typedef IsoReqResult(*USB_BACKEND_ISO_GET_RESULT)(XferReq* req, uint32_t idx);
typedef bool(*USB_BKND_OPEN_CLOSE)(HANDLE& dev, HANDLE& file);

extern const USB_BKND_OPEN_CLOSE bknd_open;
extern const USB_BKND_OPEN_CLOSE bknd_close;
extern const USB_BACKEND_CTRL_XFER control_transfer;
extern const USB_BACKEND_INIT_ISO_PRIV bknd_init_read_xfer;
extern const USB_BACKEND_XFER bknd_xfer_cleanup;
extern const USB_BACKEND_INIT_ISO_PRIV bknd_init_write_xfer;

extern const USB_BACKEND_XFER bknd_iso_read;
extern const USB_BACKEND_XFER bknd_iso_write;

extern const USB_BACKEND_XFER bknd_bulk_read;
extern const USB_BACKEND_XFER bknd_bulk_write;

extern const USB_BACKEND_ISO_GET_RESULT bknd_iso_get_result;

