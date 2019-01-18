
// AudioXtreamerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "AudioXtreamerDlg.h"
#include "afxdialogex.h"



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
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CAudioXtreamerDlg dialog



CAudioXtreamerDlg::CAudioXtreamerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_AUDIOXTREAMER_DIALOG, pParent)
  , mCheckOpenDevice(FALSE)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
  mDevice = nullptr;
}

CAudioXtreamerDlg::~CAudioXtreamerDlg()
{
  if (mDevice) {
    mDevice->Stop();
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
  }
}

void CAudioXtreamerDlg::Switch(uint32_t rxSampleSize, uint8_t * rxBuff, uint32_t txSampleSize, uint8_t * txBuff)
{
}

void CAudioXtreamerDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialogEx::DoDataExchange(pDX);
  DDX_Check(pDX, IDC_CHECK1, mCheckOpenDevice);
}

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


// CAudioXtreamerDlg message handlers

BOOL CAudioXtreamerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
  HWND hwnd;
  GetDlgItem(IDC_BUTTON2, &hwnd);
  ::EnableWindow(hwnd, FALSE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CAudioXtreamerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CAudioXtreamerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
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
  UpdateData(TRUE);
  if (mCheckOpenDevice)
    OpenDevice();
  
}


void CAudioXtreamerDlg::OnOpen()
{
  if (mDevice == nullptr)
    mDevice = UsbDevice::CreateDevice(*this, 32, 32, 85);

  if (mDevice->Open())
  {
    HWND hwnd;
    GetDlgItem(IDC_BUTTON1, &hwnd);
    ::EnableWindow( hwnd, FALSE);
    GetDlgItem(IDC_BUTTON2, &hwnd);
    ::EnableWindow(hwnd, TRUE);
  }
}


void CAudioXtreamerDlg::OnClose()
{
  if (mDevice) {
    mDevice->Close();
    delete mDevice;
    mDevice = nullptr;
    HWND hwnd;
    GetDlgItem(IDC_BUTTON1, &hwnd);
    ::EnableWindow(hwnd, TRUE);
    GetDlgItem(IDC_BUTTON2, &hwnd);
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
  if (mDevice) mDevice->Stop();
}
