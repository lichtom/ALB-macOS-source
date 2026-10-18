// ALB Agame - 2D Game Engine
// Windows: Win32 GDI backend (no external dependencies)
// Linux/macOS: SDL2 backend (requires SDL2 dev libraries)

#ifdef _WIN32
// ============================================================================
// Windows Backend - Win32 GDI (zero external dependencies)
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#define DLL_EXPORT __declspec(dllexport)

// ---------- Globals ----------
static HWND    g_hwnd      = nullptr;
static HDC     g_backDC    = nullptr;
static HBITMAP g_backBuf   = nullptr;
static COLORREF g_color    = RGB(255,255,255);
static bool    g_quit      = false;
static int     g_win_w     = 800;
static int     g_win_h     = 600;

struct Ev { int type; int key; int mx; int my; int btn; };
static std::vector<Ev> g_evq;

// Previous key state for edge detection (256 VK codes)
static short g_prev_key[256] = {0};

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch(m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_backDC) BitBlt(dc, 0, 0, g_win_w, g_win_h, g_backDC, 0, 0, SRCCOPY);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CLOSE:  g_quit = true; DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, w, l);
}

// Helper: parse "(r g b a)" or "r g b a" -> COLORREF
static COLORREF parse_col(const char* s) {
    if (!s) return RGB(255,255,255);
    while (*s == ' ' || *s == '(') s++;
    int r=255,g=255,b=255;
    sscanf(s, "%d %d %d", &r, &g, &b);
    return RGB(r<0?0:(r>255?255:r), g<0?0:(g>255?255:g), b<0?0:(b>255?255:b));
}

static const char* istr(int n) {
    static char b[32];
    snprintf(b, sizeof(b), "%d", n);
    return b;
}

// VK code -> ALB key name
static const char* vk_name(int vk) {
    switch(vk) {
    case VK_UP: return "up"; case VK_DOWN: return "down";
    case VK_LEFT: return "left"; case VK_RIGHT: return "right";
    case VK_SPACE: return "space"; case VK_RETURN: return "return";
    case VK_ESCAPE: return "escape"; case VK_BACK: return "backspace";
    case VK_TAB: return "tab";
    case VK_LSHIFT: return "lshift"; case VK_RSHIFT: return "rshift";
    case VK_LCONTROL: return "lctrl"; case VK_RCONTROL: return "rctrl";
    default:
        if (vk >= 'A' && vk <= 'Z') { static char c[2]={0,0}; c[0]=(char)(vk+32); return c; }
        if (vk >= '0' && vk <= '9') { static char c[2]={0,0}; c[0]=(char)vk; return c; }
        return nullptr;
    }
}

// ALB key name -> VK code
static int name_vk(const char* name) {
    if (!name) return 0;
    if (strcmp(name,"up")==0) return VK_UP;
    if (strcmp(name,"down")==0) return VK_DOWN;
    if (strcmp(name,"left")==0) return VK_LEFT;
    if (strcmp(name,"right")==0) return VK_RIGHT;
    if (strcmp(name,"space")==0) return VK_SPACE;
    if (strcmp(name,"return")==0) return VK_RETURN;
    if (strcmp(name,"escape")==0) return VK_ESCAPE;
    if (strcmp(name,"backspace")==0) return VK_BACK;
    if (strcmp(name,"tab")==0) return VK_TAB;
    if (strcmp(name,"lshift")==0) return VK_LSHIFT;
    if (strcmp(name,"rshift")==0) return VK_RSHIFT;
    if (strcmp(name,"lctrl")==0) return VK_LCONTROL;
    if (strcmp(name,"rctrl")==0) return VK_RCONTROL;
    if (strlen(name)==1) {
        char c = name[0];
        if (c>='a'&&c<='z') return c-32;  // VK_A..VK_Z
        if (c>='0'&&c<='9') return c;
    }
    return 0;
}

// ============================================================================
// Window Management
// ============================================================================

const char* agame_init(int argc, const char* argv[]) {
    if (argc < 3) return "error: agame-init needs (title width height)";
    const char* title = argv[0];
    int w = atoi(argv[1]), h = atoi(argv[2]);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ALBAgameWnd";
    RegisterClassExW(&wc);

    // Convert UTF-8 title to wide
    wchar_t wtitle[256] = {0};
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 255);

    RECT rc = {0, 0, w, h};
    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    AdjustWindowRect(&rc, style, FALSE);

    g_hwnd = CreateWindowExW(0, L"ALBAgameWnd", wtitle, style,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right-rc.left, rc.bottom-rc.top,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!g_hwnd) return "error: CreateWindowEx failed";

    g_win_w = w; g_win_h = h; g_quit = false;
    memset(g_prev_key, 0, sizeof(g_prev_key));

    // Back buffer
    HDC hdc = GetDC(g_hwnd);
    g_backDC = CreateCompatibleDC(hdc);
    g_backBuf = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(g_backDC, g_backBuf);
    ReleaseDC(g_hwnd, hdc);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    return "ok";
}

const char* agame_quit(int, const char**) {
    if (g_backBuf) { DeleteObject(g_backBuf); g_backBuf = nullptr; }
    if (g_backDC)  { DeleteDC(g_backDC); g_backDC = nullptr; }
    if (g_hwnd)    { DestroyWindow(g_hwnd); g_hwnd = nullptr; }
    return "ok";
}

const char* agame_clear(int, const char**) {
    if (!g_backDC) return "error: not initialized";
    RECT r = {0, 0, g_win_w, g_win_h};
    HBRUSH br = CreateSolidBrush(RGB(0,0,0));
    FillRect(g_backDC, &r, br);
    DeleteObject(br);
    return "ok";
}

const char* agame_present(int, const char**) {
    if (!g_hwnd || !g_backDC) return "error";
    HDC hdc = GetDC(g_hwnd);
    BitBlt(hdc, 0, 0, g_win_w, g_win_h, g_backDC, 0, 0, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);
    return "ok";
}

const char* agame_delay(int argc, const char* argv[]) {
    if (argc < 1) return "error";
    Sleep((DWORD)atoi(argv[0]));
    return "ok";
}

const char* agame_window_width(int, const char**)  { return istr(g_win_w); }
const char* agame_window_height(int, const char**) { return istr(g_win_h); }

const char* agame_set_title(int argc, const char* argv[]) {
    if (argc < 1 || !g_hwnd) return "error";
    wchar_t wt[256] = {0};
    MultiByteToWideChar(CP_UTF8, 0, argv[0], -1, wt, 255);
    SetWindowTextW(g_hwnd, wt);
    return "ok";
}

// ============================================================================
// Color
// ============================================================================
const char* agame_set_color(int argc, const char* argv[]) {
    if (argc < 1) return "error";
    g_color = parse_col(argv[0]);
    return "ok";
}

// ============================================================================
// Drawing
// ============================================================================

const char* agame_draw_point(int argc, const char* argv[]) {
    if (argc<2||!g_backDC) return "error";
    SetPixel(g_backDC, atoi(argv[0]), atoi(argv[1]), g_color);
    return "ok";
}

const char* agame_draw_line(int argc, const char* argv[]) {
    if (argc<4||!g_backDC) return "error";
    HPEN pen = CreatePen(PS_SOLID, 1, g_color);
    HPEN old = (HPEN)SelectObject(g_backDC, pen);
    MoveToEx(g_backDC, atoi(argv[0]), atoi(argv[1]), nullptr);
    LineTo(g_backDC, atoi(argv[2]), atoi(argv[3]));
    SelectObject(g_backDC, old);
    DeleteObject(pen);
    return "ok";
}

const char* agame_draw_rect(int argc, const char* argv[]) {
    if (argc<4||!g_backDC) return "error";
    HPEN pen = CreatePen(PS_SOLID, 1, g_color);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN old = (HPEN)SelectObject(g_backDC, pen);
    HBRUSH ob = (HBRUSH)SelectObject(g_backDC, nb);
    int x=atoi(argv[0]), y=atoi(argv[1]), w=atoi(argv[2]), h=atoi(argv[3]);
    Rectangle(g_backDC, x, y, x+w, y+h);
    SelectObject(g_backDC, old);
    SelectObject(g_backDC, ob);
    DeleteObject(pen);
    return "ok";
}

const char* agame_fill_rect(int argc, const char* argv[]) {
    if (argc<4||!g_backDC) return "error";
    int x=atoi(argv[0]), y=atoi(argv[1]), w=atoi(argv[2]), h=atoi(argv[3]);
    RECT r = {x, y, x+w, y+h};
    HBRUSH br = CreateSolidBrush(g_color);
    FillRect(g_backDC, &r, br);
    DeleteObject(br);
    return "ok";
}

// Bresenham circle (outline)
const char* agame_draw_circle(int argc, const char* argv[]) {
    if (argc<3||!g_backDC) return "error";
    int cx=atoi(argv[0]), cy=atoi(argv[1]), rad=atoi(argv[2]);
    int x=0, y=rad, d=3-2*rad;
    while (y >= x) {
        SetPixel(g_backDC, cx+x, cy+y, g_color);
        SetPixel(g_backDC, cx-x, cy+y, g_color);
        SetPixel(g_backDC, cx+x, cy-y, g_color);
        SetPixel(g_backDC, cx-x, cy-y, g_color);
        SetPixel(g_backDC, cx+y, cy+x, g_color);
        SetPixel(g_backDC, cx-y, cy+x, g_color);
        SetPixel(g_backDC, cx+y, cy-x, g_color);
        SetPixel(g_backDC, cx-y, cy-x, g_color);
        x++;
        if (d>0) { y--; d+=4*(x-y)+10; } else { d+=4*x+6; }
    }
    return "ok";
}

// Filled circle (scanline)
const char* agame_fill_circle(int argc, const char* argv[]) {
    if (argc<3||!g_backDC) return "error";
    int cx=atoi(argv[0]), cy=atoi(argv[1]), rad=atoi(argv[2]);
    HPEN pen = CreatePen(PS_SOLID, 1, g_color);
    HPEN old = (HPEN)SelectObject(g_backDC, pen);
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrt((double)(rad*rad - dy*dy));
        MoveToEx(g_backDC, cx-dx, cy+dy, nullptr);
        LineTo(g_backDC, cx+dx, cy+dy);
    }
    SelectObject(g_backDC, old);
    DeleteObject(pen);
    return "ok";
}

const char* agame_draw_triangle(int argc, const char* argv[]) {
    if (argc<6||!g_backDC) return "error";
    HPEN pen = CreatePen(PS_SOLID, 1, g_color);
    HPEN old = (HPEN)SelectObject(g_backDC, pen);
    int x1=atoi(argv[0]),y1=atoi(argv[1]),x2=atoi(argv[2]),y2=atoi(argv[3]),
        x3=atoi(argv[4]),y3=atoi(argv[5]);
    MoveToEx(g_backDC,x1,y1,nullptr); LineTo(g_backDC,x2,y2);
    LineTo(g_backDC,x3,y3); LineTo(g_backDC,x1,y1);
    SelectObject(g_backDC, old); DeleteObject(pen);
    return "ok";
}

const char* agame_fill_triangle(int argc, const char* argv[]) {
    if (argc<6||!g_backDC) return "error";
    int x1=atoi(argv[0]),y1=atoi(argv[1]),x2=atoi(argv[2]),y2=atoi(argv[3]),
        x3=atoi(argv[4]),y3=atoi(argv[5]);
    POINT pts[3] = {{x1,y1},{x2,y2},{x3,y3}};
    HBRUSH br = CreateSolidBrush(g_color);
    HPEN pen = CreatePen(PS_SOLID, 1, g_color);
    HPEN op = (HPEN)SelectObject(g_backDC, pen);
    HBRUSH ob = (HBRUSH)SelectObject(g_backDC, br);
    Polygon(g_backDC, pts, 3);
    SelectObject(g_backDC, op); SelectObject(g_backDC, ob);
    DeleteObject(pen); DeleteObject(br);
    return "ok";
}

// ============================================================================
// Events & Input
// ============================================================================

const char* agame_poll_event(int, const char**) {
    // Dispatch all pending messages
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) g_quit = true;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Check key transitions via GetAsyncKeyState
    static const int keys[] = {
        VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_SPACE,VK_RETURN,VK_ESCAPE,VK_BACK,VK_TAB,
        VK_LSHIFT,VK_RSHIFT,VK_LCONTROL,VK_RCONTROL,
        'A','B','C','D','E','F','G','H','I','J','K','L','M',
        'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
        '0','1','2','3','4','5','6','7','8','9'
    };
    static const int nk = sizeof(keys)/sizeof(keys[0]);

    for (int i = 0; i < nk; i++) {
        int vk = keys[i];
        short cur = GetAsyncKeyState(vk);
        short prev = g_prev_key[vk];
        bool was_down = (prev & 0x8000) != 0;
        bool is_down  = (cur  & 0x8000) != 0;
        if (is_down && !was_down) {
            Ev e; e.type=1; e.key=vk; e.mx=0; e.my=0; e.btn=0;
            g_evq.push_back(e);
        } else if (!is_down && was_down) {
            Ev e; e.type=2; e.key=vk; e.mx=0; e.my=0; e.btn=0;
            g_evq.push_back(e);
        }
        g_prev_key[vk] = cur;
    }

    // Return queued event
    if (!g_evq.empty()) {
        Ev e = g_evq.front(); g_evq.erase(g_evq.begin());
        static char buf[64];
        const char* kn = vk_name(e.key);
        if (e.type == 1) snprintf(buf, sizeof(buf), "keydown %s", kn ? kn : "?");
        else              snprintf(buf, sizeof(buf), "keyup %s",   kn ? kn : "?");
        return buf;
    }
    return "none";
}

const char* agame_key_down(int argc, const char* argv[]) {
    if (argc < 1) return "0";
    int vk = name_vk(argv[0]);
    if (!vk) return "0";
    return (GetAsyncKeyState(vk) & 0x8000) ? "1" : "0";
}

const char* agame_key_up(int argc, const char* argv[]) {
    return (strcmp(agame_key_down(argc, argv), "1") == 0) ? "0" : "1";
}

const char* agame_mouse_x(int, const char**) {
    if (!g_hwnd) return "0";
    POINT p; GetCursorPos(&p); ScreenToClient(g_hwnd, &p);
    return istr(p.x);
}

const char* agame_mouse_y(int, const char**) {
    if (!g_hwnd) return "0";
    POINT p; GetCursorPos(&p); ScreenToClient(g_hwnd, &p);
    return istr(p.y);
}

const char* agame_mouse_down(int argc, const char* argv[]) {
    if (argc < 1) return "0";
    int btn = atoi(argv[0]);
    if (btn == 1) return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? "1" : "0";
    if (btn == 2) return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? "1" : "0";
    if (btn == 3) return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? "1" : "0";
    return "0";
}

// ============================================================================
// Image (basic BMP support via Win32 API)
// ============================================================================

const char* agame_load_image(int argc, const char* argv[]) {
    if (argc < 2 || !g_backDC) return "error";
    const char* name = argv[0];
    const char* path = argv[1];
    // Try loading as BMP
    HBITMAP bmp = (HBITMAP)LoadImageA(nullptr, path, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!bmp) return "error: LoadImage failed (BMP only on Windows backend)";
    // Store handle (leak on reload, acceptable for game jam)
    static std::map<std::string, HBITMAP> g_bmps;
    if (g_bmps.count(name)) DeleteObject(g_bmps[name]);
    g_bmps[name] = bmp;
    return name;
}

const char* agame_draw_image(int argc, const char* argv[]) {
    if (argc < 3 || !g_backDC) return "error";
    static std::map<std::string, HBITMAP> g_bmps;
    auto it = g_bmps.find(argv[0]);
    if (it == g_bmps.end()) return "error: image not found";
    int x = atoi(argv[1]), y = atoi(argv[2]);
    HDC mdc = CreateCompatibleDC(g_backDC);
    HBITMAP ob = (HBITMAP)SelectObject(mdc, it->second);
    BITMAP bm; GetObject(it->second, sizeof(bm), &bm);
    int w = (argc>3) ? atoi(argv[3]) : bm.bmWidth;
    int h = (argc>4) ? atoi(argv[4]) : bm.bmHeight;
    BitBlt(g_backDC, x, y, w, h, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, ob); DeleteDC(mdc);
    return "ok";
}

const char* agame_free_image(int argc, const char* argv[]) {
    // Intentionally simple - no-op for now
    return "ok";
}

// ============================================================================
// Text (Win32 system fonts)
// ============================================================================

const char* agame_load_font(int argc, const char* argv[]) {
    if (argc < 3) return "error";
    const char* name = argv[0];
    // argv[1] = font name (or path, ignored for system fonts)
    int size = atoi(argv[2]);
    wchar_t wname[128] = {0};
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, wname, 127);
    HFONT f = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, wname);
    if (!f) return "error: CreateFont failed";
    static std::map<std::string, HFONT> g_fonts;
    if (g_fonts.count(name)) DeleteObject(g_fonts[name]);
    g_fonts[name] = f;
    return name;
}

const char* agame_draw_text(int argc, const char* argv[]) {
    if (argc < 4 || !g_backDC) return "error";
    static std::map<std::string, HFONT> g_fonts;
    auto it = g_fonts.find(argv[0]);
    if (it == g_fonts.end()) return "error: font not found";
    const char* text = argv[1];
    int x = atoi(argv[2]), y = atoi(argv[3]);
    HFONT of = (HFONT)SelectObject(g_backDC, it->second);
    SetTextColor(g_backDC, g_color);
    SetBkMode(g_backDC, TRANSPARENT);
    wchar_t wt[512] = {0};
    int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wt, 511);
    TextOutW(g_backDC, x, y, wt, len > 0 ? len - 1 : 0);
    SelectObject(g_backDC, of);
    return "ok";
}

const char* agame_free_font(int, const char**) { return "ok"; }

// ============================================================================
// Sound (Win32 Beep fallback)
// ============================================================================
const char* agame_load_sound(int, const char**)  { return "ok"; }
const char* agame_play_sound(int, const char**)  { Beep(440, 100); return "0"; }
const char* agame_free_sound(int, const char**)  { return "ok"; }

// ============================================================================
// DLL Exports
// ============================================================================
extern "C" DLL_EXPORT
const char** alb_get_functions() {
    static const char* funcs[] = {
        "agame-init", (const char*)agame_init,
        "agame-quit", (const char*)agame_quit,
        "agame-clear", (const char*)agame_clear,
        "agame-present", (const char*)agame_present,
        "agame-delay", (const char*)agame_delay,
        "agame-window-width", (const char*)agame_window_width,
        "agame-window-height", (const char*)agame_window_height,
        "agame-set-title", (const char*)agame_set_title,
        "agame-set-color", (const char*)agame_set_color,
        "agame-draw-point", (const char*)agame_draw_point,
        "agame-draw-line", (const char*)agame_draw_line,
        "agame-draw-rect", (const char*)agame_draw_rect,
        "agame-fill-rect", (const char*)agame_fill_rect,
        "agame-draw-circle", (const char*)agame_draw_circle,
        "agame-fill-circle", (const char*)agame_fill_circle,
        "agame-draw-triangle", (const char*)agame_draw_triangle,
        "agame-fill-triangle", (const char*)agame_fill_triangle,
        "agame-poll-event", (const char*)agame_poll_event,
        "_agame-key-down", (const char*)agame_key_down,
        "_agame-key-up", (const char*)agame_key_up,
        "agame-mouse-x", (const char*)agame_mouse_x,
        "agame-mouse-y", (const char*)agame_mouse_y,
        "agame-mouse-down", (const char*)agame_mouse_down,
        "agame-load-image", (const char*)agame_load_image,
        "agame-draw-image", (const char*)agame_draw_image,
        "agame-free-image", (const char*)agame_free_image,
        "agame-load-font", (const char*)agame_load_font,
        "agame-draw-text", (const char*)agame_draw_text,
        "agame-free-font", (const char*)agame_free_font,
        "agame-load-sound", (const char*)agame_load_sound,
        "agame-play-sound", (const char*)agame_play_sound,
        "agame-free-sound", (const char*)agame_free_sound,
        nullptr
    };
    return funcs;
}

#else
// ============================================================================
// Linux/macOS Backend - SDL2
// ============================================================================
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#define DLL_EXPORT __attribute__((visibility("default")))

static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Color g_current_color = {255, 255, 255, 255};
static bool g_should_quit = false;
static int g_window_width = 800;
static int g_window_height = 600;
static std::map<std::string, SDL_Texture*> g_images;
static std::map<std::string, TTF_Font*> g_fonts;
static std::map<std::string, Mix_Chunk*> g_sounds;
static const Uint8* g_key_state = nullptr;
static int g_mouse_x = 0, g_mouse_y = 0;
static Uint32 g_mouse_buttons = 0;

static SDL_Color parse_color(const char* str) {
    SDL_Color c = {0,0,0,255};
    if (!str) return c;
    while (*str && (*str==' '||*str=='(')) str++;
    int r=0,g=0,b=0,a=255;
    sscanf(str,"%d %d %d %d",&r,&g,&b,&a);
    c.r=(Uint8)(r<0?0:(r>255?255:r)); c.g=(Uint8)(g<0?0:(g>255?255:g));
    c.b=(Uint8)(b<0?0:(b>255?255:b)); c.a=(Uint8)(a<0?0:(a>255?255:a));
    return c;
}
static const char* int_str(int n){static char b[32];snprintf(b,sizeof(b),"%d",n);return b;}

const char* agame_init(int argc, const char* argv[]) {
    if(argc<3) return "error";
    int w=atoi(argv[1]),h=atoi(argv[2]);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0) return SDL_GetError();
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    TTF_Init();
    Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048);
    g_window=SDL_CreateWindow(argv[0],SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,SDL_WINDOW_SHOWN);
    if(!g_window) return SDL_GetError();
    g_renderer=SDL_CreateRenderer(g_window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!g_renderer) g_renderer=SDL_CreateRenderer(g_window,-1,SDL_RENDERER_SOFTWARE);
    if(!g_renderer) return SDL_GetError();
    g_window_width=w; g_window_height=h; g_should_quit=false;
    g_key_state=SDL_GetKeyboardState(nullptr);
    return "ok";
}
const char* agame_quit(int,const char**){
    for(auto&kv:g_images) SDL_DestroyTexture(kv.second); g_images.clear();
    for(auto&kv:g_fonts) TTF_CloseFont(kv.second); g_fonts.clear();
    for(auto&kv:g_sounds) Mix_FreeChunk(kv.second); g_sounds.clear();
    if(g_renderer){SDL_DestroyRenderer(g_renderer);g_renderer=nullptr;}
    if(g_window){SDL_DestroyWindow(g_window);g_window=nullptr;}
    Mix_CloseAudio(); TTF_Quit(); IMG_Quit(); SDL_Quit(); return "ok";
}
const char* agame_clear(int,const char**){
    if(!g_renderer) return "error";
    SDL_SetRenderDrawColor(g_renderer,0,0,0,255); SDL_RenderClear(g_renderer); return "ok";
}
const char* agame_present(int,const char**){
    if(!g_renderer) return "error"; SDL_RenderPresent(g_renderer); return "ok";
}
const char* agame_delay(int argc,const char*argv[]){
    if(argc<1)return "error"; SDL_Delay(atoi(argv[0])); return "ok";
}
const char* agame_window_width(int,const char**){return int_str(g_window_width);}
const char* agame_window_height(int,const char**){return int_str(g_window_height);}
const char* agame_set_title(int argc,const char*argv[]){
    if(argc<1||!g_window)return "error"; SDL_SetWindowTitle(g_window,argv[0]); return "ok";
}
const char* agame_set_color(int argc,const char*argv[]){
    if(argc<1)return "error"; g_current_color=parse_color(argv[0]); return "ok";
}
const char* agame_draw_point(int argc,const char*argv[]){
    if(argc<2||!g_renderer)return "error";
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    SDL_RenderDrawPoint(g_renderer,atoi(argv[0]),atoi(argv[1])); return "ok";
}
const char* agame_draw_line(int argc,const char*argv[]){
    if(argc<4||!g_renderer)return "error";
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    SDL_RenderDrawLine(g_renderer,atoi(argv[0]),atoi(argv[1]),atoi(argv[2]),atoi(argv[3])); return "ok";
}
const char* agame_draw_rect(int argc,const char*argv[]){
    if(argc<4||!g_renderer)return "error";
    SDL_Rect r={atoi(argv[0]),atoi(argv[1]),atoi(argv[2]),atoi(argv[3])};
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    SDL_RenderDrawRect(g_renderer,&r); return "ok";
}
const char* agame_fill_rect(int argc,const char*argv[]){
    if(argc<4||!g_renderer)return "error";
    SDL_Rect r={atoi(argv[0]),atoi(argv[1]),atoi(argv[2]),atoi(argv[3])};
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    SDL_RenderFillRect(g_renderer,&r); return "ok";
}
const char* agame_draw_circle(int argc,const char*argv[]){
    if(argc<3||!g_renderer)return "error";
    int cx=atoi(argv[0]),cy=atoi(argv[1]),rad=atoi(argv[2]);
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    int x=0,y=rad,d=3-2*rad;
    while(y>=x){
        SDL_RenderDrawPoint(g_renderer,cx+x,cy+y); SDL_RenderDrawPoint(g_renderer,cx-x,cy+y);
        SDL_RenderDrawPoint(g_renderer,cx+x,cy-y); SDL_RenderDrawPoint(g_renderer,cx-x,cy-y);
        SDL_RenderDrawPoint(g_renderer,cx+y,cy+x); SDL_RenderDrawPoint(g_renderer,cx-y,cy+x);
        SDL_RenderDrawPoint(g_renderer,cx+y,cy-x); SDL_RenderDrawPoint(g_renderer,cx-y,cy-x);
        x++; if(d>0){y--;d+=4*(x-y)+10;}else{d+=4*x+6;}
    } return "ok";
}
const char* agame_fill_circle(int argc,const char*argv[]){
    if(argc<3||!g_renderer)return "error";
    int cx=atoi(argv[0]),cy=atoi(argv[1]),rad=atoi(argv[2]);
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    for(int dy=-rad;dy<=rad;dy++){int dx=(int)sqrt((double)(rad*rad-dy*dy));
        SDL_RenderDrawLine(g_renderer,cx-dx,cy+dy,cx+dx,cy+dy);}
    return "ok";
}
const char* agame_draw_triangle(int argc,const char*argv[]){
    if(argc<6||!g_renderer)return "error";
    int x1=atoi(argv[0]),y1=atoi(argv[1]),x2=atoi(argv[2]),y2=atoi(argv[3]),x3=atoi(argv[4]),y3=atoi(argv[5]);
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    SDL_RenderDrawLine(g_renderer,x1,y1,x2,y2);SDL_RenderDrawLine(g_renderer,x2,y2,x3,y3);
    SDL_RenderDrawLine(g_renderer,x3,y3,x1,y1); return "ok";
}
const char* agame_fill_triangle(int argc,const char*argv[]){
    if(argc<6||!g_renderer)return "error";
    int x1=atoi(argv[0]),y1=atoi(argv[1]),x2=atoi(argv[2]),y2=atoi(argv[3]),x3=atoi(argv[4]),y3=atoi(argv[5]);
    SDL_SetRenderDrawColor(g_renderer,g_current_color.r,g_current_color.g,g_current_color.b,g_current_color.a);
    int min_y=y1,max_y=y1;
    if(y2<min_y)min_y=y2;if(y2>max_y)max_y=y2;if(y3<min_y)min_y=y3;if(y3>max_y)max_y=y3;
    for(int y=min_y;y<=max_y;y++){int min_x=g_window_width,max_x=0;
        auto ck=[&](int xa,int ya,int xb,int yb){if((ya<=y&&yb>y)||(yb<=y&&ya>y)){
            int x=xa+(y-ya)*(xb-xa)/(yb-ya);if(x<min_x)min_x=x;if(x>max_x)max_x=x;}};
        ck(x1,y1,x2,y2);ck(x2,y2,x3,y3);ck(x3,y3,x1,y1);
        if(min_x<=max_x)SDL_RenderDrawLine(g_renderer,min_x,y,max_x,y);}
    return "ok";
}
const char* agame_poll_event(int,const char**){
    SDL_Event e; static char r[256];
    while(SDL_PollEvent(&e)){switch(e.type){
        case SDL_QUIT: g_should_quit=true; return "quit";
        case SDL_KEYDOWN: snprintf(r,sizeof(r),"keydown %s",SDL_GetKeyName(e.key.keysym.sym)); return r;
        case SDL_KEYUP: snprintf(r,sizeof(r),"keyup %s",SDL_GetKeyName(e.key.keysym.sym)); return r;
        case SDL_MOUSEMOTION: g_mouse_x=e.motion.x;g_mouse_y=e.motion.y;
            snprintf(r,sizeof(r),"mousemove %d %d",g_mouse_x,g_mouse_y); return r;
        case SDL_MOUSEBUTTONDOWN: g_mouse_buttons=SDL_GetMouseState(&g_mouse_x,&g_mouse_y);
            snprintf(r,sizeof(r),"mousedown %d %d %d",e.button.button,g_mouse_x,g_mouse_y); return r;
        case SDL_MOUSEBUTTONUP: g_mouse_buttons=SDL_GetMouseState(&g_mouse_x,&g_mouse_y);
            snprintf(r,sizeof(r),"mouseup %d %d %d",e.button.button,g_mouse_x,g_mouse_y); return r;
    }} return "none";
}
const char* agame_key_down(int argc,const char*argv[]){
    if(argc<1||!g_key_state)return "0";
    SDL_Keycode k=SDLK_UNKNOWN; const char*n=argv[0];
    if(strcmp(n,"up")==0)k=SDLK_UP;else if(strcmp(n,"down")==0)k=SDLK_DOWN;
    else if(strcmp(n,"left")==0)k=SDLK_LEFT;else if(strcmp(n,"right")==0)k=SDLK_RIGHT;
    else if(strcmp(n,"space")==0)k=SDLK_SPACE;else if(strcmp(n,"return")==0)k=SDLK_RETURN;
    else if(strcmp(n,"escape")==0)k=SDLK_ESCAPE;else if(strcmp(n,"backspace")==0)k=SDLK_BACKSPACE;
    else if(strcmp(n,"tab")==0)k=SDLK_TAB;else if(strcmp(n,"lshift")==0)k=SDLK_LSHIFT;
    else if(strcmp(n,"rshift")==0)k=SDLK_RSHIFT;else if(strcmp(n,"lctrl")==0)k=SDLK_LCTRL;
    else if(strcmp(n,"rctrl")==0)k=SDLK_RCTRL;
    else if(strlen(n)==1&&n[0]>='a'&&n[0]<='z')k=SDLK_a+(n[0]-'a');
    else if(strlen(n)==1&&n[0]>='0'&&n[0]<='9')k=SDLK_0+(n[0]-'0');
    return (k!=SDLK_UNKNOWN&&g_key_state[SDL_GetScancodeFromKey(k)])?"1":"0";
}
const char* agame_key_up(int argc,const char*argv[]){
    return (strcmp(agame_key_down(argc,argv),"1")==0)?"0":"1";
}
const char* agame_mouse_x(int,const char**){SDL_GetMouseState(&g_mouse_x,&g_mouse_y);return int_str(g_mouse_x);}
const char* agame_mouse_y(int,const char**){SDL_GetMouseState(&g_mouse_x,&g_mouse_y);return int_str(g_mouse_y);}
const char* agame_mouse_down(int argc,const char*argv[]){
    if(argc<1)return "0"; int b=atoi(argv[0]);
    g_mouse_buttons=SDL_GetMouseState(&g_mouse_x,&g_mouse_y);
    if(b==1&&(g_mouse_buttons&SDL_BUTTON_LMASK))return "1";
    if(b==2&&(g_mouse_buttons&SDL_BUTTON_MMASK))return "1";
    if(b==3&&(g_mouse_buttons&SDL_BUTTON_RMASK))return "1"; return "0";
}
const char* agame_load_image(int argc,const char*argv[]){
    if(argc<2||!g_renderer)return "error";
    SDL_Surface*s=IMG_Load(argv[1]);if(!s)return IMG_GetError();
    SDL_Texture*t=SDL_CreateTextureFromSurface(g_renderer,s);SDL_FreeSurface(s);
    if(!t)return "error"; g_images[argv[0]]=t; return argv[0];
}
const char* agame_draw_image(int argc,const char*argv[]){
    if(argc<3||!g_renderer)return "error";
    auto it=g_images.find(argv[0]);if(it==g_images.end())return "error";
    int w=(argc>3)?atoi(argv[3]):-1,h=(argc>4)?atoi(argv[4]):-1;
    SDL_Rect d; if(w<0||h<0){int tw,th;SDL_QueryTexture(it->second,nullptr,nullptr,&tw,&th);d={atoi(argv[1]),atoi(argv[2]),tw,th};}
    else{d={atoi(argv[1]),atoi(argv[2]),w,h};}
    SDL_RenderCopy(g_renderer,it->second,nullptr,&d); return "ok";
}
const char* agame_free_image(int argc,const char*argv[]){
    if(argc<1)return "error"; auto it=g_images.find(argv[0]);
    if(it!=g_images.end()){SDL_DestroyTexture(it->second);g_images.erase(it);}return "ok";
}
const char* agame_load_font(int argc,const char*argv[]){
    if(argc<3)return "error"; TTF_Font*f=TTF_OpenFont(argv[1],atoi(argv[2]));
    if(!f)return TTF_GetError(); g_fonts[argv[0]]=f; return argv[0];
}
const char* agame_draw_text(int argc,const char*argv[]){
    if(argc<4||!g_renderer)return "error";
    auto it=g_fonts.find(argv[0]);if(it==g_fonts.end())return "error";
    SDL_Surface*s=TTF_RenderUTF8_Blended(it->second,argv[1],g_current_color);
    if(!s)return "error"; SDL_Texture*t=SDL_CreateTextureFromSurface(g_renderer,s);SDL_FreeSurface(s);
    if(!t)return "error"; int tw,th;SDL_QueryTexture(t,nullptr,nullptr,&tw,&th);
    SDL_Rect d={atoi(argv[2]),atoi(argv[3]),tw,th};SDL_RenderCopy(g_renderer,t,nullptr,&d);
    SDL_DestroyTexture(t); return "ok";
}
const char* agame_free_font(int argc,const char*argv[]){
    if(argc<1)return "error"; auto it=g_fonts.find(argv[0]);
    if(it!=g_fonts.end()){TTF_CloseFont(it->second);g_fonts.erase(it);}return "ok";
}
const char* agame_load_sound(int argc,const char*argv[]){
    if(argc<2)return "error"; Mix_Chunk*c=Mix_LoadWAV(argv[1]);
    if(!c)return Mix_GetError(); g_sounds[argv[0]]=c; return argv[0];
}
const char* agame_play_sound(int argc,const char*argv[]){
    if(argc<1)return "error"; auto it=g_sounds.find(argv[0]);
    if(it==g_sounds.end())return "error";
    return int_str(Mix_PlayChannel(-1,it->second,(argc>1)?atoi(argv[1]):0));
}
const char* agame_free_sound(int argc,const char*argv[]){
    if(argc<1)return "error"; auto it=g_sounds.find(argv[0]);
    if(it!=g_sounds.end()){Mix_FreeChunk(it->second);g_sounds.erase(it);}return "ok";
}

extern "C" DLL_EXPORT
const char** alb_get_functions() {
    static const char* funcs[] = {
        "agame-init",(const char*)agame_init,"agame-quit",(const char*)agame_quit,
        "agame-clear",(const char*)agame_clear,"agame-present",(const char*)agame_present,
        "agame-delay",(const char*)agame_delay,"agame-window-width",(const char*)agame_window_width,
        "agame-window-height",(const char*)agame_window_height,"agame-set-title",(const char*)agame_set_title,
        "agame-set-color",(const char*)agame_set_color,"agame-draw-point",(const char*)agame_draw_point,
        "agame-draw-line",(const char*)agame_draw_line,"agame-draw-rect",(const char*)agame_draw_rect,
        "agame-fill-rect",(const char*)agame_fill_rect,"agame-draw-circle",(const char*)agame_draw_circle,
        "agame-fill-circle",(const char*)agame_fill_circle,"agame-draw-triangle",(const char*)agame_draw_triangle,
        "agame-fill-triangle",(const char*)agame_fill_triangle,"agame-poll-event",(const char*)agame_poll_event,
        "_agame-key-down",(const char*)agame_key_down,"_agame-key-up",(const char*)agame_key_up,
        "agame-mouse-x",(const char*)agame_mouse_x,"agame-mouse-y",(const char*)agame_mouse_y,
        "agame-mouse-down",(const char*)agame_mouse_down,"agame-load-image",(const char*)agame_load_image,
        "agame-draw-image",(const char*)agame_draw_image,"agame-free-image",(const char*)agame_free_image,
        "agame-load-font",(const char*)agame_load_font,"agame-draw-text",(const char*)agame_draw_text,
        "agame-free-font",(const char*)agame_free_font,"agame-load-sound",(const char*)agame_load_sound,
        "agame-play-sound",(const char*)agame_play_sound,"agame-free-sound",(const char*)agame_free_sound,
        nullptr
    }; return funcs;
}
#endif
