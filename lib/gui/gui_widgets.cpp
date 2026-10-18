/*
 * ALB GUI Framework - Widget Core
 * Window creation, 13 basic controls, pack/grid/place layout
 */
#include "gui_internal.h"

// ========== Globals ==========
map<int, Widget> g_widgets;
int g_next_widget_id = 1;

// ========== Widget Registry ==========
int alloc_widget_id() { return g_next_widget_id++; }

Widget* get_widget(int id) {
    auto it = g_widgets.find(id);
    return (it != g_widgets.end()) ? &it->second : nullptr;
}

Widget* get_widget_by_hwnd(HWND hwnd) {
    for(auto& kv : g_widgets) {
        if(kv.second.hwnd == hwnd) return &kv.second;
    }
    return nullptr;
}

void register_widget(int id, Widget w) {
    w.id = id;
    g_widgets[id] = w;
}

// ========== UTF-8 Helper ==========
static wstring to_wide(const char* s) {
    if(!s) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &ws[0], len);
    if(!ws.empty() && ws.back() == 0) ws.pop_back();
    return ws;
}

// ========== Window ==========
static const char* gui_window(int argc, const char* argv[]) {
    const char* title = argc > 0 ? argv[0] : "ALB Window";
    int w = argc > 1 ? atoi(argv[1]) : 640;
    int h = argc > 2 ? atoi(argv[2]) : 480;
    
    extern HWND g_main_hwnd;
    extern HINSTANCE g_hInstance;
    extern LRESULT CALLBACK AlbWndProc(HWND, UINT, WPARAM, LPARAM);
    
    WNDCLASSEXW wc = {sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = AlbWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"ALBMainFrame";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
    
    wstring wtitle = to_wide(title);
    g_main_hwnd = CreateWindowExW(0, L"ALBMainFrame", wtitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        NULL, NULL, g_hInstance, NULL);
    
    if(!g_main_hwnd) return "-1";
    
    int id = alloc_widget_id();
    Widget win;
    win.type = "window";
    win.hwnd = g_main_hwnd;
    win.parent_hwnd = NULL;
    win.parent_id = 0;
    win.text = title;
    register_widget(id, win);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// ========== Helper: create child control ==========
static int create_control(const char* cls, DWORD style, DWORD exstyle,
    int parent_id, const char* text, int x, int y, int w, int h) {
    Widget* parent = get_widget(parent_id);
    HWND parent_hwnd = parent ? parent->hwnd : g_main_hwnd;
    if(!parent_hwnd) parent_hwnd = g_main_hwnd;
    
    wstring wcls = to_wide(cls);
    wstring wtxt = to_wide(text);
    extern HINSTANCE g_hInstance;
    
    HWND hwnd = CreateWindowExW(exstyle, wcls.c_str(), wtxt.c_str(),
        style | WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent_hwnd, NULL, g_hInstance, NULL);
    
    if(!hwnd) return -1;
    
    int id = alloc_widget_id();
    Widget cw;
    cw.hwnd = hwnd;
    cw.parent_hwnd = parent_hwnd;
    cw.parent_id = parent_id;
    cw.text = text ? text : "";
    register_widget(id, cw);
    
    // Add to parent's children
    if(parent) parent->children.push_back(id);
    
    return id;
}

// ========== Basic Controls ==========

static const char* gui_label(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "";
    int id = create_control("STATIC", SS_LEFT, 0, parent, text, 0, 0, 100, 20);
    Widget* w = get_widget(id);
    if(w) w->type = "label";
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_button(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "Button";
    const char* cmd  = argc > 2 ? argv[2] : "";
    int id = create_control("BUTTON", BS_PUSHBUTTON, 0, parent, text, 0, 0, 80, 30);
    Widget* w = get_widget(id);
    if(w) { w->type = "button"; w->command = cmd; }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_entry(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "";
    int width = argc > 2 ? atoi(argv[2]) : 200;
    int id = create_control("EDIT", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, parent, text, 0, 0, width, 25);
    Widget* w = get_widget(id);
    if(w) w->type = "entry";
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_text(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "";
    int w_px = argc > 2 ? atoi(argv[2]) : 400;
    int h_px = argc > 3 ? atoi(argv[3]) : 200;
    int id = create_control("EDIT", ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_HSCROLL,
        WS_EX_CLIENTEDGE, parent, text, 0, 0, w_px, h_px);
    Widget* w = get_widget(id);
    if(w) w->type = "text";
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_checkbutton(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "Check";
    const char* cmd  = argc > 2 ? argv[2] : "";
    int id = create_control("BUTTON", BS_AUTOCHECKBOX, 0, parent, text, 0, 0, 120, 25);
    Widget* w = get_widget(id);
    if(w) { w->type = "checkbutton"; w->command = cmd; }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_radiobutton(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "Radio";
    const char* cmd  = argc > 2 ? argv[2] : "";
    int id = create_control("BUTTON", BS_AUTORADIOBUTTON, 0, parent, text, 0, 0, 120, 25);
    Widget* w = get_widget(id);
    if(w) { w->type = "radiobutton"; w->command = cmd; }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_frame(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "";
    int w_px = argc > 2 ? atoi(argv[2]) : 200;
    int h_px = argc > 3 ? atoi(argv[3]) : 150;
    int id = create_control("STATIC", SS_ETCHEDFRAME, WS_EX_CLIENTEDGE, parent, text, 0, 0, w_px, h_px);
    Widget* w = get_widget(id);
    if(w) w->type = "frame";
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_canvas(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    int w_px = argc > 1 ? atoi(argv[1]) : 300;
    int h_px = argc > 2 ? atoi(argv[2]) : 200;
    int id = create_control("STATIC", SS_OWNERDRAW, WS_EX_CLIENTEDGE, parent, "", 0, 0, w_px, h_px);
    Widget* w = get_widget(id);
    if(w) w->type = "canvas";
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_scrollbar(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* cmd = argc > 1 ? argv[1] : "";
    bool horiz = argc > 2 && string(argv[2]) == "horizontal";
    DWORD style = horiz ? SBS_HORZ : SBS_VERT;
    int w_px = horiz ? 200 : 20;
    int h_px = horiz ? 20 : 200;
    int id = create_control("SCROLLBAR", style, 0, parent, "", 0, 0, w_px, h_px);
    Widget* w = get_widget(id);
    if(w) { w->type = "scrollbar"; w->command = cmd; }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_spinbox(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* text = argc > 1 ? argv[1] : "0";
    int width = argc > 2 ? atoi(argv[2]) : 100;
    // Create edit + updown
    int edit_id = create_control("EDIT", ES_AUTOHSCROLL|ES_NUMBER, WS_EX_CLIENTEDGE, parent, text, 0, 0, width-20, 25);
    Widget* ew = get_widget(edit_id);
    if(ew) ew->type = "spinbox";
    
    // Create UpDown control
    extern HINSTANCE g_hInstance;
    HWND parent_hwnd = ew ? ew->parent_hwnd : g_main_hwnd;
    HWND updown = CreateWindowExW(0, UPDOWN_CLASSW, L"",
        WS_CHILD|WS_VISIBLE|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|UDS_ARROWKEYS,
        0, 0, 0, 0, parent_hwnd, NULL, g_hInstance, NULL);
    if(updown && ew) {
        SendMessage(updown, UDM_SETBUDDY, (WPARAM)ew->hwnd, 0);
        SendMessage(updown, UDM_SETRANGE, 0, MAKELONG(100, 0));
    }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", edit_id);
    return str_dup(buf);
}

static const char* gui_scale(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    int min_val = argc > 1 ? atoi(argv[1]) : 0;
    int max_val = argc > 2 ? atoi(argv[2]) : 100;
    const char* cmd = argc > 3 ? argv[3] : "";
    int id = create_control("msctls_trackbar32", TBS_AUTOTICKS|TBS_BOTH, 0, parent, "", 0, 0, 200, 30);
    Widget* w = get_widget(id);
    if(w) {
        w->type = "scale"; w->command = cmd;
        SendMessage(w->hwnd, TBM_SETRANGE, TRUE, MAKELONG(min_val, max_val));
    }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

static const char* gui_combobox(int argc, const char* argv[]) {
    int parent = argc > 0 ? atoi(argv[0]) : 0;
    const char* cmd = argc > 1 ? argv[1] : "";
    int id = create_control("COMBOBOX", CBS_DROPDOWNLIST|WS_VSCROLL, 0, parent, "", 0, 0, 200, 200);
    Widget* w = get_widget(id);
    if(w) { w->type = "combobox"; w->command = cmd; }
    char buf[32]; snprintf(buf, sizeof(buf), "%d", id);
    return str_dup(buf);
}

// ========== Layout: Pack ==========
static const char* gui_pack(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(!w) return "0";
    
    w->layout.mode = LAYOUT_PACK;
    w->layout.side = SIDE_TOP;
    w->layout.fill = FILL_NONE;
    w->layout.pad_x = 2; w->layout.pad_y = 2;
    
    for(int i = 1; i < argc - 1; i += 2) {
        string key = argv[i];
        const char* val = argv[i+1];
        if(key == ":side" || key == "side") {
            if(!strcmp(val,"top")) w->layout.side = SIDE_TOP;
            else if(!strcmp(val,"bottom")) w->layout.side = SIDE_BOTTOM;
            else if(!strcmp(val,"left")) w->layout.side = SIDE_LEFT;
            else if(!strcmp(val,"right")) w->layout.side = SIDE_RIGHT;
        } else if(key == ":fill" || key == "fill") {
            if(!strcmp(val,"x")) w->layout.fill = FILL_X;
            else if(!strcmp(val,"y")) w->layout.fill = FILL_Y;
            else if(!strcmp(val,"both")) w->layout.fill = FILL_BOTH;
        } else if(key == ":pad" || key == "pad" || key == ":padx" || key == "padx") {
            w->layout.pad_x = atoi(val);
            if(key == ":pad" || key == "pad") w->layout.pad_y = atoi(val);
        } else if(key == ":pady" || key == "pady") {
            w->layout.pad_y = atoi(val);
        } else if(key == ":expand" || key == "expand") {
            w->layout.expand = (string(val) == "1" || string(val) == "yes" || string(val) == "true");
        }
    }
    apply_layout(widget_id);
    return "1";
}

// ========== Layout: Grid ==========
static const char* gui_grid(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(!w) return "0";
    
    w->layout.mode = LAYOUT_GRID;
    w->layout.grid_row = argc > 1 ? atoi(argv[1]) : 0;
    w->layout.grid_col = argc > 2 ? atoi(argv[2]) : 0;
    w->layout.pad_x = 2; w->layout.pad_y = 2;
    
    for(int i = 3; i < argc - 1; i += 2) {
        string key = argv[i];
        const char* val = argv[i+1];
        if(key == ":sticky" || key == "sticky") w->layout.sticky = val;
        else if(key == ":colspan" || key == "colspan") w->layout.colspan = atoi(val);
        else if(key == ":rowspan" || key == "rowspan") w->layout.rowspan = atoi(val);
        else if(key == ":pad" || key == "pad" || key == ":padx" || key == "padx") {
            w->layout.pad_x = atoi(val);
            if(key == ":pad" || key == "pad") w->layout.pad_y = atoi(val);
        } else if(key == ":pady" || key == "pady") w->layout.pad_y = atoi(val);
    }
    apply_layout(widget_id);
    return "1";
}

// ========== Layout: Place ==========
static const char* gui_place(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(!w) return "0";
    
    w->layout.mode = LAYOUT_PLACE;
    
    for(int i = 1; i < argc - 1; i += 2) {
        string key = argv[i];
        const char* val = argv[i+1];
        if(key == ":x" || key == "x") { w->layout.place_x = atoi(val); w->layout.place_relx = false; }
        else if(key == ":y" || key == "y") { w->layout.place_y = atoi(val); w->layout.place_rely = false; }
        else if(key == ":relx" || key == "relx") { w->layout.place_x = (int)(atof(val)*1000); w->layout.place_relx = true; }
        else if(key == ":rely" || key == "rely") { w->layout.place_y = (int)(atof(val)*1000); w->layout.place_rely = true; }
        else if(key == ":width" || key == "width") { w->layout.place_w = atoi(val); w->layout.place_relw = false; }
        else if(key == ":height" || key == "height") { w->layout.place_h = atoi(val); w->layout.place_relh = false; }
        else if(key == ":relwidth" || key == "relwidth") { w->layout.place_w = (int)(atof(val)*1000); w->layout.place_relw = true; }
        else if(key == ":relheight" || key == "relheight") { w->layout.place_h = (int)(atof(val)*1000); w->layout.place_relh = true; }
    }
    apply_layout(widget_id);
    return "1";
}

// ========== Layout: Apply ==========
void apply_layout(int widget_id) {
    Widget* w = get_widget(widget_id);
    if(!w || !w->hwnd) return;
    
    if(w->layout.mode == LAYOUT_PLACE) {
        RECT pr;
        GetClientRect(w->parent_hwnd ? w->parent_hwnd : g_main_hwnd, &pr);
        int pw = pr.right - pr.left;
        int ph = pr.bottom - pr.top;
        
        int x = w->layout.place_relx ? (pw * w->layout.place_x / 1000) : w->layout.place_x;
        int y = w->layout.place_rely ? (ph * w->layout.place_y / 1000) : w->layout.place_y;
        int ww = w->layout.place_w < 0 ? 100 : (w->layout.place_relw ? (pw * w->layout.place_w / 1000) : w->layout.place_w);
        int wh = w->layout.place_h < 0 ? 25 : (w->layout.place_relh ? (ph * w->layout.place_h / 1000) : w->layout.place_h);
        
        MoveWindow(w->hwnd, x, y, ww, wh, TRUE);
    } else if(w->layout.mode == LAYOUT_PACK || w->layout.mode == LAYOUT_GRID) {
        recalc_layout(w->parent_hwnd ? w->parent_hwnd : g_main_hwnd);
    }
}

// ========== Pack Layout Recalculation ==========
void pack_recalc(HWND parent_hwnd) {
    RECT pr;
    GetClientRect(parent_hwnd, &pr);
    int cx = 0, cy = 0, cw = pr.right - pr.left, ch = pr.bottom - pr.top;
    
    // Collect packed children of this parent
    vector<Widget*> packed;
    for(auto& kv : g_widgets) {
        Widget& w = kv.second;
        if(w.layout.mode == LAYOUT_PACK && 
           (w.parent_hwnd == parent_hwnd || (!w.parent_hwnd && parent_hwnd == g_main_hwnd)))
            packed.push_back(&w);
    }
    
    for(auto* w : packed) {
        if(!w->hwnd) continue;
        RECT wr;
        GetWindowRect(w->hwnd, &wr);
        int ww = wr.right - wr.left;
        int wh = wr.bottom - wr.top;
        
        int px = w->layout.pad_x, py = w->layout.pad_y;
        int x, y, rw, rh;
        
        switch(w->layout.side) {
        case SIDE_TOP:
            x = cx + px; y = cy + py;
            rw = (w->layout.fill == FILL_X || w->layout.fill == FILL_BOTH) ? cw - 2*px : ww;
            rh = wh;
            cy += py + wh + py;
            ch -= py + wh + py;
            break;
        case SIDE_BOTTOM:
            rh = wh;
            rw = (w->layout.fill == FILL_X || w->layout.fill == FILL_BOTH) ? cw - 2*px : ww;
            x = cx + px;
            y = cy + ch - py - wh;
            ch -= py + wh + py;
            break;
        case SIDE_LEFT:
            x = cx + px; y = cy + py;
            rw = ww;
            rh = (w->layout.fill == FILL_Y || w->layout.fill == FILL_BOTH) ? ch - 2*py : wh;
            cx += px + ww + px;
            cw -= px + ww + px;
            break;
        case SIDE_RIGHT:
            rw = ww;
            rh = (w->layout.fill == FILL_Y || w->layout.fill == FILL_BOTH) ? ch - 2*py : wh;
            x = cx + cw - px - ww;
            y = cy + py;
            cw -= px + ww + px;
            break;
        default:
            x = cx; y = cy; rw = ww; rh = wh;
        }
        
        MoveWindow(w->hwnd, x, y, max(rw,1), max(rh,1), TRUE);
    }
}

// ========== Grid Layout Recalculation ==========
void grid_recalc(HWND parent_hwnd) {
    RECT pr;
    GetClientRect(parent_hwnd, &pr);
    int pw = pr.right - pr.left;
    int ph = pr.bottom - pr.top;
    
    // Collect grid children
    vector<Widget*> gridded;
    int max_row = 0, max_col = 0;
    for(auto& kv : g_widgets) {
        Widget& w = kv.second;
        if(w.layout.mode == LAYOUT_GRID &&
           (w.parent_hwnd == parent_hwnd || (!w.parent_hwnd && parent_hwnd == g_main_hwnd))) {
            gridded.push_back(&w);
            if(w.layout.grid_row > max_row) max_row = w.layout.grid_row;
            if(w.layout.grid_col > max_col) max_col = w.layout.grid_col;
        }
    }
    
    if(gridded.empty()) return;
    
    // Calculate cell sizes (simple equal distribution)
    int cell_w = pw / (max_col + 1);
    int cell_h = ph / (max_row + 1);
    
    for(auto* w : gridded) {
        if(!w->hwnd) continue;
        int x = w->layout.grid_col * cell_w + w->layout.pad_x;
        int y = w->layout.grid_row * cell_h + w->layout.pad_y;
        int rw = cell_w * w->layout.colspan - 2 * w->layout.pad_x;
        int rh = cell_h * w->layout.rowspan - 2 * w->layout.pad_y;
        
        // Apply sticky
        if(w->layout.sticky.find('e') != string::npos ||
           w->layout.sticky.find('w') != string::npos) {
            // Already fills width
        }
        if(w->layout.sticky.find('n') != string::npos ||
           w->layout.sticky.find('s') != string::npos) {
            // Already fills height
        }
        
        MoveWindow(w->hwnd, x, y, max(rw,1), max(rh,1), TRUE);
    }
}

void recalc_layout(HWND parent_hwnd) {
    pack_recalc(parent_hwnd);
    grid_recalc(parent_hwnd);
}

// ========== Get/Set Properties ==========
static const char* gui_get(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    const char* prop = argc > 1 ? argv[1] : "text";
    Widget* w = get_widget(widget_id);
    if(!w || !w->hwnd) return "";
    
    if(!strcmp(prop, "text") || !strcmp(prop, ":text")) {
        wchar_t buf[4096];
        GetWindowTextW(w->hwnd, buf, 4096);
        static string result;
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
        result.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &result[0], len, NULL, NULL);
        if(!result.empty() && result.back() == 0) result.pop_back();
        return str_dup(result);
    } else if(!strcmp(prop, "command") || !strcmp(prop, ":command")) {
        return str_dup(w->command);
    } else if(!strcmp(prop, "width") || !strcmp(prop, ":width")) {
        RECT r; GetWindowRect(w->hwnd, &r);
        char buf[32]; snprintf(buf, sizeof(buf), "%d", r.right - r.left);
        return str_dup(buf);
    } else if(!strcmp(prop, "height") || !strcmp(prop, ":height")) {
        RECT r; GetWindowRect(w->hwnd, &r);
        char buf[32]; snprintf(buf, sizeof(buf), "%d", r.bottom - r.top);
        return str_dup(buf);
    } else if(!strcmp(prop, "state") || !strcmp(prop, ":state")) {
        return IsWindowEnabled(w->hwnd) ? "normal" : "disabled";
    } else if(!strcmp(prop, "value") || !strcmp(prop, ":value")) {
        if(w->type == "checkbutton" || w->type == "radiobutton") {
            LRESULT checked = SendMessage(w->hwnd, BM_GETCHECK, 0, 0);
            return checked == BST_CHECKED ? "1" : "0";
        } else if(w->type == "scale") {
            LRESULT pos = SendMessage(w->hwnd, TBM_GETPOS, 0, 0);
            char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)pos);
            return str_dup(buf);
        } else if(w->type == "combobox") {
            LRESULT idx = SendMessage(w->hwnd, CB_GETCURSEL, 0, 0);
            char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)idx);
            return str_dup(buf);
        }
    }
    return "";
}

static const char* gui_set(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    const char* prop = argc > 1 ? argv[1] : "text";
    const char* val  = argc > 2 ? argv[2] : "";
    Widget* w = get_widget(widget_id);
    if(!w || !w->hwnd) return "0";
    
    if(!strcmp(prop, "text") || !strcmp(prop, ":text")) {
        SetWindowTextW(w->hwnd, to_wide(val).c_str());
    } else if(!strcmp(prop, "command") || !strcmp(prop, ":command")) {
        w->command = val;
    } else if(!strcmp(prop, "state") || !strcmp(prop, ":state")) {
        EnableWindow(w->hwnd, strcmp(val, "disabled") != 0);
    } else if(!strcmp(prop, "value") || !strcmp(prop, ":value")) {
        if(w->type == "checkbutton" || w->type == "radiobutton") {
            SendMessage(w->hwnd, BM_SETCHECK, atoi(val) ? BST_CHECKED : BST_UNCHECKED, 0);
        } else if(w->type == "scale") {
            SendMessage(w->hwnd, TBM_SETPOS, TRUE, atoi(val));
        } else if(w->type == "combobox") {
            SendMessage(w->hwnd, CB_SETCURSEL, atoi(val), 0);
        }
    }
    return "1";
}

static const char* gui_config(int argc, const char* argv[]) {
    // gui-config widget-id :prop1 val1 :prop2 val2 ...
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    for(int i = 1; i < argc - 1; i += 2) {
        const char* a3[] = { argv[0], argv[i], argv[i+1] };
        gui_set(3, a3);
    }
    return "1";
}

static const char* gui_destroy(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(w && w->hwnd) {
        DestroyWindow(w->hwnd);
        w->hwnd = NULL;
        g_widgets.erase(widget_id);
    }
    return "1";
}

static const char* gui_show(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(w && w->hwnd) ShowWindow(w->hwnd, SW_SHOW);
    return "1";
}

static const char* gui_hide(int argc, const char* argv[]) {
    int widget_id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(widget_id);
    if(w && w->hwnd) ShowWindow(w->hwnd, SW_HIDE);
    return "1";
}

// ========== Dialog Functions (from old gui.cpp) ==========
static const char* gui_msgbox(int argc, const char* argv[]) {
    const char* msg   = argc > 0 ? argv[0] : "Hello";
    const char* title = argc > 1 ? argv[1] : "ALB";
    int buttons = 0;
    if(argc > 2) {
        if(strstr(argv[2], "yesno"))  buttons = MB_YESNO;
        if(strstr(argv[2], "okcancel")) buttons = MB_OKCANCEL;
        if(strstr(argv[2], "retry")) buttons = MB_RETRYCANCEL;
    }
    int r = MessageBoxA(NULL, msg, title, buttons | MB_ICONINFORMATION);
    switch(r) {
        case IDYES:    return "yes";
        case IDNO:     return "no";
        case IDOK:     return "ok";
        case IDCANCEL: return "cancel";
        case IDRETRY:  return "retry";
        default:       return "ok";
    }
}

static const char* gui_choose_file(int argc, const char* argv[]) {
    const char* filter_str = argc > 0 ? argv[0] : "All Files\0*.*\0";
    const char* title = argc > 1 ? argv[1] : "Choose File";
    char file[MAX_PATH] = "";
    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    static string result;
    if(GetOpenFileNameA(&ofn)) { result = file; return str_dup(result); }
    return "";
}

static const char* gui_open_url(int argc, const char* argv[]) {
    if(argc < 1) return "0";
    ShellExecuteA(NULL, "open", argv[0], NULL, NULL, SW_SHOWNORMAL);
    return "1";
}

static const char* gui_beep(int argc, const char* argv[]) {
    int freq = argc > 0 ? atoi(argv[0]) : 750;
    int dur  = argc > 1 ? atoi(argv[1]) : 300;
    Beep(freq, dur);
    return "1";
}

// ========== Combobox item management ==========
static const char* gui_combobox_add(int argc, const char* argv[]) {
    int id = argc > 0 ? atoi(argv[0]) : -1;
    const char* item = argc > 1 ? argv[1] : "";
    Widget* w = get_widget(id);
    if(w && w->hwnd && w->type == "combobox") {
        SendMessageW(w->hwnd, CB_ADDSTRING, 0, (LPARAM)to_wide(item).c_str());
    }
    return "1";
}

static const char* gui_combobox_clear(int argc, const char* argv[]) {
    int id = argc > 0 ? atoi(argv[0]) : -1;
    Widget* w = get_widget(id);
    if(w && w->hwnd && w->type == "combobox") {
        SendMessage(w->hwnd, CB_RESETCONTENT, 0, 0);
    }
    return "1";
}

// ========== Export Table ==========
// Forward declarations for functions in other .cpp files
extern const char* gui_mainloop_impl(int argc, const char* argv[]);
extern const char* gui_update_impl(int argc, const char* argv[]);

extern "C" __declspec(dllexport)
const char** alb_get_functions() {
    static const char* table[] = {
        // Window
        "gui-window",       (const char*)gui_window,
        // Basic controls
        "gui-label",        (const char*)gui_label,
        "gui-button",       (const char*)gui_button,
        "gui-entry",        (const char*)gui_entry,
        "gui-text",         (const char*)gui_text,
        "gui-checkbutton",  (const char*)gui_checkbutton,
        "gui-radiobutton",  (const char*)gui_radiobutton,
        "gui-frame",        (const char*)gui_frame,
        "gui-canvas",       (const char*)gui_canvas,
        "gui-scrollbar",    (const char*)gui_scrollbar,
        "gui-spinbox",      (const char*)gui_spinbox,
        "gui-scale",        (const char*)gui_scale,
        "gui-combobox",     (const char*)gui_combobox,
        // Layout
        "gui-pack",         (const char*)gui_pack,
        "gui-grid",         (const char*)gui_grid,
        "gui-place",        (const char*)gui_place,
        // Properties
        "gui-get",          (const char*)gui_get,
        "gui-set",          (const char*)gui_set,
        "gui-config",       (const char*)gui_config,
        // Lifecycle
        "gui-destroy",      (const char*)gui_destroy,
        "gui-show",         (const char*)gui_show,
        "gui-hide",         (const char*)gui_hide,
        // Dialogs
        "msgbox",           (const char*)gui_msgbox,
        "gui-choose-file",  (const char*)gui_choose_file,
        "gui-open-url",     (const char*)gui_open_url,
        "gui-beep",         (const char*)gui_beep,
        // Combobox management
        "gui-combobox-add", (const char*)gui_combobox_add,
        "gui-combobox-clear",(const char*)gui_combobox_clear,
        // Event loop (gui_event.cpp)
        "gui-mainloop",     (const char*)gui_mainloop_impl,
        "gui-update",       (const char*)gui_update_impl,
        // Menu (gui_menu.cpp)
        "gui-menubar",      (const char*)gui_menubar_create,
        "gui-menu-add",     (const char*)gui_menu_add,
        "gui-toolbar",      (const char*)gui_toolbar_create,
        "gui-toolbar-add",  (const char*)gui_toolbar_add_button,
        "gui-statusbar",    (const char*)gui_statusbar_create,
        "gui-statusbar-set",(const char*)gui_statusbar_set_text,
        // Advanced controls (gui_advanced.cpp)
        "gui-listbox",      (const char*)gui_listbox_create,
        "gui-listbox-add",  (const char*)gui_listbox_add,
        "gui-listbox-clear",(const char*)gui_listbox_clear,
        "gui-listbox-get",  (const char*)gui_listbox_get_selection,
        "gui-treeview",     (const char*)gui_treeview_create,
        "gui-tree-insert",  (const char*)gui_tree_insert,
        "gui-notebook",     (const char*)gui_notebook_create,
        "gui-tab-add",      (const char*)gui_tab_add,
        "gui-tab-select",   (const char*)gui_tab_select,
        "gui-progressbar",  (const char*)gui_progressbar_create,
        "gui-progressbar-set",(const char*)gui_progressbar_set,
        NULL, NULL
    };
    return table;
}
