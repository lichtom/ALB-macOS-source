/*
 * ALB GUI Framework - Event Loop & Callback Dispatch
 * WndProc, mainloop, event firing
 */
#include "gui_internal.h"
#include <commctrl.h>

// ========== Globals ==========
alb_callback_fn g_alb_callback = nullptr;
HWND g_main_hwnd = NULL;
HINSTANCE g_hInstance = NULL;

// ========== DLL Entry & Callback Setup ==========
extern "C" __declspec(dllexport)
void alb_set_callback(alb_callback_fn fn) {
    g_alb_callback = fn;
}

// DLL entry point - initialize common controls
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if(fdwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hinstDLL;
        INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&icc);
    }
    return TRUE;
}

// ========== Event Firing ==========
void fire_event(int widget_id, const char* event_type, const char* data) {
    Widget* w = get_widget(widget_id);
    if(!w || w->command.empty()) return;
    if(!g_alb_callback) return;
    
    // Call ALB: (command-name widget-id event-type data)
    char wid_str[32];
    snprintf(wid_str, sizeof(wid_str), "%d", widget_id);
    const char* args[] = { wid_str, event_type, data ? data : "" };
    g_alb_callback(w->command.c_str(), 3, args);
}

// ========== WndProc ==========
LRESULT CALLBACK AlbWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
    case WM_COMMAND: {
        HWND ctrl = (HWND)lParam;
        int notif = HIWORD(wParam);
        
        // Menu/toolbar commands have lParam==0
        if(lParam == 0) {
            int cmd_id = LOWORD(wParam);
            process_menu_command(cmd_id);
            break;
        }
        
        // Find widget by HWND
        Widget* w = get_widget_by_hwnd(ctrl);
        if(!w) break;
        
        switch(notif) {
        case BN_CLICKED:
            fire_event(w->id, "click");
            break;
        case EN_CHANGE:
            fire_event(w->id, "change");
            break;
        case CBN_SELCHANGE:
            fire_event(w->id, "select");
            break;
        }
        break;
    }
    case WM_NOTIFY: {
        NMHDR* nm = (NMHDR*)lParam;
        Widget* w = get_widget_by_hwnd(nm->hwndFrom);
        if(!w) break;
        
        if(w->type == "treeview") {
            if(nm->code == TVN_SELCHANGEDW || nm->code == TVN_SELCHANGEDA) {
                NMTREEVIEW* pntv = (NMTREEVIEW*)lParam;
                char buf[256] = "";
                TVITEMA item = {0};
                item.mask = TVIF_TEXT;
                item.hItem = pntv->itemNew.hItem;
                item.pszText = buf;
                item.cchTextMax = 256;
                TreeView_GetItem(nm->hwndFrom, &item);
                fire_event(w->id, "select", buf);
            }
        } else if(w->type == "notebook") {
            if(nm->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(nm->hwndFrom);
                char buf[32]; snprintf(buf, sizeof(buf), "%d", sel);
                fire_event(w->id, "tab-change", buf);
            }
        }
        break;
    }
    case WM_HSCROLL:
    case WM_VSCROLL: {
        HWND ctrl = (HWND)lParam;
        Widget* w = get_widget_by_hwnd(ctrl);
        if(!w) break;
        
        if(w->type == "scale") {
            int pos = (int)SendMessage(ctrl, TBM_GETPOS, 0, 0);
            char buf[32]; snprintf(buf, sizeof(buf), "%d", pos);
            fire_event(w->id, "change", buf);
        } else if(w->type == "scrollbar") {
            SCROLLINFO si = {sizeof(si), SIF_POS};
            GetScrollInfo(ctrl, SB_CTL, &si);
            char buf[32]; snprintf(buf, sizeof(buf), "%d", si.nPos);
            fire_event(w->id, "scroll", buf);
        }
        break;
    }
    case WM_SIZE: {
        if(hwnd == g_main_hwnd) {
            recalc_layout(hwnd);
        }
        break;
    }
    case WM_CLOSE: {
        // Fire close event on main window
        Widget* w = get_widget_by_hwnd(hwnd);
        if(w && !w->command.empty() && g_alb_callback) {
            char wid_str[32];
            snprintf(wid_str, sizeof(wid_str), "%d", w->id);
            const char* args[] = { wid_str, "close" };
            const char* result = g_alb_callback(w->command.c_str(), 2, args);
            if(result && !strcmp(result, "0")) {
                return 0;  // Cancel close
            }
        }
        if(hwnd == g_main_hwnd) {
            DestroyWindow(hwnd);
        } else {
            DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY: {
        if(hwnd == g_main_hwnd) {
            PostQuitMessage(0);
        }
        break;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ========== Mainloop ==========
const char* gui_mainloop_impl(int argc, const char* argv[]) {
    if(!g_main_hwnd) return "0";
    
    ShowWindow(g_main_hwnd, SW_SHOW);
    UpdateWindow(g_main_hwnd);
    
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    g_main_hwnd = NULL;
    return "1";
}

const char* gui_update_impl(int argc, const char* argv[]) {
    MSG msg;
    while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return "1";
}

// ========== Export (functions are registered in gui_widgets.cpp) ==========
