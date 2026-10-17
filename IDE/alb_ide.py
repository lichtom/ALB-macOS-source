"""
ALB IDE - Python + Tkinter
Version: 0.0.6
A lightweight IDE for the ALB programming language.
"""
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
import subprocess
import os
import sys
import re
import json
import tempfile
import threading
import glob as glob_mod
from collections import OrderedDict

# Platform detection
IS_WINDOWS = sys.platform == 'win32'
IS_MACOS = sys.platform == 'darwin'
ALB_EXE_NAME = "ALB.exe" if IS_WINDOWS else "ALB"

VERSION = "0.1.1"
_RECENT_FILE = os.path.join(os.path.expanduser("~"), ".alb_ide_recent")

# Icon path: bundled with PyInstaller or relative to script
if getattr(sys, 'frozen', False):
    _BASE = getattr(sys, '_MEIPASS', os.path.dirname(sys.executable))
    _EXE_DIR = os.path.dirname(sys.executable)
else:
    _BASE = os.path.dirname(os.path.abspath(__file__))
    _EXE_DIR = _BASE

_icon_candidates = [
    os.path.join(_BASE, "ALB.ico"),
    os.path.join(_BASE, "ALB_IDE.ico"),
    os.path.join(_BASE, "ALB IDE", "ALB_IDE.ico"),
    os.path.join(_EXE_DIR, "ALB.ico"),
    os.path.join(_EXE_DIR, "ALB_IDE.ico"),
]
_ICON_PATH = ""
for _p in _icon_candidates:
    if os.path.isfile(_p):
        _ICON_PATH = _p
        break

# ========== ALB Function Database ==========
# (name, params, description, category)
ALB_FUNCTIONS = [
    # -- Core I/O --
    ("print",    "args...",    "Print values with newline",       "io"),
    ("prints",   "args...",    "Print values without newline",    "io"),
    ("input",    "[prompt]",   "Read a line from stdin",          "io"),
    # -- Type & Meta --
    ("type",     "x",          "Return type name of value",       "meta"),
    ("len",      "x",          "Length of string/list/map",       "meta"),
    ("doc",      "fn",         "Print function documentation",    "meta"),
    ("test",     "actual expected", "Assert actual == expected",  "meta"),
    ("assert",   "cond msg?",    "Assert condition, throw if false", "meta"),
    ("json-parse",    "str",     "Parse JSON string to ALB value",  "json"),
    ("json-serialize","val",     "Convert ALB value to JSON string", "json"),
    # -- Conversion --
    ("str",      "args...",    "Convert/concatenate to string",   "convert"),
    ("num",      "x",          "Convert to number",               "convert"),
    # -- Arithmetic --
    ("+",        "nums...",    "Add numbers (or concat strings)", "math"),
    ("-",        "nums...",    "Subtract (or negate if 1 arg)",   "math"),
    ("*",        "nums...",    "Multiply numbers",                "math"),
    ("/",        "a b",        "Divide a by b",                  "math"),
    ("%",        "a b",        "Modulo a % b",                    "math"),
    ("^",        "base exp",   "Power: base^exp",                 "math"),
    # -- Comparison --
    ("==",       "a b",        "Equal (string compare)",          "compare"),
    ("!=",       "a b",        "Not equal",                       "compare"),
    ("<",        "a b",        "Less than",                       "compare"),
    (">",        "a b",        "Greater than",                    "compare"),
    ("<=",       "a b",        "Less or equal",                   "compare"),
    (">=",       "a b",        "Greater or equal",                "compare"),
    # -- Logic --
    ("and",      "args...",    "Logical AND (all truthy)",        "logic"),
    ("or",       "args...",    "Logical OR (any truthy)",         "logic"),
    ("not",      "x",          "Logical NOT",                     "logic"),
    # -- Math functions --
    ("sqrt",     "x",          "Square root",                     "math"),
    ("abs",      "x",          "Absolute value",                  "math"),
    ("floor",    "x",          "Floor (round down)",              "math"),
    ("ceil",     "x",          "Ceiling (round up)",             "math"),
    ("round",    "x",          "Round to nearest integer",        "math"),
    ("sin",      "x",          "Sine (radians)",                  "math"),
    ("cos",      "x",          "Cosine (radians)",                "math"),
    ("tan",      "x",          "Tangent (radians)",               "math"),
    ("asin",     "x",          "Arc sine",                        "math"),
    ("acos",     "x",          "Arc cosine",                      "math"),
    ("atan",     "x",          "Arc tangent",                     "math"),
    ("atan2",    "y x",        "Two-arg arc tangent",             "math"),
    ("log",      "x",          "Natural logarithm (ln)",          "math"),
    ("log2",     "x",          "Log base 2",                      "math"),
    ("log10",    "x",          "Log base 10",                     "math"),
    ("exp",      "x",          "e^x (exponential)",               "math"),
    ("min",      "a b",        "Minimum of two values",           "math"),
    ("max",      "a b",        "Maximum of two values",           "math"),
    ("clamp",    "x lo hi",    "Clamp x to [lo, hi]",            "math"),
    ("random",   "[n]",        "Random float [0,1) or [0,n)",    "math"),
    ("random-int","a b",       "Random integer in [a, b]",       "math"),
    # -- List operations --
    ("list",     "items...",   "Create a list",                   "list"),
    ("car",      "lst",        "First element of list",           "list"),
    ("cdr",      "lst",        "List without first element",      "list"),
    ("cons",     "x lst",      "Prepend x to list",               "list"),
    ("nth",      "lst n",      "Get nth element (0-based)",       "list"),
    ("push",     "lst x",      "Append x to list",               "list"),
    ("range",    "end | start end [step]", "Generate number range","list"),
    # -- Higher-order --
    ("apply",    "fn args",    "Call fn with args list",          "higher"),
    ("map",      "fn lst",     "Apply fn to each element",        "higher"),
    ("filter",   "fn lst",     "Keep elements where fn is true",  "higher"),
    ("fold",     "fn lst acc", "Reduce list with accumulator",    "higher"),
    # -- String operations --
    ("str-split",   "s sep",   "Split string by separator",      "string"),
    ("str-join",    "lst sep", "Join list with separator",       "string"),
    ("str-replace", "s from to","Replace all occurrences",       "string"),
    ("str-find",    "s sub",   "Find substring position (-1=not found)","string"),
    ("str-trim",    "s",       "Trim leading/trailing whitespace","string"),
    ("str-upper",   "s",       "Convert to uppercase",           "string"),
    ("str-lower",   "s",       "Convert to lowercase",           "string"),
    ("str-starts?", "s prefix","Check if string starts with prefix","string"),
    ("str-ends?",   "s suffix","Check if string ends with suffix","string"),
    ("str-repeat",  "s n",     "Repeat string n times",          "string"),
    ("str-reverse", "s",       "Reverse string",                 "string"),
    ("char-code",   "c",       "Character to ASCII code",        "string"),
    ("code-char",   "n",       "ASCII code to character",        "string"),
    # -- File I/O --
    ("file-read",    "path",   "Read entire file as string",     "file"),
    ("file-write",   "path content","Write string to file",      "file"),
    ("file-append",  "path content","Append string to file",     "file"),
    ("file-lines",   "path",   "Read file as list of lines",     "file"),
    ("file-exists?", "path",   "Check if file exists (0/1)",    "file"),
    ("file-delete",  "path",   "Delete a file",                  "file"),
    # -- Directory --
    ("dir-list",     "[path]", "List directory contents",        "dir"),
    ("dir-exists?",  "path",   "Check if directory exists (0/1)","dir"),
    ("dir-create",   "path",   "Create a directory",             "dir"),
    # -- Path utilities --
    ("path-join",    "parts...", "Join path components",         "path"),
    ("path-ext",     "path",     "Get file extension",           "path"),
    ("path-name",    "path",     "Get filename without extension","path"),
    # -- Misc --
    ("time-now",     "",        "Current Unix timestamp (seconds)","time"),
    ("write",        "value",   "Serialize value to s-expr string","serial"),
    ("read",         "s",       "Deserialize s-expr string to value","serial"),
    ("error",        "type msg [data]","Create error map",       "error"),
    ("throw",        "msg|map", "Throw an error",                "error"),
    # -- Interactive (press) --
    ("press",        "[keyname]","Wait for key press; returns key name or 1/0","io"),
    # -- Vector --
    ("vector",       "items...", "Create vector (O(1) random access)", "vector"),
    ("vector-ref",   "v i",      "Get vector element at index",       "vector"),
    ("vector-set!",  "v i val",  "Set vector element at index",       "vector"),
    ("vector-push!", "v val",    "Append to vector",                  "vector"),
    ("vector-pop!",  "v",        "Remove and return last element",    "vector"),
    ("vector-size",  "v",        "Vector size",                       "vector"),
    ("vector->list", "v",        "Convert vector to list",             "vector"),
    ("list->vector", "lst",      "Convert list to vector",             "vector"),
    # -- Map operations --
    ("dict",         "k1 v1 ...", "Create map from key-value pairs",   "map"),
    ("map-get",      "m key",    "Get value from map",                 "map"),
    ("map-set",      "m key val","Set value in map (returns new map)", "map"),
    ("map-del",      "m key",    "Delete key from map",                "map"),
    ("map-has?",     "m key",    "Check if key exists in map",         "map"),
    ("map-keys",     "m",        "List of all keys",                   "map"),
    ("map-values",   "m",        "List of all values",                 "map"),
    ("map-merge",    "m1 m2",    "Merge two maps",                     "map"),
    # -- Struct --
    ("struct-get",   "obj field", "Get struct field by name",          "struct"),
    ("struct-set!",  "obj field val","Set struct field by name",       "struct"),
    # -- Regex --
    ("re-match",     "pattern str","Return regex match list or nil",   "regex"),
    ("re-find",      "pattern str","Return first regex match",         "regex"),
    ("re-replace",   "pattern str repl","Replace all regex matches",   "regex"),
    ("re-split",     "pattern str","Split string by regex",            "regex"),
    ("re-test?",     "pattern str","Test if regex matches (0/1)",      "regex"),
    # -- Higher-order (new) --
    ("sort",         "lst [fn]", "Sort list (optional comparison fn)", "higher"),
    ("reverse",      "lst",      "Reverse list or string",             "higher"),
    ("zip",          "lst1 lst2","Zip two lists into pairs",           "higher"),
    ("unzip",        "lst",      "Unzip pairs into two lists",         "higher"),
    ("any",          "fn lst",   "True if any element satisfies fn",   "higher"),
    ("all",          "fn lst",   "True if all elements satisfy fn",    "higher"),
    ("compose",      "f g",      "Function composition (f . g)",       "higher"),
    ("pipe",         "f g",      "Function pipe (f then g)",           "higher"),
    ("partial",      "fn args...","Partial application",               "higher"),
    ("for",          "lst fn",   "Execute fn for each (side effects)",  "higher"),
    ("each",         "lst fn",   "Alias for for",                      "higher"),
    ("flatten",      "lst",      "Flatten nested lists",               "higher"),
    ("uniq",         "lst",      "Remove duplicates",                  "higher"),
    ("take",         "n lst",    "Take first n elements",              "higher"),
    ("drop",         "n lst",    "Drop first n elements",              "higher"),
    ("copy",         "val",      "Deep copy value",                    "higher"),
    # -- Bit operations --
    ("bit-and",      "a b",      "Bitwise AND",                        "bit"),
    ("bit-or",       "a b",      "Bitwise OR",                         "bit"),
    ("bit-xor",      "a b",      "Bitwise XOR",                        "bit"),
    ("bit-not",      "a",        "Bitwise NOT",                        "bit"),
    ("bit-lshift",   "a n",      "Left shift",                         "bit"),
    ("bit-rshift",   "a n",      "Right shift",                        "bit"),
    # -- Time (new) --
    ("time-format",  "ts [fmt]", "Format timestamp to string",         "time"),
    ("time-parse",   "str [fmt]","Parse time string to timestamp",     "time"),
    ("time-diff",    "a b",      "Difference in seconds",              "time"),
    ("time-sleep",   "ms",       "Sleep for milliseconds",             "time"),
    ("time-now-ms",  "",         "Current time in milliseconds",       "time"),
    ("time-local",   "",         "Local time as map",                  "time"),
    # -- System --
    ("exit",         "[code]",   "Exit program with code",             "system"),
    ("sleep",        "ms",       "Sleep for milliseconds",             "system"),
    ("system",       "cmd",      "Run system command, return exit code","system"),
    ("exec",         "cmd",      "Run system command, return output",  "system"),
    # -- Network --
    ("net-connect",  "host port","TCP connect, return socket handle",  "network"),
    ("net-send",     "sock msg", "Send data on socket",                "network"),
    ("net-recv",     "sock [bufsize]","Receive data from socket",      "network"),
    ("net-close",    "sock",     "Close socket",                       "network"),
    ("net-listen",   "port",     "TCP listen on port",                 "network"),
    ("net-accept",   "sock",     "Accept TCP connection",              "network"),
    ("http-get",     "url",      "HTTP GET request, return body",      "network"),
    ("http-post",    "url data", "HTTP POST request, return body",     "network"),
]

# Build lookup dicts
_FUNC_DB = {name: (params, desc, cat) for name, params, desc, cat in ALB_FUNCTIONS}
_KEYWORDS = [
    "let", "set", "func", "const", "if", "when", "while", "for",
    "do", "match", "try", "catch", "throw", "use", "quote",
    "def", "break", "defmacro", "export", "struct",
    "define", "let*", "letrec", "cond", "for-i", "else",
    "quasiquote", "unquote", "unquote-splicing",
]
_CONSTANTS = ["nil", "true", "false", "real", "unrl", "rl", "ul",
              "pi", "e", "PI", "E", "INF"]

# All completable names
_COMPLETIONS = {}
for name, params, desc, cat in ALB_FUNCTIONS:
    _COMPLETIONS[name] = ("fn", params, desc, cat)
for kw in _KEYWORDS:
    _COMPLETIONS[kw] = ("kw", "", f"Keyword: {kw}", "keyword")
for c in _CONSTANTS:
    _COMPLETIONS[c] = ("const", "", f"Constant: {c}", "constant")


# ========== Dynamic Library Doc Parser ==========

# Regex patterns for parsing ALB source files
_RE_FUNC_LET = re.compile(
    r'\(let\s+([\w\-?!+*/=<>]+)\s+\(func\s+\(list\s*([\w\s\-]*)\)\s*"([^"]*)"')
_RE_CONST = re.compile(
    r'\(const\s+([\w\-]+)\s+(.+)\)\s*(?:;\s*(.*))?$',
    re.MULTILINE)
_RE_DOC_FUNC = re.compile(
    r';\s*@func\s+([\w\-?!+*/=<>]+)\s+([\w\s\[\]\-]*)\s*"([^"]*)"')
_RE_CPP_EXPORT = re.compile(
    r'"([\w\-?!]+)"\s*,\s*\(const char\*\)')
_RE_USE = re.compile(r'\(use\s+"([^"]+)"')


def _find_lib_root(lib_name):
    """Find library root directory or file, mirroring interpreter resolve_module logic.
    Returns ("folder", path) or ("file", path) or None."""
    # Build list of lib directories to search
    lib_dirs = []
    seen = set()
    for base in [_BASE, os.path.dirname(_BASE), _EXE_DIR, os.getcwd()]:
        candidate = os.path.normpath(os.path.join(base, "lib"))
        if candidate not in seen:
            seen.add(candidate)
            lib_dirs.append(candidate)

    for lib_dir in lib_dirs:
        folder = os.path.join(lib_dir, lib_name)
        if os.path.isdir(folder):
            # Check for __init__.alb, cli.alb, or any .alb/*.cpp
            try:
                entries = os.listdir(folder)
            except OSError:
                continue
            has_init = os.path.isfile(os.path.join(folder, "__init__.alb"))
            has_cli = os.path.isfile(os.path.join(folder, "cli.alb"))
            has_alb = any(f.endswith(".alb") for f in entries)
            has_cpp = any(f.endswith(".cpp") for f in entries)
            if has_init or has_cli or has_alb or has_cpp:
                return ("folder", folder)
        single = os.path.join(lib_dir, lib_name + ".alb")
        if os.path.isfile(single):
            return ("file", single)
    return None


def _parse_alb_file(filepath):
    """Parse an ALB file and extract function definitions and constants.
    Returns (funcs, consts) where:
      funcs = [(name, params_str, desc_str), ...]
      consts = [(name, desc_str), ...]
    """
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            content = f.read()
    except Exception:
        return [], []

    funcs = []
    consts = []

    # Parse @func annotations in comments first
    for m in _RE_DOC_FUNC.finditer(content):
        name = m.group(1)
        params = m.group(2).strip()
        desc = m.group(3)
        funcs.append((name, params, desc))

    # Parse (let name (func (list params) "docstring" ...))
    for m in _RE_FUNC_LET.finditer(content):
        name = m.group(1)
        params = m.group(2).strip()
        desc = m.group(3)
        # Skip if already added via @func
        if not any(f[0] == name for f in funcs):
            funcs.append((name, params, desc))

    # Parse (const NAME value) ; comment
    for m in _RE_CONST.finditer(content):
        name = m.group(1)
        value = m.group(2).strip()
        comment = (m.group(3) or "").strip()
        desc = comment if comment else f"= {value}"
        consts.append((name, desc))

    return funcs, consts


def _parse_cpp_exports(filepath):
    """Parse a C++ file and extract function names from alb_get_functions export table.
    Returns list of function names."""
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
    except Exception:
        return []
    return _RE_CPP_EXPORT.findall(content)


class TabInfo:
    """Represents a single editor tab."""
    __slots__ = ('editor', 'line_numbers', 'frame', 'file_path', 'title', 'dirty',
                 'loaded_libs', 'hl_timer', 'use_scan_timer', 'saved_content')

    def __init__(self):
        self.editor = None
        self.line_numbers = None
        self.frame = None
        self.file_path = None
        self.title = "Untitled"
        self.dirty = False
        self.loaded_libs = set()
        self.hl_timer = None
        self.use_scan_timer = None
        self.saved_content = ""  # content at last save, for dirty detection


class ALBIDE:
    def __init__(self, root):
        self.root = root
        self.root.title(f"ALB IDE v{VERSION}")
        self.root.geometry("900x650")
        self.root.minsize(600, 400)

        self.alb_exe = self._find_alb_exe()

        # Tab management
        self._tabs = OrderedDict()  # tab_id -> TabInfo
        self._tab_counter = 0
        self._current_tab_id = None
        self.notebook = None

        # Project management
        self._project_path = None   # .albprj file path
        self._project_dir = None    # project root directory

        # Find/Replace dialog
        self._find_dlg = None
        # Cursor position label ref
        self._cursor_label = None
        # References popup
        self._refs_popup = None
        # Recent files
        self._recent_files = self._load_recent_files()
        # Interactive process state
        self._process = None
        self._tmp_code_file = None
        self._press_tmp = None
        self._key_capture_active = False

        # Set window icon
        try:
            if _ICON_PATH:
                self.root.iconbitmap(default=_ICON_PATH)
        except Exception:
            pass

        self._setup_menu()
        self._setup_toolbar()
        self._setup_layout()
        self._setup_output()
        self._setup_statusbar()

        # Key bindings
        self.root.bind("<Control-s>", lambda e: self.save_file())
        self.root.bind("<Control-o>", lambda e: self._open_file_tab())
        self.root.bind("<Control-n>", lambda e: self._new_file_tab())
        self.root.bind("<Control-N>", lambda e: self._new_project())
        self.root.bind("<Control-w>", lambda e: self._close_current_tab())
        self.root.bind("<F5>", lambda e: self.run_code())
        self.root.bind("<Control-Shift-r>", lambda e: self.run_code())
        self.root.bind("<F7>", lambda e: self.build_code("normal"))
        self.root.bind("<F8>", lambda e: self.build_code("embed"))
        self.root.bind("<F9>", lambda e: self.compile_bytecode())
        self.root.bind("<F10>", lambda e: self.package_app())

        # Close window handler
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        # Auto-complete popup (hidden initially)
        self._ac_win = None
        self._ac_items = []
        self._ac_sel = -1
        self._ac_prefix = ""
        self._ac_start_idx = ""

        # Tooltip popup
        self._tooltip = None
        self._tooltip_timer = None

        self._update_status("Ready")
        self._update_cursor_pos()

        # Create initial empty tab
        self._new_file_tab()

    # ---- Properties for backward compatibility ----
    @property
    def editor(self):
        """Current tab's editor widget."""
        tab = self._get_current_tab()
        return tab.editor if tab else None

    @property
    def line_numbers(self):
        """Current tab's line numbers widget."""
        tab = self._get_current_tab()
        return tab.line_numbers if tab else None

    @property
    def current_file(self):
        """Current tab's file path."""
        tab = self._get_current_tab()
        return tab.file_path if tab else None

    @current_file.setter
    def current_file(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.file_path = value

    @property
    def _dirty(self):
        """Current tab's dirty state."""
        tab = self._get_current_tab()
        return tab.dirty if tab else False

    @_dirty.setter
    def _dirty(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.dirty = value

    @property
    def _loaded_libs(self):
        """Current tab's loaded libraries set."""
        tab = self._get_current_tab()
        return tab.loaded_libs if tab else set()

    @_loaded_libs.setter
    def _loaded_libs(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.loaded_libs = value

    @property
    def _hl_timer(self):
        tab = self._get_current_tab()
        return tab.hl_timer if tab else None

    @_hl_timer.setter
    def _hl_timer(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.hl_timer = value

    @property
    def _use_scan_timer(self):
        tab = self._get_current_tab()
        return tab.use_scan_timer if tab else None

    @_use_scan_timer.setter
    def _use_scan_timer(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.use_scan_timer = value

    @property
    def _saved_content(self):
        tab = self._get_current_tab()
        return tab.saved_content if tab else ""

    @_saved_content.setter
    def _saved_content(self, value):
        tab = self._get_current_tab()
        if tab:
            tab.saved_content = value

    def _find_alb_exe(self):
        """Find ALB executable in common locations."""
        exe_dir = os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else _BASE
        candidates = [
            os.path.join(exe_dir, ALB_EXE_NAME),
            os.path.join(_BASE, ALB_EXE_NAME),
            os.path.join(_BASE, "..", ALB_EXE_NAME),
            ALB_EXE_NAME,
        ]
        # macOS .app bundle: ALB_IDE.app and ALB.app are siblings
        # exe_dir = ALB_IDE.app/Contents/MacOS, go up to parent dir
        if getattr(sys, 'frozen', False):
            # exe_dir/../../.. = directory containing the .app bundle
            app_parent = os.path.normpath(os.path.join(exe_dir, "..", "..", ".."))
            candidates.insert(0, os.path.join(app_parent, "ALB.app", "Contents", "Resources", "ALB"))
            candidates.insert(1, os.path.join(app_parent, "ALB.app", "Contents", "MacOS", "ALB"))
            # Also check ALB binary directly next to ALB_IDE.app
            candidates.insert(2, os.path.join(app_parent, ALB_EXE_NAME))
        # On Linux/macOS, also check PATH
        if not IS_WINDOWS:
            path_dirs = os.environ.get('PATH', '').split(':')
            for d in path_dirs:
                candidates.append(os.path.join(d, "ALB"))
        for c in candidates:
            if os.path.isfile(c):
                return os.path.abspath(c)
        return None

    # ---- Menu ----
    def _setup_menu(self):
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)

        file_menu = tk.Menu(menubar, tearoff=0)

        # New submenu
        new_menu = tk.Menu(file_menu, tearoff=0)
        new_menu.add_command(label="File", command=self._new_file_tab, accelerator="Ctrl+N")
        new_menu.add_command(label="Project", command=self._new_project, accelerator="Ctrl+Shift+N")
        file_menu.add_cascade(label="New", menu=new_menu)

        # Open submenu
        open_menu = tk.Menu(file_menu, tearoff=0)
        open_menu.add_command(label="File", command=self._open_file_tab, accelerator="Ctrl+O")
        open_menu.add_command(label="Project", command=self._open_project)
        file_menu.add_cascade(label="Open", menu=open_menu)

        file_menu.add_command(label="Save", command=self.save_file, accelerator="Ctrl+S")
        file_menu.add_command(label="Save As...", command=self.save_file_as)
        file_menu.add_separator()
        file_menu.add_command(label="Close Tab", command=self._close_current_tab, accelerator="Ctrl+W")
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self._on_close)
        menubar.add_cascade(label="File", menu=file_menu)

        edit_menu = tk.Menu(menubar, tearoff=0)
        edit_menu.add_command(label="Find...", command=lambda: self._find_replace(False), accelerator="Ctrl+F")
        edit_menu.add_command(label="Replace...", command=lambda: self._find_replace(True), accelerator="Ctrl+H")
        edit_menu.add_separator()
        edit_menu.add_command(label="Go to Line...", command=self._go_to_line, accelerator="Ctrl+G")
        menubar.add_cascade(label="Edit", menu=edit_menu)

        self._recent_menu = tk.Menu(menubar, tearoff=0)
        self._rebuild_recent_menu()
        menubar.add_cascade(label="Recent Files", menu=self._recent_menu)

        run_menu = tk.Menu(menubar, tearoff=0)
        run_menu.add_command(label="Run (F5)", command=self.run_code, accelerator="F5")
        run_menu.add_command(label="Run in REPL", command=self.run_repl)
        run_menu.add_separator()
        run_menu.add_command(label="Build Executable (F7)", command=lambda: self.build_code("normal"), accelerator="F7")
        run_menu.add_command(label="Build with Libraries (F8)", command=lambda: self.build_code("embed"), accelerator="F8")
        run_menu.add_separator()
        run_menu.add_command(label="Compile to Bytecode (F9)", command=self.compile_bytecode, accelerator="F9")
        run_menu.add_command(label="Package Distribution (F10)", command=self.package_app, accelerator="F10")
        menubar.add_cascade(label="Run", menu=run_menu)

        help_menu = tk.Menu(menubar, tearoff=0)
        help_menu.add_command(label="About", command=self.show_about)
        menubar.add_cascade(label="Help", menu=help_menu)

    # ---- Toolbar ----
    def _setup_toolbar(self):
        toolbar = ttk.Frame(self.root)
        toolbar.pack(fill=tk.X, padx=2, pady=2)

        # New button with dropdown
        new_mb = ttk.Menubutton(toolbar, text="New", width=6)
        new_menu = tk.Menu(new_mb, tearoff=0)
        new_menu.add_command(label="File", command=self._new_file_tab)
        new_menu.add_command(label="Project", command=self._new_project)
        new_mb.config(menu=new_menu)
        new_mb.pack(side=tk.LEFT, padx=1)

        # Open button with dropdown
        open_mb = ttk.Menubutton(toolbar, text="Open", width=6)
        open_menu = tk.Menu(open_mb, tearoff=0)
        open_menu.add_command(label="File", command=self._open_file_tab)
        open_menu.add_command(label="Project", command=self._open_project)
        open_mb.config(menu=open_menu)
        open_mb.pack(side=tk.LEFT, padx=1)

        ttk.Button(toolbar, text="Save", command=self.save_file, width=6).pack(side=tk.LEFT, padx=1)
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=4)

        self.run_btn = ttk.Button(toolbar, text="Run (F5)", command=self.run_code, width=10)
        self.run_btn.pack(side=tk.LEFT, padx=1)
        ttk.Button(toolbar, text="Go Ln", command=self._go_to_line, width=6).pack(side=tk.LEFT, padx=1)

        self.alb_label = ttk.Label(toolbar, text=f"  ALB: {self.alb_exe or 'NOT FOUND'}",
                                    foreground="green" if self.alb_exe else "red")
        self.alb_label.pack(side=tk.RIGHT)

    # ---- Layout ----
    def _setup_layout(self):
        """Create the main layout with file tree, notebook tabs, and output."""
        # Main horizontal paned window (left=file tree, right=editor+output)
        self._hpaned = tk.PanedWindow(self.root, orient=tk.HORIZONTAL, sashwidth=4,
                                       sashrelief=tk.RAISED, bg="#cccccc")
        self._hpaned.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        # File tree panel (initially hidden)
        self._tree_frame = ttk.Frame(self._hpaned, width=200)
        self._hpaned.add(self._tree_frame, minsize=100, width=200, stretch="never")

        # Right side: vertical paned (editor notebook + output)
        self._vpaned = ttk.PanedWindow(self._hpaned, orient=tk.VERTICAL)
        self._hpaned.add(self._vpaned, minsize=400, stretch="always")

        # Notebook for tabs
        self.notebook = ttk.Notebook(self._vpaned)
        self._vpaned.add(self.notebook, weight=3)
        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)
        self.notebook.bind("<Button-3>", self._on_tab_right_click)  # Right-click on tab

        # File tree widget
        self._file_tree = ttk.Treeview(self._tree_frame, show="tree headings",
                                        selectmode="browse")
        self._file_tree.heading("#0", text="Project", anchor=tk.W)
        self._file_tree.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self._file_tree.bind("<Double-1>", self._on_tree_double_click)
        self._file_tree.bind("<Button-3>", self._on_tree_right_click)

        # Hide file tree initially
        self._hpaned.paneconfig(self._tree_frame, width=0, minsize=0)
        self._tree_visible = False

        # Find / Replace / Go to line
        self.root.bind("<Control-f>", lambda e: self._find_replace(False))
        self.root.bind("<Control-h>", lambda e: self._find_replace(True))
        self.root.bind("<Control-g>", lambda e: self._go_to_line())

    # ---- Tab Management ----
    def _get_current_tab(self):
        """Get the current TabInfo object."""
        if self._current_tab_id is None:
            return None
        return self._tabs.get(self._current_tab_id)

    def _new_tab(self, title="Untitled", content="", file_path=None):
        """Create a new tab with its own editor."""
        self._tab_counter += 1
        tab_id = self._tab_counter
        tab = TabInfo()
        tab.title = title
        tab.file_path = file_path

        # Create tab frame
        tab.frame = ttk.Frame(self.notebook)

        # Code frame (editor + line numbers)
        code_frame = ttk.Frame(tab.frame)
        code_frame.pack(fill=tk.BOTH, expand=True)

        # Line numbers
        tab.line_numbers = tk.Text(code_frame, width=5, bg="#f0f0f0", fg="#999",
                                    font=("Consolas", 11), state=tk.DISABLED,
                                    borderwidth=0, highlightthickness=0)
        tab.line_numbers.pack(side=tk.LEFT, fill=tk.Y)

        # Main text editor
        tab.editor = tk.Text(code_frame, font=("Consolas", 11),
                              undo=True, wrap=tk.NONE,
                              bg="#ffffff", fg="#1e1e1e",
                              insertbackground="#333",
                              selectbackground="#add6ff",
                              tabs=("16",))
        tab.editor.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Scrollbars
        vsb = ttk.Scrollbar(code_frame, orient=tk.VERTICAL, command=tab.editor.yview)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)
        hsb = ttk.Scrollbar(code_frame, orient=tk.HORIZONTAL, command=tab.editor.xview)
        hsb.pack(side=tk.BOTTOM, fill=tk.X)
        tab.editor.config(yscrollcommand=vsb.set, xscrollcommand=hsb.set)

        # Bindings
        tab.editor.bind("<KeyRelease>", self._on_key_release)
        tab.editor.bind("<MouseWheel>", self._on_scroll)
        tab.editor.bind("<Button-1>", self._on_click)
        tab.editor.bind("<Motion>", self._on_mouse_move)
        tab.editor.bind("<Leave>", self._hide_tooltip)
        tab.editor.bind("<Escape>", self._close_popups)
        tab.editor.bind("<Tab>", self._on_tab)
        tab.editor.bind("<Control-space>", self._trigger_completion)
        tab.editor.bind("<Control-Button-1>", self._on_ctrl_click)
        tab.editor.bind("<KeyPress-parenleft>", self._on_bracket_open_paren)
        tab.editor.bind("<KeyPress-bracketleft>", self._on_bracket_open_bracket)
        tab.editor.bind("<KeyPress-braceleft>", self._on_bracket_open_brace)
        tab.editor.bind("<Return>", self._on_enter)

        # Store tab_id in editor for event handling
        tab.editor._tab_id = tab_id

        # Insert content
        if content:
            tab.editor.insert("1.0", content)

        # Setup highlighting tags
        self._setup_tags_for(tab.editor)

        # Add to notebook
        self.notebook.add(tab.frame, text=title)
        self._tabs[tab_id] = tab

        # Track saved content for dirty detection
        tab.saved_content = content

        # Select this tab
        self.notebook.select(tab.frame)
        self._current_tab_id = tab_id

        # Initial highlight and line numbers
        self._highlight_syntax()
        self._update_line_numbers()

        return tab_id

    def _close_tab(self, tab_id):
        """Close a tab, with save confirmation if dirty."""
        tab = self._tabs.get(tab_id)
        if not tab:
            return False

        # Check if dirty
        if tab.dirty:
            title = tab.title or "Untitled"
            ans = messagebox.askyesnocancel("Unsaved Changes",
                f"'{title}' has unsaved changes. Save before closing?")
            if ans is None:  # Cancel
                return False
            if ans:  # Yes
                # Temporarily switch to this tab for save
                old_id = self._current_tab_id
                self._current_tab_id = tab_id
                self.save_file()
                self._current_tab_id = old_id

        # Remove from notebook
        self.notebook.forget(tab.frame)
        tab.frame.destroy()
        del self._tabs[tab_id]

        # Update current tab
        if self._current_tab_id == tab_id:
            if self._tabs:
                self._current_tab_id = list(self._tabs.keys())[-1]
            else:
                self._current_tab_id = None

        return True

    def _close_current_tab(self):
        """Close the current tab."""
        if self._current_tab_id is not None:
            self._close_tab(self._current_tab_id)
        # If no tabs left, create a new empty tab
        if not self._tabs:
            self._new_file_tab()

    def _on_tab_changed(self, event=None):
        """Handle tab switch."""
        try:
            selected = self.notebook.select()
            if not selected:
                return
            # Find tab_id by frame
            for tab_id, tab in self._tabs.items():
                if str(tab.frame) == selected:
                    self._current_tab_id = tab_id
                    self._update_title()
                    self._update_line_numbers()
                    self._update_cursor_pos()
                    break
        except Exception:
            pass

    def _on_tab_right_click(self, event):
        """Show context menu when right-clicking on a tab."""
        # Find which tab was clicked
        clicked_tab = self.notebook.identify(event.x, event.y)
        if not clicked_tab or clicked_tab != "label":
            return

        # Get the tab index and frame
        try:
            tab_index = self.notebook.index(f"@{event.x},{event.y}")
            tab_frame = self.notebook.tabs()[tab_index]
        except Exception:
            return

        # Find tab_id by frame
        target_tab_id = None
        for tab_id, tab in self._tabs.items():
            if str(tab.frame) == str(tab_frame):
                target_tab_id = tab_id
                break

        if target_tab_id is None:
            return

        # Select this tab first
        self.notebook.select(tab_frame)
        self._current_tab_id = target_tab_id

        # Create context menu
        menu = tk.Menu(self.root, tearoff=0)
        menu.add_command(label="Rename...", command=lambda: self._rename_tab(target_tab_id))
        menu.add_separator()
        menu.add_command(label="Close", command=lambda: self._close_tab(target_tab_id))
        menu.add_command(label="Close Others", command=lambda: self._close_other_tabs(target_tab_id))
        menu.add_command(label="Close All", command=self._close_all_tabs)
        menu.add_separator()
        menu.add_command(label="Copy Full Path", command=lambda: self._copy_tab_path(target_tab_id))

        menu.post(event.x_root, event.y_root)

    def _rename_tab(self, tab_id):
        """Rename a tab (for unsaved files)."""
        tab = self._tabs.get(tab_id)
        if not tab:
            return

        if tab.file_path:
            # If file is saved, show info
            messagebox.showinfo("Rename", f"File is saved as:\n{tab.file_path}\n\nUse 'Save As...' to rename.")
            return

        # Ask for new name
        new_name = simpledialog.askstring("Rename Tab", "Enter new name:",
                                          initialvalue=tab.title, parent=self.root)
        if not new_name:
            return

        # Remove asterisk if present
        clean_name = new_name.lstrip("*")
        if not clean_name:
            return

        display_name = f"{'*' if tab.dirty else ''}{clean_name}"
        self._update_tab_title(tab_id, clean_name, display_name)
        if tab_id == self._current_tab_id:
            self._update_title()

    def _close_other_tabs(self, keep_tab_id):
        """Close all tabs except the specified one."""
        for tab_id in list(self._tabs.keys()):
            if tab_id != keep_tab_id:
                if not self._close_tab(tab_id):
                    return  # User cancelled
        # If no tabs left, create a new empty tab
        if not self._tabs:
            self._new_file_tab()

    def _close_all_tabs(self):
        """Close all tabs."""
        for tab_id in list(self._tabs.keys()):
            if not self._close_tab(tab_id):
                return  # User cancelled
        # Create a new empty tab
        if not self._tabs:
            self._new_file_tab()

    def _copy_tab_path(self, tab_id):
        """Copy the file path of a tab to clipboard."""
        tab = self._tabs.get(tab_id)
        if not tab:
            return
        path = tab.file_path or "(unsaved)"
        self.root.clipboard_clear()
        self.root.clipboard_append(path)
        self._update_status(f"Copied: {path}")

    def _new_file_tab(self, content="", file_path=None, title=None):
        """Create a new empty file tab."""
        if title is None:
            title = "Untitled"
        tab_id = self._new_tab(title, content, file_path)
        self.root.title(f"ALB IDE v{VERSION} - {title}")
        return tab_id

    def _open_file_in_tab(self, path):
        """Open a file in a new tab, or switch to existing tab if already open."""
        path = os.path.abspath(path)

        # Check if already open
        for tab_id, tab in self._tabs.items():
            if tab.file_path and os.path.abspath(tab.file_path) == path:
                self.notebook.select(tab.frame)
                self._current_tab_id = tab_id
                self._update_title()
                return tab_id

        # Read file
        try:
            with open(path, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            messagebox.showerror("Error", f"Cannot open file:\n{e}")
            return None

        title = os.path.basename(path)
        tab_id = self._new_tab(title, content, path)
        self._add_recent_file(path)
        self.root.title(f"ALB IDE v{VERSION} - {title}")
        self._scan_use_statements()
        return tab_id

    def _open_file_tab(self):
        """Open a file dialog and open in a new tab."""
        path = filedialog.askopenfilename(
            title="Open ALB Script",
            filetypes=[("ALB Scripts", "*.alb"), ("All Files", "*.*")]
        )
        if path:
            self._open_file_in_tab(path)

    def _update_tab_title(self, tab_id, base_name, display_name=None):
        """Update a tab's title.

        tab.title stores the clean base name (no asterisk).
        The notebook tab label shows display_name (with asterisk if dirty).
        """
        tab = self._tabs.get(tab_id)
        if not tab:
            return
        tab.title = base_name  # always clean, no star
        label = display_name if display_name is not None else base_name
        self.notebook.tab(tab.frame, text=label)

    # ---- Code Editor (legacy, kept for compatibility) ----
    def _setup_editor(self):
        """Legacy - now handled by _setup_layout and _new_tab."""
        pass

    def _setup_tags(self):
        """Configure syntax highlighting tags for current editor."""
        if self.editor:
            self._setup_tags_for(self.editor)

    def _setup_tags_for(self, editor):
        """Configure syntax highlighting tags (VS Code Light+ theme)."""
        base_font = ("Consolas", 11)
        bold_font = ("Consolas", 11, "bold")
        italic_font = ("Consolas", 11, "italic")
        editor.tag_configure("keyword",   foreground="#0000ff", font=bold_font)
        editor.tag_configure("builtin",   foreground="#795e26", font=base_font)
        editor.tag_configure("string",    foreground="#a31515", font=base_font)
        editor.tag_configure("comment",   foreground="#008000", font=italic_font)
        editor.tag_configure("number",    foreground="#098658", font=base_font)
        editor.tag_configure("constant",  foreground="#0070c1", font=bold_font)
        editor.tag_configure("paren",     foreground="#888888", font=base_font)
        # Tag priorities: comment > string > keyword > builtin > constant > number > paren
        editor.tag_raise("comment")
        editor.tag_raise("string")
        # Bracket match highlights
        editor.tag_configure("bracket_match", background="#c8e6c9",
                                   foreground="#1b5e20", font=bold_font)
        editor.tag_configure("bracket_mismatch", background="#ffcdd2",
                                   foreground="#b71c1c", font=bold_font)

    # ---- Syntax Highlighting ----
    _RE_NUM = re.compile(r'^-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?$')

    def _highlight_syntax(self):
        """Full document syntax highlighting."""
        for tag in ("keyword", "builtin", "string", "comment", "number", "constant", "paren"):
            self.editor.tag_remove(tag, "1.0", tk.END)

        content = self.editor.get("1.0", tk.END)
        lines = content.split("\n")

        for i, line in enumerate(lines, 1):
            self._highlight_line(i, line)

    def _highlight_line(self, lineno, line):
        """Highlight a single line."""
        ln = str(lineno)
        # Remove existing tags on this line
        for tag in ("keyword", "builtin", "string", "comment", "number", "constant", "paren"):
            self.editor.tag_remove(tag, f"{ln}.0", f"{ln}.end")

        # Find comment start (respecting strings)
        in_str = False
        comment_start = -1
        for j, c in enumerate(line):
            if c == '"' and (j == 0 or line[j-1] != '\\'):
                in_str = not in_str
            elif c == ';' and not in_str:
                comment_start = j
                break
        code_part = line
        if comment_start >= 0:
            self.editor.tag_add("comment", f"{ln}.{comment_start}", f"{ln}.end")
            code_part = line[:comment_start]

        # Strings (only on code portion, not inside comments)
        in_str = False
        start = 0
        for j, c in enumerate(code_part):
            if c == '"' and (j == 0 or code_part[j-1] != '\\'):
                if in_str:
                    self.editor.tag_add("string", f"{ln}.{start}", f"{ln}.{j+1}")
                    in_str = False
                else:
                    start = j
                    in_str = True

        # Backtick strings: `...` — highlight as string
        in_bt = False
        bt_start = -1
        for j, c in enumerate(code_part):
            if c == '`':
                if not in_bt:
                    in_bt = True
                    bt_start = j
                else:
                    # End of backtick string
                    self.editor.tag_add("string", f"{ln}.{bt_start}", f"{ln}.{j+1}")
                    in_bt = False

        # Tokenize (only on code portion)
        for m in re.finditer(r'\(|\)|,\@?|`|[^\s()`]+', code_part):
            tok = m.group()
            idx = m.start()
            s = f"{ln}.{idx}"
            e = f"{ln}.{idx + len(tok)}"

            # Skip tokens that are inside strings
            if tok.startswith('"') or tok.endswith('"'):
                continue
            # Skip backtick (already highlighted above)
            if tok == '`':
                continue

            if tok in ("(", ")"):
                self.editor.tag_add("paren", s, e)
            elif tok in (",", ",@"):
                # Comma and unquote-splicing markers as keywords
                self.editor.tag_add("keyword", s, e)
            elif self._RE_NUM.match(tok):
                self.editor.tag_add("number", s, e)
            elif tok in _CONSTANTS:
                self.editor.tag_add("constant", s, e)
            elif tok in _KEYWORDS:
                self.editor.tag_add("keyword", s, e)
            elif tok in _FUNC_DB:
                self.editor.tag_add("builtin", s, e)

    # ---- Event handlers ----
    def _on_key_release(self, event=None):
        self._update_line_numbers()
        self._update_cursor_pos()
        # Track dirty on text modification
        # Use content comparison to reliably detect actual edits
        # (event.state Ctrl check is unreliable on Windows tkinter KeyRelease)
        if event and event.char and event.char.isprintable():
            cur = self.editor.get("1.0", tk.END + "-1c")
            if cur != self._saved_content:
                self._dirty = True
                self._update_title()
        # Debounce highlighting
        if self._hl_timer:
            self.root.after_cancel(self._hl_timer)
        self._hl_timer = self.root.after(150, self._highlight_syntax)
        # Debounce use-statement scanning (1.5s)
        if event and event.char:
            self._schedule_use_scan()
        # Update argument hint while typing inside a function call
        if event and event.char and event.char.isprintable():
            self._update_arg_hint()
        # Handle completion
        if event and event.keysym in ("Return", "space", "parenleft", "parenright",
                                       "BackSpace", "Delete"):
            if event.keysym == "parenleft":
                self._show_function_hint()
            elif event.keysym in ("parenright", "Return"):
                self._hide_tooltip()
            self._hide_completion()
        elif event and event.char and event.char.isprintable():
            self._update_completion()

    def _on_scroll(self, event=None):
        self._update_line_numbers()
        self._hide_tooltip()

    # ---- Dynamic Library Doc Scanning ----
    def _scan_use_statements(self):
        """Scan editor content for (use "xxx") and parse new libraries."""
        content = self.editor.get("1.0", tk.END)
        uses = set(_RE_USE.findall(content))

        # Load newly referenced libraries
        for lib_name in uses:
            if lib_name not in self._loaded_libs:
                self._load_lib_docs(lib_name)

        # Unload libraries no longer referenced
        removed = self._loaded_libs - uses
        for lib_name in removed:
            self._unload_lib_docs(lib_name)

    def _load_lib_docs(self, lib_name):
        """Parse a library and register its functions dynamically."""
        result = _find_lib_root(lib_name)
        if not result:
            return

        kind, path = result
        registered_funcs = []
        registered_consts = []

        if kind == "folder":
            # Parse all .alb files (sorted by name)
            alb_files = sorted(glob_mod.glob(os.path.join(path, "*.alb")))
            for f in alb_files:
                funcs, consts = _parse_alb_file(f)
                for name, params, desc in funcs:
                    if name not in _FUNC_DB:
                        _FUNC_DB[name] = (params, desc, lib_name)
                        _COMPLETIONS[name] = ("fn", params, desc, lib_name)
                        registered_funcs.append(name)
                for name, desc in consts:
                    if name not in _CONSTANTS:
                        _CONSTANTS.append(name)
                        _COMPLETIONS[name] = ("const", "", desc or f"Constant from {lib_name}", lib_name)
                        registered_consts.append(name)
            # Parse .cpp files for exported DLL function names
            cpp_files = glob_mod.glob(os.path.join(path, "*.cpp"))
            for f in cpp_files:
                names = _parse_cpp_exports(f)
                for name in names:
                    if name not in _FUNC_DB:
                        _FUNC_DB[name] = ("", "(C++ DLL function)", lib_name)
                        _COMPLETIONS[name] = ("fn", "", f"C++ DLL function from {lib_name}", lib_name)
                        registered_funcs.append(name)
        else:  # single .alb file
            funcs, consts = _parse_alb_file(path)
            for name, params, desc in funcs:
                if name not in _FUNC_DB:
                    _FUNC_DB[name] = (params, desc, lib_name)
                    _COMPLETIONS[name] = ("fn", params, desc, lib_name)
                    registered_funcs.append(name)
            for name, desc in consts:
                if name not in _CONSTANTS:
                    _CONSTANTS.append(name)
                    _COMPLETIONS[name] = ("const", "", desc or f"Constant from {lib_name}", lib_name)
                    registered_consts.append(name)

        self._loaded_libs.add(lib_name)
        # Re-highlight to show new builtin tokens
        self._highlight_syntax()

    def _unload_lib_docs(self, lib_name):
        """Remove dynamically loaded entries for a library."""
        # Remove functions registered by this lib
        to_remove_fn = [name for name, (p, d, c) in _FUNC_DB.items()
                        if c == lib_name]
        for name in to_remove_fn:
            del _FUNC_DB[name]
            _COMPLETIONS.pop(name, None)
        # Remove constants from this lib
        to_remove_const = [name for name, (kind, p, d, c) in _COMPLETIONS.items()
                           if kind == "const" and c == lib_name]
        for name in to_remove_const:
            del _COMPLETIONS[name]
            if name in _CONSTANTS:
                _CONSTANTS.remove(name)
        self._loaded_libs.discard(lib_name)
        self._highlight_syntax()

    def _schedule_use_scan(self):
        """Debounce use-statement scanning (wait 1.5s after last edit)."""
        if self._use_scan_timer:
            self.root.after_cancel(self._use_scan_timer)
        self._use_scan_timer = self.root.after(1500, self._scan_use_statements)

    def _on_click(self, event=None):
        self._hide_completion()
        self._hide_tooltip()
        self._hide_refs_popup()
        self._update_line_numbers()
        self._update_cursor_pos()
        self._bracket_match()

    def _on_mouse_move(self, event=None):
        """Show tooltip / references when hovering over a function name."""
        if self._tooltip_timer:
            self.root.after_cancel(self._tooltip_timer)
        self._tooltip_timer = self.root.after(500, lambda: self._try_hover_or_refs(event))

    def _close_popups(self, event=None):
        self._hide_completion()
        self._hide_tooltip()
        self._hide_ac_detail()
        self._hide_refs_popup()
        return "break"

    # ---- Bracket Matching & Auto-pair ----
    _BRACKETS = {'(': ')', '[': ']', '{': '}'}
    _CLOSE_BRACKETS = {')': '(', ']': '[', '}': '{'}

    def _bracket_match(self):
        """Highlight matching bracket pair near cursor."""
        self.editor.tag_remove("bracket_match", "1.0", tk.END)
        self.editor.tag_remove("bracket_mismatch", "1.0", tk.END)
        pos = self.editor.index(tk.INSERT)
        for offset in ("-1c", ""):
            try:
                ci = self.editor.index(f"{pos}{offset}") if offset else pos
                ch = self.editor.get(ci, f"{ci}+1c")
            except Exception:
                continue
            if ch in self._BRACKETS:
                self._find_match(ci, ch, self._BRACKETS[ch], 1)
                break
            elif ch in self._CLOSE_BRACKETS:
                self._find_match(ci, ch, self._CLOSE_BRACKETS[ch], -1)
                break

    def _find_match(self, pos, ch, target, direction):
        """Find matching bracket and highlight the pair."""
        all_brk = set(self._BRACKETS.keys()) | set(self._BRACKETS.values())
        depth, cur = 1, pos
        try:
            while depth > 0:
                cur = self.editor.index(f"{cur}+{direction}c")
                c = self.editor.get(cur, f"{cur}+1c")
                if not c or c == '\n':
                    break
                if c == ch:
                    depth += 1
                elif c == target:
                    depth -= 1
        except Exception:
            pass
        if depth == 0:
            self.editor.tag_add("bracket_match", pos, f"{pos}+1c")
            self.editor.tag_add("bracket_match", cur, f"{cur}+1c")
        else:
            self.editor.tag_add("bracket_mismatch", pos, f"{pos}+1c")

    def _on_bracket_open_paren(self, event=None):
        self.editor.insert(tk.INSERT, "()")
        pos = self.editor.index(tk.INSERT)
        line, col = pos.split('.')
        self.editor.mark_set(tk.INSERT, f"{line}.{int(col)-1}")
        return "break"

    def _on_bracket_open_bracket(self, event=None):
        self.editor.insert(tk.INSERT, "[]")
        pos = self.editor.index(tk.INSERT)
        line, col = pos.split('.')
        self.editor.mark_set(tk.INSERT, f"{line}.{int(col)-1}")
        return "break"

    def _on_bracket_open_brace(self, event=None):
        self.editor.insert(tk.INSERT, "{}")
        pos = self.editor.index(tk.INSERT)
        line, col = pos.split('.')
        self.editor.mark_set(tk.INSERT, f"{line}.{int(col)-1}")
        return "break"

    # ---- Auto-indent ----
    def _on_enter(self, event=None):
        """Smart auto-indent based on previous line's indentation and paren balance."""
        pos = self.editor.index(tk.INSERT)
        cur_line = int(pos.split('.')[0])
        line_text = self.editor.get(f"{cur_line}.0", pos)
        tab_width = 2  # ALB uses 2-space indent

        # Get the current line's indentation level (in spaces)
        cur_indent = 0
        for c in line_text:
            if c == ' ':
                cur_indent += 1
            elif c == '\t':
                cur_indent += tab_width
            else:
                break

        # Strip comment and count paren balance on current line (up to cursor)
        code_part = line_text.split(';')[0] if ';' in line_text else line_text
        # Also handle strings: skip parens inside ""
        opens = 0
        closes = 0
        in_str = False
        for ch in code_part:
            if ch == '"':
                in_str = not in_str
            elif not in_str:
                if ch == '(':
                    opens += 1
                elif ch == ')':
                    closes += 1

        balance = opens - closes  # net open parens on this line

        if balance > 0:
            # More opens than closes → next line should indent deeper
            # But only indent by 1 level (tab_width), regardless of how many opens
            new_indent = cur_indent + tab_width
        elif balance < 0:
            # More closes than opens → next line should dedent
            new_indent = max(0, cur_indent + balance * tab_width)
        else:
            # Balanced → keep same indent
            new_indent = cur_indent

        indent = " " * new_indent
        self.editor.insert(tk.INSERT, "\n" + indent)
        self._dirty = True
        self._update_title()
        self._update_line_numbers()
        return "break"

    # ---- Go to Line ----
    def _go_to_line(self):
        """Show go-to-line dialog (Ctrl+G)."""
        dlg = tk.Toplevel(self.root)
        dlg.title("Go to Line")
        dlg.geometry("280x80")
        dlg.transient(self.root)
        dlg.resizable(False, False)
        try:
            if _ICON_PATH:
                dlg.iconbitmap(default=_ICON_PATH)
        except Exception:
            pass
        frame = ttk.Frame(dlg, padding=10)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, text="Line number:").pack(anchor="w")
        entry = ttk.Entry(frame, font=("Consolas", 11))
        entry.pack(fill=tk.X, pady=2)
        entry.focus_set()
        def go():
            try:
                ln = int(entry.get())
                total = int(self.editor.index(tk.END + "-1c").split(".")[0])
                if 1 <= ln <= total:
                    self.editor.mark_set(tk.INSERT, f"{ln}.0")
                    self.editor.see(f"{ln}.0")
                    self._update_cursor_pos()
                    dlg.destroy()
                else:
                    messagebox.showwarning("Go to Line", f"Enter 1-{total}", parent=dlg)
            except ValueError:
                messagebox.showwarning("Go to Line", "Invalid number", parent=dlg)
        entry.bind("<Return>", lambda e: go())
        dlg.bind("<Escape>", lambda e: dlg.destroy())

    # ---- Find / Replace ----
    def _find_replace(self, show_replace=False):
        """Open Find or Find & Replace dialog."""
        if (self._find_dlg and self._find_dlg.winfo_exists()):
            self._find_dlg.lift()
            self._find_dlg.focus_force()
            if show_replace:
                self._find_dlg_replace_frame.pack(fill=tk.X, padx=5, pady=(0, 5))
            else:
                self._find_dlg_replace_frame.pack_forget()
            self._find_dlg_find_entry.focus_set()
            return
        dlg = tk.Toplevel(self.root)
        dlg.title("Find & Replace" if show_replace else "Find")
        dlg.geometry("420x170" if show_replace else "420x130")
        dlg.transient(self.root)
        dlg.resizable(False, False)
        self._find_dlg = dlg
        self._find_last_pos = "1.0"
        try:
            if _ICON_PATH:
                dlg.iconbitmap(default=_ICON_PATH)
        except Exception:
            pass
        # Find row
        ff = ttk.Frame(dlg, padding=5)
        ff.pack(fill=tk.X)
        ttk.Label(ff, text="Find:", width=8).pack(side=tk.LEFT)
        fe = ttk.Entry(ff, font=("Consolas", 10))
        fe.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self._find_dlg_find_entry = fe
        # Options
        of = ttk.Frame(dlg, padding=(5, 0, 5, 5))
        of.pack(fill=tk.X)
        self._find_case_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(of, text="Case sensitive", variable=self._find_case_var).pack(side=tk.LEFT)
        # Buttons
        bf = ttk.Frame(dlg, padding=(5, 0, 5, 5))
        bf.pack(fill=tk.X)
        ttk.Button(bf, text="Find Next", command=self._find_next, width=10).pack(side=tk.LEFT, padx=2)
        ttk.Button(bf, text="Find Prev", command=self._find_prev, width=10).pack(side=tk.LEFT, padx=2)
        self._find_status_lbl = ttk.Label(bf, text="")
        self._find_status_lbl.pack(side=tk.RIGHT, padx=5)
        # Replace row
        rf = ttk.Frame(dlg, padding=(5, 0, 5, 5))
        self._find_dlg_replace_frame = rf
        ttk.Label(rf, text="Replace:", width=8).pack(side=tk.LEFT)
        re_ = ttk.Entry(rf, font=("Consolas", 10))
        re_.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self._find_dlg_replace_entry = re_
        rf2 = ttk.Frame(dlg, padding=(5, 0, 5, 5))
        self._find_replace_frame2 = rf2
        ttk.Button(rf2, text="Replace", command=self._do_replace, width=10).pack(side=tk.LEFT, padx=2)
        ttk.Button(rf2, text="Replace All", command=self._do_replace_all, width=10).pack(side=tk.LEFT, padx=2)
        if show_replace:
            rf.pack(fill=tk.X)
            rf2.pack(fill=tk.X)
        fe.bind("<Return>", lambda e: self._find_next())
        fe.focus_set()
        dlg.bind("<Escape>", lambda e: dlg.destroy())

    def _find_next(self):
        """Search forward for next occurrence."""
        term = self._find_dlg_find_entry.get()
        if not term:
            return
        self.editor.tag_remove("sel", "1.0", tk.END)
        nocase = not self._find_case_var.get()
        start = self.editor.index(f"{tk.INSERT}+1c")
        pos = self.editor.search(term, start, stopindex=tk.END, nocase=nocase)
        if not pos:
            pos = self.editor.search(term, "1.0", stopindex=start, nocase=nocase)
        if pos:
            end_pos = f"{pos}+{len(term)}c"
            self.editor.tag_add("sel", pos, end_pos)
            self.editor.mark_set(tk.INSERT, end_pos)
            self.editor.see(pos)
            self._find_status_lbl.config(text="Found")
        else:
            self._find_status_lbl.config(text="Not found")

    def _find_prev(self):
        """Search backward for previous occurrence."""
        term = self._find_dlg_find_entry.get()
        if not term:
            return
        self.editor.tag_remove("sel", "1.0", tk.END)
        nocase = not self._find_case_var.get()
        pos = self.editor.search(term, tk.INSERT, stopindex="1.0",
                                  backwards=True, nocase=nocase)
        if not pos:
            pos = self.editor.search(term, tk.END, stopindex=tk.INSERT,
                                      backwards=True, nocase=nocase)
        if pos:
            end_pos = f"{pos}+{len(term)}c"
            self.editor.tag_add("sel", pos, end_pos)
            self.editor.mark_set(tk.INSERT, pos)
            self.editor.see(pos)
            self._find_status_lbl.config(text="Found")
        else:
            self._find_status_lbl.config(text="Not found")

    def _do_replace(self):
        """Replace current selection and find next."""
        term = self._find_dlg_find_entry.get()
        repl = self._find_dlg_replace_entry.get()
        if not term:
            return
        try:
            sel = self.editor.get("sel.first", "sel.last")
            cmp_sel = sel if self._find_case_var.get() else sel.lower()
            cmp_term = term if self._find_case_var.get() else term.lower()
            if cmp_sel == cmp_term:
                self.editor.delete("sel.first", "sel.last")
                self.editor.insert("sel.first", repl)
                self._dirty = True
                self._update_title()
        except tk.TclError:
            pass
        self._find_next()

    def _do_replace_all(self):
        """Replace all occurrences of find text."""
        term = self._find_dlg_find_entry.get()
        repl = self._find_dlg_replace_entry.get()
        if not term:
            return
        nocase = not self._find_case_var.get()
        count = 0
        pos = "1.0"
        while True:
            pos = self.editor.search(term, pos, stopindex=tk.END, nocase=nocase)
            if not pos:
                break
            end_pos = f"{pos}+{len(term)}c"
            self.editor.delete(pos, end_pos)
            self.editor.insert(pos, repl)
            pos = f"{pos}+{len(repl)}c"
            count += 1
        if count:
            self._dirty = True
            self._update_title()
            self._highlight_syntax()
        self._find_status_lbl.config(text=f"Replaced {count}")

    # ---- Cursor Position ----
    def _update_cursor_pos(self):
        """Update cursor line:col in statusbar."""
        if self._cursor_label:
            try:
                pos = self.editor.index(tk.INSERT)
                parts = pos.split(".")
                self._cursor_label.config(text=f"  Ln {parts[0]}, Col {int(parts[1])+1}  ")
            except Exception:
                pass

    # ---- Recent Files ----
    def _load_recent_files(self):
        """Load recent files list from disk."""
        try:
            if os.path.isfile(_RECENT_FILE):
                with open(_RECENT_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    return [p for p in data if os.path.isfile(p)][:10]
        except Exception:
            pass
        return []

    def _save_recent_files(self):
        """Persist recent files list."""
        try:
            with open(_RECENT_FILE, "w", encoding="utf-8") as f:
                json.dump(self._recent_files, f)
        except Exception:
            pass

    def _add_recent_file(self, path):
        """Add a file to recent files list."""
        apath = os.path.abspath(path)
        if apath in self._recent_files:
            self._recent_files.remove(apath)
        self._recent_files.insert(0, apath)
        self._recent_files = self._recent_files[:10]
        self._save_recent_files()
        self._rebuild_recent_menu()

    def _rebuild_recent_menu(self):
        """Rebuild the Recent Files submenu."""
        self._recent_menu.delete(0, tk.END)
        for i, path in enumerate(self._recent_files):
            label = f"{i+1}. {os.path.basename(path)}"
            self._recent_menu.add_command(
                label=label,
                command=lambda p=path: self._open_recent(p))
        if not self._recent_files:
            self._recent_menu.add_command(label="(empty)", state=tk.DISABLED)

    def _open_recent(self, path):
        """Open a file from recent files list."""
        if not os.path.isfile(path):
            messagebox.showwarning("Open", f"File not found:\n{path}")
            self._recent_files.remove(path)
            self._rebuild_recent_menu()
            return
        self._open_file_in_tab(path)

    def _update_title(self):
        """Update window title and tab title with dirty indicator."""
        tab = self._get_current_tab()
        if not tab:
            return
        # Base name: prefer file_path basename, else clean tab.title (strip any leading *)
        if tab.file_path:
            base_name = os.path.basename(tab.file_path)
        else:
            base_name = tab.title.lstrip("*") if tab.title else "Untitled"
        star = "*" if tab.dirty else ""
        display_title = f"{star}{base_name}"
        self.root.title(f"ALB IDE v{VERSION} - {display_title}")
        # Update tab label and store clean base name (no star) in tab.title
        self._update_tab_title(self._current_tab_id, base_name, display_title)

    # ---- File Operations ----
    def _confirm_discard(self):
        """Ask user to save if dirty. Returns False if cancelled."""
        if not self._dirty:
            return True
        ans = messagebox.askyesnocancel("Unsaved Changes",
            "You have unsaved changes. Save before continuing?")
        if ans is None:
            return False  # Cancel
        if ans:
            self.save_file()
        return True

    def new_file(self):
        """Legacy - redirect to _new_file_tab."""
        self._new_file_tab()

    def open_file(self):
        """Legacy - redirect to _open_file_tab."""
        self._open_file_tab()

    def _do_open_file(self, path):
        """Legacy - redirect to _open_file_in_tab."""
        self._open_file_in_tab(path)

    def save_file(self):
        tab = self._get_current_tab()
        if not tab:
            return
        if tab.file_path:
            self._write_file(tab.file_path)
        else:
            self.save_file_as()

    def save_file_as(self):
        tab = self._get_current_tab()
        if not tab:
            return
        path = filedialog.asksaveasfilename(
            title="Save ALB Script",
            defaultextension=".alb",
            filetypes=[("ALB Scripts", "*.alb"), ("All Files", "*.*")]
        )
        if not path:
            return
        self._write_file(path)
        tab.file_path = path
        tab.title = os.path.basename(path)
        self._add_recent_file(path)
        self._update_title()
        # Update project if open
        if self._project_path:
            self._add_file_to_project(path)

    def _write_file(self, path):
        try:
            content = self.editor.get("1.0", tk.END + "-1c")
            with open(path, "w", encoding="utf-8") as f:
                f.write(content)
            self._saved_content = content  # track for dirty detection
            self._dirty = False
            self._update_title()
            self._add_recent_file(path)
            self._update_status(f"Saved: {path}")
            # Re-scan use statements after save
            self._scan_use_statements()
        except Exception as e:
            messagebox.showerror("Error", f"Cannot save file:\n{e}")

    # ---- Project Management ----
    def _new_project(self):
        """Create a new project."""
        # Dialog to get project name and location
        dialog = tk.Toplevel(self.root)
        dialog.title("New Project")
        dialog.geometry("400x200")
        dialog.transient(self.root)
        dialog.grab_set()

        ttk.Label(dialog, text="Project Name:").grid(row=0, column=0, padx=10, pady=10, sticky=tk.W)
        name_var = tk.StringVar()
        name_entry = ttk.Entry(dialog, textvariable=name_var, width=30)
        name_entry.grid(row=0, column=1, padx=10, pady=10)

        ttk.Label(dialog, text="Location:").grid(row=1, column=0, padx=10, pady=10, sticky=tk.W)
        loc_var = tk.StringVar(value=os.path.expanduser("~"))
        loc_entry = ttk.Entry(dialog, textvariable=loc_var, width=30)
        loc_entry.grid(row=1, column=1, padx=10, pady=10)

        def browse_location():
            path = filedialog.askdirectory(initialdir=loc_var.get())
            if path:
                loc_var.set(path)

        ttk.Button(dialog, text="Browse...", command=browse_location).grid(row=1, column=2, padx=5)

        result = [None]
        def on_ok():
            name = name_var.get().strip()
            loc = loc_var.get().strip()
            if not name:
                messagebox.showwarning("New Project", "Project name is required.", parent=dialog)
                return
            if not loc or not os.path.isdir(loc):
                messagebox.showwarning("New Project", "Valid location is required.", parent=dialog)
                return
            result[0] = (name, loc)
            dialog.destroy()

        ttk.Button(dialog, text="Create", command=on_ok).grid(row=2, column=1, pady=20)
        self.root.wait_window(dialog)

        if not result[0]:
            return

        name, loc = result[0]
        project_dir = os.path.join(loc, name)
        project_file = os.path.join(project_dir, f"{name}.albprj")
        main_file = os.path.join(project_dir, "main.alb")

        # Create project directory
        try:
            os.makedirs(project_dir, exist_ok=True)
        except Exception as e:
            messagebox.showerror("New Project", f"Cannot create directory:\n{e}")
            return

        # Create .albprj file
        try:
            with open(project_file, "w", encoding="utf-8") as f:
                f.write(f"[Project]\n")
                f.write(f"Name={name}\n")
                f.write(f"Version=1.0\n")
                f.write(f"MainFile=main.alb\n\n")
                f.write(f"[Files]\n")
                f.write(f"main.alb\n\n")
                f.write(f"[Dependencies]\n")
                f.write(f"; (use \"xxx\") references\n")
        except Exception as e:
            messagebox.showerror("New Project", f"Cannot create project file:\n{e}")
            return

        # Create main.alb
        try:
            with open(main_file, "w", encoding="utf-8") as f:
                f.write(f"; {name} - ALB Project\n")
                f.write(f"; Created with ALB IDE v{VERSION}\n\n")
                f.write(f'(print "Hello from {name}!")\n')
        except Exception as e:
            messagebox.showerror("New Project", f"Cannot create main.alb:\n{e}")
            return

        # Open the project
        self._load_project(project_file)
        self._open_file_in_tab(main_file)

    def _open_project(self):
        """Open an existing project."""
        path = filedialog.askopenfilename(
            title="Open ALB Project",
            filetypes=[("ALB Projects", "*.albprj"), ("All Files", "*.*")]
        )
        if not path:
            return
        self._load_project(path)

    def _load_project(self, project_file):
        """Load a project and show file tree."""
        self._project_path = os.path.abspath(project_file)
        self._project_dir = os.path.dirname(self._project_path)

        # Parse project file
        config = self._parse_albprj(self._project_path)
        if not config:
            messagebox.showerror("Open Project", f"Cannot parse project file:\n{self._project_path}")
            return

        project_name = config.get("Project", {}).get("Name", os.path.basename(self._project_dir))

        # Show file tree
        self._show_file_tree(project_name)
        self._refresh_file_tree()

        self.root.title(f"ALB IDE v{VERSION} - {project_name}")

    def _parse_albprj(self, path):
        """Parse .albprj file (INI-like format)."""
        config = {}
        current_section = None
        try:
            with open(path, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith(";"):
                        continue
                    if line.startswith("[") and line.endswith("]"):
                        current_section = line[1:-1]
                        config[current_section] = {}
                    elif "=" in line and current_section:
                        key, value = line.split("=", 1)
                        config[current_section][key.strip()] = value.strip()
        except Exception:
            return None
        return config

    def _save_albprj(self, config):
        """Save .albprj file."""
        if not self._project_path:
            return
        try:
            with open(self._project_path, "w", encoding="utf-8") as f:
                for section, data in config.items():
                    f.write(f"[{section}]\n")
                    if isinstance(data, dict):
                        for key, value in data.items():
                            f.write(f"{key}={value}\n")
                    elif isinstance(data, list):
                        for item in data:
                            f.write(f"{item}\n")
                    f.write("\n")
        except Exception as e:
            messagebox.showerror("Save Project", f"Cannot save project:\n{e}")

    def _add_file_to_project(self, file_path):
        """Add a file to the current project."""
        if not self._project_path or not self._project_dir:
            return
        try:
            config = self._parse_albprj(self._project_path)
            if "Files" not in config:
                config["Files"] = {}
            # Get relative path
            rel_path = os.path.relpath(file_path, self._project_dir)
            # Add to Files section (as key with empty value)
            if rel_path not in config["Files"]:
                config["Files"][rel_path] = ""
                self._save_albprj(config)
                self._refresh_file_tree()
        except Exception:
            pass

    def _show_file_tree(self, project_name):
        """Show the file tree panel."""
        if self._tree_visible:
            return
        self._tree_visible = True
        self._hpaned.paneconfig(self._tree_frame, width=200, minsize=100)
        self._file_tree.heading("#0", text=project_name, anchor=tk.W)

    def _hide_file_tree(self):
        """Hide the file tree panel."""
        if not self._tree_visible:
            return
        self._tree_visible = False
        self._hpaned.paneconfig(self._tree_frame, width=0, minsize=0)
        self._project_path = None
        self._project_dir = None

    def _refresh_file_tree(self):
        """Refresh the file tree with project files."""
        if not self._project_dir:
            return

        # Clear existing items
        self._file_tree.delete(*self._file_tree.get_children())

        # Add project root
        root_id = self._file_tree.insert("", tk.END, text=os.path.basename(self._project_dir),
                                          open=True, tags=("dir",))

        # Walk project directory
        for root, dirs, files in os.walk(self._project_dir):
            # Skip hidden directories
            dirs[:] = [d for d in dirs if not d.startswith(".")]

            # Get relative path
            rel_root = os.path.relpath(root, self._project_dir)
            if rel_root == ".":
                parent_id = root_id
            else:
                # Find or create parent directory node
                parent_id = root_id
                for part in rel_root.split(os.sep):
                    # Find child with this name
                    for child in self._file_tree.get_children(parent_id):
                        if self._file_tree.item(child, "text") == part:
                            parent_id = child
                            break

            # Add directories
            for d in sorted(dirs):
                self._file_tree.insert(parent_id, tk.END, text=d, open=False, tags=("dir",))

            # Add files (sorted, .alb first)
            alb_files = sorted([f for f in files if f.endswith(".alb")])
            other_files = sorted([f for f in files if not f.endswith(".alb") and not f.startswith(".")])
            for f in alb_files + other_files:
                full_path = os.path.join(root, f)
                self._file_tree.insert(parent_id, tk.END, text=f,
                                        values=(full_path,), tags=("file",))

    def _on_tree_double_click(self, event):
        """Handle double-click on file tree item."""
        item = self._file_tree.selection()
        if not item:
            return
        values = self._file_tree.item(item[0], "values")
        if values:
            # It's a file
            self._open_file_in_tab(values[0])

    def _on_tree_right_click(self, event):
        """Handle right-click on file tree item."""
        item = self._file_tree.identify_row(event.y)
        if not item:
            return
        self._file_tree.selection_set(item)

        menu = tk.Menu(self.root, tearoff=0)
        tags = self._file_tree.item(item, "tags")

        if "file" in tags:
            menu.add_command(label="Open", command=lambda: self._tree_open_file(item))
        menu.add_separator()
        menu.add_command(label="New File...", command=lambda: self._tree_new_file(item))
        menu.add_command(label="Delete", command=lambda: self._tree_delete_file(item))

        menu.post(event.x_root, event.y_root)

    def _tree_open_file(self, item):
        """Open a file from tree."""
        values = self._file_tree.item(item, "values")
        if values:
            self._open_file_in_tab(values[0])

    def _tree_new_file(self, item):
        """Create a new file in the project."""
        # Get parent directory
        tags = self._file_tree.item(item, "tags")
        if "file" in tags:
            # Get parent
            parent = self._file_tree.parent(item)
            values = self._file_tree.item(parent, "values")
            if values:
                parent_dir = os.path.dirname(values[0])
            else:
                parent_dir = self._project_dir
        else:
            parent_dir = self._project_dir

        # Ask for filename
        filename = simpledialog.askstring("New File", "Enter filename:", parent=self.root)
        if not filename:
            return

        # Add .alb extension if not present
        if not filename.endswith(".alb"):
            filename += ".alb"

        new_path = os.path.join(parent_dir, filename)
        if os.path.exists(new_path):
            messagebox.showwarning("New File", f"File already exists:\n{new_path}", parent=self.root)
            return

        # Create empty file
        try:
            with open(new_path, "w", encoding="utf-8") as f:
                f.write(f"; {filename}\n\n")
        except Exception as e:
            messagebox.showerror("New File", f"Cannot create file:\n{e}", parent=self.root)
            return

        # Add to project and refresh
        self._add_file_to_project(new_path)
        self._open_file_in_tab(new_path)

    def _tree_delete_file(self, item):
        """Delete a file from the project."""
        tags = self._file_tree.item(item, "tags")
        if "file" not in tags:
            return
        values = self._file_tree.item(item, "values")
        if not values:
            return

        file_path = values[0]
        filename = os.path.basename(file_path)

        if not messagebox.askyesno("Delete File", f"Delete '{filename}'?", parent=self.root):
            return

        # Close tab if open
        for tab_id, tab in list(self._tabs.items()):
            if tab.file_path and os.path.abspath(tab.file_path) == os.path.abspath(file_path):
                self._close_tab(tab_id)
                break

        # Delete file
        try:
            os.remove(file_path)
        except Exception as e:
            messagebox.showerror("Delete File", f"Cannot delete:\n{e}", parent=self.root)
            return

        # Remove from project and refresh
        self._refresh_file_tree()

    # ---- Auto-completion ----
    def _get_current_word(self):
        """Get the word being typed at cursor position."""
        pos = self.editor.index(tk.INSERT)
        line_text = self.editor.get(f"{pos} linestart", pos)
        # Find word boundary
        m = re.search(r'([\w\-?!+*/=<>]+)$', line_text)
        if m:
            word = m.group(1)
            col = int(pos.split(".")[1])
            start_col = col - len(word)
            return word, f"{pos.split('.')[0]}.{start_col}"
        return "", pos

    def _update_completion(self):
        """Update autocomplete popup based on current word."""
        word, start_idx = self._get_current_word()
        if len(word) < 1:
            self._hide_completion()
            return

        word_lower = word.lower()
        matches = []
        for name, (kind, params, desc, cat) in _COMPLETIONS.items():
            if name.lower().startswith(word_lower) and name != word:
                matches.append(name)

        if not matches:
            self._hide_completion()
            return

        # Sort: exact prefix first, then alphabetical
        matches.sort(key=lambda x: (not x.startswith(word), x.lower()))
        matches = matches[:12]  # Limit to 12 items

        self._ac_items = matches
        self._ac_prefix = word
        self._ac_start_idx = start_idx
        self._ac_sel = -1
        self._show_completion()

    def _show_completion(self):
        """Show the autocomplete dropdown."""
        if not self._ac_items:
            return

        if self._ac_win is None or not self._ac_win.winfo_exists():
            self._ac_win = tk.Toplevel(self.root)
            self._ac_win.overrideredirect(True)
            self._ac_win.attributes("-topmost", True)

            self._ac_list = tk.Listbox(self._ac_win, font=("Consolas", 10),
                                        bg="#ffffff", fg="#333",
                                        selectbackground="#0060c0",
                                        selectforeground="white",
                                        activestyle="none",
                                        borderwidth=1, relief=tk.SOLID,
                                        highlightthickness=0,
                                        height=1)
            self._ac_list.pack(fill=tk.BOTH, expand=True)

            # Scrollbar
            sb = ttk.Scrollbar(self._ac_list, orient=tk.VERTICAL, command=self._ac_list.yview)
            self._ac_list.config(yscrollcommand=sb.set)

            self._ac_list.bind("<Double-Button-1>", lambda e: self._accept_completion())

        self._ac_list.delete(0, tk.END)
        for item in self._ac_items:
            kind, params, desc, cat = _COMPLETIONS.get(item, ("", "", "", ""))
            icons = {"fn": "\u0192", "kw": "\u2318", "const": "\u2261"}
            icon = icons.get(kind, "\u2022")
            label = f" {icon} {item}"
            if params:
                label += f"  ({params})"
            self._ac_list.insert(tk.END, label)

        # Size the listbox
        max_w = max(len(f"{it} ({_COMPLETIONS.get(it, ('','','',''))[1]})") for it in self._ac_items)
        self._ac_list.config(width=min(max_w + 4, 50), height=min(len(self._ac_items), 10))

        # Position near cursor
        try:
            bbox = self.editor.bbox(tk.INSERT)
            if bbox:
                x, y, w, h = bbox
                ex = self.editor.winfo_rootx() + x + w + 2
                ey = self.editor.winfo_rooty() + y + h + 2
                self._ac_win.geometry(f"+{ex}+{ey}")
        except Exception:
            pass

        self._ac_win.deiconify()
        self._ac_win.lift()

        # Bind keys for navigation
        self.editor.bind("<Up>", self._ac_nav_up, add="+")
        self.editor.bind("<Down>", self._ac_nav_down, add="+")
        self.editor.bind("<Return>", self._ac_accept, add="+")

    def _hide_completion(self):
        """Hide the autocomplete dropdown and detail panel."""
        if self._ac_win and self._ac_win.winfo_exists():
            self._ac_win.withdraw()
        self._hide_ac_detail()
        self._ac_items = []
        self._ac_sel = -1
        # Unbind nav keys
        try:
            self.editor.unbind("<Up>")
            self.editor.unbind("<Down>")
            self.editor.unbind("<Return>")
        except Exception:
            pass
        # Restore normal Return bindings
        self.editor.bind("<Return>", self._on_enter)
        self.editor.bind("<KeyRelease>", self._on_key_release)

    def _ac_nav_up(self, event=None):
        if not self._ac_items:
            return
        self._ac_sel = max(0, self._ac_sel - 1)
        self._ac_list.selection_clear(0, tk.END)
        self._ac_list.selection_set(self._ac_sel)
        self._ac_list.see(self._ac_sel)
        self._show_ac_tooltip(self._ac_items[self._ac_sel])
        return "break"

    def _ac_nav_down(self, event=None):
        if not self._ac_items:
            return
        self._ac_sel = min(len(self._ac_items) - 1, self._ac_sel + 1)
        self._ac_list.selection_clear(0, tk.END)
        self._ac_list.selection_set(self._ac_sel)
        self._ac_list.see(self._ac_sel)
        self._show_ac_tooltip(self._ac_items[self._ac_sel])
        return "break"

    def _ac_accept(self, event=None):
        if self._ac_sel >= 0 and self._ac_items:
            self._accept_completion()
            return "break"
        return None

    def _accept_completion(self):
        """Insert the selected completion."""
        if self._ac_sel < 0 or not self._ac_items:
            return
        name = self._ac_items[self._ac_sel]
        # Delete current prefix and insert full name
        self.editor.delete(self._ac_start_idx, tk.INSERT)
        self.editor.insert(tk.INSERT, name)
        self._hide_completion()
        # Trigger highlighting
        self._highlight_syntax()

    def _show_ac_tooltip(self, name):
        """Show detail panel next to completion list for selected item."""
        info = _FUNC_DB.get(name)
        kind, params, desc, cat = _COMPLETIONS.get(name, ("", "", "", ""))
        # Update status bar
        if info:
            self._update_status(f"{name}({params}) - {desc} [{cat}]")
        else:
            self._update_status(f"{name} [{kind}]")
        # Show/update floating detail panel
        self._update_ac_detail(name, kind, params, desc, cat)

    def _update_ac_detail(self, name, kind, params, desc, cat):
        """Show or update the autocomplete detail panel."""
        if not self._ac_win or not self._ac_win.winfo_exists():
            return
        if not self._ac_win.winfo_viewable():
            return
        # Create detail panel if needed
        if not hasattr(self, '_ac_detail') or self._ac_detail is None:
            self._ac_detail = tk.Toplevel(self.root)
            self._ac_detail.overrideredirect(True)
            self._ac_detail.attributes("-topmost", True)
            self._ac_detail_frame = tk.Frame(self._ac_detail,
                                              bg="#1e1e1e", bd=1, relief=tk.SOLID)
            self._ac_detail_frame.pack(fill=tk.BOTH, expand=True)
            self._ac_d_name = tk.Label(self._ac_detail_frame,
                                        font=("Consolas", 11, "bold"),
                                        bg="#1e1e1e", fg="#dcdcaa", anchor="w", padx=8, pady=(5, 0))
            self._ac_d_name.pack(fill=tk.X)
            self._ac_d_sig = tk.Label(self._ac_detail_frame,
                                       font=("Consolas", 10),
                                       bg="#1e1e1e", fg="#9cdcfe", anchor="w", padx=8, pady=(0, 2))
            self._ac_d_sig.pack(fill=tk.X)
            self._ac_d_sep = tk.Frame(self._ac_detail_frame, bg="#444", height=1)
            self._ac_d_sep.pack(fill=tk.X, padx=6, pady=4)
            self._ac_d_desc = tk.Label(self._ac_detail_frame,
                                        font=("Consolas", 9),
                                        bg="#1e1e1e", fg="#ce9178", anchor="w",
                                        padx=8, pady=(0, 2), wraplength=280, justify="left")
            self._ac_d_desc.pack(fill=tk.X)
            self._ac_d_cat = tk.Label(self._ac_detail_frame,
                                       font=("Consolas", 8, "bold"),
                                       bg="#1e1e1e", fg="#569cd6", anchor="w",
                                       padx=8, pady=(0, 5))
            self._ac_d_cat.pack(fill=tk.X)
        # Update content
        kind_icons = {"fn": "\u0192", "kw": "\u2318", "const": "\u2261"}
        icon = kind_icons.get(kind, "\u2022")
        self._ac_d_name.config(text=f" {icon} {name}")
        self._ac_d_sig.config(text=f" ({name} {params})" if params else f" {name}")
        self._ac_d_desc.config(text=f" {desc}" if desc else "")
        cat_colors = {
            "math": "#b5cea8", "string": "#ce9178", "io": "#9cdcfe",
            "file": "#c586c0", "dir": "#c586c0", "list": "#4ec9b0",
            "higher": "#dcdcaa", "keyword": "#569cd6", "constant": "#b5cea8",
            "compare": "#d7ba7d", "logic": "#d7ba7d", "convert": "#dcdcaa",
            "meta": "#808080", "path": "#c586c0", "error": "#f44747",
            "serial": "#dcdcaa", "time": "#9cdcfe",
            "math-lib": "#6a9955", "str-lib": "#ce9178", "io-lib": "#569cd6",
            "gui": "#d7ba7d",
        }
        color = cat_colors.get(cat, "#569cd6")
        self._ac_d_cat.config(text=f" \u25cf {cat.upper()} ", fg=color)
        # Position: right side of completion window
        self._ac_detail.update_idletasks()
        ac_x = self._ac_win.winfo_x()
        ac_y = self._ac_win.winfo_y()
        ac_w = self._ac_win.winfo_width()
        dx = ac_x + ac_w + 4
        dy = ac_y
        # Keep on screen
        sw = self.root.winfo_screenwidth()
        dw = self._ac_detail.winfo_reqwidth()
        if dx + dw > sw - 10:
            dx = ac_x - dw - 4
        self._ac_detail.geometry(f"+{dx}+{dy}")
        self._ac_detail.deiconify()
        self._ac_detail.lift()

    def _hide_ac_detail(self):
        """Hide the autocomplete detail panel."""
        if hasattr(self, '_ac_detail') and self._ac_detail and self._ac_detail.winfo_exists():
            self._ac_detail.withdraw()

    def _on_tab(self, event=None):
        """Tab key: accept completion if visible, else insert spaces."""
        if self._ac_items and self._ac_win and self._ac_win.winfo_viewable():
            if self._ac_sel < 0:
                self._ac_sel = 0
            self._accept_completion()
            return "break"
        # Insert 2 spaces
        self.editor.insert(tk.INSERT, "  ")
        return "break"

    def _trigger_completion(self, event=None):
        """Ctrl+Space: force show completion."""
        word, start_idx = self._get_current_word()
        if not word:
            # Show all completions
            self._ac_items = sorted(_COMPLETIONS.keys())[:20]
            self._ac_prefix = ""
            self._ac_start_idx = self.editor.index(tk.INSERT)
            self._ac_sel = -1
            self._show_completion()
        else:
            self._update_completion()
        return "break"

    # ---- Function Signature Tooltip ----
    def _show_function_hint(self):
        """Show function signature when ( is typed after a function name."""
        pos = self.editor.index(tk.INSERT)
        col = int(pos.split(".")[1])
        line_text = self.editor.get(f"{pos} linestart", pos)
        # Look backwards from cursor for function name before (
        before = line_text[:col-1]  # -1 for the ( we just typed
        m = re.search(r'([\w\-?!+*/=<>]+)\s*$', before)
        if not m:
            return
        fname = m.group(1)
        info = _FUNC_DB.get(fname)
        if not info:
            return
        params, desc, cat = info
        self._show_tooltip(f"({fname} {params})", f"{desc}  [{cat}]")

    def _update_arg_hint(self):
        """Show/update function signature while editing arguments inside a function call.
        Walks backward from cursor to find the innermost unclosed '(' and its function name."""
        pos = self.editor.index(tk.INSERT)
        parts = pos.split(".")
        line_no, col = int(parts[0]), int(parts[1])

        # Get text from line start to cursor (for single-line scan first)
        line_text = self.editor.get(f"{line_no}.0", pos)

        # Quick check: any '(' on current line before cursor?
        # If not, try multi-line scan (up to 20 lines back)
        text_before = ""
        start_line = max(1, line_no - 20)
        if '(' in line_text:
            text_before = line_text
        elif start_line < line_no:
            text_before = self.editor.get(f"{start_line}.0", pos)
        if not text_before:
            return

        # Walk backwards to find innermost unclosed '('
        depth = 0
        paren_pos = -1
        for i in range(len(text_before) - 1, -1, -1):
            ch = text_before[i]
            if ch == ')':
                depth += 1
            elif ch == '(':
                depth -= 1
                if depth < 0:
                    paren_pos = i
                    break
        if paren_pos < 0:
            return

        # Extract function name before the '('
        before_paren = text_before[:paren_pos].rstrip()
        m = re.search(r'([\w\-?!+*/=<>]+)\s*$', before_paren)
        if not m:
            return
        fname = m.group(1)
        info = _FUNC_DB.get(fname)
        if not info:
            return
        params, desc, cat = info
        # Count which argument we're on (for highlighting)
        after_paren = text_before[paren_pos + 1:]
        arg_idx = 0
        inner_depth = 0
        for i, ch in enumerate(after_paren):
            if ch == '(':
                inner_depth += 1
            elif ch == ')':
                inner_depth -= 1
            elif ch == ' ' and inner_depth == 0 and i > 0:
                # A space at depth 0 separates arguments
                prev = after_paren[i-1]
                if prev != ' ' and prev != '(':
                    arg_idx += 1
        # Show hint with current arg highlighted
        param_list = params.split() if params else []
        if param_list and arg_idx < len(param_list):
            highlighted = []
            for j, p in enumerate(param_list):
                if j == arg_idx:
                    highlighted.append(f"[{p}]")
                else:
                    highlighted.append(p)
            param_str = " ".join(highlighted)
        else:
            param_str = params
        self._show_tooltip(f"({fname} {param_str})", f"{desc}  [{cat}]")

    def _try_hover_or_refs(self, event):
        """Show tooltip on use-site, or references popup on definition-site."""
        if not event:
            return
        try:
            idx = self.editor.index(f"@{event.x},{event.y}")
        except Exception:
            return
        # Custom word extraction: handles hyphenated names like str-split
        line_text = self.editor.get(f"{idx} linestart", f"{idx} lineend")
        parts = idx.split(".")
        col = int(parts[1])
        if col >= len(line_text) or not line_text:
            return
        _ID_CHARS = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
                        '0123456789_-!?+*/=<>%&|^~')
        if line_text[col] not in _ID_CHARS:
            return
        start = col
        while start > 0 and line_text[start - 1] in _ID_CHARS:
            start -= 1
        end = col
        while end < len(line_text) and line_text[end] in _ID_CHARS:
            end += 1
        word = line_text[start:end].strip()
        lineno = int(idx.split(".")[0])
        # Check if hovering on a definition
        defs = self._scan_symbols().get("defs", {})
        if word in defs:
            d_line, d_col = defs[word]
            if lineno == d_line:
                # On definition site -> show references popup
                self._show_refs_popup(word, event)
                return
        # Show regular tooltip
        if word in _FUNC_DB:
            params, desc, cat = _FUNC_DB[word]
            self._show_tooltip(f"({word} {params})", f"{desc}  [{cat}]",
                               x=event.x_root + 15, y=event.y_root + 10)
        elif word in defs:
            d_line, d_col = defs[word]
            self._show_tooltip(f"{word}", f"Defined at line {d_line}",
                               x=event.x_root + 15, y=event.y_root + 10)

    def _show_tooltip(self, line1, line2="", x=None, y=None):
        """Show a floating tooltip window."""
        self._hide_tooltip()
        self._tooltip = tk.Toplevel(self.root)
        self._tooltip.overrideredirect(True)
        self._tooltip.attributes("-topmost", True)

        frame = tk.Frame(self._tooltip, bg="#2d2d2d", bd=1, relief=tk.SOLID)
        frame.pack(fill=tk.BOTH, expand=True)

        # Function signature
        lbl1 = tk.Label(frame, text=line1, font=("Consolas", 10, "bold"),
                         bg="#2d2d2d", fg="#d4d4d4", anchor="w", padx=6, pady=2)
        lbl1.pack(fill=tk.X)

        # Description
        if line2:
            lbl2 = tk.Label(frame, text=line2, font=("Consolas", 9),
                             bg="#2d2d2d", fg="#9cdcfe", anchor="w", padx=6, pady=(0, 3))
            lbl2.pack(fill=tk.X)

        # Category tag
        # Already embedded in line2

        if x is None or y is None:
            try:
                bbox = self.editor.bbox(tk.INSERT)
                if bbox:
                    bx, by, bw, bh = bbox
                    x = self.editor.winfo_rootx() + bx + bw + 4
                    y = self.editor.winfo_rooty() + by + bh + 4
            except Exception:
                x, y = 100, 100

        self._tooltip.geometry(f"+{x}+{y}")

    def _hide_tooltip(self, event=None):
        """Hide the tooltip."""
        if self._tooltip and self._tooltip.winfo_exists():
            self._tooltip.destroy()
        self._tooltip = None

    # ---- Line Numbers ----
    def _update_line_numbers(self, event=None):
        self.line_numbers.config(state=tk.NORMAL)
        self.line_numbers.delete("1.0", tk.END)
        line_count = int(self.editor.index(tk.END + "-1c").split(".")[0])
        lines = "\n".join(str(i) for i in range(1, line_count + 1))
        self.line_numbers.insert("1.0", lines)
        self.line_numbers.config(state=tk.DISABLED)
        self.line_numbers.yview_moveto(self.editor.yview()[0])

    # ---- Output Panel ----
    def _setup_output(self):
        output_frame = ttk.LabelFrame(self.root, text="Output")
        output_frame.pack(fill=tk.BOTH, expand=False, padx=2, pady=(0, 2))

        self.output = tk.Text(output_frame, font=("Consolas", 10),
                               bg="#f8f8f8", fg="#333",
                               height=10, wrap=tk.WORD,
                               state=tk.DISABLED)
        self.output.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)
        self.output.bind("<Return>", self._on_output_input)

        osb = ttk.Scrollbar(output_frame, orient=tk.VERTICAL, command=self.output.yview)
        osb.pack(side=tk.RIGHT, fill=tk.Y)
        self.output.config(yscrollcommand=osb.set)

        # Interactive input entry (hidden by default)
        input_frame = ttk.Frame(output_frame)
        self._output_input_frame = input_frame
        self._output_input = ttk.Entry(input_frame, font=("Consolas", 10))
        self._output_input.pack(fill=tk.X, padx=2, pady=2)
        self._output_input.bind("<Return>", self._send_input)
        ttk.Label(input_frame, text="\u25b6", foreground="#0a0").pack(side=tk.RIGHT, padx=2)
        self._input_hint = ttk.Label(input_frame, text="", foreground="#888",
                                      font=("Consolas", 9))
        self._input_hint.pack(fill=tk.X, padx=4)

    def _setup_statusbar(self):
        sb_frame = ttk.Frame(self.root)
        sb_frame.pack(fill=tk.X, side=tk.BOTTOM)
        self.statusbar = ttk.Label(sb_frame, text="Ready", relief=tk.SUNKEN, anchor=tk.W)
        self.statusbar.pack(fill=tk.X, side=tk.LEFT, expand=True)
        self._cursor_label = ttk.Label(sb_frame, text="  Ln 1, Col 1  ",
                                        relief=tk.SUNKEN, anchor=tk.E)
        self._cursor_label.pack(side=tk.RIGHT)

    def _update_status(self, text):
        self.statusbar.config(text=text)

    def _append_output(self, text, tag=None):
        self.output.config(state=tk.NORMAL)
        self.output.insert(tk.END, text)
        self.output.see(tk.END)
        self.output.config(state=tk.DISABLED)

    def _clear_output(self):
        self.output.config(state=tk.NORMAL)
        self.output.delete("1.0", tk.END)
        self.output.config(state=tk.DISABLED)

    # ---- Run ----
    def run_code(self):
        if not self.alb_exe:
            messagebox.showerror("Error", f"{ALB_EXE_NAME} not found!")
            return
        # Stop previous process if running
        if self._process and self._process.poll() is None:
            self._process.kill()

        if self.current_file:
            code_file = self.current_file
            self._tmp_code_file = None
        else:
            tmp = tempfile.NamedTemporaryFile(suffix=".alb", delete=False, dir=".")
            tmp.write(self.editor.get("1.0", tk.END + "-1c").encode("utf-8"))
            tmp.close()
            code_file = tmp.name
            self._tmp_code_file = code_file

        self._clear_output()
        self._append_output(f">>> Running: {os.path.basename(code_file)}\n")
        self._update_status("Running...")
        self.run_btn.config(state=tk.DISABLED)

        # Enable interactive output
        self.output.config(state=tk.NORMAL)
        self._show_input_frame(True)
        self._output_input.focus_set()

        # Prepare press temp file
        self._press_tmp = os.path.join(tempfile.gettempdir(), "alb_press.tmp")
        if os.path.exists(self._press_tmp):
            os.unlink(self._press_tmp)

        work_dir = os.path.dirname(os.path.abspath(code_file))
        env = os.environ.copy()
        env["ALB_IDE_MODE"] = "1"
        env["ALB_PRESS_TMP"] = self._press_tmp

        try:
            self._process = subprocess.Popen(
                [self.alb_exe, code_file],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=work_dir, env=env
            )
        except Exception as e:
            self._append_output(f"[ERROR] Cannot start: {e}\n")
            self._finish_run()
            return

        threading.Thread(target=self._stdout_reader, daemon=True).start()
        threading.Thread(target=self._stderr_reader, daemon=True).start()
        threading.Thread(target=self._monitor_process, daemon=True).start()

    def build_code(self, mode="normal"):
        """Build standalone executable from current ALB file.
        
        Args:
            mode: 'normal' (requires lib/ at runtime) or 'embed' (bundle libraries into exe)
        """
        if not self.alb_exe:
            messagebox.showerror("Error", f"{ALB_EXE_NAME} not found!")
            return
        
        # Need a saved file to build
        if not self.current_file:
            # Try to save first
            if not self.save_file():
                return
        
        src_file = self.current_file
        if not src_file:
            messagebox.showerror("Error", "Please save the file first!")
            return
        
        # Ask for output filename
        default_out = os.path.splitext(src_file)[0] + ".exe"
        out_file = filedialog.asksaveasfilename(
            title="Save Executable As",
            defaultextension=".exe",
            filetypes=[("Executable", "*.exe"), ("All Files", "*.*")],
            initialdir=os.path.dirname(src_file),
            initialfile=os.path.basename(default_out)
        )
        if not out_file:
            return  # User cancelled
        
        # Kill any running instance of the target exe
        exe_name = os.path.basename(out_file)
        try:
            subprocess.run(
                ["taskkill", "/f", "/im", exe_name],
                capture_output=True,
                timeout=5
            )
        except Exception:
            pass  # Process not running, ignore
        
        self._clear_output()
        self._append_output(f">>> Building: {os.path.basename(src_file)}\n")
        self._append_output(f"    Output: {out_file}\n")
        self._append_output(f"    Mode: {mode}\n")
        self._update_status("Building...")
        
        try:
            # Run ALB -b command
            cmd = [self.alb_exe, "-b", src_file, "-o", out_file]
            if mode == "embed":
                cmd.append("--embed")
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=os.path.dirname(os.path.abspath(src_file)),
                timeout=120  # 2 minute timeout
            )
            
            # Show output
            if result.stdout:
                self._append_output(result.stdout)
            if result.stderr:
                self._append_output(result.stderr)
            
            if result.returncode == 0:
                self._append_output(f"\n[SUCCESS] Build completed: {out_file}\n")
                self._update_status("Build successful")
            else:
                self._append_output(f"\n[ERROR] Build failed with code {result.returncode}\n")
                self._update_status("Build failed")
                
        except subprocess.TimeoutExpired:
            self._append_output("[ERROR] Build timed out (>120s)\n")
            self._update_status("Build timeout")
        except Exception as e:
            self._append_output(f"[ERROR] Build failed: {e}\n")
            self._update_status("Build failed")

    def compile_bytecode(self):
        """Compile current ALB file to bytecode (.albc)."""
        if not self.alb_exe:
            messagebox.showerror("Error", f"{ALB_EXE_NAME} not found!")
            return
        
        # Need a saved file
        if not self.current_file:
            if not self.save_file():
                return
        
        src_file = self.current_file
        if not src_file:
            messagebox.showerror("Error", "Please save the file first!")
            return
        
        self._clear_output()
        self._append_output(f">>> Compiling to bytecode: {os.path.basename(src_file)}\n")
        self._update_status("Compiling to bytecode...")
        
        try:
            cmd = [self.alb_exe, "compile", src_file]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=os.path.dirname(os.path.abspath(src_file)),
                timeout=60
            )
            
            if result.stdout:
                self._append_output(result.stdout)
            if result.stderr:
                self._append_output(result.stderr)
            
            if result.returncode == 0:
                albc_file = os.path.splitext(src_file)[0] + ".albc"
                self._append_output(f"\n[SUCCESS] Bytecode compiled: {albc_file}\n")
                self._update_status("Bytecode compiled")
            else:
                self._append_output(f"\n[ERROR] Compilation failed with code {result.returncode}\n")
                self._update_status("Compilation failed")
                
        except subprocess.TimeoutExpired:
            self._append_output("[ERROR] Compilation timed out (>60s)\n")
            self._update_status("Compilation timeout")
        except Exception as e:
            self._append_output(f"[ERROR] Compilation failed: {e}\n")
            self._update_status("Compilation failed")

    def package_app(self):
        """Package current ALB file into distribution zip."""
        if not self.alb_exe:
            messagebox.showerror("Error", f"{ALB_EXE_NAME} not found!")
            return
        
        # Need a saved file
        if not self.current_file:
            if not self.save_file():
                return
        
        src_file = self.current_file
        if not src_file:
            messagebox.showerror("Error", "Please save the file first!")
            return
        
        # Ask for output filename
        default_out = os.path.splitext(src_file)[0] + ".zip"
        out_file = filedialog.asksaveasfilename(
            title="Save Package As",
            defaultextension=".zip",
            filetypes=[("ZIP Archive", "*.zip"), ("All Files", "*.*")],
            initialdir=os.path.dirname(src_file),
            initialfile=os.path.basename(default_out)
        )
        if not out_file:
            return  # User cancelled
        
        self._clear_output()
        self._append_output(f">>> Packaging: {os.path.basename(src_file)}\n")
        self._append_output(f"    Output: {out_file}\n")
        self._update_status("Packaging...")
        
        try:
            cmd = [self.alb_exe, "package", src_file, "-o", out_file]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=os.path.dirname(os.path.abspath(src_file)),
                timeout=120
            )
            
            if result.stdout:
                self._append_output(result.stdout)
            if result.stderr:
                self._append_output(result.stderr)
            
            if result.returncode == 0:
                self._append_output(f"\n[SUCCESS] Package created: {out_file}\n")
                self._update_status("Package created")
            else:
                self._append_output(f"\n[ERROR] Packaging failed with code {result.returncode}\n")
                self._update_status("Packaging failed")
                
        except subprocess.TimeoutExpired:
            self._append_output("[ERROR] Packaging timed out (>120s)\n")
            self._update_status("Packaging timeout")
        except Exception as e:
            self._append_output(f"[ERROR] Packaging failed: {e}\n")
            self._update_status("Packaging failed")

    def _stdout_reader(self):
        """Read stdout line by line, display in output panel."""
        try:
            for line in iter(self._process.stdout.readline, ''):
                if not line:
                    break
                text = line if isinstance(line, str) else line.decode('utf-8', errors='replace')
                self.root.after(0, lambda t=text: self._append_output(t))
        except Exception:
            pass

    def _stderr_reader(self):
        """Read stderr, detect [INPUT_NEEDED] and [PRESS_KEY] markers."""
        buf = ""
        try:
            while True:
                chunk = self._process.stderr.read(1)
                if not chunk:
                    break
                ch = chunk if isinstance(chunk, str) else chunk.decode('utf-8', errors='replace')
                buf += ch
                if ch == '\n':
                    line = buf.strip()
                    buf = ""
                    if line == "[INPUT_NEEDED]":
                        self.root.after(0, self._on_input_needed)
                    elif line == "[PRESS_KEY]":
                        self.root.after(0, self._on_press_needed)
                    elif line:
                        self.root.after(0, lambda t=line: self._append_output(f"[ERR] {t}\n"))
        except Exception:
            pass

    def _monitor_process(self):
        """Wait for process to exit, then cleanup."""
        try:
            self._process.wait(timeout=300)
        except subprocess.TimeoutExpired:
            self._process.kill()
            self.root.after(0, lambda: self._append_output("\n[TIMEOUT] Process killed after 5 min\n"))
        finally:
            rc = self._process.returncode
            status = f"Exited with code {rc}" if rc and rc != 0 else "Finished"
            self.root.after(0, lambda: self._update_status(status))
            self.root.after(0, self._finish_run)

    def _finish_run(self):
        """Cleanup after process exits."""
        self.output.config(state=tk.DISABLED)
        self._show_input_frame(False)
        self.run_btn.config(state=tk.NORMAL)
        if self._tmp_code_file and os.path.exists(self._tmp_code_file):
            os.unlink(self._tmp_code_file)
        if self._press_tmp and os.path.exists(self._press_tmp):
            try:
                os.unlink(self._press_tmp)
            except Exception:
                pass
        self._process = None

    def _on_input_needed(self):
        """ALB called input() - show input hint and focus entry."""
        self._append_output("\n  \u2315 Waiting for input...\n")
        self._output_input.focus_set()

    def _on_press_needed(self):
        """ALB called press() - show key capture dialog."""
        if self._key_capture_active:
            return
        self._append_output("\n  \u2328 Waiting for key press...\n")
        self._show_key_capture()

    def _send_input(self, event=None):
        """Send text from input entry to ALB process stdin."""
        text = self._output_input.get()
        self._output_input.delete(0, tk.END)
        if self._process and self._process.poll() is None:
            try:
                self._process.stdin.write((text + "\n").encode("utf-8"))
                self._process.stdin.flush()
                self._append_output(f"> {text}\n")
            except Exception as e:
                self._append_output(f"[INPUT ERROR] {e}\n")
        return "break"

    def _on_output_input(self, event=None):
        """Handle Enter key in output text area."""
        return "break"  # Prevent editing

    def _show_input_frame(self, show):
        """Show or hide the interactive input frame."""
        if show:
            self._output_input_frame.pack(fill=tk.X, padx=2, pady=(0, 2))
        else:
            self._output_input_frame.pack_forget()

    def _show_key_capture(self):
        """Show modal dialog to capture a key press."""
        self._key_capture_active = True
        self._captured_key = ""
        dlg = tk.Toplevel(self.root)
        dlg.title("Press a Key")
        dlg.geometry("320x150")
        dlg.transient(self.root)
        dlg.resizable(False, False)
        dlg.attributes("-topmost", True)
        try:
            if _ICON_PATH:
                dlg.iconbitmap(default=_ICON_PATH)
        except Exception:
            pass
        # Center on parent
        dlg.update_idletasks()
        px = self.root.winfo_x() + (self.root.winfo_width() - 320) // 2
        py = self.root.winfo_y() + (self.root.winfo_height() - 150) // 2
        dlg.geometry(f"+{px}+{py}")

        frame = ttk.Frame(dlg, padding=20)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, text="\u2328  Press any key...",
                  font=("Consolas", 16, "bold")).pack()
        ttk.Label(frame, text="The key will be sent to ALB",
                  foreground="#888").pack(pady=5)

        def on_key(event):
            self._captured_key = event.keysym
            dlg.destroy()

        dlg.bind("<Key>", on_key)
        dlg.bind("<Destroy>", lambda e: None)
        dlg.protocol("WM_DELETE_WINDOW", lambda: dlg.destroy())
        dlg.grab_set()
        dlg.focus_force()
        dlg.wait_window()
        self._key_capture_active = False

        key = self._captured_key or "escape"
        # Write key to temp file for ALB to read
        if self._press_tmp:
            try:
                with open(self._press_tmp, "w") as f:
                    f.write(key + "\n")
            except Exception:
                pass
        self._append_output(f"  \u2328 Key: {key}\n")

    def _send_to_process(self, text):
        """Send text to the running ALB process stdin."""
        if self._process and self._process.poll() is None:
            try:
                self._process.stdin.write((text + "\n").encode("utf-8"))
                self._process.stdin.flush()
            except Exception:
                pass

    def run_repl(self):
        if not self.alb_exe:
            messagebox.showerror("Error", f"{ALB_EXE_NAME} not found!")
            return

        repl_win = tk.Toplevel(self.root)
        repl_win.title("ALB REPL")
        repl_win.geometry("600x400")
        try:
            if _ICON_PATH:
                repl_win.iconbitmap(default=_ICON_PATH)
        except Exception:
            pass

        text = tk.Text(repl_win, font=("Consolas", 11), bg="#f8f8f8", fg="#333")
        text.pack(fill=tk.BOTH, expand=True, padx=2, pady=2)

        entry_frame = ttk.Frame(repl_win)
        entry_frame.pack(fill=tk.X, padx=2, pady=2)

        entry = ttk.Entry(entry_frame, font=("Consolas", 11))
        entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 4))

        def send():
            code = entry.get()
            entry.delete(0, tk.END)
            text.insert(tk.END, f"alb> {code}\n")
            text.see(tk.END)
            try:
                work_dir = os.path.dirname(os.path.abspath(self.current_file)) if self.current_file else "."
                result = subprocess.run(
                    [self.alb_exe, "-e", code],
                    capture_output=True, text=True, timeout=10,
                    cwd=work_dir, encoding="utf-8"
                )
                if result.stdout:
                    text.insert(tk.END, result.stdout + "\n")
                if result.stderr:
                    text.insert(tk.END, result.stderr + "\n")
            except Exception as e:
                text.insert(tk.END, f"[ERROR] {e}\n")
            text.see(tk.END)

        ttk.Button(entry_frame, text="Run", command=send).pack(side=tk.RIGHT)
        entry.bind("<Return>", lambda e: send())

    def show_about(self):
        messagebox.showinfo("About",
            f"ALB IDE v{VERSION}\n\n"
            f"A lightweight IDE for the ALB programming language.\n\n"
            f"Interpreter: {self.alb_exe or 'Not found'}\n"
            f"Python: {sys.version.split()[0]}\n"
            f"Tkinter: {tk.TkVersion}")

    # ---- Go to Definition (Ctrl+Click) ----
    def _on_ctrl_click(self, event=None):
        """Ctrl+Click on a symbol to jump to its definition."""
        idx = self.editor.index(f"@{event.x},{event.y}")
        line_text = self.editor.get(f"{idx} linestart", f"{idx} lineend")
        parts = idx.split(".")
        col = int(parts[1])
        if col >= len(line_text) or not line_text:
            return
        _ID_CHARS = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
                        '0123456789_-!?+*/=<>%&|^~')
        if line_text[col] not in _ID_CHARS:
            return
        start = col
        while start > 0 and line_text[start - 1] in _ID_CHARS:
            start -= 1
        end = col
        while end < len(line_text) and line_text[end] in _ID_CHARS:
            end += 1
        word = line_text[start:end].strip()
        if not word:
            return
        symbols = self._scan_symbols()
        defs = symbols.get("defs", {})
        if word in defs:
            d_line, d_col = defs[word]
            self.editor.mark_set(tk.INSERT, f"{d_line}.{d_col}")
            self.editor.see(f"{d_line}.{d_col}")
            self.editor.tag_add("sel", f"{d_line}.{d_col}", f"{d_line}.{d_col + len(word)}")
            self._update_cursor_pos()

    # ---- Symbol Scanner ----
    def _scan_symbols(self):
        """Scan document for all definitions and references."""
        content = self.editor.get("1.0", tk.END + "-1c")
        lines = content.split("\n")
        defs = {}   # name -> (line, col)
        refs = {}   # name -> [(line, col), ...]
        _ID_CHARS = set('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
                        '0123456789_-!?+*/=<>%&|^~')
        for i, line in enumerate(lines, 1):
            stripped = line.lstrip()
            # Check for definitions: (let name ...) or (const name ...)
            for kw in ("let", "const", "def"):
                pat = re.compile(r'\(\s*' + kw + r'\s+([\w\-!?+*/=<>%&|^~]+)')
                m = pat.search(stripped)
                if m:
                    name = m.group(1)
                    col = line.index(name, m.start())
                    defs[name] = (i, col)
                    break
            # Check for (func (list name ...) ...) pattern
            fm = re.match(r'\s*\(func\s+\(list\s+([\w\-!?]+)', stripped)
            if fm:
                name = fm.group(1)
                col = line.index(name)
                if name not in defs:
                    defs[name] = (i, col)
            # Check for use statement: (use "module" ...)
            um = re.match(r'\s*\(use\s+"([^"]+)"', stripped)
            if um:
                mod_name = um.group(1)
                col = line.index(mod_name)
                defs[mod_name] = (i, col)
            # Collect all references (all occurrences of identifier-like words)
            for wm in re.finditer(r'[\w][\w\-!?]*', line):
                word = wm.group()
                if len(word) < 2:
                    continue
                if word not in refs:
                    refs[word] = []
                refs[word].append((i, wm.start()))
        return {"defs": defs, "refs": refs}

    # ---- Find References (hover on definition) ----
    def _show_refs_popup(self, name, event):
        """Show popup with all reference locations for a symbol."""
        self._hide_refs_popup()
        symbols = self._scan_symbols()
        all_refs = symbols.get("refs", {}).get(name, [])
        defs = symbols.get("defs", {})
        def_line = defs.get(name, (None,))[0]
        # Filter out the definition itself
        other_refs = [(ln, col) for ln, col in all_refs
                      if ln != def_line or col != defs[name][1]]
        # Also filter out string literals (simple heuristic: skip refs inside "")
        content = self.editor.get("1.0", tk.END + "-1c")
        lines = content.split("\n")
        filtered = []
        for ln, col in other_refs:
            if ln <= len(lines):
                line = lines[ln - 1]
                # Count quotes before this position
                before = line[:col]
                quotes = before.count('"') - before.count('\\"')
                if quotes % 2 == 0:  # not inside a string
                    filtered.append((ln, col))
        other_refs = filtered[:20]  # Limit to 20
        if not other_refs:
            self._show_tooltip(f"{name}", "No other references found",
                               x=event.x_root + 15, y=event.y_root + 10)
            return
        popup = tk.Toplevel(self.root)
        popup.overrideredirect(True)
        popup.attributes("-topmost", True)
        self._refs_popup = popup
        frame = tk.Frame(popup, bg="#1e1e1e", bd=1, relief=tk.SOLID)
        frame.pack(fill=tk.BOTH, expand=True)
        # Header
        hdr = tk.Label(frame, text=f" \U0001f4cd References: {name} ({len(other_refs)} uses)",
                        font=("Consolas", 10, "bold"),
                        bg="#1e1e1e", fg="#dcdcaa", anchor="w", padx=6, pady=4)
        hdr.pack(fill=tk.X)
        sep = tk.Frame(frame, bg="#444", height=1)
        sep.pack(fill=tk.X, padx=4)
        # Reference list
        list_frame = tk.Frame(frame, bg="#1e1e1e")
        list_frame.pack(fill=tk.BOTH, expand=True)
        for ln, col in other_refs:
            line = lines[ln - 1] if ln <= len(lines) else ""
            preview = line.strip()[:60]
            ref_text = f"  L{ln}: {preview}"
            lbl = tk.Label(list_frame, text=ref_text,
                           font=("Consolas", 9), bg="#1e1e1e", fg="#9cdcfe",
                           anchor="w", padx=8, pady=1, cursor="hand2")
            lbl.pack(fill=tk.X)
            lbl.bind("<Button-1>",
                     lambda e, l=ln, c=col: self._on_ref_click(l, c, len(name)))
            lbl.bind("<Enter>", lambda e, w=lbl: w.config(bg="#2d4a6a"))
            lbl.bind("<Leave>", lambda e, w=lbl: w.config(bg="#1e1e1e"))
        # Position
        popup.update_idletasks()
        pw = min(popup.winfo_reqwidth(), 500)
        ph = min(popup.winfo_reqheight(), 400)
        px = event.x_root + 15
        py = event.y_root + 10
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        if px + pw > sw - 10:
            px = sw - pw - 10
        if py + ph > sh - 10:
            py = sh - ph - 10
        popup.geometry(f"{pw}x{ph}+{px}+{py}")

    def _on_ref_click(self, line, col, name_len):
        """Navigate to a reference location."""
        self._hide_refs_popup()
        self.editor.mark_set(tk.INSERT, f"{line}.{col}")
        self.editor.see(f"{line}.{col}")
        self.editor.tag_remove("sel", "1.0", tk.END)
        self.editor.tag_add("sel", f"{line}.{col}", f"{line}.{col + name_len}")
        self._update_cursor_pos()

    def _hide_refs_popup(self, event=None):
        """Hide the references popup."""
        if self._refs_popup and self._refs_popup.winfo_exists():
            self._refs_popup.destroy()
        self._refs_popup = None

    def _on_close(self):
        """Handle window close with dirty check."""
        if self._dirty:
            ans = messagebox.askyesnocancel("Exit",
                "You have unsaved changes. Save before exit?")
            if ans is None:
                return  # Cancel
            if ans:
                self.save_file()
        self.root.destroy()


def main():
    root = tk.Tk()
    style = ttk.Style()
    style.theme_use("clam")
    style.configure("TButton", padding=2)
    style.configure("TLabelframe", background="#f0f0f0")
    app = ALBIDE(root)
    # Open files passed as command-line arguments
    import sys
    for arg in sys.argv[1:]:
        path = os.path.abspath(arg)
        if os.path.isfile(path):
            if path.lower().endswith(".albprj"):
                app._load_project(path)
            else:
                app._open_file_in_tab(path)
    root.mainloop()


if __name__ == "__main__":
    main()
