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

/** @file
The Core API for C.
An Introduction can be found in the <a href="index.html">Mainpage</a>
*/

  
#ifndef __ztex_H
#define __ztex_H


#include <stdint.h>

/// @cond macros
/// Expands to the directory seperator of current OS
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define DIRSEP "\\"
#else
#define DIRSEP "/" 
#endif

#define TWO_TRIES( status, cmd )  	\
{status = cmd;}				\
if ( status < 0 ) { status=cmd; }
/// @endcond

/** A structure with information about the device. */
struct ztex_device_info {
	/// product string, null-terminated
	unsigned char  product_string[128];
	/// serial number string, null-terminated
	unsigned char  sn_string[64];
	/// 2 for FX2, 3 for FX3
	uint8_t  fx_version;
	/// e.g. 2 for USB-FPGA Modules 2.14b; 255 means unknown
	uint8_t  board_series;
	/// e.g. 14 for USB-FPGA Modules 2.14b; 255 means unknown
	uint8_t  board_number;
	/// e.g. 'b' for USB-FPGA Modules 2.14b; null-terminated
	unsigned char  board_variant[3];
	/// Endpoint for fast FPGA configuration; 0 means unsupported
	uint8_t  fast_config_ep;
	/// Interface for fast FPGA configuration
	uint8_t  fast_config_if;
	/// default interface major version number; 0 means default interface not available
	uint8_t  default_version1;
	/// default interface minor version number
	uint8_t  default_version2;
	/// output endpoint of default interface; 255 means not available
	uint8_t  default_out_ep;
	/// input endpoint of default interface; 255 means not available
	uint8_t  default_in_ep;
} ;

typedef struct ztex_device_info ztex_device_info;

/** Scans the bus and performs certain operations on it. 
  * @param devs A null terminated list of devices
  * @param op Operation to perform. <0: print bus and ignore filters, 0: find a device using the filters, >0 print all devices matching the filters
  * @param id_vendor,id_product Filter by USB ID's that specify the device, ignored if negative
  * @param busnum,devnum Filter by bus number and device address, ignored if negative
  * @param sn Filter by serial number, ignored if NULL
  * @param ps Filter by product string, ignored if NULL
  * @return The device index if filter result unique, otherwise -1
  */


int ztex_get_device_info(HANDLE handle, ztex_device_info *info);
//int ztex_print_device_info(char* sbuf, int sbuflen, const ztex_device_info *info);

int ztex_get_fpga_config(HANDLE handle);

int ztex_default_gpio_ctl (HANDLE handle, int mask, int value) ;
int ztex_default_reset(HANDLE handle,  int leave );
int ztex_xlabs_init_fifos(HANDLE handle);
int ztex_default_lsi_set1 (HANDLE handle, uint8_t addr, uint32_t val);
int ztex_default_lsi_set2 (HANDLE handle, uint8_t addr, const uint32_t *val, int length);
int ztex_default_lsi_set3 (HANDLE handle, const uint8_t *addr, const uint32_t *val, int length);
int64_t ztex_default_lsi_get1 (HANDLE handle, uint8_t addr);
int ztex_default_lsi_get2 (HANDLE handle, uint8_t addr, uint32_t *val, int length);

#endif
