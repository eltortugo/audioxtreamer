/*%
ZTEX Core API for C with examples
Copyright (C) 2009-2017 ZTEX GmbH.
http://www.ztex.de

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this file,
You can obtain one at http://mozilla.org/MPL/2.0/.

Alternatively, the contents of this file may be used under the terms
of the GNU General Public License Version 3, as described below:

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see http://www.gnu.org/licenses/.
%*/

/// @file ztex.h 

/** @mainpage
@brief
The C Core API of the <a href="http://www.ztex.de/firmware-kit/index.e.html">ZTEX SDK</a>.
<p>
This API is used for developing host software in C. It is a re-implementation a subset of the <a href="../../java/index.html">JAVA API</a>.
<p>
<h2>Features</h2>
The main features are:
<ul>
<li> Full support of <a href="http://www.ztex.de/firmware-kit/default.e.html">Default Interface</a>
<ul>
<li> Multiple communication interfaces: high speed, low speed, GPIO's, reset signal
<li> Compatibility allows board independent host software
</ul>
</li>
<li> Bitstream upload directly to the FPGA
<li> Utility functions (gathering information about FPGA Boards, finding devices using filters, ...)
</ul>

<p>
<h2>Usage, Examples</h2>
The library consists in the single file pair 'ztex.c' and 'ztex.h'. It has been tested with gcc and should work with no/minor
modifications with other compilers.
<p>
Its recommended to link the application directly against the object file.
<p>
The package contains two examples, 'ucecho.c' and 'memfifo.c' which demonstrate the usage of the API including the default interface.
<p>
Object files and examples can be built using make. Under Windows it has been tested with MSYS/MinGW, see
<a href="http://wiki.ztex.de/doku.php?id=en:software:windows_hints#hints">Hints for Windows users</a>


<h2>Related Resources</h2>
Additional information can be found at
<ul>
<li> <a href="http://www.ztex.de/firmware-kit/index.e.html">ZTEX SDK</a>
<li> <a href="http://wiki.ztex.de/">ZTEX Wiki</a>
</ul>
*/

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include "ztexdev.h"

#include "UsbBackend.h"

#define SPRINTF_INIT( buf, maxlen )  	\
char* buf__ = buf;			\
int bufptr__ = 0;			\
int bufremain__ = maxlen-1;		\
buf__[0]=0;

#define SPRINTF_RETURN return bufptr__;

#define SPRINTF( ... ) { 						\
    if ( bufremain__>0 ) {						\
	int bufincr = snprintf(buf__+bufptr__,bufremain__, __VA_ARGS__ );	\
	if ( bufincr>0 ) {						\
	    if (bufincr > bufremain__) bufincr = bufremain__;		\
	    bufptr__+=bufincr;						\
	    bufremain__-=bufincr;					\
	    buf__[bufptr__] = 0;					\
	}								\
    }									\
}


// ******* ztex_get_device_info ************************************************
/** Get device information.
* @param handle device handle
* @param info structure where device information are stored.
* @return the error code or 0, if no error occurs.
*/
int ztex_get_device_info(HANDLE handle, ztex_device_info *info) {

  unsigned char *buf = info->product_string; // product string is used as buffer
  int64_t status;

  // VR 0x33: fast configuration info
  /*TWO_TRIES(status, control_transfer(handle, 0xc0, 0x33, 0, 0, buf, 128, 1500));
  info->fast_config_ep = status > 0 ? buf[0] : 0;
  info->fast_config_if = status == 2 ? buf[1] : 0;
  */
  // VR 0x3b: configuration data
  TWO_TRIES(status, control_transfer(handle, 0xc0, 0x3b, 0, 0, buf, 128, 1500));
  if (status < 0) status = control_transfer(handle, 0xc0, 0x3b, 0, 0, buf, 128, 1500);
  if ((status == 128) && (buf[0] == 67) && (buf[1] == 68) && (buf[2] == 48)) {
    info->fx_version = buf[3];
    info->board_series = buf[4];
    info->board_number = buf[5];
    info->board_variant[0] = buf[6];
    info->board_variant[1] = buf[7];
    info->board_variant[2] = 0;
  }
  else {
    info->fx_version = 0;
    info->board_series = 255;
    info->board_number = 255;
    info->board_variant[0] = 0;
  }

  // VR 0x64: default interface info
  TWO_TRIES(status, control_transfer(handle, 0xc0, 0x64, 0, 0, buf, 128, 1500));
  if (status>2 && buf[0]>0) {
    info->default_version1 = buf[0];
    info->default_version2 = status>3 ? buf[3] : 0;
    info->default_out_ep = buf[1] & 127;
    info->default_in_ep = buf[2] | 128;
  }
  else {
    info->default_version1 = 0;
    info->default_version2 = 0;
    info->default_out_ep = 255;
    info->default_in_ep = 255;
  }
  /*
  // string descriptors
  status = libusb_get_device_descriptor(libusb_get_device(handle), &dev_desc);
  if (status < 0) return status;
  if (libusb_get_string_descriptor_ascii(handle, dev_desc.iProduct, info->product_string, 128) < 0) info->product_string[0] = 0;
  if (libusb_get_string_descriptor_ascii(handle, dev_desc.iSerialNumber, info->sn_string, 64) < 0) info->sn_string[0] = 0;
  */
  return 0;
}


//// ******* ztex_print_device_info **********************************************
///** Print device info to a string.
//* The output may be truncated and is always null-terminated.
//* @param sbuf string buffer for output
//* @param sbuflen length of the string buffer
//* @param info structure where device information are stored.
//* @return length of the output string (may be truncated)
//*/
//int ztex_print_device_info(char* sbuf, int sbuflen, const ztex_device_info *info) {
//  SPRINTF_INIT(sbuf, sbuflen);
//  SPRINTF("Product: '%s'\n", info->product_string);
//  SPRINTF("Serial number: '%s'\n", info->sn_string);
//  SPRINTF("USB-Controller: %s\n", info->fx_version == 2 ? "EZ-USB FX2" : info->fx_version == 3 ? "EZ-USB FX3" : "unknown");
//  if ((info->board_series<255) && (info->board_number<255))
//    SPRINTF("Board: ZTEX USB-FPGA %d.%.2d%s\n", info->board_series, info->board_number, info->board_variant)
//  else
//    SPRINTF("Board: unknown\n")
//    if (info->fast_config_ep)
//      SPRINTF("High speed FPGA configuration: EP %d,  IF %d\n", info->fast_config_ep, info->fast_config_if)
//    else
//      SPRINTF("High speed FPGA configuration: not supported\n")
//      if ((info->default_version1>0))
//        SPRINTF("Default interface: v%d.%d,  OUT EP %d,  IN EP %d\n", info->default_version1, info->default_version2, info->default_out_ep & 127, info->default_in_ep & 127)
//      else
//        SPRINTF("Default interface: not available\n")
//        SPRINTF_RETURN;
//}


// ******* ztex_get_fpga_config ************************************************
/** Get device information.
* @param handle device handle
* @return <0 if an USB error occurred, 0 if unconfigured, 1 if configured
*/
int ztex_get_fpga_config(HANDLE handle) {
  unsigned char buf[16];
  int64_t status;
  TWO_TRIES(status, control_transfer(handle, 0xc0, 0x30, 0, 0, buf, 16, 1500));
  return status < 0 ? (int)status : status == 0 ? -255 : buf[0] == 0 ? 1 : 0;
}

// ******* ztex_default_gpio_ctl ***********************************************
/**
* Reads and modifies the 4 GPIO pins.
* @param handle device handle
* @param mask Bitmask for the pins which are modified. 1 means a bit is set. Only the lowest 4 bits are significant.
* @param value The bit values which are to be set. Only the lowest 4 bits are significant.
* @return current values of the GPIO's or <0 if an USB error occurred
*/
int ztex_default_gpio_ctl(HANDLE handle, int mask, int value) {
  unsigned char buf[8];
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0xc0, 0x61, value, mask, buf, 8, 1500));
  return status < 0 ? status : buf[0];
}

// ******* ztex_default_reset **************************************************
/**
* Assert the reset signal.
* @param handle device handle
* @param leave if >0, the signal is left active. Otherwise only a short impulse is sent.
* @return 0 on success or <0 if an USB error occurred
*/
int ztex_default_reset(HANDLE handle, int leave) {
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x60, leave ? 1 : 0, 0, NULL, 0, 1500));
  return status;
}


int ztex_xlabs_init_fifos(HANDLE handle) {
  int status;
  TWO_TRIES( status, (int)control_transfer(handle, 0x40, 0x70, 0, 0, NULL, 0, 1500));
  return status;
}


// ******* ztex_default_lsi_set1 ***********************************************
/**
* Send data to the low speed interface of default firmwares.
* It's implemented as a SRAM-like interface and is typically used used to read/write configuration data, debug information or other things.
* This function sets one register.
* @param handle device handle
* @param addr The address. Valid values are 0 to 255.
* @param val The register data with a width of 32 bit.
* @return 0 on success or <0 if an USB error occurred
*/
int ztex_default_lsi_set1(HANDLE handle, uint8_t addr, uint32_t val) {
  uint8_t buf[] = { addr, (uint8_t)(val), (uint8_t)(val >> 8), (uint8_t)(val >> 16), (uint8_t)(val >> 24) };
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x62, 0, 0, buf, 5, 1500));
  return status;
}


// ******* ztex_default_lsi_set2 ***********************************************
/**
* Send data to the low speed interface of default firmwares.
* It's implemented as a SRAM-like interface and is typically used used to read/write configuration data, debug information or other things.
* This function sets a sequential set of registers.
* @param handle device handle
* @param addr The starting address address. Valid values are 0 to 255. Address is wrapped from 255 to 0.
* @param val The register data array with a word width of 32 bit.
* @param length The length of the data array.
* @return 0 on success or <0 if an USB error occurred
*/
int ztex_default_lsi_set2(HANDLE handle, uint8_t addr, const uint32_t *val, int length) {
  if (length<0) return 0;
  int ia = length - 256;
  if (ia<0) ia = 0;
  uint8_t *buf = (uint8_t *)malloc((length - ia) * 5);
  for (int i = ia; i<length; i++) {
    buf[(i - ia) * 5 + 0] = addr + i;
    buf[(i - ia) * 5 + 1] = val[i];
    buf[(i - ia) * 5 + 2] = val[i] >> 8;
    buf[(i - ia) * 5 + 3] = val[i] >> 16;
    buf[(i - ia) * 5 + 4] = val[i] >> 24;
  }
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x62, 0, 0, buf, (length - ia) * 5, 1500));
  free(buf);
  return status;
}


// ******* ztex_default_lsi_set3 **********************************************
/**
* Send data to the low speed interface of default firmwares.
* It's implemented as a SRAM-like interface and is typically used used to read/write configuration data, debug information or other things.
* This function sets a sequential set of registers.
* @param handle device handle
* @param addr The register addresses. Valid values are 0 to 255.
* @param val The register data array with a word width of 32 bit.
* @param length The length of the data array.
* @return 0 on success or <0 if an USB error occurred
*/
int ztex_default_lsi_set3(HANDLE handle, const uint8_t *addr, const uint32_t *val, int length) {
  if (length<0) return 0;
  int ia = length - 256;
  if (ia<0) ia = 0;
  uint8_t *buf = (uint8_t *)malloc((length - ia) * 5);
  for (int i = ia; i<length; i++) {
    buf[(i - ia) * 5 + 0] = addr[i];
    buf[(i - ia) * 5 + 1] = val[i];
    buf[(i - ia) * 5 + 2] = val[i] >> 8;
    buf[(i - ia) * 5 + 3] = val[i] >> 16;
    buf[(i - ia) * 5 + 4] = val[i] >> 24;
  }                         
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x62, 0, 0, buf, (length - ia) * 5, 1500));
  free(buf);
  return status;
}


// ******* ztex_default_lsi_get1 ***********************************************
/**
* Read data from the low speed interface of default firmwares.
* It's implemented as a SRAM-like interface and is typically used used to read/write configuration data, debug information or other things.
* This function reads one register.
* @param handle device handle
* @param addr The address. Valid values are 0 to 255.
* @return The unsigned register value (32 Bits) or <0 if an error occurred.
*/
int64_t ztex_default_lsi_get1(HANDLE handle, uint8_t addr) {
  uint8_t buf[4];
  int64_t status;
  TWO_TRIES(status, control_transfer(handle, 0xc0, 0x63, 0, addr, buf, 4, 1500));
  return status<0 ? status : buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// ******* ztex_default_lsi_get2 ***********************************************
/**
* Read data from the low speed interface of default firmwares.
* It's implemented as a SRAM-like interface and is typically used used to read/write configuration data, debug information or other things.
* This function reads a sequential set of registers.
* @param handle device handle
* @param addr The start address. Valid values are 0 to 255. Address is wrapped from 255 to 0.
* @param val The array where to store the register data with a word width of 32 Bit.
* @param length The amount of register to be read.
* @return 0 on success or <0 if an USB error occurred
*/
int ztex_default_lsi_get2(HANDLE handle, uint8_t addr, uint32_t *val, int length) {
  int l = length;
  if (l>256) l = 256;
  uint8_t *buf = (uint8_t *)malloc(l * 4);
  int status;
  TWO_TRIES(status, (int)control_transfer(handle, 0xc0, 0x63, 0, addr, buf, l * 4, 1500));
  if (status < 0) return status;
  for (int i = 0; i<length; i++) {
    int j = i & 255;
    val[i] = buf[j * 4 + 0] | (buf[j * 4 + 1] << 8) | (buf[j * 4 + 2] << 16) | (buf[j * 4 + 3] << 24);
  }
  free(buf);
  return 0;
}
