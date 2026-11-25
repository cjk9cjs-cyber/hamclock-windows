/* Native window for HamClock. The drawing surface is the same in-memory
 * canvas the X11 build uses; this thread copies it into a Win32 window and
 * turns mouse and keyboard input into the events the rest of HamClock expects.
 */
#include "HamClock.h"

#include <windowsx.h>

static Adafruit_RA8875 *g_tft;
static HWND g_hwnd;
static volatile bool g_fs_req;
static bool g_fs;
static bool g_cursor_on = true;
static int g_view_w = FB_XRES;
static int g_view_h = FB_YRES;

/* Mouse events arrive in window pixels. HamClock wants the unscaled canvas. */
static void clientToCanvas(int x, int y, int *ox, int *oy)
{
    if (x < 0 || y < 0 || g_view_w <= 0 || g_view_h <= 0) {
        *ox = x;
        *oy = y;
        return;
    }
    *ox = (int)((long long)x * FB_XRES / g_view_w);
    *oy = (int)((long long)y * FB_YRES / g_view_h);
    if (*ox >= FB_XRES)
        *ox = FB_XRES - 1;
    if (*oy >= FB_YRES)
        *oy = FB_YRES - 1;
}

static void note_key(int c, bool ctrl, bool shift)
{
    if (g_tft && c)
        g_tft->win32Key(c, ctrl, shift);
}

static void paste_clipboard(void)
{
    if (!OpenClipboard(g_hwnd))
        return;
    HANDLE data = GetClipboardData(CF_TEXT);
    if (data) {
        const char *text = (const char *)GlobalLock(data);
        if (text) {
            for (const char *p = text; *p && p < text + 4000; p++)
                note_key((unsigned char)*p, false, false);
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
}

static LRESULT CALLBACK hamclock_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        if (g_tft)
            g_tft->win32Close();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (g_tft && wp != SIZE_MINIMIZED)
            g_tft->win32Resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (g_tft)
            g_tft->win32Pointer(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0, 0, 0, true);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        if (g_tft)
            g_tft->win32Pointer(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1, 1, 0, true);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        if (g_tft)
            g_tft->win32Pointer(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 1, 0, 1, true);
        return 0;
    case WM_RBUTTONDOWN:
        if (g_tft)
            g_tft->win32Pointer(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2, 1, 0, true);
        return 0;
    case WM_RBUTTONUP:
        if (g_tft)
            g_tft->win32Pointer(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2, 0, 1, true);
        return 0;
    case WM_MOUSELEAVE:
        if (g_tft)
            g_tft->win32Pointer(-1, -1, 0, 0, 0, false);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(g_cursor_on ? LoadCursor(NULL, IDC_ARROW) : NULL);
            return TRUE;
        }
        break;
    case WM_KEYDOWN: {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int c = 0;
        switch (wp) {
        case VK_LEFT: c = CHAR_LEFT; break;
        case VK_DOWN: c = CHAR_DOWN; break;
        case VK_UP: c = CHAR_UP; break;
        case VK_RIGHT: c = CHAR_RIGHT; break;
        case VK_DELETE: c = CHAR_DEL; break;
        case 'V':
            if (ctrl) {
                paste_clipboard();
                return 0;
            }
            break;
        default:
            break;
        }
        if (c)
            note_key(c, ctrl, shift);
        return 0;
    }
    case WM_CHAR: {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        unsigned c = (unsigned)wp;
        if (c == 22) {
            paste_clipboard();
            return 0;
        }
        if (c == '\r')
            c = CHAR_NL;
        if (c >= 32 || c == '\b' || c == '\t' || c == CHAR_NL || c == 27)
            note_key((int)c, ctrl, shift);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void Adafruit_RA8875::win32Pointer(int x, int y, int button, int down_delta, int up_delta, bool valid)
{
    int cx = x, cy = y;
    if (valid)
        clientToCanvas(x, y, &cx, &cy);
    pthread_mutex_lock(&mouse_lock);
    mouse_x = valid ? (int16_t)cx : -1;
    mouse_y = valid ? (int16_t)cy : -1;
    if (button)
        mouse_button = button;
    mouse_downs += down_delta;
    mouse_ups += up_delta;
    gettimeofday(&mouse_tv, NULL);
    pthread_mutex_unlock(&mouse_lock);
}

void Adafruit_RA8875::win32Key(int c, bool ctrl, bool shift)
{
    putChar((char)c, ctrl, shift);
}

void Adafruit_RA8875::win32Resize(int w, int h)
{
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    /* The canvas stays 1600x960. A larger window, including full screen, stretches it. */
    g_view_w = w;
    g_view_h = h;
    fb_si.xres = w;
    fb_si.yres = h;
    FB_X0 = 0;
    FB_Y0 = 0;
    if (fb_stage)
        memset(fb_stage, 0xff, fb_nbytes);
    fb_dirty = true;
}

void Adafruit_RA8875::win32Close(void)
{
    if (!g_fs && g_hwnd && !ignore_x11geom) {
        RECT rc;
        if (GetWindowRect(g_hwnd, &rc))
            NVWriteX11Geom(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    }
}

void *Adafruit_RA8875::fbThreadHelper(void *me)
{
    pthread_detach(pthread_self());
    ((Adafruit_RA8875 *)me)->fbThread();
    return NULL;
}

void Adafruit_RA8875::drawCanvas()
{
    for (int y = 0; y < FB_YRES; y++) {
        int max_x = pr_draw || pr_w == 0 || y < pr_y ? FB_XRES : pr_x;
        fbpix_t *stage_p = &fb_stage[y * FB_XRES];
        fbpix_t *canvas_p = &fb_canvas[y * FB_XRES];
        for (int x = 0; x < FB_XRES; x++) {
            int max_y = pr_draw || pr_h == 0 || x < pr_x ? FB_YRES : pr_y;
            if (x >= max_x && y >= max_y)
                continue;
            stage_p[x] = canvas_p[x];
        }
    }

    if (!g_hwnd)
        return;

    HDC hdc = GetDC(g_hwnd);
    if (!hdc)
        return;

    RECT rc;
    GetClientRect(g_hwnd, &rc);
    int dw = rc.right > 0 ? rc.right : FB_XRES;
    int dh = rc.bottom > 0 ? rc.bottom : FB_YRES;
    g_view_w = dw;
    g_view_h = dh;

    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = FB_XRES;
    bi.bmiHeader.biHeight = -FB_YRES;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc, 0, 0, dw, dh,
            0, 0, FB_XRES, FB_YRES, fb_stage, &bi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
}

void Adafruit_RA8875::X11OptionsEngageNow(bool fs)
{
    g_fs = fs;
    g_fs_req = true;
    while (g_fs_req)
        usleep(1000);
}

void Adafruit_RA8875::getScreenSize(int *w, int *h)
{
    *w = GetSystemMetrics(SM_CXSCREEN);
    *h = GetSystemMetrics(SM_CYSCREEN);
}

bool Adafruit_RA8875::getWinScreenGeom(int *x, int *y, int *w, int *h)
{
    if (!g_hwnd)
        return false;
    RECT rc;
    if (!GetWindowRect(g_hwnd, &rc))
        return false;
    *x = rc.left;
    *y = rc.top;
    *w = rc.right - rc.left;
    *h = rc.bottom - rc.top;
    return true;
}

bool Adafruit_RA8875::warpCursor(char dir, unsigned n, int *xp, int *yp)
{
    if (!g_hwnd)
        return false;

    POINT pt;
    if (!GetCursorPos(&pt) || !ScreenToClient(g_hwnd, &pt))
        return false;

    int step_x = g_view_w > 0 ? (int)n * SCALESZ * g_view_w / FB_XRES : (int)n * SCALESZ;
    int step_y = g_view_h > 0 ? (int)n * SCALESZ * g_view_h / FB_YRES : (int)n * SCALESZ;
    int new_x = pt.x;
    int new_y = pt.y;
    switch (dir) {
    case CHAR_LEFT:  new_x = pt.x - step_x; break;
    case CHAR_DOWN:  new_y = pt.y + step_y; break;
    case CHAR_UP:    new_y = pt.y - step_y; break;
    case CHAR_RIGHT: new_x = pt.x + step_x; break;
    default: break;
    }

    int vw = g_view_w > 0 ? g_view_w : FB_XRES;
    int vh = g_view_h > 0 ? g_view_h : FB_YRES;
    new_x = (new_x % vw + vw) % vw;
    new_y = (new_y % vh + vh) % vh;

    POINT dest = {new_x, new_y};
    ClientToScreen(g_hwnd, &dest);
    SetCursorPos(dest.x, dest.y);

    int cx, cy;
    clientToCanvas(new_x, new_y, &cx, &cy);
    if (xp)
        *xp = cx / SCALESZ;
    if (yp)
        *yp = cy / SCALESZ;
    return true;
}

void Adafruit_RA8875::fbThread()
{
    g_tft = this;
    SetProcessDPIAware();

    HINSTANCE inst = GetModuleHandle(NULL);
    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = hamclock_wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "HamClockWindow";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    RECT want = {0, 0, FB_XRES, FB_YRES};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    AdjustWindowRect(&want, style, FALSE);
    int win_w = want.right - want.left;
    int win_h = want.bottom - want.top;

    /* The monitor under the cursor is the one the user is looking at. */
    POINT cursor = {0, 0};
    GetCursorPos(&cursor);
    HMONITOR here = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    int win_x = 40;
    int win_y = 40;
    if (GetMonitorInfo(here, &mi)) {
        int work_w = mi.rcWork.right - mi.rcWork.left;
        int work_h = mi.rcWork.bottom - mi.rcWork.top;
        win_x = mi.rcWork.left + (work_w - win_w) / 2;
        win_y = mi.rcWork.top + (work_h - win_h) / 2;
        if (win_x < mi.rcWork.left)
            win_x = mi.rcWork.left;
        if (win_y < mi.rcWork.top)
            win_y = mi.rcWork.top;
    }

    g_hwnd = CreateWindow(wc.lpszClassName, "HamClock", style,
            win_x, win_y, win_w, win_h,
            NULL, NULL, inst, NULL);
    if (!g_hwnd) {
        ::printf("Can not create HamClock window\n");
        exit(1);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    for (;;) {
        ready = true;

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                doExit();
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_fs_req && g_hwnd) {
            if (g_fs) {
                /* Full screen stays on whichever monitor the window is already on. */
                HMONITOR here = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi;
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfo(here, &mi)) {
                    SetWindowPos(g_hwnd, NULL,
                            mi.rcMonitor.left, mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top,
                            SWP_NOZORDER | SWP_NOACTIVATE);
                } else {
                    ShowWindow(g_hwnd, SW_MAXIMIZE);
                }
            } else {
                ShowWindow(g_hwnd, SW_RESTORE);
            }
            g_fs_req = false;
        }

        struct timeval tv;
        gettimeofday(&tv, NULL);
        mouse_idle = (tv.tv_sec - mouse_tv.tv_sec) * 1000 + (tv.tv_usec - mouse_tv.tv_usec) / 1000;
        bool want_cursor = mouse_idle <= MOUSE_FADE;
        if (want_cursor != g_cursor_on) {
            g_cursor_on = want_cursor;
            PostMessage(g_hwnd, WM_SETCURSOR, (WPARAM)g_hwnd, HTCLIENT);
        }

        pthread_mutex_lock(&fb_lock);
        if (fb_dirty || pr_draw) {
            drawCanvas();
            fb_dirty = false;
            pr_draw = false;
        }
        pthread_mutex_unlock(&fb_lock);

        usleep(REFRESH_US);
    }
}
