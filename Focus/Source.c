#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

//////////////////////////////////////////////// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("win32app");

//static int s_intervalSec = 20 * 60; // 20min
static int s_intervalSec = 10; // 20min

static SYSTEMTIME s_time;

static double s_ratio; // 0..1 in the interval

enum BUTTONS
{
	Parent = 0,
	Focus,
	Time,
	Date,
	Timer,
	Reset,
	Status,
	Exit,
	BUTTONS_COUNT
};

HWND ctls[BUTTONS_COUNT] = { 0 };

HFONT hFont = NULL;

enum BRUSHES
{
	BackgroundRect = 0,
	ForegroundRect,
	Text,
	BRUSHES_COUNT
};
COLORREF cols[BRUSHES_COUNT] = { RGB(0, 0, 0), RGB(234, 123, 86), RGB(127, 255, 127) };
HBRUSH brshs[BRUSHES_COUNT] = { 0 };

///////////////////////////////////////////////// forward decl

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


///////////////////////////////////////////////// Hash

static unsigned long int FNV_hash(const void* dataToHash, size_t length)
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

static void HashMap_Init(struct HashMap* p)
{
	p->arr = NULL;
	p->arrSize = 0;
}

static void HashMap_Add(struct HashMap* p, LPCTSTR key, void* data)
{
	p->arrSize += 1;
	p->arr = (struct HashMapData*)realloc(p->arr, p->arrSize * sizeof(struct HashMapData));
	p->arr[p->arrSize - 1].keyString = _tcsdup(key);
	p->arr[p->arrSize - 1].keyHash = FNV_hash(key, _tcslen(key));
	p->arr[p->arrSize - 1].data = data;
}

static void HashMap_Remove(struct HashMap* p, LPCTSTR key)
{
}

static void* HashMap_Find(struct HashMap* p, LPCTSTR key)
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

static struct HashMap s_HashMap;

struct FocusData
{
	size_t timeSpend;
	LPTSTR title;
};

/////////////////////////////////////////////////////// named intervals

struct array_t
{
	void** data;
	size_t size;
	size_t allocated;
};

static void list_Init(struct array_t* v)
{
	v->data = (void**)malloc(10 * sizeof(void*));
	v->size = 0;
	v->allocated = 10;
}

static void list_Add(struct array_t* v, void* data)
{
	if (v->size == v->allocated)
	{
		v->data = (void**)realloc(v->data, v->size + 10 * sizeof(void*));
		v->allocated = v->size + 10;
	}
	v->data[v->size] = data;
	v->size++;
}

static struct array_t s_NamedIntervals;

/////////////////////////////////////////////////////// Helpers

static ULONGLONG MilisFromSysTime(const SYSTEMTIME* t)
{
	FILETIME ft;
	ULARGE_INTEGER li;
	SystemTimeToFileTime(t, &ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	return li.QuadPart / 10 / 1000;
}

static SYSTEMTIME SysTimeFromMilis(ULONGLONG m)
{
	SYSTEMTIME t;
	FILETIME ft;
	ULARGE_INTEGER li;
	li.QuadPart = m * 10 * 1000;
	ft.dwLowDateTime = li.LowPart;
	ft.dwHighDateTime = li.HighPart;
	FileTimeToSystemTime(&ft, &t);
	return t;
}

static SYSTEMTIME diffTimes(const SYSTEMTIME* pSr, const SYSTEMTIME* pSl)
{
	LONGLONG diff = (LONGLONG)MilisFromSysTime(pSr) - (LONGLONG)MilisFromSysTime(pSl);
	SYSTEMTIME t = SysTimeFromMilis((ULONGLONG)diff);
	if (diff < 0)
		t.wYear = 1000;
	return t;
}

static void UpdateRatio(const SYSTEMTIME* pS)
{
	if (pS->wYear != 1000)
	{
		double s = (double)MilisFromSysTime(pS) / 1000.0;
		s_ratio = (double)s / s_intervalSec;
	}
	else
	{
		s_ratio = 1.0;
	}
}

// max 1 day add
static void addSec(SYSTEMTIME* p, int sec)
{
	p->wSecond += sec;
	int m = p->wSecond / 60;
	p->wSecond -= m * 60;
	p->wMinute += m;
	int h = p->wMinute / 60;
	p->wMinute -= h * 60;
	p->wHour += h;
}

static void UpdateFocusData()
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

static void StartNewInterval()
{
	GetLocalTime(&s_time);
	addSec(&s_time, s_intervalSec);
	static i = 0;
	char* v = (char*)malloc(10);
	(void*)_itoa_s(i++, v, 10, 10);
	list_Add(&s_NamedIntervals, v);
}

static void UpdateControls()
{
	SYSTEMTIME t;
	GetLocalTime(&t);
	static TCHAR b[256];
	_stprintf_s(b, 256, _T("%02d:%02d:%02d"), t.wHour, t.wMinute, t.wSecond);
	SendMessage(ctls[Time], WM_SETTEXT, 0, (LPARAM)b);

	static TCHAR c[256];
	_stprintf_s(c, 256, _T("%02d/%02d"), t.wDay, t.wMonth);
	SendMessage(ctls[Date], WM_SETTEXT, 0, (LPARAM)c);

	// diff time
	SYSTEMTIME s = diffTimes(&s_time, &t);
	UpdateRatio(&s);
	if (s.wYear == 1000) // overflow
	{
		StartNewInterval();
	}
	static TCHAR d[256];
	_stprintf_s(d, 256, _T("%02d:%02d:%02d"), s.wHour, s.wMinute, s.wSecond);
	SendMessage(ctls[Timer], WM_SETTEXT, 0, (LPARAM)d);
}

#define BUFSIZE 1024
static void PrintFocusData()
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

static void CreateControls(HINSTANCE hInstance)
{
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
			_T("Focus"),
			MB_OK);

		exit(2);
	}
	
	static int w = 375;
	static int h = 30;

	ctls[Parent] = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
		szWindowClass,
		_T("Focus"),
		WS_BORDER | WS_POPUP, //WS_POPUPWINDOW, //WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		w, h,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!ctls[Parent])
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Win32 Guided Tour"),
			MB_OK);

		exit(1);
	}

	int cx = 5;

	ctls[Focus] = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("Focus!"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		65,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 65 + 5;

	ctls[Time] = CreateWindow(
		_T("STATIC"),  // Predefined class; Unicode assumed 
		_T(""),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		80,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 80 + 5;

	ctls[Date] = CreateWindow(
		_T("STATIC"),  // Predefined class; Unicode assumed 
		_T(""),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		50,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 50 + 5;

	ctls[Reset] = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("R"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		20,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 20 + 5;

	ctls[Status] = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("S"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		20,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 20 + 5;

	ctls[Timer] = CreateWindow(
		_T("STATIC"),  // Predefined class; Unicode assumed 
		_T(""),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD /*| BS_DEFPUSHBUTTON*/,  // Styles 
		cx,         // x position 
		5,         // y position 
		80,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.
	cx += 80 + 5;

	ctls[Exit] = CreateWindow(
		_T("BUTTON"),  // Predefined class; Unicode assumed 
		_T("X"),      // Button text 
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT /*| BS_DEFPUSHBUTTON*/,  // Styles 
		w - 20 - 5,         // x position 
		h - 20 - 5,         // y position 
		20,        // Button width
		18,        // Button height
		ctls[Parent],     // Parent window
		NULL,       // No menu.
		(HINSTANCE)(LONG_PTR)GetWindowLong(ctls[Parent], GWLP_HINSTANCE),
		NULL);      // Pointer not needed.

	hFont = CreateFont(20, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, _T("Courier New"));
	if (hFont)
	{
		for (UINT i = 0; i < BUTTONS_COUNT; ++i)
		{
			SendMessage(ctls[i], WM_SETFONT, (WPARAM)hFont, 0);
		}
	}
	for (UINT i=0; i<BRUSHES_COUNT; ++i)
		brshs[i] = CreateSolidBrush(cols[i]);
}

//////////////////////////////////////////////////// core stuff

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_NCHITTEST:
	{
		LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
		if (hit == HTCLIENT) hit = HTCAPTION;
		return hit;
	}
	break;

	case WM_PAINT:
	{
		hdc = BeginPaint(hWnd, &ps);
		UpdateControls();
		RECT r;
		GetClientRect(hWnd, &r);
		FillRect(hdc, &r, brshs[BackgroundRect]);
		r.right = (LONG)((1.0 - s_ratio) * (double)r.right);
		FillRect(hdc, &r, brshs[ForegroundRect]);
		EndPaint(hWnd, &ps);
	}
	break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetBkMode((HDC)wParam, TRANSPARENT);
		//SetBkMode((HDC)wParam, TRANSPARENT);
		SetTextColor(hdcStatic, cols[Text]);
		//SetBkColor(hdcStatic, RGB(0, 0, 0));
		return (LRESULT)GetStockObject(HOLLOW_BRUSH);
	}
	break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;

	case WM_COMMAND:
	{
		if (lParam == (LPARAM)ctls[Exit]) {
			SendMessage(hWnd, WM_DESTROY, 0, 0);
		}
		else if (lParam == (LPARAM)ctls[Reset]) {
			StartNewInterval();
		}
		else if (lParam == (LPARAM)ctls[Status]) {
			PrintFocusData();
			// write stats file
			// open it with notepad
		}
	}
	break;

	case WM_TIMER:
	{
		UpdateFocusData();
		SendMessage(hWnd, WM_PAINT, 0, 0);
		RECT z;
		GetClientRect(hWnd, &z);
		InvalidateRect(hWnd, &z, TRUE);
	}
	break;

	default:
	{
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;

	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	HashMap_Init(&s_HashMap);

	GetLocalTime(&s_time);

	CreateControls(hInstance);

	// The parameters to ShowWindow explained:
	// hWnd: the value returned from CreateWindow
	// nCmdShow: the fourth parameter from WinMain
	ShowWindow(ctls[Parent],
		nCmdShow);
	UpdateWindow(ctls[Parent]);

	SetTimer(ctls[Parent], (UINT_PTR)NULL, 1000, NULL);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DeleteObject(hFont);
	for (UINT i=0; i<BRUSHES_COUNT; ++i)
		DeleteObject(brshs[i]);

	return (int)msg.wParam;
}
