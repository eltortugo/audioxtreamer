#include "stdafx.h"

#include <process.h>
#include <stdio.h>


#include "CypressDevice.h"

#include "UsbDK\UsbDKWrap.h"
#include "ZTEXDev\ztexdev.h"
#include "resource.h"


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

uint8_t setyb[256];

CypressDevice::CypressDevice(UsbDeviceClient & client, ASIOSettings::Settings &params )
  : UsbDevice(client,params)
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

  for (int b = 0; b < 256; ++b)
  {
    setyb [b] = ((b & 128) >> 7) |
      ((b & 64) >> 5) |
      ((b & 32) >> 3) |
      ((b & 16) >> 1) |
      ((b & 8) << 1) |
      ((b & 4) << 3) |
      ((b & 2) << 5) |
      ((b & 1) << 7);
  }

  HRSRC hrc = FindResource(NULL, MAKEINTRESOURCE(IDR_FPGA_BIN), _T("RC_DATA"));
  HGLOBAL hg = LoadResource(NULL, hrc);
  uint8_t* bits = (uint8_t*)LockResource(hg);
  mBitstream = nullptr;

  if (bits)
  {
    mRsize = SizeofResource(NULL, hrc);
    
    uint8_t * bitstream = (uint8_t *)malloc(mRsize +512);
    ZeroMemory(bitstream, 512);

    uint8_t* buf = bitstream + 512;
    for (uint32_t c = 0; c < mRsize; c++, buf++) {
      register uint8_t b = bits[c];
      *buf = setyb[b];
    }

    mBitstream = bitstream;
    mRsize += 512;
  }
}

CypressDevice::~CypressDevice()
{
  LOG0("CypressDevice::~CypressDevice");

  if (mBitstream) {
    free(mBitstream);
    mBitstream = nullptr;
  }

  if (mExitHandle != INVALID_HANDLE_VALUE)
    DebugBreak();

  unload_usbdk();
  CloseHandle(hSem);
}

bool CypressDevice::Start()
{
  LOG0("CypressDevice::Start");

  //let's try to identify the fpga
  int64_t get_result = ztex_default_lsi_get1(mDevHandle, 0);
  if (get_result != 0x47545254)
    return false;


  if (hth_Worker != INVALID_HANDLE_VALUE) {
    DWORD dwExCode;
    GetExitCodeThread(hth_Worker, &dwExCode);
    if (dwExCode == STILL_ACTIVE)
      return true;
  }

  hth_Worker = (HANDLE)_beginthread(StaticWorkerThread, 0, this);
  if (hth_Worker != INVALID_HANDLE_VALUE) {
    ::SetThreadPriority(hth_Worker, THREAD_PRIORITY_TIME_CRITICAL);
    return true;
  }
  return false;
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

int GetDeviceID(USB_DK_DEVICE_ID * ret)
{
  const int id_vendor = 0x221A;  	// ZTEX vendor ID
  const int id_product = 0x100; 	// default product ID for ZTEX firmware

  PUSB_DK_DEVICE_INFO DevInfo = nullptr;
  ULONG DevCount = 0;
  int dev_idx = -1;

  BOOL r = UsbDk.GetDevicesList(&DevInfo, &DevCount);
  if (r == FALSE)
    goto exit;

  // find device
 
  for (ULONG i = 0; i < DevCount; i++) {
    USB_DEVICE_DESCRIPTOR *desc = &DevInfo[i].DeviceDescriptor;
    if ((desc->idVendor == id_vendor) && (desc->idProduct == id_product)) {
      dev_idx = i;
      break;
    }
  }

  if(dev_idx >= 0 && ret)
    *ret = DevInfo[dev_idx].ID;

  exit:

  if (DevInfo)
    UsbDk.ReleaseDevicesList(DevInfo);

  return dev_idx;
}

//---------------------------------------------------------------------------------------------

bool CypressDevice::Open()
{
  LOG0("CypressDevice::Open");
  ztex_device_info info;
  mDefOutEP = 0;
  mDefInEP = 0;
  memset(&info, 0, sizeof(ztex_device_info));

  if (!load_usbdk())
    return false;

  HANDLE handle = INVALID_HANDLE_VALUE;

  USB_DK_DEVICE_ID devid;
  if (GetDeviceID(&devid) < 0)
    return false;

  handle = UsbDk.StartRedirect(&devid);

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
   if (mBitstream != nullptr) {
#define EP0_TRANSACTION_SIZE 2048
#define BUF_SIZE 32768

      // reset FPGA
      TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x31, 0, 0, NULL, 0, 1500));
      // transfer data
      status = 0;
      uint32_t last_idx = mRsize % BUF_SIZE;
      int bufs_idx = (mRsize / BUF_SIZE) + ( last_idx == 0 ? 0 : 1);
      //TODO: simplify this to a single loop
      for (int i = 0; (status >= 0) && (i<bufs_idx); i++) {
        int j = i == (bufs_idx - 1) ? last_idx : BUF_SIZE;
        uint8_t *buf = mBitstream + (i*BUF_SIZE);

        for (int k = 0; (status >= 0) && (k<j); k += EP0_TRANSACTION_SIZE) {
          int l = j - k < EP0_TRANSACTION_SIZE ? j - k : EP0_TRANSACTION_SIZE;
          TWO_TRIES(status, (int)control_transfer(handle, 0x40, 0x32, 0, 0, buf + k, l, 1500));
          if (status<0) {
            status = -1;
          }
          else if (status != l) {
            status = -1;
          }
        }
      }
    } else {
     goto err;
    }

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

  ztex_default_reset(handle, 0);

  status = 0;
  goto noerr;

err:
  status = 1;
   if(handle !=  INVALID_HANDLE_VALUE)
    UsbDk.StopRedirect(handle);
noerr:

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
    BOOL result = UsbDk.StopRedirect(mDevHandle);
    //wait for disconnection before a reattempt to open
    Sleep(10);
    mDevHandle = INVALID_HANDLE_VALUE;
  }

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

bool CypressDevice::IsPresent() {
  return mDevHandle != INVALID_HANDLE_VALUE;
}

#define LAP(start,stop) ((uint32_t)(((stop.QuadPart - start.QuadPart) * 1000000) / lif.QuadPart))

void CypressDevice::main()
{
  LOG0("CypressDevice::main");
  if ( mDevHandle == INVALID_HANDLE_VALUE)
    //signal the parent of the thread failure
    return;

  HANDLE handles[2] = { 0,0 };
  handles[0] = CreateEvent(NULL, FALSE, FALSE, _T("TORTUGASIO_TX_EVENT"));
  handles[1] = CreateEvent(NULL, FALSE, FALSE, _T("TORTUGASIO_RX_EVENT"));
  mExitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
  ResetEvent(mExitHandle);

  OVERLAPPED tx_ovlp;
  OVERLAPPED rx_ovlp;

  tx_ovlp.hEvent = handles[0];
  rx_ovlp.hEvent = handles[1];

  const uint32_t nrIns = (devParams[NrIns].val+1)*2;
  const uint32_t nrOuts = (devParams[NrOuts].val+1)*2;
  const uint32_t nrSamples = devParams[NrSamples].val;
  const uint32_t fifoDepth = devParams[FifoDepth].val;

  const uint32_t InStride = nrIns * 3;
  const uint32_t InDataSize = RxHeaderSize + (InStride * nrSamples);
  const uint32_t INBuffSize = ALIGN_UP(InDataSize, USB_BLK);

  const uint32_t OUTStride = TxHeaderSize + (nrOuts * 3);
  const uint32_t OUTBuffSize = OUTStride * nrSamples;

  uint8_t *mINBuff = nullptr, *mOUTBuff = nullptr;
  devClient.AllocBuffers(INBuffSize * 2, mINBuff, OUTBuffSize * 2, mOUTBuff);

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

  ZeroMemory(&mDevStatus, sizeof(mDevStatus));
  uint16_t BuffNr = 0, Addr = 0;
  USB_DK_TRANSFER_REQUEST TXreq = { mDefOutEP, mOUTBuff, OUTBuffSize, BulkTransferType };
  USB_DK_TRANSFER_REQUEST RXreq = { mDefInEP, mINBuff, INBuffSize, BulkTransferType };



  while ( WaitForSingleObject( mExitHandle, 0) == WAIT_TIMEOUT ) {
    TXreq.Buffer = outPtr[BuffNr];
    RXreq.Buffer = inPtr[BuffNr];

    //TransferResult  wr_res = UsbDk_WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    TransferResult  rd_res = UsbDk.ReadPipe(mDevHandle, &RXreq, &rx_ovlp);
    TransferResult  wr_res = UsbDk.WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    //start of the heavy work
    //do as much as you can from here

    BuffNr ^= 1;
    int64_t get_result = ztex_default_lsi_get1(mDevHandle, 2);

    //data is on the way so we can pass what we got from the last operation to the client
    devClient.Switch(InStride , inPtr[BuffNr] + RxHeaderSize + (INBuffSize-InDataSize), OUTStride, outPtr[BuffNr] + TxHeaderSize);


    //2 steps to identify a timeout
    DWORD wresult = WaitForMultipleObjects(2, handles, false, 1000);
    DWORD result0 = WAIT_FAILED, result1 = WAIT_FAILED;
    switch (wresult)
    {
    case WAIT_OBJECT_0:
      result0 = WAIT_OBJECT_0;
      result1 = WaitForSingleObject(handles[1], 1000);
      break;
    case (WAIT_OBJECT_0 + 1):
      result1 = WAIT_OBJECT_0;
      result0 = WaitForSingleObject(handles[0], 1000);
      break;
    }

    //check that the fpga is pushing data and that we have usb connection with the fx2lp
    if ( result0 != WAIT_OBJECT_0 || result1 != WAIT_OBJECT_0 || get_result < 0)
    {
      LOGN("WorkerError result0:0x%08X result1:0x%08X get_result:%d\n", result0, result1 , get_result);
      devClient.DeviceStopped(true);
      break;
    }
    else
      mDevStatus.LastSR = uint32_t(get_result);

    uint8_t * ptr = (uint8_t *)RXreq.Buffer;
    mDevStatus.FifoLevel = ptr[2];
    mDevStatus.SkipCount = ptr[3];


    if(ptr[0] != 0x55 || ptr[1] != 0xaa || ptr[4] != 0xff || ptr[5] != 0x00) {
      //out of sync, reset fifo and reprogram
      //flush and init fifos
      status = ztex_xlabs_init_fifos(mDevHandle);
      status = ztex_default_lsi_set1(mDevHandle, 4, ch_params.u32);
      mDevStatus.ResyncErrors++;
    }
  }

  devClient.FreeBuffers(mINBuff, mOUTBuff);
 
  CloseHandle(handles[0]);
  CloseHandle(handles[1]);
  CloseHandle(mExitHandle);
  mExitHandle = INVALID_HANDLE_VALUE;
  LOG0("CypressDevice::main Exit");
}


bool CypressDevice::GetStatus(UsbDeviceStatus & status)
{
  if (mDevHandle != INVALID_HANDLE_VALUE) {
    if (mExitHandle != INVALID_HANDLE_VALUE) {
      status = mDevStatus;
    } else {
      int64_t result = ztex_default_lsi_get1(mDevHandle, 2);
      if (result < 0) {
        return false;
      } else {
        ZeroMemory(&status, sizeof(UsbDeviceStatus));
        status.LastSR = (uint32_t)result;
      }
    }
    return true;
  }
  return false;
}

