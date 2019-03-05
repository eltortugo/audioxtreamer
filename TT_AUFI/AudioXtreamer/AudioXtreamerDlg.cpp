
// AudioXtreamerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "AudioXtreamerDlg.h"




#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange& DX);    // DDX/DDV support

// Implementation
protected:

};

CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange& DX)
{
	CDialog::DoDataExchange(DX);
}


// CAudioXtreamerDlg dialog



CAudioXtreamerDlg::CAudioXtreamerDlg()
	: CDialog(IDD_AUDIOXTREAMER_DIALOG)
  , mCheckOpenDevice(FALSE)
{
	m_hIcon = GetApp().LoadIcon(IDR_MAINFRAME);
  mDevice = nullptr;
}

CAudioXtreamerDlg::~CAudioXtreamerDlg()
{
  if (mDevice) {
    mDevice->Stop(true);
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
  }
}

void CAudioXtreamerDlg::Switch(uint32_t rxSampleSize, uint8_t * rxBuff, uint32_t txSampleSize, uint8_t * txBuff)
{
}

void CAudioXtreamerDlg::DoDataExchange(CDataExchange& DX)
{
  CDialog::DoDataExchange(DX);
  DX.DDX_Check( IDC_CHECK1, mCheckOpenDevice);
}
/*
#include "afxwin.h"
BEGIN_MESSAGE_MAP(CAudioXtreamerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
  ON_BN_CLICKED(IDC_CHECK1, &CAudioXtreamerDlg::OnBnClickedCheck1)
  ON_BN_CLICKED(IDC_BUTTON1, &CAudioXtreamerDlg::OnOpen)
  ON_BN_CLICKED(IDC_BUTTON2, &CAudioXtreamerDlg::OnClose)
  ON_BN_CLICKED(IDC_BUTTON3, &CAudioXtreamerDlg::OnSendAsync)
  ON_BN_CLICKED(IDC_BUTTON4, &CAudioXtreamerDlg::OnRecvAsync)
  ON_BN_CLICKED(IDC_BUTTON5, &CAudioXtreamerDlg::OnStartStream)
  ON_BN_CLICKED(IDC_BUTTON6, &CAudioXtreamerDlg::OnStopStream)
END_MESSAGE_MAP()
*/

BOOL CAudioXtreamerDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
  UNREFERENCED_PARAMETER(lParam);

  UINT nID = LOWORD(wParam);
  switch (nID)
  {
  case IDC_CHECK1: OnBnClickedCheck1(); return TRUE;
  case IDC_BUTTON1: OnOpen(); return TRUE;
  case IDC_BUTTON2: OnClose(); return TRUE;
  case IDC_BUTTON3: OnSendAsync(); return TRUE;
  case IDC_BUTTON4: OnRecvAsync(); return TRUE;
  case IDC_BUTTON5: OnStartStream(); return TRUE;
  case IDC_BUTTON6: OnStopStream(); return TRUE;
  }

  return FALSE;
}

// CAudioXtreamerDlg message handlers

BOOL CAudioXtreamerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	assert((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  assert(IDM_ABOUTBOX < 0xF000);

	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		assert(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
      GetSystemMenu(FALSE).AppendMenu(MF_SEPARATOR);
      GetSystemMenu(FALSE).AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
  HWND hwnd = GetDlgItem(IDC_BUTTON2).GetHwnd();
  ::EnableWindow(hwnd, FALSE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

INT_PTR CAudioXtreamerDlg::DialogProc(UINT uMsg, WPARAM wp, LPARAM lp)
{
  if (uMsg == WM_SYSCOMMAND) {
    if ((wp & 0xFFF0) == IDM_ABOUTBOX)
    {
      CAboutDlg dlgAbout;
      dlgAbout.DoModal(GetHwnd());
      return 0;
    }
  }

  return CDialog::DialogProc(uMsg,wp,lp);
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

LRESULT CAudioXtreamerDlg::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (IsIconic())
	{
		CPaintDC dc(GetHwnd()); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetHDC()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect = GetClientRect();
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
    // No more drawing required
    return 0L;
	}
	else
	{
		return CDialog::OnPaint(uMsg, wParam, lParam);
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CAudioXtreamerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void OpenDevice()
{}

void CAudioXtreamerDlg::OnBnClickedCheck1()
{
  CDataExchange dx;
  UpdateData(dx,TRUE);
  if (mCheckOpenDevice)
    OpenDevice();
}


void CAudioXtreamerDlg::OnOpen()
{
  if (mDevice == nullptr)
    mDevice = UsbDevice::CreateDevice(*this, 32, 32, 85);

  if (mDevice->Open())
  {
    HWND hwnd = GetDlgItem(IDC_BUTTON1);
    ::EnableWindow( hwnd, FALSE);
    hwnd = GetDlgItem(IDC_BUTTON2);
    ::EnableWindow(hwnd, TRUE);
  }
}


void CAudioXtreamerDlg::OnClose()
{
  if (mDevice) {
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
    HWND hwnd = GetDlgItem(IDC_BUTTON1);
    ::EnableWindow(hwnd, TRUE);
    hwnd = GetDlgItem(IDC_BUTTON2);
    ::EnableWindow(hwnd, FALSE);
  }
}


void CAudioXtreamerDlg::OnSendAsync()
{
  //if (mDevice) mDevice->SendAsync(512,0);
}


void CAudioXtreamerDlg::OnRecvAsync()
{
  //if (mDevice) mDevice->RecvAsync(512,0);
}


void CAudioXtreamerDlg::OnStartStream()
{
  if (mDevice) mDevice->Start();
}


void CAudioXtreamerDlg::OnStopStream()
{
  if (mDevice) mDevice->Stop(true);
}
