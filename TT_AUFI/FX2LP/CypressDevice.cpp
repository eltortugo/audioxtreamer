#include "stdafx.h"

#include <process.h>
#include <stdio.h>


#include "CypressDevice.h"

#include "UsbDK\UsbDKWrap.h"
#include "ZTEXDev\ztexdev.h"


#include <tchar.h>


#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Rpcrt4.lib")

#define out_fifo_size  0b0011
#define RxHeaderSize 6
#define TxHeaderSize 4

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

using namespace ASIOSettings;

CypressDevice::CypressDevice(UsbDeviceClient & client, ASIOSettings::Settings &params )
  : devClient(client)
  , devParams(params)
{
  LOG0("CypressDevice::CypressDevice");

  mDefOutEP = 0;
  mDefInEP = 0;
  mDevHandle = INVALID_HANDLE_VALUE;
  mExitHandle = INVALID_HANDLE_VALUE;

  hSem = CreateSemaphore(
    NULL,           // default security attributes
    1,  // initial count
    1,  // maximum count
    NULL);
}

CypressDevice::~CypressDevice()
{
  LOG0("CypressDevice::~CypressDevice");
  if (mExitHandle != INVALID_HANDLE_VALUE)
    DebugBreak();

  unload_usbdk();
  CloseHandle(hSem);
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
    BOOL result = SetEvent(mExitHandle);
    if (result == 0) {
      LOG0("CypressDevice::Stop SetEvent Exit FAILED!");
      wait = false;
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

  BOOL r = UsbDk.GetDevicesList(&DevInfo, &DevCount);
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
    UsbDk.ReleaseDevicesList(DevInfo);

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

  if (!load_usbdk())
    return false;

  USB_DK_DEVICE_ID devid = GetDeviceID();
  HANDLE handle = UsbDk.StartRedirect(&devid);

  if (INVALID_HANDLE_VALUE == handle)
    goto err;

  BOOL status = UsbDk.ResetDevice(handle);
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
    UsbDk.StopRedirect(mDevHandle);
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


/*Register Description accessed through the lsi 256 32bit regs
  0x00 COOKIE must read allways "TRTG"
  0x01 VERSION NR
  0x02 Detected word clock refs(16)/cycles(16) with 48mhz as ref so
       a perfect 48k sampling rate should count 0x0000BB80
  0x03 Debug register 
  0x04  b3: padding | b2: fifo depth | b1: nr_samples | b0(ins):ins  | b0(4):outs

  0x08 i/o matrix mapping
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

  const uint32_t nrIns = (devParams[NrIns].val+1)*2;
  const uint32_t nrOuts = (devParams[NrOuts].val+1)*2;
  const uint32_t nrSamples = devParams[NrSamples].val;
  const uint32_t fifoDepth = devParams[FifoDepth].val-1;

  const uint32_t InStride = nrIns * 3;
  const uint32_t InDataSize = RxHeaderSize + (InStride * nrSamples);
  const uint32_t INBuffSize = ALIGN_UP(InDataSize, USB_BLK);
  uint8_t* mINBuff = new uint8_t[INBuffSize * 2];
  ZeroMemory(mINBuff, INBuffSize * 2);

  const uint32_t OUTStride = TxHeaderSize + (nrOuts * 3);
  const uint32_t OUTBuffSize = OUTStride * nrSamples;
  uint8_t* mOUTBuff = new uint8_t[OUTBuffSize * 2];
  ZeroMemory(mOUTBuff, OUTBuffSize * 2);
  uint8_t * inPtr[2] = { mINBuff , mINBuff+ INBuffSize };
  uint8_t * outPtr[2] = { mOUTBuff , mOUTBuff + OUTBuffSize };
  //initialize header mark
 
  for (uint32_t i = 0; i < nrSamples*2 ; i++)
  {
    *(mOUTBuff + i*OUTStride) = 0xAA;
    *(mOUTBuff + i*OUTStride + 1) = 0x55;
    *(mOUTBuff + i*OUTStride + 2) = 0x55;
    *(mOUTBuff + i*OUTStride + 3) = 0xAA;
#ifdef LOOPBACK_TEST
    for (uint32_t c = 0; c < mNrOuts; ++c) {
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 0) = i % mNrSamples;
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 1) = i % mNrSamples;
      *(mOUTBuff + i*OUTStride + TxHeaderSize + c * 3 + 2) = i % mNrSamples;
    }
#endif
  }

  // configure the fpga channel params
  int status, transferred = 0;
  union {
    struct { uint32_t outs : 4, ins : 4, samples : 8, fifo : 8, padding : 8; };
    uint32_t u32;
  } ch_params = {
    (uint32_t)devParams[NrOuts].val , (uint32_t)devParams[NrIns].val, nrSamples, fifoDepth, (uint8_t)((INBuffSize - InDataSize) / 2)
  };


  //flush and init fifos
  status = ztex_xlabs_init_fifos(mDevHandle);

  status = ztex_default_lsi_set1(mDevHandle, 4, ch_params.u32);

 

  for (uint8_t r = 0; r < 8; r++)
    mDevStatus.st[r] = (uint32_t)ztex_default_lsi_get1(mDevHandle, r);

  uint16_t BuffNr = 0, Addr = 0;
  USB_DK_TRANSFER_REQUEST TXreq = { mDefOutEP, mOUTBuff, OUTBuffSize, BulkTransferType };
  USB_DK_TRANSFER_REQUEST RXreq = { mDefInEP, mINBuff, INBuffSize, BulkTransferType };

  while ( WaitForSingleObject( mExitHandle, 0) == WAIT_TIMEOUT )
  {
    TXreq.Buffer = outPtr[BuffNr];
    RXreq.Buffer = inPtr[BuffNr];

    //TransferResult  wr_res = UsbDk_WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    TransferResult  rd_res = UsbDk.ReadPipe(mDevHandle, &RXreq, &rx_ovlp);
    TransferResult  wr_res = UsbDk.WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    //start of the heavy work
    //do as much as you can from here

    //if (WaitForSingleObject(hSem, 0) == WAIT_OBJECT_0)
    //{
    //  for (uint8_t r = 0; r < 8; r++)
    //    mDevStatus.st[0] = (uint32_t)ztex_default_lsi_get1(mDevHandle, 0);
    //  ReleaseSemaphore(hSem, 1, NULL);
    //}

    BuffNr ^= 1;

    
    //data is on the way so we can pass what we got from the last operation to the client
    devClient.Switch(InStride , inPtr[BuffNr] + RxHeaderSize + (INBuffSize-InDataSize), OUTStride, outPtr[BuffNr] + TxHeaderSize);
    //stop of the heavy work

    //Work done, now we wait for the usb completion
    WaitForMultipleObjects(2, handles, true, 100);
    //WaitForSingleObject(tx_ovlp.hEvent, 100);
    uint8_t * ptr = (uint8_t *)RXreq.Buffer;
    /*ASSERT(ptr[0] == 0x55 &&
           ptr[1] == 0xaa &&
           //ptr[2] >  0 &&
           ptr[4] == 0xff &&
           ptr[5] == 0x00);*/
  }

  CloseHandle(handles[0]);
  CloseHandle(handles[1]);
  delete mINBuff;
  delete mOUTBuff;
  CloseHandle(mExitHandle);
  mExitHandle = INVALID_HANDLE_VALUE;
  LOG0("CypressDevice::WorkerThread Exit");
}

//---------------------------------------------------------------------------------------------
bool CypressDevice::GetStatus(UsbDeviceStatus &param)
{
  bool result = false;
  if (mDevHandle != INVALID_HANDLE_VALUE)
    if (WaitForSingleObject(hSem, 0) == WAIT_OBJECT_0)
    {
      if(mExitHandle != INVALID_HANDLE_VALUE)
        param = mDevStatus;
      else
        for (uint8_t r = 0; r < 8; r++)
          param.st[r] = (uint32_t)ztex_default_lsi_get1(mDevHandle, r);


      ReleaseSemaphore(hSem, 1, NULL);
      result = true;
    }

  return result; 
}







