
// AudioXtreamer.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols
#include "wxx_appcore.h"


// CAudioXtreamerApp:
// See AudioXtreamer.cpp for the implementation of this class
//

class CAudioXtreamerApp : public CWinApp
{
public:
	CAudioXtreamerApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation


};

extern CAudioXtreamerApp theApp;