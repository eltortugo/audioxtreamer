
// AudioXtreamer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "fx2lp\cypressdevice.h"

#include "MainFrame.h"
#include "resource.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CAudioXtreamerApp theApp;


// CAudioXtreamerApp construction
CAudioXtreamerApp::CAudioXtreamerApp()
: hMapFile(NULL)
, pBuf(nullptr)
, hAsioEvent(NULL)
, hXtreamerEvent(NULL)
, mDevice(new CypressDevice(*this, theSettings))
, mClientActive(false)
{
}





BOOL CAudioXtreamerApp::InitInstance()
{
	CWinAppEx::InitInstance();

  hAsioEvent = CreateEvent( NULL, FALSE, TRUE, szNameAsioEvent );

  if (ERROR_ALREADY_EXISTS == GetLastError())
  {
    MessageBox(0, _T("Only one instance allowed!"), szNameApp, MB_ICONINFORMATION);
    return FALSE;
  }
 
  ResetEvent(hAsioEvent);

  hXtreamerEvent = CreateEvent(NULL, FALSE, TRUE, szNameXtreamerEvent);
  ResetEvent(hXtreamerEvent);

  //Create the named shared memory mapping (4*64k) and synchronization events for IPC

  hMapFile = CreateFileMapping(
    INVALID_HANDLE_VALUE,    // use paging file
    NULL,                    // default security
    PAGE_READWRITE,          // read/write access
    0,                       // maximum object size (high-order DWORD)
    (4<<16),                // maximum object size (low-order DWORD)
    szNameShMem );           // name of mapping object

  if (hMapFile == NULL) {
    _tprintf(TEXT("Could not create file mapping object (%d).\n"), GetLastError());
    return FALSE;
  }

   pBuf = (uint8_t*)MapViewOfFile(hMapFile,   // handle to map object
    FILE_MAP_ALL_ACCESS, // read/write permission
    0,
    0,
    0);

  if (pBuf == NULL) {
    _tprintf(TEXT("Could not map view of file (%d).\n"), GetLastError());
    CloseHandle(hMapFile);

    return FALSE;
  }

  pMainFrame = new MainFrame(*mDevice);
  if (pMainFrame == nullptr || !pMainFrame->Create(szNameClass, szNameApp))
    return FALSE;

  m_pMainWnd = pMainFrame;
  pMainFrame->ShowWindow(SW_HIDE);

  return TRUE;
}

//Callback from the USB driver
//we dont process the data here, just signal the client that the data is ready in the shared memory
//pass the strides and offsets to the shared struct.
// Check that the asio dll signaled its event, otherwise skip and move forward.
// after providing the data, set the xtreamer event
static uint32_t ClientTimeout = 0;
bool CAudioXtreamerApp::Switch(uint32_t timeout, uint32_t rxSampleSize, uint8_t * rxBuff, uint32_t txSampleSize, uint8_t * txBuff)
{
  volatile ASIOSettings::StreamInfo *info = (ASIOSettings::StreamInfo *)pBuf;

  DWORD result = WaitForSingleObject(hAsioEvent, mClientActive ? timeout : 0);
  if (result == WAIT_TIMEOUT) {
    if (mClientActive) {
      LOGN("ASIO client timeout :%u\n", ++ClientTimeout);
    }
  }

  info->Flags &= ~((uint32_t)0x2);

  bool Active = info->Flags & (uint32_t)0x1 ? true : false;

  if (Active)
    mClientCleanup = false;

  if (Active != mClientActive || mClientCleanup)
  {
    mClientActive = Active;

    if (!mClientActive) //zero the buffers
    {
      if (!mClientCleanup)
      {
        LOG0("ASIO client disconnected\n");
      }

      mClientCleanup ^= 1; //do it twice
      ClientTimeout = 0;

      int &NumSamples = theSettings[ASIOSettings::NrSamples].val;
      int &NumOuts = theSettings[ASIOSettings::NrOuts].val;
      for (int s = 0; s < NumSamples; s++)
      {
        for (int c = 0; c < NumOuts; c++)
        {
          *(txBuff + s * txSampleSize + c * 3 + 0) = 0;
          *(txBuff + s * txSampleSize + c * 3 + 1) = 0;
          *(txBuff + s * txSampleSize + c * 3 + 2) = 0;
        }
      }
    }
  }


  info->RxStride = rxSampleSize;
  info->RxOffset = (uint32_t)(rxBuff - pBuf) - (1 << 16);
  info->TxStride = txSampleSize;
  info->TxOffset = (uint32_t)(txBuff - pBuf) - (2 << 16);
  info->Flags &= ~((uint32_t)0x1);


  if (SetEvent(hXtreamerEvent) == FALSE)
  {
    DWORD error = GetLastError();
    _tprintf(TEXT("Could not release Mutex (%d).\n"), error);
  }


  return Active;
}

void CAudioXtreamerApp::AllocBuffers(uint32_t rxSize, uint8_t *& rxBuff, uint32_t txSize, uint8_t *& txBuff)
{
  ZeroMemory(pBuf, (4 << 16));
  rxBuff = (uint8_t*)(pBuf + (1 << 16));
  txBuff = (uint8_t*)(pBuf + (2 << 16));
  mClientActive = false;
}

void CAudioXtreamerApp::FreeBuffers(uint8_t *& rxBuff, uint8_t *& txBuff)
{
  rxBuff = nullptr;
  txBuff = nullptr;
}


void CAudioXtreamerApp::DeviceStopped(bool error)
{
  if (error) {
    LOG0("CAudioXtreamerApp::DeviceStopped with error");
  }
}

void CAudioXtreamerApp::SampleRateChanged()
{
  LOG0("CAudioXtreamerApp::SampleRateChanged");
  ASIOSettings::StreamInfo *info = (ASIOSettings::StreamInfo *)pBuf;
  info->Flags |= ((uint32_t)0x2);
}

int CAudioXtreamerApp::ExitInstance()
{
  if (m_pMainWnd)
    m_pMainWnd->DestroyWindow(); //and deleted

  pMainFrame = nullptr;

  if (mDevice != nullptr) {
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
  }

  if (hAsioEvent != NULL) {
    CloseHandle(hAsioEvent);
    hAsioEvent = NULL;
  }

  if (hXtreamerEvent != NULL) {
    CloseHandle(hXtreamerEvent);
    hXtreamerEvent = NULL;
  }

  if(pBuf && !UnmapViewOfFile(pBuf))
  {
    _tprintf(TEXT("Could not Unmap view of file (%d).\n"), GetLastError());
  }
  pBuf = nullptr;
  
  if (hMapFile != NULL) {
    CloseHandle(hMapFile);
    hMapFile = NULL;
  }

  return CWinAppEx::ExitInstance();
}



