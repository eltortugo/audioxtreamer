#include "stdafx.h"

#if defined (USBDK_LIB)
#if defined _M_IX86
#if defined( _DEBUG)
#pragma comment(lib,"lib\\UsbDkHelperx86d.lib")
#else
#pragma comment(lib,"lib\\UsbDkHelperx86.lib")
#endif
#elif defined _M_X64
#if defined( _DEBUG)
#pragma comment(lib, "lib\\UsbDkHelperx64d.lib")
#else
#pragma comment(lib, "lib\\UsbDkHelperx64.lib")
#endif
#endif
#else
#if defined _M_IX86
#if defined( _DEBUG)
#pragma comment(lib,"Install_Debug\\x86\\Win10Debug\\UsbDkHelper.lib")
#else
#pragma comment(lib,"Install\\x86\\Win10Release\\UsbDkHelper.lib")
#endif
#elif defined _M_X64
#if defined( _DEBUG)
#pragma comment(lib,"Install_Debug\\x64\\Win10Debug\\UsbDkHelper.lib")
#else
#pragma comment(lib,"Install\\x64\\Win10Release\\UsbDkHelper.lib")
#endif
#endif
#endif

#include "UsbDKWrap.h"

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

int control_transfer(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char *data, uint16_t wLength, unsigned int timeout)
{
  uint32_t length = sizeof(libusb_control_setup) + wLength;
  uint8_t *buffer = (uint8_t *)malloc(length);
  struct libusb_control_setup &setup = *(struct libusb_control_setup *)buffer;
  setup.bmRequestType = bmRequestType;
  setup.bRequest = bRequest;
  setup.wValue = wValue;
  setup.wIndex = wIndex;
  setup.wLength = wLength;

  USB_DK_TRANSFER_REQUEST xfer;
  ZeroMemory(&xfer, sizeof(USB_DK_TRANSFER_REQUEST));
  xfer.EndpointAddress = 0;
  xfer.TransferType = ControlTransferType;
  xfer.Buffer = buffer;
  xfer.BufferLength = length;

  TransferResult result;

  if (bmRequestType & 0x80)
    result = UsbDk_ReadPipe(handle, &xfer, nullptr);// &ovlp);
  else {
    memcpy(buffer + sizeof(libusb_control_setup), data, wLength);
    result = UsbDk_WritePipe(handle, &xfer, nullptr);// &ovlp);
  }

  if (result != TransferFailure) {

    if (bmRequestType & 0x80)
      memcpy(data, buffer + sizeof(libusb_control_setup), (size_t)xfer.Result.GenResult.BytesTransferred);

    free(buffer);
    return (int)xfer.Result.GenResult.BytesTransferred;
  }
  else {
    free(buffer);
    return -1;
  }
}

int bulk_transfer(HANDLE handle,
  uint8_t endpoint, uint8_t *data, int length,
  int *actual_length, OVERLAPPED * ovlp, unsigned int timeout)
{
  USB_DK_TRANSFER_REQUEST xfer;
  ZeroMemory(&xfer, sizeof(USB_DK_TRANSFER_REQUEST));
  xfer.EndpointAddress = endpoint;
  xfer.TransferType = BulkTransferType;
  xfer.Buffer = data;
  xfer.BufferLength = length;

  TransferResult result;
  if (endpoint & 0x80)
    result = UsbDk_ReadPipe(handle, &xfer, ovlp);
  else
    result = UsbDk_WritePipe(handle, &xfer, ovlp);

  if(ovlp)
    WaitForSingleObject(ovlp->hEvent, timeout);

  USBD_STATUS status = (USBD_STATUS)xfer.Result.GenResult.UsbdStatus;
  *actual_length = (int)xfer.Result.GenResult.BytesTransferred;

  switch (result)
  {
  case TransferFailure: return -1;
  case TransferSuccess: return 0;
  default:
    return 1;
  }
}
