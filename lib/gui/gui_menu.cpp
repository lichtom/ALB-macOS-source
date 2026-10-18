/*
 * ALB GUI Framework - Menu, Toolbar, Statusbar
 */
#include "gui_internal.h"
#include <commctrl.h>

// ========== Menu ID tracking ==========
static int g_next_menu_id = 1000;
static map<int, string> g_menu_commands;  // menu_item_id -> ALB callback

// Helper: convert UTF-8 to wide
static wstring menu_to_wide(const char* s) {
    if(!s) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
    if(!ws.empty() && ws.back() == 0) ws.pop_back();
    return ws;
}

// Process menu command from WM_COMMAND
void process_menu_command(int cmd_id) {
    auto it = g_menu_commands.find(cmd_id);
    if(it != g_menu_commands.end() && g_alb_callback) {
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d", cmd_id);
        const char* args[] = { id_str, "menu-click" };
        g_alb_callback(it->second.c_str(), 2, args);
    }
}

// ========== Menubar ==========
// gui-menubar parent-id → menubar widget id
const char* gui_menubar_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    HMENU menubar = CreateMenu();
    if(!menubar) return "-1";
    
    SetMenu(parent_hwnd, menubar);
    DrawMenuBar(parent_hwnd);
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "menubar";
    w.hwnd = parent_hwnd;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    w.data["hmenu"] = to_string((long long)menubar);
    register_widget(id, w);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-menu-add menubar-id "File" "New|Open|-|Save|Exit" callback-name
const char* gui_menu_add(int argc, const char* argv[]) {
    int menubar_id = argc > 0 ? atoi(argv[0]) : -1;
    const char* label = argc > 1 ? argv[1] : "Menu";
    const char* items_str = argc > 2 ? argv[2] : "";
    const char* command = argc > 3 ? argv[3] : "";
    
    Widget* mb = get_widget(menubar_id);
    if(!mb) return "0";
    
    HMENU menubar = (HMENU)(long long)atoll(mb->data["hmenu"].c_str());
    HMENU submenu = CreatePopupMenu();
    
    // Parse items separated by |
    string items(items_str);
    size_t pos = 0;
    while(pos < items.size()) {
        size_t next = items.find('|', pos);
        string item = (next == string::npos) ? items.substr(pos) : items.substr(pos, next - pos);
        
        if(item == "-") {
            AppendMenuW(submenu, MF_SEPARATOR, 0, NULL);
        } else {
            int cmd_id = g_next_menu_id++;
            g_menu_commands[cmd_id] = command;
            AppendMenuW(submenu, MF_STRING, cmd_id, menu_to_wide(item.c_str()).c_str());
        }
        
        if(next == string::npos) break;
        pos = next + 1;
    }
    
    AppendMenuW(menubar, MF_POPUP, (UINT_PTR)submenu, menu_to_wide(label).c_str());
    DrawMenuBar(mb->parent_hwnd);
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "submenu";
    w.hwnd = mb->parent_hwnd;
    w.parent_hwnd = mb->parent_hwnd;
    w.parent_id = menubar_id;
    w.text = label;
    w.command = command;
    w.data["hmenu"] = to_string((long long)submenu);
    register_widget(id, w);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// ========== Toolbar ==========
// gui-toolbar parent-id → toolbar widget id
const char* gui_toolbar_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_TOP,
        0, 0, 0, 0, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!toolbar) return "-1";
    SendMessage(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "toolbar";
    w.hwnd = toolbar;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    w.data["btn_count"] = "0";
    register_widget(id, w);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-toolbar-add toolbar-id cmd-id "text" callback-name
const char* gui_toolbar_add_button(int argc, const char* argv[]) {
    int toolbar_id = argc > 0 ? atoi(argv[0]) : -1;
    int cmd_id = argc > 1 ? atoi(argv[1]) : 0;
    const char* text = argc > 2 ? argv[2] : "";
    const char* command = argc > 3 ? argv[3] : "";
    
    Widget* tb = get_widget(toolbar_id);
    if(!tb || !tb->hwnd) return "0";
    
    if(cmd_id == 0) cmd_id = g_next_menu_id++;
    g_menu_commands[cmd_id] = command;
    
    // Add string to toolbar's image list (using text as bitmap index for simplicity)
    int str_idx = (int)SendMessageW(tb->hwnd, TB_ADDSTRINGW, 0, (LPARAM)menu_to_wide(text).c_str());
    
    TBBUTTON btn = {0};
    btn.iBitmap = str_idx;
    btn.idCommand = cmd_id;
    btn.fsState = TBSTATE_ENABLED;
    btn.fsStyle = BTNS_BUTTON;
    btn.iString = str_idx;
    
    SendMessage(tb->hwnd, TB_INSERTBUTTON, (WPARAM)-1, (LPARAM)&btn);
    SendMessage(tb->hwnd, TB_AUTOSIZE, 0, 0);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", cmd_id);
    return str_dup(buf);
}

// ========== Statusbar ==========
// gui-statusbar parent-id num-parts
const char* gui_statusbar_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    int parts = argc > 1 ? atoi(argv[1]) : 1;
    
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND statusbar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!statusbar) return "-1";
    
    // Set up parts
    if(parts > 1) {
        RECT rc;
        GetClientRect(parent_hwnd, &rc);
        vector<int> edges(parts);
        int w = (rc.right - rc.left) / parts;
        for(int i = 0; i < parts; i++) {
            edges[i] = w * (i + 1);
        }
        edges[parts-1] = -1;  // Last part stretches
        SendMessage(statusbar, SB_SETPARTS, parts, (LPARAM)edges.data());
    }
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "statusbar";
    w.hwnd = statusbar;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    register_widget(id, w);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-statusbar-set statusbar-id part "text"
const char* gui_statusbar_set_text(int argc, const char* argv[]) {
    int sb_id = argc > 0 ? atoi(argv[0]) : -1;
    int part = argc > 1 ? atoi(argv[1]) : 0;
    const char* text = argc > 2 ? argv[2] : "";
    
    Widget* sb = get_widget(sb_id);
    if(!sb || !sb->hwnd) return "0";
    
    SendMessageW(sb->hwnd, SB_SETTEXTW, part, (LPARAM)menu_to_wide(text).c_str());
    return "1";
}
