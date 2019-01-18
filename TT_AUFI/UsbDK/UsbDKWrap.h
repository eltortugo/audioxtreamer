#pragma once


int control_transfer(HANDLE handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
  unsigned char *data, uint16_t wLength, unsigned int timeout);

int bulk_transfer(HANDLE handle,
  uint8_t endpoint, uint8_t *data, int length,
  int *actual_length, OVERLAPPED * ovlp, unsigned int timeout);
