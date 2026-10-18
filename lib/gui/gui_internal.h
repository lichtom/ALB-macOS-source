/*
 * ALB GUI Framework - Internal Shared Header
 * All gui/*.cpp files include this header
 */
#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

using namespace std;

// ========== Callback from DLL to ALB ==========
typedef const char* (*alb_callback_fn)(const char* func_name, int argc, const char* argv[]);
extern alb_callback_fn g_alb_callback;  // Defined in gui_event.cpp

// DLL must export this to receive callback
extern "C" __declspec(dllexport)
void alb_set_callback(alb_callback_fn fn);

// ========== Layout System ==========
enum LayoutMode { LAYOUT_NONE, LAYOUT_PACK, LAYOUT_GRID, LAYOUT_PLACE };
enum PackSide { SIDE_TOP, SIDE_BOTTOM, SIDE_LEFT, SIDE_RIGHT };
enum PackFill { FILL_NONE, FILL_X, FILL_Y, FILL_BOTH };

struct LayoutInfo {
    LayoutMode mode = LAYOUT_NONE;
    
    // Pack parameters
    PackSide side = SIDE_TOP;
    PackFill fill = FILL_NONE;
    int pad_x = 0, pad_y = 0;
    int ipadx = 0, ipady = 0;
    bool expand = false;
    
    // Grid parameters
    int grid_row = 0, grid_col = 0;
    int rowspan = 1, colspan = 1;
    string sticky;  // "nsew"
    
    // Place parameters
    int place_x = 0, place_y = 0;
    int place_w = -1, place_h = -1;  // -1 = auto
    bool place_relx = false, place_rely = false;
    bool place_relw = false, place_relh = false;
};

// ========== Widget Registry ==========
struct Widget {
    int id;
    string type;           // "window", "button", "label", etc.
    HWND hwnd;
    HWND parent_hwnd;      // Parent window handle
    int parent_id;         // Parent widget ID (0 = main window)
    string text;
    string command;        // ALB callback function name
    LayoutInfo layout;
    
    // Widget-specific data
    vector<int> children;  // Child widget IDs
    map<string, string> data;  // Extra data (e.g., combobox items)
    
    Widget() : id(0), hwnd(NULL), parent_hwnd(NULL), parent_id(0) {}
};

extern map<int, Widget> g_widgets;        // Defined in gui_widgets.cpp
extern int g_next_widget_id;              // Defined in gui_widgets.cpp
extern HWND g_main_hwnd;                  // Defined in gui_event.cpp
extern HINSTANCE g_hInstance;             // Defined in gui_event.cpp

// ========== Widget Registry Functions ==========
int alloc_widget_id();
Widget* get_widget(int id);
Widget* get_widget_by_hwnd(HWND hwnd);
void register_widget(int id, Widget w);

// ========== Layout Functions ==========
void apply_layout(int widget_id);
void recalc_layout(HWND parent_hwnd);
void pack_recalc(HWND parent_hwnd);
void grid_recalc(HWND parent_hwnd);

// ========== Event Functions (gui_event.cpp) ==========
LRESULT CALLBACK AlbWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void fire_event(int widget_id, const char* event_type, const char* data = "");

// ========== Menu Command Processing (gui_menu.cpp) ==========
void process_menu_command(int cmd_id);

// GUI mainloop and update (gui_event.cpp)
const char* gui_mainloop_impl(int argc, const char* argv[]);
const char* gui_update_impl(int argc, const char* argv[]);

// ========== Menu Functions (gui_menu.cpp) ==========
const char* gui_menubar_create(int argc, const char* argv[]);
const char* gui_menu_add(int argc, const char* argv[]);
const char* gui_toolbar_create(int argc, const char* argv[]);
const char* gui_toolbar_add_button(int argc, const char* argv[]);
const char* gui_statusbar_create(int argc, const char* argv[]);
const char* gui_statusbar_set_text(int argc, const char* argv[]);

// ========== Advanced Controls (gui_advanced.cpp) ==========
const char* gui_listbox_create(int argc, const char* argv[]);
const char* gui_listbox_add(int argc, const char* argv[]);
const char* gui_listbox_clear(int argc, const char* argv[]);
const char* gui_listbox_get_selection(int argc, const char* argv[]);
const char* gui_treeview_create(int argc, const char* argv[]);
const char* gui_tree_insert(int argc, const char* argv[]);
const char* gui_notebook_create(int argc, const char* argv[]);
const char* gui_tab_add(int argc, const char* argv[]);
const char* gui_tab_select(int argc, const char* argv[]);
const char* gui_progressbar_create(int argc, const char* argv[]);
const char* gui_progressbar_set(int argc, const char* argv[]);

// ========== Utility ==========
inline const char* str_dup(const string& s) {
    static string buf;
    buf = s;
    return buf.c_str();
}

#endif // GUI_INTERNAL_H
