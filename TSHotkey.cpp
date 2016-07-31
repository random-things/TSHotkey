// TSHotkey.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "TSHotkey.h"

#define MAX_LOADSTRING 100

#define TRAYICONID 1
#define SWM_TRAYMSG WM_APP
#define SWM_EXIT WM_APP + 1

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shell32.lib")

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

NOTIFYICONDATA niData;

HDEVINFO hDevInfo;
SP_DEVINFO_DATA device;

bool ts_state_enabled = FALSE;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

DWORD ToggleDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA device, DWORD state)
{
	SP_PROPCHANGE_PARAMS params;
	params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
	params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
	params.Scope = DICS_FLAG_GLOBAL;
	params.StateChange = state;

	if (!SetupDiSetClassInstallParams(hDevInfo, &device, &params.ClassInstallHeader, sizeof(params)))
	{
		return GetLastError();
	}

	if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevInfo, &device))
	{
		return GetLastError();
	}

	return FALSE;
}

int FindDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA *DeviceInfoData, std::wstring search)
{
	DWORD i;
	std::wstring device;

	DeviceInfoData->cbSize = sizeof(SP_DEVINFO_DATA);
	for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, DeviceInfoData); i++)
	{
		DWORD DataT;
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		while (!SetupDiGetDeviceRegistryProperty(
			hDevInfo,
			DeviceInfoData,
			SPDRP_DEVICEDESC,
			&DataT,
			(PBYTE)buffer,
			buffersize,
			&buffersize
		))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (buffer) LocalFree(buffer);
				buffer = (LPTSTR)LocalAlloc(LPTR, buffersize * 2);
			}
			else {
				break;
			}
		}

		if (buffer)
		{
			device = buffer;

			if (device.find(search) != std::string::npos)
			{
				LocalFree(buffer);
				return FALSE;
			}

			LocalFree(buffer);
		}
	}

	return TRUE;
}

int DisplayNotification(LPCWSTR notification)
{
	StringCchCopy(niData.szInfo, ARRAYSIZE(niData.szInfo), notification);
	StringCchCopy(niData.szTip, ARRAYSIZE(niData.szTip), notification);
	Shell_NotifyIcon(NIM_MODIFY, &niData);
	return FALSE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	ULONG devStatus, devProblemCode;

	hDevInfo = SetupDiGetClassDevs(0L, 0L, 0L, DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_PROFILE);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	// CTRL + ALT + T, for "Touchscreen".
	if (RegisterHotKey(NULL, 1, MOD_ALT | MOD_CONTROL | MOD_NOREPEAT, 0x54))
	{
		// Something. Don't know. Don't care.
	}

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_TSHOTKEY, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TSHOTKEY));

	// All right, before we start processing Windows messages, let's see if we can find the touchscreen.
	if (FindDevice(hDevInfo, &device, L"touch screen"))
	{
		// Let's try one more time, just in case...
		if (FindDevice(hDevInfo, &device, L"touchscreen"))
		{
			MessageBox(NULL, L"Could not find a touchscreen device to use.", L"Error", MB_OK);
			return FALSE;
		}
	}

	// Now let's see if we can grab the current status of the device...
	if (CM_Get_DevNode_Status(&devStatus, &devProblemCode, device.DevInst, 0) != CR_SUCCESS)
	{
		// Couldn't determine the status for the device even though we found it.
		MessageBox(NULL, L"Although the device was found, could not determine the current device status.", L"Warning", MB_OK);
	}

	if (devStatus & DN_STARTED) ts_state_enabled = TRUE;

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

		if (msg.message == WM_HOTKEY)
		{
			if (ts_state_enabled)
			{
				ToggleDevice(hDevInfo, device, DICS_DISABLE);
				ts_state_enabled = FALSE;
				DisplayNotification(L"Touchscreen disabled.");

			}
			else {
				ToggleDevice(hDevInfo, device, DICS_ENABLE);
				ts_state_enabled = TRUE;
				DisplayNotification(L"Touchscreen enabled.");
			}
		}
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TSHOTKEY));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TSHOTKEY);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ZeroMemory(&niData, sizeof(NOTIFYICONDATA));

   niData.cbSize = sizeof(NOTIFYICONDATA);
   niData.uID = TRAYICONID;
   niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
   niData.hIcon = (HICON)LoadImage(hInstance,
	   MAKEINTRESOURCE(IDI_SMALL),
	   IMAGE_ICON,
	   GetSystemMetrics(SM_CXSMICON),
	   GetSystemMetrics(SM_CYSMICON),
	   LR_DEFAULTCOLOR);
   niData.hWnd = hWnd;
   niData.uCallbackMessage = SWM_TRAYMSG;

   Shell_NotifyIcon(NIM_ADD, &niData);

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

void ShowContextMenu(HWND hWnd)
{
	HMENU hMenu;

	hMenu = CreatePopupMenu();

	if (hMenu)
	{
		InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, L"Exit");
	}

	POINT pt;
	GetCursorPos(&pt);
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
			case SWM_EXIT:
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &niData);
		SetupDiDestroyDeviceInfoList(hDevInfo);
        PostQuitMessage(0);
        break;
	case SWM_TRAYMSG:
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			//ShowWindow(hWnd, SW_RESTORE);
			break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
			break;
		}
		break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
