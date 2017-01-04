// GT_HelloWorldWin32.cpp
// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

unsigned long int FNV_hash(const void* dataToHash, size_t length)
{
	const unsigned char* p = (const unsigned char *)dataToHash;
	unsigned long int h = 2166136261UL;
	unsigned long int i;

	for (i = 0; i < length; i++)
		h = (h ^ 16777619) * p[i];

	return h;
}

struct HashMapData
{
	unsigned long int keyHash;
	TCHAR* keyString;
	void* data;
};

struct HashMap
{
	struct HashMapData* arr;
	unsigned int arrSize;
};

void HashMap_Init(struct HashMap* p)
{
	p->arr = NULL;
	p->arrSize = 0;
}

void HashMap_Add(struct HashMap* p, LPCTSTR key, void* data)
{
	p->arrSize += 1;
	p->arr = (struct HashMapData*)realloc(p->arr, p->arrSize * sizeof(struct HashMapData));
	p->arr[p->arrSize - 1].keyString = _tcsdup(key);
	p->arr[p->arrSize - 1].keyHash = FNV_hash(key, _tcslen(key));
	p->arr[p->arrSize - 1].data = data;
}

void HashMap_Remove(struct HashMap* p, LPCTSTR key)
{
}

void* HashMap_Find(struct HashMap* p, LPCTSTR key)
{
	unsigned long hashKey = FNV_hash((void*)key, _tcslen(key));
	for (UINT i = 0; i < p->arrSize; ++i)
	{
		struct HashMapData* d = p->arr+i;
		if (d->keyHash == hashKey)
			return d->data;
	}
	return NULL;
}

struct HashMap s_HashMap;

struct FocusData
{
	size_t timeSpend;
	LPTSTR title;
};

void UpdateFocusData()
{
	HWND hWnd = GetForegroundWindow();

	SetLastError(0);
	TCHAR buff[256];
	GetWindowText(hWnd, buff, 256);
	if (GetLastError() != 0)
		return;

	void* data = HashMap_Find(&s_HashMap, buff);
	if (data)
	{
		((struct FocusData*)data)->timeSpend += 1;
	}
	else
	{
		//int size = GetWindowTextLength(hWnd);
		struct FocusData* p = (struct FocusData*)malloc(sizeof(struct FocusData));
		p->timeSpend = 1;
		p->title = _tcsdup(buff);// (TCHAR*)malloc(sizeof(TCHAR)*(size + 1));
		HashMap_Add(&s_HashMap, buff, p);
		//GetWindowText(hWnd, p->title, size + 1);
	}
}

#define BUFSIZE 1024
void PrintFocusData()
{
	HANDLE hTempFile = INVALID_HANDLE_VALUE;
	TCHAR lpTempPathBuffer[MAX_PATH];
	TCHAR szTempFileName[MAX_PATH];
	TCHAR chBuffer[BUFSIZE];
	DWORD dwBytesWritten = 0;
	DWORD dwRetVal = GetTempPath(MAX_PATH,          // length of the buffer
		lpTempPathBuffer); // buffer for path 
	UINT uRetVal = GetTempFileName(lpTempPathBuffer, // directory for tmp files
		TEXT("DEMO"),     // temp file name prefix 
		0,                // create unique name 
		szTempFileName);  // buffer for name 
	hTempFile = CreateFile((LPTSTR)szTempFileName, // file name 
		GENERIC_WRITE,        // open for write 
		0,                    // do not share 
		NULL,                 // default security 
		CREATE_ALWAYS,        // overwrite existing
		FILE_ATTRIBUTE_NORMAL,// normal file 
		NULL);                // no template 

	for (size_t i = 0; i < s_HashMap.arrSize; ++i)
	{
		//OutputDebugString(s_HashMap.arr[i].keyString);
		struct FocusData* p = ((struct FocusData*)s_HashMap.arr[i].data);
		_stprintf_s(chBuffer, BUFSIZE, _T("%zd \"%s\"\n"), p->timeSpend, p->title);

		BOOL fSuccess = WriteFile(hTempFile,
			chBuffer,
			(DWORD)(sizeof(TCHAR) * _tcslen(chBuffer)),
			&dwBytesWritten,
			NULL);

		//OutputDebugString(chBuffer);
	}
	CloseHandle(hTempFile);
	ShellExecute(0, _T("open"), szTempFileName, NULL, NULL, SW_SHOW);
}

// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("win32app");

int s_intervalSec = 20 * 60; // 20min

SYSTEMTIME s_time;
HWND hwndQuitButton, hwndRestartButton, hwndStatusButton;
HWND hwndCurrentTimeText, hwndTimerText;

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("Focus");

HINSTANCE hInst;

// max 1 day add
void addSec(SYSTEMTIME* p, int sec)
{
	p->wSecond += sec;
	int m = p->wSecond / 60;
	p->wSecond -= m * 60;
	p->wMinute += m;
	int h = p->wMinute / 60;
	p->wMinute -= h * 60;
	p->wHour += h;
}

SYSTEMTIME diffTimes(const SYSTEMTIME* pSr, const SYSTEMTIME* pSl)
{
	SYSTEMTIME t_res;
	FILETIME v_ftime;
	ULARGE_INTEGER v_ui;
	__int64 v_right, v_left, v_res;
	SystemTimeToFileTime(pSr, &v_ftime);
	v_ui.LowPart = v_ftime.dwLowDateTime;
	v_ui.HighPart = v_ftime.dwHighDateTime;
	v_right = v_ui.QuadPart;

	SystemTimeToFileTime(pSl, &v_ftime);
	v_ui.LowPart = v_ftime.dwLowDateTime;
	v_ui.HighPart = v_ftime.dwHighDateTime;
	v_left = v_ui.QuadPart;

	v_res = v_right - v_left;

	v_ui.QuadPart = v_res;
	v_ftime.dwLowDateTime = v_ui.LowPart;
	v_ftime.dwHighDateTime = v_ui.HighPart;
	FileTimeToSystemTime(&v_ftime, &t_res);

	if (v_res < 0)
	{
		t_res.wYear = 1000;
		t_res.wHour = 0;
		t_res.wMinute = 0;
		t_res.wSecond = 0;
		t_res.wMilliseconds = 0;
	}

	return t_res;
}


// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	HashMap_Init(&s_HashMap);

	GetLocalTime(&s_time);

	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Win32 Guided Tour"),
			MB_OK);

		return 1;
	}

	hInst = hInstance;

	static int w = 220;
	static int h = 30;

	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		szWindowClass,
		szTitle,
		WS_BORDER|WS_POPUP, //WS_POPUPWINDOW, //WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		w, h,
		NULL,
		NULL,
		hInstance,
		NULL
		);

	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Win32 Guided Tour"),
			MB_OK);

		return 1;
	}

	int cx = 5;

	hwndRestartButton = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("R"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		20,        // Button width
		18,        // Button height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 20 + 5;

	hwndStatusButton = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("S"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		20,        // Button width
		18,        // Button height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 20 + 5;

	hwndCurrentTimeText = CreateWindow(
		_T("STATIC"),  // Predefined class; Unicode assumed 
		_T(""),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		60,        // Button width
		18,        // Button height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 60 + 5;

	hwndTimerText = CreateWindow(
		_T("STATIC"),  // Predefined class; Unicode assumed 
		_T(""),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		60,        // Button width
		18,        // Button height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 60 + 5;

	hwndQuitButton = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("X"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		w - 20 - 5,         // x position 
		h - 20 - 5,         // y position 
		20,        // Button width
		18,        // Button height
		hWnd,     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(hWnd, GWLP_HINSTANCE),
		NULL);      // Pointer not needed.


	// The parameters to ShowWindow explained:
	// hWnd: the value returned from CreateWindow
	// nCmdShow: the fourth parameter from WinMain
	ShowWindow(hWnd,
		nCmdShow);
	UpdateWindow(hWnd);

	SetTimer(hWnd, (UINT_PTR)NULL, 1000, NULL);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	TCHAR greeting[] = _T("Hello, World!");

	switch (message)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
		if (hit == HTCLIENT) hit = HTCAPTION;
		return hit;
	}
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		
		SYSTEMTIME t;
		GetLocalTime(&t);
		static TCHAR b[256];
		GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT, &t, L"HH':'mm':'ss", b, 256);
		SendMessage(hwndCurrentTimeText, WM_SETTEXT, 0, (LPARAM)b);

		// diff time
		SYSTEMTIME s = diffTimes(&s_time, &t);
		if (s.wYear == 1000) // overflow
		{
			GetLocalTime(&s_time);
			addSec(&s_time, s_intervalSec);
		}
		static TCHAR c[256];
		GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT, &s, L"HH':'mm':'ss", c, 256);
		SendMessage(hwndTimerText, WM_SETTEXT, 0, (LPARAM)c);

		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_COMMAND:
		if (lParam == (LPARAM)hwndQuitButton) {
			SendMessage(hWnd, WM_DESTROY, 0, 0);
		}
		else if (lParam == (LPARAM)hwndRestartButton) {
			GetLocalTime(&s_time);
			addSec(&s_time, s_intervalSec);
		}
		else if (lParam == (LPARAM)hwndStatusButton) {
			PrintFocusData();
			// write stats file
			// open it with notepad
		}
		break;

	case WM_TIMER:
		UpdateFocusData();
		SendMessage(hWnd, WM_PAINT, 0, 0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}