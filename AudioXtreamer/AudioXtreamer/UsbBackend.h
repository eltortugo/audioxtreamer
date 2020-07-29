#pragma once


struct usb_control_setup {
  uint8_t  bmRequestType;
  uint8_t  bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
};


typedef enum
{
  eControl,
  eBulk,
  eInterrupt,
  eIsochronous
} XferType;

typedef struct _XferReq
{
  HANDLE handle;
  XferType xtype;
  struct usb_control_setup stp;
  uint8_t endpoint;
  OVERLAPPED ovlp;
  uint32_t bufflen;
  uint8_t* buff;
  void* ctx;
  ULONG xfered;
} XferReq;

typedef struct _IsoReqResult
{
  ULONG64 length;
  USBD_STATUS status;
} IsoReqResult;

#define LSI8_READ 0x63

typedef int64_t(*USB_BACKEND_CTRL_XFER)(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char* data, uint16_t wLength, unsigned int timeout);

typedef BOOL(*USB_BACKEND_CTRL_XFER_ASYNCH)(XferReq & xfer);


typedef bool(*USB_BACKEND_INIT_XFER)(HANDLE handle, XferReq* req, const size_t pktCount, const size_t pktSize);
typedef bool(*USB_BACKEND_XFER)(XferReq* req);
typedef IsoReqResult(*USB_BACKEND_ISO_GET_RESULT)(XferReq* req, uint32_t idx);
typedef bool(*USB_BKND_OPEN_CLOSE)(HANDLE& dev, HANDLE& file);
typedef bool(*USB_BACKEND_HI)(HANDLE handle, uint8_t i);

extern const USB_BKND_OPEN_CLOSE bknd_open;
extern const USB_BKND_OPEN_CLOSE bknd_close;
extern const USB_BACKEND_INIT_XFER bknd_init_xfer;
extern const USB_BACKEND_XFER bknd_xfer_cleanup;

extern const USB_BACKEND_CTRL_XFER control_transfer;
extern const USB_BACKEND_CTRL_XFER_ASYNCH control_xfer;

extern const USB_BACKEND_XFER bknd_iso_read;
extern const USB_BACKEND_XFER bknd_iso_write;

extern const USB_BACKEND_XFER bknd_bulk_read;
extern const USB_BACKEND_XFER bknd_bulk_write;

extern const USB_BACKEND_ISO_GET_RESULT bknd_iso_get_result;

extern const USB_BACKEND_HI bknd_abort_pipe;
extern const USB_BACKEND_HI bknd_select_alt_ifc;


