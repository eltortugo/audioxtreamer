
// AudioXtreamerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "MainFrame.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif



template <class T>
CAudioXtreamerDlg<T>::CAudioXtreamerDlg(UsbDevice & device, ASIOSettings::Settings &info)
	: T(IDD_DLG_DEVEL)
  , mCheckOpenDevice(FALSE)
  , mDevice (device)
{
  m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_AXG));
}

template <class T>
CAudioXtreamerDlg<T>::~CAudioXtreamerDlg()
{

}

template <class T>
void CAudioXtreamerDlg<T>::DoDataExchange(CDataExchange * DX)
{
  CDialog::DoDataExchange(DX);
  DDX_Check( DX, IDC_CHECK1, mCheckOpenDevice);
}


BEGIN_TEMPLATE_MESSAGE_MAP(CAudioXtreamerDlg, T, T)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
  ON_BN_CLICKED(IDC_CHECK1, &CAudioXtreamerDlg<T>::OnBnClickedCheck1)
  ON_BN_CLICKED(IDC_BUTTON1, &CAudioXtreamerDlg<T>::OnOpen)
  ON_BN_CLICKED(IDC_BUTTON2, &CAudioXtreamerDlg<T>::OnClose)
  ON_BN_CLICKED(IDC_BUTTON3, &CAudioXtreamerDlg<T>::OnSendAsync)
  ON_BN_CLICKED(IDC_BUTTON4, &CAudioXtreamerDlg<T>::OnRecvAsync)
  ON_BN_CLICKED(IDC_BUTTON5, &CAudioXtreamerDlg<T>::OnStartStream)
  ON_BN_CLICKED(IDC_BUTTON6, &CAudioXtreamerDlg<T>::OnStopStream)
  ON_WM_PAINT()
  ON_WM_SYSCOMMAND()
END_MESSAGE_MAP()

template <class T>
BOOL CAudioXtreamerDlg<T>::OnCommand(WPARAM wParam, LPARAM lParam)
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

  return T::OnCommand(wParam, lParam);
}

// CAudioXtreamerDlg message handlers
template <class T>
BOOL CAudioXtreamerDlg<T>::OnInitDialog()
{
	T::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
  ASSERT(IDM_ABOUTBOX < 0xF000);

	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
    ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
      CMenu *menu = GetSystemMenu(FALSE);
      if (menu) {
        menu->AppendMenu(MF_SEPARATOR);
        menu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
      }
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	return TRUE;  // return TRUE  unless you set the focus to a control
}


template <class T>
void CAudioXtreamerDlg<T>::OnSysCommand(UINT nID, LPARAM lParam)
{
  if ((nID & 0xFFF0) == IDM_ABOUTBOX)
  {
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
  }
  else
  {
    T::OnSysCommand(nID, lParam);
  }
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

template <class T>
void CAudioXtreamerDlg<T>::OnPaint()
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
    // No more drawing required
	}
	else
	{
		T::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
template <class T>
HCURSOR CAudioXtreamerDlg<T>::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



template <class T>
void CAudioXtreamerDlg<T>::OnBnClickedCheck1()
{
  UpdateData(TRUE);
}

template <class T>
void CAudioXtreamerDlg<T>::OnOpen()
{
  if (mDevice.Open())
  {
    HWND hwnd = GetDlgItem(IDC_BUTTON1)->GetSafeHwnd();
    ::EnableWindow( hwnd, FALSE);
    hwnd = GetDlgItem(IDC_BUTTON2)->GetSafeHwnd();
    ::EnableWindow(hwnd, TRUE);
  }
}

template <class T>
void CAudioXtreamerDlg<T>::OnClose()
{
  if ( mDevice.Close()){
    HWND hwnd = GetDlgItem(IDC_BUTTON1)->GetSafeHwnd();
    ::EnableWindow(hwnd, TRUE);
    hwnd = GetDlgItem(IDC_BUTTON2)->GetSafeHwnd();
    ::EnableWindow(hwnd, FALSE);
  }
}

template <class T>
void CAudioXtreamerDlg<T>::OnSendAsync()
{
  //if (mDevice) mDevice->SendAsync(512,0);
}

template <class T>
void CAudioXtreamerDlg<T>::OnRecvAsync()
{
  //if (mDevice) mDevice->RecvAsync(512,0);
}

template <class T>
void CAudioXtreamerDlg<T>::OnStartStream()
{
  mDevice.Start();
}

template <class T>
void CAudioXtreamerDlg<T>::OnStopStream()
{
  mDevice.Stop(true);
}

