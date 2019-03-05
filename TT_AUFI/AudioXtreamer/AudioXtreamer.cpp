
// AudioXtreamer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "AudioXtreamer.h"
#include "AudioXtreamerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  try
  {
    // Start Win32++
    CAudioXtreamerApp theApp;

    // Run the modal application returning true to the caller because the initinstance will return false
    theApp.Run();
    return 0;
  }

  // catch all unhandled CException types
  catch (const CException &e)
  {
    // Display the exception and quit
    MessageBox(NULL, e.GetText(), AtoT(e.what()), MB_ICONERROR);

    return -1;
  }
}

// CAudioXtreamerApp construction

CAudioXtreamerApp::CAudioXtreamerApp()
{
	// support Restart Manager
	//m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// CAudioXtreamerApp initialization

BOOL CAudioXtreamerApp::InitInstance()
{
	CWinApp::InitInstance();



	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	//SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	CAudioXtreamerDlg dlg;
	
	INT_PTR nResponse = dlg.DoModal(0);
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE("Warning: dialog creation failed, so application is terminating unexpectedly.\n");
	}


	//returning true must start the message pump and we need to be sure a close message is sent to kill the app
  ::PostQuitMessage(0);
	return TRUE; 
}

