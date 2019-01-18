#include "stdafx.h"

#include <process.h>
#include <stdio.h>


#include "CypressDevice.h"

#include "UsbDK\UsbDKWrap.h"
#include "ZTEXDev\ztexdev.h"


#include <tchar.h>


#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Rpcrt4.lib")

#define out_fifo_size  0b0001// 64
#define RxHeaderSize 6
#define TxHeaderSize 8

int gcd(int a, int b)
{
  int r; // remainder
  while (b > 0) {
    r = a % b;
    a = b;
    b = r;
  }
  return a;
}

#define LCM(a,b) (((a)*(b))/gcd(a,b))




UsbDevice * UsbDevice::CreateDevice(UsbDeviceClient & client, uint32_t NrOuts, uint32_t NrIns, uint32_t NrSamples)
{
  return new CypressDevice(client, NrOuts, NrIns, NrSamples);
}


CypressDevice::CypressDevice(UsbDeviceClient & client, uint32_t NrOuts, uint32_t NrIns, uint32_t NrSamples)
  : devClient(client),
  mNrIns(NrIns),
  mNrOuts(NrOuts),
  mNrSamples(NrSamples)
{
  LOG0("CypressDevice::CypressDevice");

  mDefOutEP = 0;
  mDefInEP = 0;
  mDevHandle = INVALID_HANDLE_VALUE;
  mExitHandle = INVALID_HANDLE_VALUE;
}

CypressDevice::~CypressDevice()
{
  LOG0("CypressDevice::~CypressDevice");
  if (mExitHandle != INVALID_HANDLE_VALUE)
    DebugBreak();
}

uint32_t CypressDevice::GetFifoSize() {
  LOG0("CypressDevice::GetFifoSize");
  return ((out_fifo_size + 1) << 2);
}

bool CypressDevice::Start()
{
  LOG0("CypressDevice::Start");

  DWORD dwExCode;

  if (hth_Worker != INVALID_HANDLE_VALUE) {
    GetExitCodeThread(hth_Worker, &dwExCode);
    if (dwExCode == STILL_ACTIVE)
      return true;
  }

  hth_Worker = (HANDLE)_beginthread(StaticWorkerThread, 0, this);
  if (hth_Worker != INVALID_HANDLE_VALUE) {
    ::SetThreadPriority(hth_Worker, THREAD_PRIORITY_TIME_CRITICAL);
    return true;
  } return false;
}

bool CypressDevice::Stop(bool wait)
{
  LOG0("CypressDevice::Stop");

  
  if (mExitHandle != INVALID_HANDLE_VALUE) {
    if (SetEvent(mExitHandle) == 0) {
      LOG0("CypressDevice::Stop SetEvent Exit FAILED!");
      DebugBreak();
    }

    if (wait) {
      WaitForSingleObject(hth_Worker, INFINITE);
      hth_Worker = INVALID_HANDLE_VALUE;
    }

    return true;
  }
  else
    return false;
}

//---------------------------------------------------------------------------------------------

USB_DK_DEVICE_ID GetDeviceID()
{
  const int id_vendor = 0x221A;  	// ZTEX vendor ID
  const int id_product = 0x100; 	// default product ID for ZTEX firmware

  PUSB_DK_DEVICE_INFO DevInfo = nullptr;
  ULONG DevCount = 0;
  USB_DK_DEVICE_ID ret = { 0,0 };

  BOOL r = UsbDk_GetDevicesList(&DevInfo, &DevCount);
  if (r == FALSE)
    goto exit;

  // find device
  int dev_idx = -1;
  for (ULONG i = 0; i < DevCount; i++) {
    USB_DEVICE_DESCRIPTOR *desc = &DevInfo[i].DeviceDescriptor;
    if ((desc->idVendor == id_vendor) && (desc->idProduct == id_product)) {
      dev_idx = i;
      break;
    }
  }

  if (dev_idx<0) {
    if (dev_idx == -1)
      fprintf(stderr, "Error: No device found\n");
    goto exit;
  }

  ret = DevInfo[dev_idx].ID;
exit:
  if (DevInfo)
    UsbDk_ReleaseDevicesList(DevInfo);

  return ret;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::Open()
{
  LOG0("CypressDevice::Open");
  ztex_device_info info;
  mDefOutEP = 0;
  mDefInEP = 0;
  memset(&info, 0, sizeof(ztex_device_info));
  char *bitstream_fn = NULL, *bitstream_path = NULL;

  USB_DK_DEVICE_ID devid = GetDeviceID();
  HANDLE handle = UsbDk_StartRedirect(&devid);

  if (INVALID_HANDLE_VALUE == handle)
    goto err;

  BOOL status = UsbDk_ResetDevice(handle);
  if (status == FALSE) {
    fprintf(stderr, "Error: Unable to reset device\n");
    goto err;
  }

  status = ztex_get_device_info(handle, &info);
  if (status < 0) {
    fprintf(stderr, "Error: Unable to get device info\n");
    goto err;
  }

  mDefInEP = info.default_in_ep;
  mDefOutEP = info.default_out_ep;

  status = ztex_get_fpga_config(handle);
  if (status == -1)
    goto err;
  else if (status == 0) {
    // find bitstream
    bitstream_fn = ztex_find_bitstream(&info, bitstream_path ? bitstream_path : "F:\\devel\\FPGA\\ZTEX201\\USB32chAudio", "cy16_to_iis");
    if (bitstream_fn) {
      printf("Using bitstream '%s'\n", bitstream_fn);
      fflush(stdout);
    }
    else {
      fprintf(stderr, "Warning: Bitstream not found\n");
      goto nobitstream;
    }

    // read and upload bitstream
    FILE *fd = fopen(bitstream_fn, "rb");
    if (fd == NULL) {
      fprintf(stderr, "Warning: Error opening file '%s'\n", bitstream_fn);
      goto nobitstream;
    }
    status = ztex_upload_bitstream(handle, &info, fd, -1);
    fclose(fd);


  nobitstream:
    fflush(stderr);
    // check config
    status = ztex_get_fpga_config(handle);
    if (status < 0) {
      fprintf(stderr, "Error: Unable to get FPGA configuration state\n");
      goto err;
    }
    else if (status == 0) {
      fprintf(stderr, "Error: FPGA not configured\n");
      goto err;
    }
  }

  //ztex_default_gpio_ctl(handle, 0, 0);
  ztex_default_reset(handle, 0);

  status = 0;
  goto noerr;

err:
  status = 1;
noerr:
  if (bitstream_fn)
    free(bitstream_fn);

  mDevHandle = status == 0 ? handle : INVALID_HANDLE_VALUE;
  if (status == 0)
    return true;
  else
    return false;
}

//---------------------------------------------------------------------------------------------
bool CypressDevice::Close()
{
  LOG0("CypressDevice::Close");
  //Check Thread Handle
  DWORD dwExCode;

  if (hth_Worker != INVALID_HANDLE_VALUE) {
    GetExitCodeThread(hth_Worker, &dwExCode);
    if (dwExCode == STILL_ACTIVE)
      Stop(true);
  }

  if (mDevHandle != INVALID_HANDLE_VALUE) {
    UsbDk_StopRedirect(mDevHandle);
    //wait for disconnection before a reattempt to open
    do {
      Sleep(10);
    } while (GetDeviceID().DeviceID[0] == 0);
    mDevHandle = INVALID_HANDLE_VALUE;
  }

  return true;
}

//---------------------------------------------------------------------------------------------

bool CheckIntegrity(uint8_t *buffer, uint32_t stride, uint32_t nrPackets) //receive buffer of RxPayloadSize
{
  uint8_t * ptr = buffer;

  for (uint32_t i = 0; i < nrPackets; ++i)
    if (ptr[0] == 0x55 && ptr[1] == 0xAA && ptr[4] == 0xff && ptr[5] == 0x00)
      ptr += stride;
    else
      return false;

  return true;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::SendAsync(unsigned int size, unsigned int timeout)
{
  //int xfered = 0;
  //int r = bulk_transfer(mDevHandle, mDef_out_ep, mOUTBuff, TxPayloadSize , &xfered  , nullptr, 1000);
  return true;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::RecvAsync(unsigned int size, unsigned int timeout)
{
  //int xfered = 0;
  //OVERLAPPED ovlp;
  //ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  ////uint8_t buff[512] = { 0xAA, 0x55, 0, kNumInputs / 2, 0xFF , 0,0,0,0,0,0,0 };
  //ztex_default_reset(mDevHandle, 0);
  //bulk_transfer(mDevHandle, info->default_out_ep, mOUTBuff[0], TxPayloadSize, &xfered, nullptr, 1000);
  //int r = bulk_transfer(mDevHandle, info->default_in_ep, mINBuff[0], RxPayloadSize , &xfered, &ovlp, 1000);
  //CheckIntegrity(mINBuff[0]);

  //CloseHandle(ovlp.hEvent);
  return true;
}

//---------------------------------------------------------------------------------------------
#define USB_BLK 512
#define UDIV_UP(a, b) (((a) + (b) - 1) / (b))
#define ALIGN_UP(a, b) (UDIV_UP(a, b) * (b))

/*
nrOuts/nrIns
0  when "000",
1  when "001",
2  when "010",
4  when "011",
8  when "100",
12 when "101",
16 when "110",
max_sdo_lines when others; --nr SD Lines = 2x channels

constant header_size : natural := 3;
constant max_padding : natural := 32; -- number of words (fifo writes)
constant max_samples : natural := 32*4; -- nr *4
*/
//#define LOOPBACK_TEST
bool CypressDevice::IsRunning() {
  return mExitHandle != INVALID_HANDLE_VALUE;
}

void CypressDevice::WorkerThread()
{
  LOG0("CypressDevice::WorkerThread");
  if ( mDevHandle == INVALID_HANDLE_VALUE)
    //signal the parent of the thread failure
    return;

  HANDLE handles[2] = { 0,0 };
  handles[0] = CreateEvent(NULL, FALSE, FALSE, _T("TORTUGASIO_TX_EVENT"));
  handles[1] = CreateEvent(NULL, FALSE, FALSE, _T("TORTUGASIO_RX_EVENT"));
  mExitHandle = CreateEvent(NULL, TRUE, FALSE, _T("TORTUGASIO_EXIT_EVENT"));
  ResetEvent(mExitHandle);

  OVERLAPPED tx_ovlp;
  OVERLAPPED rx_ovlp;

  tx_ovlp.hEvent = handles[0];
  rx_ovlp.hEvent = handles[1];

  const uint32_t InStride = mNrIns * 3;
  const uint32_t InDataSize = RxHeaderSize + (InStride * mNrSamples);
  const uint32_t INBuffSize = ALIGN_UP(InDataSize, USB_BLK);
  uint8_t* mINBuff = new uint8_t[INBuffSize * 2];
  ZeroMemory(mINBuff, INBuffSize * 2);

  const uint32_t OUTStride = TxHeaderSize + (mNrOuts * 3);
  const uint32_t OUTBuffSize = OUTStride * mNrSamples;
  uint8_t* mOUTBuff = new uint8_t[OUTBuffSize * 2];
  ZeroMemory(mOUTBuff, OUTBuffSize * 2);
  uint8_t * inPtr[2] = { mINBuff , mINBuff+ INBuffSize };
  uint8_t * outPtr[2] = { mOUTBuff , mOUTBuff + OUTBuffSize };
  //initialize header mark
  struct { 
    uint16_t
      o : 3,
      i : 3,
      out_fifo  : 4;
  } prms1 = { 6 , 6, out_fifo_size };
  struct {
    uint16_t
      pad : 8,
      nrsamples : 8;
  } prms2 = { (uint8_t)((INBuffSize -InDataSize)/2) ,  (uint8_t)mNrSamples };

  for (uint32_t i = 0; i < mNrSamples*2 ; i++)
  {
    *(mOUTBuff + i*OUTStride) = 0xAA;
    *(mOUTBuff + i*OUTStride + 1) = 0x55;
    memcpy(mOUTBuff + i*OUTStride + 2, &prms1, sizeof(prms1));
    memcpy(mOUTBuff + i*OUTStride + 4, &prms2, sizeof(prms2));
    *(mOUTBuff + i*OUTStride + 6) = 0xFF;
    *(mOUTBuff + i*OUTStride + 7) = 0x00;
#ifdef LOOPBACK_TEST
    for (uint32_t c = 0; c < mNrOuts; ++c) {
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 0) = i % mNrSamples;
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 1) = i % mNrSamples;
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 2) = i % mNrSamples;
    }
#endif
  }

  int status, transferred = 0;
  ztex_xlabs_init_fifos(mDevHandle);

  //precharge the fifo to the max
  status = bulk_transfer(mDevHandle, mDefOutEP, mOUTBuff, OUTBuffSize*2, &transferred, nullptr , 1000);

  uint16_t BuffNr = 0, Addr = 0;
  USB_DK_TRANSFER_REQUEST TXreq = { mDefOutEP, mOUTBuff, OUTBuffSize, BulkTransferType };
  USB_DK_TRANSFER_REQUEST RXreq = { mDefInEP, mINBuff, INBuffSize, BulkTransferType };
  
  while ( WaitForSingleObject( mExitHandle, 0) == WAIT_TIMEOUT )
  {
    TXreq.Buffer = outPtr[BuffNr];
    RXreq.Buffer = inPtr[BuffNr];

    //TransferResult  wr_res = UsbDk_WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    TransferResult  rd_res = UsbDk_ReadPipe(mDevHandle, &RXreq, &rx_ovlp);
    TransferResult  wr_res = UsbDk_WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    //start of the heavy work
    //do as much as you can from here
    uint64_t readVal = ztex_default_lsi_get1(mDevHandle, (Addr--)>>4);

    BuffNr ^= 1;

    //data is on the way so we can pass what we got from the last operation to the client
    devClient.Switch(InStride , inPtr[BuffNr] + RxHeaderSize + (INBuffSize-InDataSize), OUTStride, outPtr[BuffNr] + TxHeaderSize);
    //stop of the heavy work

    //Work done, now we wait for the usb completion
    WaitForMultipleObjects(2, handles, true, 100);
    //WaitForSingleObject(tx_ovlp.hEvent, 100);
  }

  CloseHandle(handles[0]);
  CloseHandle(handles[1]);
  delete mINBuff;
  delete mOUTBuff;
  CloseHandle(mExitHandle);
  mExitHandle = INVALID_HANDLE_VALUE;
  LOG0("CypressDevice::WorkerThread Exit");
}






