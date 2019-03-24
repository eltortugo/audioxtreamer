#include "stdafx.h"

#include <process.h>
#include <stdio.h>


#include "CypressDevice.h"

#include "UsbBackend.h"

#include "ZTEXDev\ztexdev.h"
#include "resource.h"


#include <tchar.h>


#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Rpcrt4.lib")


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
  hth_Worker = INVALID_HANDLE_VALUE;
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
    mResourceSize = SizeofResource(NULL, hrc);
    
    uint8_t * bitstream = (uint8_t *)malloc(mResourceSize +512);
    ZeroMemory(bitstream, 512);

    uint8_t* buf = bitstream + 512;
    for (uint32_t c = 0; c < mResourceSize; c++, buf++) {
      register uint8_t b = bits[c];
      *buf = setyb[b];
    }

    mBitstream = bitstream;
    mResourceSize += 512;
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


  CloseHandle(hSem);
}

bool CypressDevice::Start()
{
  LOG0("CypressDevice::Start");
  if (mDevHandle == INVALID_HANDLE_VALUE)
    return false;
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

bool CypressDevice::Open()
{
  LOG0("CypressDevice::Open");
  HANDLE handle = INVALID_HANDLE_VALUE;
  ztex_device_info info;
  mDefOutEP = 0;
  mDefInEP = 0;
  memset(&info, 0, sizeof(ztex_device_info));

  if (!bknd_open(handle, mFileHandle))
    goto err;

  int status = ztex_get_device_info(handle, &info);
  if (status < 0) {
    fprintf(stderr, "Error: Unable to get device info\n");
    goto err;
  }

  mDefInEP = info.default_in_ep;
  mDefOutEP = info.default_out_ep;

  //status = ztex_get_fpga_config(handle);
  //if (status == -1)
  //  goto err;
  //else if (status == 0)
  {

   if (mBitstream != nullptr) {
#define EP0_TRANSACTION_SIZE 2048

      // reset FPGA
      status = (BOOL)control_transfer(handle, 0x40, 0x31, 0, 0, NULL, 0, 1500);
      // transfer data
      status = 0;
      uint32_t last_idx = mResourceSize % EP0_TRANSACTION_SIZE;
      int bufs_idx = (mResourceSize / EP0_TRANSACTION_SIZE);

      for (int i = 0; (status >= 0) && (i < bufs_idx); i++)
        status =(BOOL)control_transfer(handle, 0x40, 0x32, 0, 0, mBitstream + (i * EP0_TRANSACTION_SIZE), EP0_TRANSACTION_SIZE, 1500);

      if (last_idx)
        status =(BOOL)control_transfer(handle, 0x40, 0x32, 0, 0, mBitstream + bufs_idx * EP0_TRANSACTION_SIZE, last_idx, 1500);

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
  if (handle != INVALID_HANDLE_VALUE)
    bknd_close(handle, mFileHandle);

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

    bknd_close(mDevHandle, mFileHandle);
    //wait for disconnection before a reattempt to open
    Sleep(10);
    mDevHandle = INVALID_HANDLE_VALUE;
  }

  return true;
}

//---------------------------------------------------------------------------------------------


/*Register Description accessed through the lsi 256 32bit regs
  0x00 COOKIE must read allways "TRTG"
  0x01 VERSION NR
  0x02 Detected word clock refs(16)/cycles(16) with 48mhz as ref so
       a perfect 48k sampling rate should count 0x0000BB80
  0x03 Debug register 
  0x04  b3: padding | b2: fifo depth | b1: nr_samples | b0(ins):ins  | b0(4):outs
  0x05 header filling(16bit)

  0x08 i/o matrix mapping
*/



bool CypressDevice::IsRunning() {
  return mExitHandle != INVALID_HANDLE_VALUE;
}

bool CypressDevice::IsPresent() {
  return mDevHandle != INVALID_HANDLE_VALUE;
}

#define LAP(start,stop,freq) ((uint32_t)(((stop.QuadPart - start.QuadPart) * 1000000) / freq.QuadPart))
template<typename T1, typename T2>
constexpr auto NrPackets(T1 size, T2 len) { return ( (size / len) + (size % len ? 1 : 0) ); }

#define SNAP_TOLERANCE 300
#define SNAP_TO_AND_RET(val,snapto) { if(val > (snapto - SNAP_TOLERANCE) && val < (snapto + SNAP_TOLERANCE)) return snapto; }

constexpr auto ConvertSampleRate(uint32_t srReg)
{
  uint16_t count = (uint16_t)(srReg & 0xFFFF);
  uint16_t fract = (uint16_t)(srReg >> 16);
  uint32_t sr = count * 10;
  //mSRacc -= mSRvals[mSRidx];
  //mSRacc += sr;
  //mSRvals[mSRidx] = sr;
  //mSRidx = (mSRidx + 1) & 0xf;

  SNAP_TO_AND_RET(sr, 44100);
  SNAP_TO_AND_RET(sr, 48000);
  SNAP_TO_AND_RET(sr, 88200);
  SNAP_TO_AND_RET(sr, 96000);

  return 0;
}

uint32_t InitFpga(HANDLE handle, uint32_t settings, uint16_t payload)
{
  uint32_t status = ztex_xlabs_init_fifos(handle);
  status = ztex_default_lsi_set1(handle, 5, payload);
  status = ztex_default_lsi_set1(handle, 4, settings);
  return status;
}

static const uint16_t Sine48[48] =
{ 32768, 37045, 41248, 45307, 49151, 52715, 55938, 58764,
61145, 63041, 64418, 65255, 65535, 65255, 64418, 63041,
61145, 58764, 55938, 52715, 49151, 45307, 41248, 37045,
32768, 28490, 24287, 20228, 16384, 12820, 9597, 6771,
4390, 2494, 1117, 280, 0, 280, 1117, 2494,
4390, 6771, 9597, 12820, 16384, 20228, 24287, 28490 };

static const uint32_t rxpktSize = 1024;
static const uint32_t rxpktCount = 16;

void CypressDevice::main()
{
  LOG0("CypressDevice::main");
  if (mDevHandle == INVALID_HANDLE_VALUE)
    //signal the parent of the thread failure
    return;


  mExitHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
  ResetEvent(mExitHandle);

  const uint32_t nrIns = (devParams[NrIns].val + 1) * 2;
  const uint32_t nrOuts = (devParams[NrOuts].val + 1) * 2;
  const uint32_t nrSamples = devParams[NrSamples].val;
  const uint32_t fifoDepth = devParams[FifoDepth].val;

  uint32_t RxHeaderPayload = 14;
  const uint32_t InStride = nrIns * 3;
  const uint32_t Padding = 0;// 1024 - ((4 + RxHeaderPayload + (InStride * nrSamples)) & (1024 - 1));
  RxHeaderPayload += Padding;
  const uint32_t RxHeaderSize = 4 + RxHeaderPayload;
  
  const uint32_t INBuffSize = RxHeaderSize + (InStride * nrSamples);

  const uint32_t TxHeaderSize = 4;
  const uint32_t OUTStride = TxHeaderSize + (nrOuts * 3);
  const uint32_t OUTBuffSize = OUTStride * nrSamples;

  uint8_t * mINBuff = nullptr, *mOUTBuff = nullptr;
  devClient.AllocBuffers(INBuffSize * 2, mINBuff, OUTBuffSize * 2, mOUTBuff);

  auto ProcessHdr = [this, RxHeaderSize](uint8_t* pHdr)
  {
    if (pHdr[0] == 0xaa && pHdr[1] == 0xaa && pHdr[RxHeaderSize - 2] == 0x55 && pHdr[RxHeaderSize - 1] == 0x55)
    {

      uint32_t SR = ConvertSampleRate((uint16_t)(pHdr[7] << 8 | pHdr[6]));
      if (mDevStatus.LastSR != SR && mDevStatus.LastSR != -1 && mDevStatus.LastSR != 0)
      {
        devClient.SampleRateChanged();
      }
      mDevStatus.LastSR = SR;

      mDevStatus.FifoLevel = pHdr[2];
      mDevStatus.OutSkipCount = (pHdr[5] << 8) | pHdr[4];
      mDevStatus.InFullCount = (pHdr[11] << 8) | pHdr[10];
      mDevStatus.Ep6IsoErr = (pHdr[13] << 8) | pHdr[12];
      return true;
    }
    return false;
  };


  uint8_t * inPtr[2] = { mINBuff , mINBuff + (1 << 15) };
  uint8_t* outPtr[2] = { mOUTBuff , mOUTBuff + (1 << 15) };
  uint8_t BuffNr = 0;

  struct { 
    uint32_t time_ms;
    uint32_t samples;
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t fifo_lvl;
    uint32_t fifo_empty;
    uint32_t ep6_isoerr;
  } Stats[1024];
  uint32_t StatsCount = 0;

  //initialize header mark
  auto InitTxHeaders = [OUTStride,nrOuts,TxHeaderSize](uint8_t * ptr, uint32_t Samples)
  {
    USHORT w = 0;
    for (uint32_t i = 0; i < Samples; i++)
    {
      PUCHAR p = ptr + i * OUTStride;
      *p       = 0xAA;
      *(p + 1) = 0x55;
      *(p + 2) = 0x55;
      *(p + 3) = 0xAA;
#if LOOPBACK_TEST
      /*for (uint32_t c = 0; c < (nrOuts/2)*3; ++c)
      {
        *(PUSHORT)(ptr + i * OUTStride + TxHeaderSize + c * 2) = w >>8 | w <<8;
        w++;
      }*/
      for (uint16_t c = 0; c < nrOuts; ++c)
      {
        *(p + TxHeaderSize + c * 3) = 0;
        *(p + TxHeaderSize + c * 3 + 1) = uint8_t(Sine48[i % 48] & 0xff);
        *(p + TxHeaderSize + c * 3 + 2) = uint8_t(Sine48[i % 48] >> 8);
      }
#endif
    }
  };

  ZeroMemory(&mDevStatus, sizeof(mDevStatus));
  InitTxHeaders(outPtr[0], nrSamples);
  InitTxHeaders(outPtr[1], nrSamples);

  // configure the fpga channel params
  union {
    struct { uint32_t
      outs : 4,
       ins : 4,
   samples : 8,
      fifo : 8,
   padding : 8;
    };
    uint32_t u32;
  } ch_params = {
    (uint32_t)devParams[NrOuts].val , (uint32_t)devParams[NrIns].val, nrSamples, fifoDepth, 0
  };

 
  //must be a power of two
  static const uint8_t NrXfers = 4;

  XferReq mRxRequests[NrXfers];
  XferReq mTxRequests[NrXfers];
  uint8_t TxReqIdx = 0;
  uint8_t RxReqIdx = 0;

  ZeroMemory(mRxRequests, sizeof(mRxRequests));
  ZeroMemory(mTxRequests, sizeof(mTxRequests));

#define ISO_XFER

#if defined(ISO_XFER)

  LARGE_INTEGER li1;
  QueryPerformanceCounter(&li1);
  LARGE_INTEGER lif;
  QueryPerformanceFrequency(&lif);

  uint32_t SampleCounter = 0;
  uint64_t SRAcc = 0;
  uint16_t SRAvgIdx = 0;
  uint64_t SRVals[256]; ZeroMemory(&SRVals, sizeof(SRVals));


  uint32_t RxProgress = 0;
  uint32_t TxXferSize = 0;

  const uint32_t precharge = 48;

  for (uint32_t c = 0; c < NrXfers; ++c)
  {
    mRxRequests[c].handle   = mDevHandle;
    mRxRequests[c].endpoint = mDefInEP;
    mRxRequests[c].bufflen = rxpktCount * rxpktSize;

    if (bknd_init_read_xfer(mDevHandle, &mRxRequests[c], rxpktCount, rxpktSize)) {
      mRxRequests[c].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
      bknd_iso_read(&mRxRequests[c]);
    }

    mTxRequests[c].handle = mDevHandle;
    mTxRequests[c].endpoint = mDefOutEP;
    mTxRequests[c].bufflen = OUTBuffSize;

    if (bknd_init_write_xfer(mDevHandle, &mTxRequests[c], 0, 0)) {
      mTxRequests[c].buff = (PVOID64) new uint8_t[64*1024];
      mTxRequests[c].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
      InitTxHeaders((uint8_t*)mTxRequests[c].buff, precharge);
      mTxRequests[c].bufflen = OUTStride * precharge;
      bknd_bulk_write(&mTxRequests[c]);
      //SetEvent(mTxRequests[c].ovlp.hEvent);
    }
  }

#else

  for (uint32_t c = 0; c < NrXfers; ++c)
  {
    mRxRequests[c] = { mDevHandle,mDefInEP };
    if (bknd_init_read_xfer(mDevHandle, &mRxRequests[c])) {
      mRxRequests[c].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
      mRxRequests[c].bufflen = INBuffSize;
      //bknd_bulk_read(&mRxRequests[c]);
      SetEvent(mRxRequests[c].ovlp.hEvent);
    }

    mTxRequests[c] = { mDevHandle,mDefOutEP };
    if (bknd_init_write_xfer(mDevHandle, &mTxRequests[c]))
    {
      mTxRequests[c].ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, nullptr);
      mTxRequests[c].bufflen = OUTBuffSize;
      InitTxHeaders((uint8_t*)mTxRequests[c].buff, nrSamples);
      bknd_bulk_write(&mTxRequests[c]);
      //SetEvent(mTxRequests[c].ovlp.hEvent);
    }
  }

#endif

  InitFpga(mDevHandle, ch_params.u32, RxHeaderPayload / 2);
  mDevStatus.LastSR = -1;
  int64_t lsiResult = -1;/*ztex_default_lsi_get1(mDevHandle, 2);
  if (lsiResult > 0)
    mDevStatus.LastSR = ConvertSampleRate(uint32_t(lsiResult));*/

  //TransferResult  init_res = UsbDk.WritePipe(mDevHandle, &TXreq, nullptr);

  while (WaitForSingleObject(mExitHandle, 0) == WAIT_TIMEOUT)
  {
#if defined(ISO_XFER)
    LARGE_INTEGER li2;
    DWORD wsoRxResult = WaitForSingleObject(mRxRequests[RxReqIdx].ovlp.hEvent, INFINITE);
    QueryPerformanceCounter(&li2);
    if (WAIT_OBJECT_0 == wsoRxResult)
    {
      XferReq* RxReq = &mRxRequests[RxReqIdx];
      uint32_t rxBytes = 0;

      for (uint32_t i = 0; i < rxpktCount; i++)
      {
        IsoReqResult result = bknd_iso_get_result(RxReq, i);
        if (result.status == 0 && result.length > 0) //a filled block
        {
          rxBytes += (uint32_t)result.length;
          uint8_t* ptr = ((uint8_t*)RxReq->buff) + (uintptr_t(i) * rxpktSize);
          //-------------------------------------------------
          if (ProcessHdr(ptr))
          {
            if (RxProgress == 0)
            {
              //found start so start copying from here
              RxProgress = (uint32_t)result.length;
              memcpy(inPtr[BuffNr], ptr, RxProgress);
            }
            else {
              mDevStatus.ResyncErrors++;
              LOGN("Missed %u ISOCH packets\n", mDevStatus.ResyncErrors);
              RxProgress = 0;//start again
            }
          }
          else {
            if (RxProgress > 0) {
              memcpy(inPtr[BuffNr] + RxProgress, ptr, (size_t)result.length);
              RxProgress += (uint32_t)result.length;
            }
            else {
              mDevStatus.ResyncErrors++;
              LOGN("Missed %u ISOCH packets\n", mDevStatus.ResyncErrors);
            }
          }

          if (RxProgress == INBuffSize)//Packet complete, dispatch to client
          {
            devClient.Switch(2, InStride, inPtr[BuffNr] + RxHeaderSize, OUTStride, outPtr[BuffNr] + TxHeaderSize);
            BuffNr ^= 1;

            if(TxXferSize == 0)
              WaitForSingleObject(mTxRequests[TxReqIdx].ovlp.hEvent, 20);
            memcpy((uint8_t*)mTxRequests[TxReqIdx].buff + TxXferSize, outPtr[BuffNr], OUTBuffSize);
            TxXferSize += OUTBuffSize;
            RxProgress = 0;
            SampleCounter++;
          }

          if (RxProgress > INBuffSize) //missed something, start again
          {
            mDevStatus.ResyncErrors++;
            LOGN("Missed %u ISOCH packets\n", mDevStatus.ResyncErrors);
            RxProgress = 0;
          }
        }
      }
      //fire again
      bknd_iso_read(RxReq);
      ++RxReqIdx &= (NrXfers-1);

      if (StatsCount < 1024)
      {
        Stats[StatsCount++] = {
          uint32_t((li2.QuadPart - li1.QuadPart) * 1000 / lif.QuadPart),
          SampleCounter * nrSamples,
          rxBytes,
          mTxRequests[TxReqIdx].bufflen,
          mDevStatus.FifoLevel,
          mDevStatus.OutSkipCount,
          mDevStatus.Ep6IsoErr
        };
        SampleCounter = 0;
      }

      if (TxXferSize > 0) {
        mTxRequests[TxReqIdx].bufflen = TxXferSize;
        bknd_bulk_write(&mTxRequests[TxReqIdx]);
        ++TxReqIdx &= (NrXfers - 1);
        TxXferSize = 0;
      }

    }
    else
    {
      if (wsoRxResult == 0xffffffff)
      {
        LOGN("Wait error 0x%08X GetLastError:0x%08X\n", wsoRxResult, GetLastError());
        Sleep(100);//otherwise we will block the universe
      }
      else
      {
        LOGN("Wait failed 0x%08X\n", wsoRxResult);
      }
    }
#else

    TXreq.Buffer = outPtr[BuffNr];
    RXreq.Buffer = inPtr[BuffNr];

    TransferResult  wr_res = UsbDk.WritePipe(mDevHandle, &TXreq, &tx_ovlp);
    TransferResult  rd_res = UsbDk.ReadPipe(mDevHandle, &RXreq, &rx_ovlp);

    BuffNr ^= 1;

    //data is on the way so we can pass what we got from the last operation to the client
    devClient.Switch(InStride , inPtr[BuffNr] + RxHeaderSize , OUTStride, outPtr[BuffNr] + TxHeaderSize);


    //2 steps to identify a timeout

    //check we have connectivity
    lsiResult = ztex_default_lsi_get1(mDevHandle, 2);

    //DWORD wresult = WaitForMultipleObjects(2, handles, true, 100);
    ////check that the fpga is pushing data and that we have usb connection with the fx2lp
    //if ( wresult != WAIT_OBJECT_0 || get_result < 0)
    //{
    //  LOGN("WorkerError wresult:0x%08X get_result:%d\n", wresult, get_result);
    //  devClient.DeviceStopped(true);
    //  break;
    //}
    //else
    //  mDevStatus.LastSR = uint32_t(get_result);

    DWORD wresult = WaitForMultipleObjects(2, handles, false, 500);
    DWORD result0 = WAIT_FAILED, result1 = WAIT_FAILED;
    switch (wresult)
    {
    case WAIT_OBJECT_0:
      result0 = WAIT_OBJECT_0;
      result1 = WaitForSingleObject(handles[1], 500);
      break;
    case (WAIT_OBJECT_0 + 1):
      result1 = WAIT_OBJECT_0;
      result0 = WaitForSingleObject(handles[0], 500);
      break;
    }

    //check that the fpga is pushing data and that we have usb connection with the fx2lp
    if (result0 != WAIT_OBJECT_0 || result1 != WAIT_OBJECT_0 || lsiResult < 0)
    {
      LOGN("WorkerError result0:0x%08X result1:0x%08X get_result:%d\n", result0, result1, lsiResult);
      //devClient.DeviceStopped(true);
      //break;
    } else {
      uint32_t SR = ConvertSampleRate(uint32_t(lsiResult));
      if (mDevStatus.LastSR != SR && mDevStatus.LastSR != -1 && mDevStatus.LastSR != 0 )
      {
        devClient.SampleRateChanged();
        //InitFpga(mDevHandle, ch_params.u32);
      }

      mDevStatus.LastSR = SR;
      uint8_t * ptr = (uint8_t *)RXreq.Buffer;
      if ( ptr[0] != 0xaa || ptr[1] != 0xaa || ptr[RxHeaderSize - 2] != 0x55 || ptr[RxHeaderSize - 1] != 0x55) {
        InitFpga(mDevHandle, ch_params.u32);
        mDevStatus.ResyncErrors++;
      }
      else {
        mDevStatus.FifoLevel = ptr[2];
        mDevStatus.OutSkipCount = (ptr[5] << 8) | ptr[4];
        mDevStatus.InFullCount = (ptr[11] << 8) | ptr[10];
      }
    }
#endif
  }

  devClient.FreeBuffers(mINBuff, mOUTBuff);


  for (uint32_t c = 0; c < NrXfers; ++c){
    WaitForSingleObject(mRxRequests[c].ovlp.hEvent, INFINITE);
    CloseHandle(mRxRequests[c].ovlp.hEvent);
    bknd_xfer_cleanup(&mRxRequests[c]);
    WaitForSingleObject(mTxRequests[c].ovlp.hEvent, INFINITE);
    if(mTxRequests[c].ovlp.hEvent)
      CloseHandle(mTxRequests[c].ovlp.hEvent);
    bknd_xfer_cleanup(&mTxRequests[c]);
    if (mTxRequests[c].buff)
      delete mTxRequests[c].buff;
  }

  for (UINT16 c = 0; c < 1024; ++c)
  {
    LOGN("%u,%u,%1.3f,%1.3f,%u,%u\n", Stats[c].time_ms, Stats[c].samples, Stats[c].rx_bytes/1000.f, Stats[c].tx_bytes/1000.f, Stats[c].fifo_lvl, Stats[c].fifo_empty);
  }

  FILE* f = fopen("stats.csv", "w");
  if (f)
  {
    char str[256];
    fputs("ms,samples, rxkB, txkB, fifo_lvl, fifo_empty\n", f);
    for (UINT16 c = 0; c < 1024; ++c) {
      sprintf_s(str, "%u,%u,%1.3f,%1.3f,%u,%u\n", Stats[c].time_ms, Stats[c].samples, Stats[c].rx_bytes / 1000.f, Stats[c].tx_bytes / 1000.f, Stats[c].fifo_lvl, Stats[c].fifo_empty);
      fputs(str, f);
    }
    fflush(f);
    fclose(f);
  }

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

uint32_t CypressDevice::GetSampleRate()
{
  uint32_t lastSR = (uint32_t)(-1);
  if (mDevHandle != INVALID_HANDLE_VALUE) {
    if (mExitHandle != INVALID_HANDLE_VALUE) {
      lastSR = mDevStatus.LastSR;
    } else {
      int64_t result = ztex_default_lsi_get1(mDevHandle, 2);
      if (result > 0)
        lastSR = ConvertSampleRate((uint32_t)result);
    }
  }
  return lastSR;
}

