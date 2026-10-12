/* =========================================================================
 * Win32 GUI Counter App Sample for x86_64
 * A native Win32 window with a button and counter label.
 * ========================================================================= */

#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_VISIBLE          0x10000000
#define WS_CHILD            0x40000000
#define BS_PUSHBUTTON       0x00000000
#define SS_CENTER           0x00000001
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_COMMAND          0x0111
#define CW_USEDEFAULT       0x80000000

#define ID_BUTTON_INCREMENT 1001
#define ID_LABEL_COUNTER    1002

struct POINT {
    long x;
    long y;
};

struct MSG {
    void *hwnd;
    int message;
    long long wParam;
    long long lParam;
    int time;
    struct POINT pt;
};

/* Exact 72-byte binary layout for 64-bit Windows WNDCLASSA */
struct WNDCLASSA {
    long long style;
    void *lpfnWndProc;
    long long cbClsWndExtra;
    void *hInstance;
    void *hIcon;
    void *hCursor;
    void *hbrBackground;
    const char *lpszMenuName;
    const char *lpszClassName;
};

/* Win32 and C Runtime API Declarations */
int GetLastError();
void *GetModuleHandleA(const char *lpModuleName);
short RegisterClassA(const void *lpWndClass);
void *CreateWindowExA(
    int dwExStyle,
    const char *lpClassName,
    const char *lpWindowName,
    int dwStyle,
    int X, int Y, int nWidth, int nHeight,
    void *hWndParent,
    void *hMenu,
    void *hInstance,
    void *lpParam
);
int ShowWindow(void *hWnd, int nCmdShow);
int UpdateWindow(void *hWnd);
int GetMessageA(void *lpMsg, void *hWnd, int wMsgFilterMin, int wMsgFilterMax);
int TranslateMessage(const void *lpMsg);
long long DispatchMessageA(const void *lpMsg);
void PostQuitMessage(int nExitCode);
long long DefWindowProcA(void *hWnd, int Msg, long long wParam, long long lParam);
int SetWindowTextA(void *hWnd, const char *lpString);
int sprintf(char *str, const char *format, ...);

/* Application State */
static int g_counter = 0;
static void *g_hLabel = 0;
static void *g_hButton = 0;

long long WndProc(void *hwnd, int msg, long long wParam, long long lParam) {
    if (msg == WM_CREATE) {
        /* Label Control */
        g_hLabel = CreateWindowExA(
            0, "STATIC", "Count: 0",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            50, 30, 200, 30,
            hwnd, ID_LABEL_COUNTER, 0, 0
        );

        /* Button Control */
        g_hButton = CreateWindowExA(
            0, "BUTTON", "Increment",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            75, 80, 150, 35,
            hwnd, ID_BUTTON_INCREMENT, 0, 0
        );
        return 0;
    }

    if (msg == WM_COMMAND) {
        int id = wParam & 65535;
        if (id == ID_BUTTON_INCREMENT || lParam == g_hButton) {
            char buffer[64];
            g_counter = g_counter + 1;
            sprintf(buffer, "Count: %d", g_counter);
            SetWindowTextA(g_hLabel, buffer);
        }
        return 0;
    }

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int main() {
    void *hInstance = GetModuleHandleA(0);
    const char *CLASS_NAME = "CounterWindowClass";

    struct WNDCLASSA wc;
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = 6; /* COLOR_WINDOW + 1 */
    wc.lpszMenuName = 0;
    wc.lpszClassName = CLASS_NAME;

    short reg = RegisterClassA(&wc);
    if (!reg) {
        return 1;
    }

    void *hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "Win32 Counter GUI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 320, 200,
        0, 0, hInstance, 0
    );

    if (!hwnd) {
        return 2;
    }

    ShowWindow(hwnd, 5 /* SW_SHOW */);
    UpdateWindow(hwnd);

    struct MSG msg;
    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return msg.wParam;
}
