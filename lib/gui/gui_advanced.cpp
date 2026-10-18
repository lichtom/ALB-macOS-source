/*
 * ALB GUI Framework - Advanced Controls
 * Listbox, TreeView, Notebook/Tabs, ProgressBar
 */
#include "gui_internal.h"
#include <commctrl.h>

// Helper
static wstring adv_to_wide(const char* s) {
    if(!s) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
    if(!ws.empty() && ws.back() == 0) ws.pop_back();
    return ws;
}

// ========== Listbox ==========
// gui-listbox parent-id callback-name → widget-id
const char* gui_listbox_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    const char* cmd = argc > 1 ? argv[1] : "";
    int w_px = argc > 2 ? atoi(argv[2]) : 200;
    int h_px = argc > 3 ? atoi(argv[3]) : 150;
    
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        0, 0, w_px, h_px, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!hwnd) return "-1";
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "listbox";
    w.hwnd = hwnd;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    w.command = cmd;
    register_widget(id, w);
    
    if(parent) parent->children.push_back(id);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-listbox-add listbox-id "item"
const char* gui_listbox_add(int argc, const char* argv[]) {
    int id = argc > 0 ? atoi(argv[0]) : -1;
    const char* item = argc > 1 ? argv[1] : "";
    Widget* w = get_widget(id);
    if(w && w->hwnd) {
        SendMessageW(w->hwnd, LB_ADDSTRING, 0, (LPARAM)adv_to_wide(item).c_str());
    }
    return "1";
}

// gui-listbox-clear listbox-id
const char* gui_listbox_clear(int argc, const char* argv[]) {
    int id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(id);
    if(w && w->hwnd) {
        SendMessage(w->hwnd, LB_RESETCONTENT, 0, 0);
    }
    return "1";
}

// gui-listbox-get listbox-id → selected text
const char* gui_listbox_get_selection(int argc, const char* argv[]) {
    int id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(id);
    if(!w || !w->hwnd) return "";
    
    LRESULT sel = SendMessage(w->hwnd, LB_GETCURSEL, 0, 0);
    if(sel == LB_ERR) return "";
    
    wchar_t buf[1024] = {0};
    SendMessageW(w->hwnd, LB_GETTEXT, sel, (LPARAM)buf);
    
    static string result;
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
    result.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &result[0], len, NULL, NULL);
    if(!result.empty() && result.back() == 0) result.pop_back();
    return str_dup(result);
}

// ========== TreeView ==========
// gui-treeview parent-id callback-name → widget-id
const char* gui_treeview_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    const char* cmd = argc > 1 ? argv[1] : "";
    int w_px = argc > 2 ? atoi(argv[2]) : 200;
    int h_px = argc > 3 ? atoi(argv[3]) : 300;
    
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
        0, 0, w_px, h_px, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!hwnd) return "-1";
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "treeview";
    w.hwnd = hwnd;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    w.command = cmd;
    register_widget(id, w);
    
    if(parent) parent->children.push_back(id);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-tree-insert tree-id "parent-key" "key" "text" → key
const char* gui_tree_insert(int argc, const char* argv[]) {
    int tree_id = argc > 0 ? atoi(argv[0]) : -1;
    const char* parent_key = argc > 1 ? argv[1] : "";
    const char* key = argc > 2 ? argv[2] : "";
    const char* text = argc > 3 ? argv[3] : "";
    
    Widget* w = get_widget(tree_id);
    if(!w || !w->hwnd) return "";
    
    HTREEITEM parent_item = TVI_ROOT;
    if(strlen(parent_key) > 0) {
        // Search for parent by key (stored in item's lParam or data)
        // Simple approach: use data map to track key->HTREEITEM
        string map_key = string("item_") + parent_key;
        auto it = w->data.find(map_key);
        if(it != w->data.end()) {
            parent_item = (HTREEITEM)(long long)atoll(it->second.c_str());
        }
    }
    
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = parent_item;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    wstring wtext = adv_to_wide(text);
    tvis.item.pszText = (LPWSTR)wtext.c_str();
    
    HTREEITEM hItem = TreeView_InsertItem(w->hwnd, &tvis);
    
    // Store key->HTREEITEM mapping
    if(strlen(key) > 0) {
        string map_key = string("item_") + key;
        w->data[map_key] = to_string((long long)hItem);
    }
    
    return str_dup(key);
}

// ========== Notebook (Tab Control) ==========
// gui-notebook parent-id callback-name → widget-id
const char* gui_notebook_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    const char* cmd = argc > 1 ? argv[1] : "";
    int w_px = argc > 2 ? atoi(argv[2]) : 400;
    int h_px = argc > 3 ? atoi(argv[3]) : 300;
    
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND hwnd = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | TCS_TABS,
        0, 0, w_px, h_px, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!hwnd) return "-1";
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "notebook";
    w.hwnd = hwnd;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    w.command = cmd;
    register_widget(id, w);
    
    if(parent) parent->children.push_back(id);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-tab-add notebook-id "title" → tab-index
const char* gui_tab_add(int argc, const char* argv[]) {
    int nb_id = argc > 0 ? atoi(argv[0]) : -1;
    const char* title = argc > 1 ? argv[1] : "Tab";
    
    Widget* nb = get_widget(nb_id);
    if(!nb || !nb->hwnd) return "-1";
    
    int count = TabCtrl_GetItemCount(nb->hwnd);
    
    TCITEMW tci = {0};
    tci.mask = TCIF_TEXT;
    wstring wtitle = adv_to_wide(title);
    tci.pszText = (LPWSTR)wtitle.c_str();
    
    TabCtrl_InsertItem(nb->hwnd, count, &tci);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", count);
    return str_dup(buf);
}

// gui-tab-select notebook-id index
const char* gui_tab_select(int argc, const char* argv[]) {
    int nb_id = argc > 0 ? atoi(argv[0]) : -1;
    int index = argc > 1 ? atoi(argv[1]) : 0;
    
    Widget* nb = get_widget(nb_id);
    if(nb && nb->hwnd) {
        TabCtrl_SetCurSel(nb->hwnd, index);
    }
    return "1";
}

// ========== ProgressBar ==========
// gui-progressbar parent-id → widget-id
const char* gui_progressbar_create(int argc, const char* argv[]) {
    int parent_id = argc > 0 ? atoi(argv[0]) : 0;
    int w_px = argc > 1 ? atoi(argv[1]) : 300;
    int h_px = argc > 2 ? atoi(argv[2]) : 25;
    
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    
    extern HINSTANCE g_hInstance;
    HWND hwnd = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, w_px, h_px, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!hwnd) return "-1";
    
    SendMessage(hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    
    int id = alloc_widget_id();
    Widget w;
    w.type = "progressbar";
    w.hwnd = hwnd;
    w.parent_hwnd = parent_hwnd;
    w.parent_id = parent_id;
    register_widget(id, w);
    
    if(parent) parent->children.push_back(id);
    
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// gui-progressbar-set progressbar-id value
const char* gui_progressbar_set(int argc, const char* argv[]) {
    int pb_id = argc > 0 ? atoi(argv[0]) : -1;
    int value = argc > 1 ? atoi(argv[1]) : 0;
    
    Widget* pb = get_widget(pb_id);
    if(pb && pb->hwnd) {
        SendMessage(pb->hwnd, PBM_SETPOS, value, 0);
    }
    return "1";
}
