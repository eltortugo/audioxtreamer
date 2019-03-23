
// AudioXtreamer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "fx2lp\cypressdevice.h"

#include "MainFrame.h"
#include "resource.h"

#include "SettingsDlg.h"
#include "AudioXtreamerDlg.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CAudioXtreamerApp theApp;




enum State { stClosed, stOpen, stReady, stActive };

// CAudioXtreamerApp construction
CAudioXtreamerApp::CAudioXtreamerApp()
: mIniFile (theSettings)
, hMapFile(NULL)
, pBuf(nullptr)
, hAsioMutex(NULL)
, mDevice(nullptr)
, mClientActive(false)
, mPropertySheetDlg(szNameApp)
, mState(stClosed)
{
}


BOOL CAudioXtreamerApp::InitInstance()
{
	CWinAppEx::InitInstance();

  hAsioMutex = CreateMutex( NULL, FALSE, szNameMutex );
  if (ERROR_ALREADY_EXISTS == GetLastError())
  {
    MessageBox(0, _T("Only one instance allowed!"), szNameApp, MB_ICONINFORMATION);
    return FALSE;
  }

  mIniFile.Load();

  //Create the named shared memory mapping (4*64k) and synchronization events for IPC

  hMapFile = CreateFileMapping(
    INVALID_HANDLE_VALUE,    // use paging file
    NULL,                    // default security
    PAGE_READWRITE,          // read/write access
    0,                       // maximum object size (high-order DWORD)
    (4<<16),                 // maximum object size (low-order DWORD)
    szNameShMem );             // name of mapping object

  if (hMapFile == NULL) {
    _tprintf(TEXT("Could not create file mapping object (%d).\n"), GetLastError());
    return FALSE;
  }

   pBuf = (uint8_t*)MapViewOfFile(hMapFile,   // handle to map object
    FILE_MAP_ALL_ACCESS, // read/write permission
    0,
    0,
    (4<<16));

  if (pBuf == NULL) {
    _tprintf(TEXT("Could not map view of file (%d).\n"), GetLastError());
    CloseHandle(hMapFile);

    return FALSE;
  }

  if (mDevice == nullptr)
    mDevice = new CypressDevice( *this, theSettings);

 
  pMainFrame = new MainFrame;
  if (pMainFrame == nullptr || !pMainFrame->Create(szNameClass, szNameApp))
    return FALSE;

  m_pMainWnd = pMainFrame; 
  pMainFrame->ShowWindow(SW_HIDE);

  return TRUE;
}


BEGIN_MESSAGE_MAP(CAudioXtreamerApp, CWinAppEx)
  ON_COMMAND(ID_AUDIOXTREAMER_QUIT, &CAudioXtreamerApp::OnAudioxtreamerQuit)
  ON_UPDATE_COMMAND_UI(ID_AUDIOXTREAMER_QUIT, &CAudioXtreamerApp::OnUpdateAudioxtreamerQuit)
  ON_COMMAND(ID_AUDIOXTREAMER_OPEN, &CAudioXtreamerApp::OnAudioxtreamerOpen)
  ON_UPDATE_COMMAND_UI(ID_AUDIOXTREAMER_OPEN, &CAudioXtreamerApp::OnUpdateAudioxtreamerOpen)
END_MESSAGE_MAP()


LRESULT CAudioXtreamerApp::XtreamerMessage(WPARAM wp, LPARAM lp)
{
  switch (wp)
  {
  case 0: NextState((enum State)lp); return LRESULT(0);
  case 1: return (mDevice != nullptr && mDevice->IsPresent()) ? LRESULT(1) : LRESULT(0);
  case 2: return (mDevice != nullptr && mDevice->IsRunning()) ? LRESULT(1) : LRESULT(0);
  case 3: return OpenControlPanel(true) == IDOK ? LRESULT(1) : LRESULT(0);
  }
  return LRESULT(0);
}

void CAudioXtreamerApp::NextState(enum State newState)
{
  switch (mState)
  {
  case stClosed:
    if (mDevice->Open())
      mState = stOpen;
    break;

  case stOpen:
    if (mPropertySheetDlg.GetSafeHwnd()) //dont try to start while editing
      break;
    if (mDevice->Start()) {
      mState = stReady;
      pMainFrame->SetState(MainFrame::Started);
    } else {
      mDevice->Close();
      mState = stClosed;
      break;
    }

  case stReady:
    if (mDevice->IsRunning()) {
      if (mClientActive) {
        pMainFrame->SetState(MainFrame::Active);
        mState = stActive;
      }
    } else {
      pMainFrame->SetState(MainFrame::Stopped);
      mState = stOpen;
    }
    break;

  case stActive:
    if (mDevice->IsRunning()) {
      if (!mClientActive) {
        pMainFrame->SetState(MainFrame::Started);
        mState = stReady;
      }
    } else {
      pMainFrame->SetState(MainFrame::Stopped);
      mState = stOpen;
    }
    break;
  default: break;
  } 
}



void CAudioXtreamerApp::Switch(uint32_t rxSampleSize, uint8_t * rxBuff, uint32_t txSampleSize, uint8_t * txBuff)
{
  //we dont process the data here, just signal the client that the data is ready in the shared memory  
  //pass the strides and offsets to the shared struct
  ASIOSettings::StreamInfo *info = (ASIOSettings::StreamInfo *)pBuf;

  info->RxStride = rxSampleSize;
  info->RxOffset = (uint32_t)(rxBuff - pBuf) - (1<<16);
  info->TxStride = txSampleSize;
  info->TxOffset = (uint32_t)(txBuff - pBuf) - (2<<16);
  info->Flags &= ~((uint32_t)0x1);
  

  if (!ReleaseMutex(hAsioMutex))
  {
    DWORD error = GetLastError();
    _tprintf(TEXT("Could not release Mutex (%d).\n"), error);
  }

  DWORD result = WaitForSingleObject(hAsioMutex, 100);
  if (result == WAIT_TIMEOUT) {
    LOG0("CAudioXtreamerApp::Switch TIMEOUT the asio client took too long to process the audio");
  }

  bool Active = info->Flags & (uint32_t)0x1 ? true : false;

  if (Active)
    mClientCleanup = false;

  if (Active != mClientActive || mClientCleanup)
  {
    mClientActive = Active;
    

    if (!mClientActive) //zero the buffers
    {
      mClientCleanup ^= 1; //do it twice

      int &NumSamples = theSettings[NrSamples].val;
      int &NumOuts = theSettings[NrOuts].val;
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
}

void CAudioXtreamerApp::AllocBuffers(uint32_t rxSize, uint8_t *& rxBuff, uint32_t txSize, uint8_t *& txBuff)
{
  //get the mutex in the right thread
  WaitForSingleObject(hAsioMutex, INFINITE);
  ZeroMemory(pBuf, (4 << 16));
  rxBuff = (uint8_t*)(pBuf + (1 << 16));
  txBuff = (uint8_t*)(pBuf + (2 << 16));
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

  if (hAsioMutex != NULL)
    CloseHandle(hAsioMutex);

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


INT_PTR CAudioXtreamerApp::OpenControlPanel(bool pause)
{
  //the window exists so bring it to the front and leave
  if (mPropertySheetDlg.GetSafeHwnd() != NULL) {
    mPropertySheetDlg.SetWindowPos(&CWnd::wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    mPropertySheetDlg.SetWindowPos(&CWnd::wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    return 0;
  }

  bool restart = false;
  if (pause && mDevice != nullptr && mDevice->IsRunning())
  {
    //OnAudioxtreamerStop();
    restart = true;
  }

  ASIOSettingsDlg<CPropertyPage> pp1(*mDevice, theSettings);
  CAudioXtreamerDlg<CPropertyPage> pp2(*mDevice, theSettings);
  mPropertySheetDlg.AddPage(&pp1);
  mPropertySheetDlg.AddPage(&pp2);
  INT_PTR result = mPropertySheetDlg.DoModal();
  mPropertySheetDlg.RemovePage(&pp2);
  mPropertySheetDlg.RemovePage(&pp1);

  if (result == IDOK)
    mIniFile.Save();

  //if (restart)
    //OnAudioxtreamerStart();
  return result;
}



void CAudioXtreamerApp::OnAudioxtreamerQuit()
{
  //OnAudioxtreamerStop();
  PostQuitMessage(0);
}


void CAudioXtreamerApp::OnUpdateAudioxtreamerQuit(CCmdUI *pCmdUI)
{
  pCmdUI->Enable( mPropertySheetDlg.GetSafeHwnd() == NULL  );
}


void CAudioXtreamerApp::OnAudioxtreamerOpen()
{
  OpenControlPanel(false);
}


void CAudioXtreamerApp::OnUpdateAudioxtreamerOpen(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(mPropertySheetDlg.GetSafeHwnd() ? FALSE : TRUE);
}

