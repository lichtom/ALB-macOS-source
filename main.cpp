#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <conio.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
// POSIX (Linux / macOS)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif

using namespace std;

static bool alb_ide_mode = false;

// Portable directory scanning
struct DirEntry { string name; bool is_dir; };
static void scan_dir_entries(const string& dir, vector<DirEntry>& out) {
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    string search = dir + "\\*";
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            string n = fd.cFileName;
            if(n == "." || n == "..") continue;
            out.push_back({n, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0});
        } while(FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* d = opendir(dir.c_str());
    if(d) {
        struct dirent* ent;
        while((ent = readdir(d)) != nullptr) {
            string n = ent->d_name;
            if(n == "." || n == "..") continue;
            bool isd = (ent->d_type == DT_DIR);
            out.push_back({n, isd});
        }
        closedir(d);
    }
#endif
}


string __version__ = "0.1.1";

// ========== PATH auto-setup ==========
string get_exe_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    string full(path);
    return full.substr(0, full.find_last_of("\\/"));
#else
    char buf[4096];
#ifdef __APPLE__
    uint32_t size = sizeof(buf);
    if(_NSGetExecutablePath(buf, &size) == 0) {
        char real[PATH_MAX];
        if(realpath(buf, real)) {
            string full(real);
            return full.substr(0, full.find_last_of("/"));
        }
    }
#else
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if(len > 0) {
        buf[len] = 0;
        string full(buf);
        return full.substr(0, full.find_last_of("/"));
    }
#endif
    // Fallback: use argv[0] via /proc or current dir
    return ".";
#endif
}

bool path_contains(const string& path_env, const string& dir) {
    string lower_path = path_env, lower_dir = dir;
    for(auto& c : lower_path) c = tolower(c);
    for(auto& c : lower_dir) c = tolower(c);
    return lower_path.find(lower_dir) != string::npos;
}

void auto_add_to_path() {
#ifdef _WIN32
    string exe_dir = get_exe_dir();
    HKEY hkey;
    if(RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ|KEY_WRITE, &hkey) != ERROR_SUCCESS)
        return;
    char buf[32767];
    DWORD size = sizeof(buf);
    string cur_path;
    if(RegQueryValueExA(hkey, "Path", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS)
        cur_path = buf;
    if(path_contains(cur_path, exe_dir)) { RegCloseKey(hkey); return; }
    string new_path = cur_path;
    if(!new_path.empty() && new_path.back() != ';') new_path += ';';
    new_path += exe_dir;
    RegSetValueExA(hkey, "Path", 0, REG_EXPAND_SZ, (const BYTE*)new_path.c_str(), new_path.size()+1);
    RegCloseKey(hkey);
    DWORD_PTR dwReturnValue;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, &dwReturnValue);
    cout<<"[ALB] Added to PATH: "<<exe_dir<<"\n"
        <<"[ALB] Please restart your terminal to use 'ALB' from any directory.\n\n";
#elif defined(__APPLE__)
    // macOS: create symlink in /usr/local/bin
    string exe_dir = get_exe_dir();
    string exe_path = exe_dir + "/ALB";
    string link_path = "/usr/local/bin/ALB";
    
    // Check if already linked correctly
    char buf[4096];
    ssize_t len = readlink(link_path.c_str(), buf, sizeof(buf)-1);
    if(len > 0) {
        buf[len] = '\0';
        string current_target(buf);
        // Resolve to absolute path for comparison
        char real_current[PATH_MAX];
        if(realpath(current_target.c_str(), real_current)) {
            if(string(real_current) == exe_path) return;  // Already correctly linked
        } else if(current_target == exe_path) {
            return;  // Already correctly linked
        }
    }
    
    // Create /usr/local/bin if not exists
    struct stat st;
    if(stat("/usr/local/bin", &st) != 0) {
        system("mkdir -p /usr/local/bin 2>/dev/null");
    }
    
    // Remove old link if exists
    unlink(link_path.c_str());
    
    // Try to create symlink (may need sudo)
    if(symlink(exe_path.c_str(), link_path.c_str()) == 0) {
        cout << "[ALB] Registered command: ALB -> " << exe_path << "\n\n";
    } else {
        // Failed, try with sudo via osascript
        string cmd = "osascript -e 'do shell script \"ln -sf \\\"" + exe_path + "\\\" \\\"" + link_path + "\\\"\" with administrator privileges' 2>/dev/null";
        if(system(cmd.c_str()) == 0) {
            cout << "[ALB] Registered command: ALB -> " << exe_path << "\n\n";
        }
    }
#endif
}

// ========== File association ==========
void auto_file_assoc() {
#ifdef _WIN32
    string exe_dir=get_exe_dir();
    string ico_path=exe_dir+"\\ALB File.ico";
    string prj_ico_path=exe_dir+"\\Project File.ico";
    string exe="\""+exe_dir+"\\ALB_IDE.exe\" \"%1\"";
    HKEY k;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\Classes",0,KEY_WRITE,&k)!=ERROR_SUCCESS)
        return;
    auto ws=[&](const char*p,const char*n,const char*v){
        HKEY sk;
        if(RegCreateKeyExA(k,p,0,NULL,0,KEY_WRITE,NULL,&sk,NULL)==ERROR_SUCCESS){
            if(n&&v) RegSetValueExA(sk,n,0,REG_SZ,(const BYTE*)v,strlen(v)+1);
            else if(v) RegSetValueExA(sk,"",0,REG_SZ,(const BYTE*)v,strlen(v)+1);
            RegCloseKey(sk);
        }
    };
    // .alb files
    ws(".alb",NULL,"ALB.File");
    ws("ALB.File\\DefaultIcon",NULL,ico_path.c_str());
    ws("ALB.File\\shell\\open\\command",NULL,exe.c_str());
    // .albprj project files
    ws(".albprj",NULL,"ALB.Project");
    ws("ALB.Project\\DefaultIcon",NULL,prj_ico_path.c_str());
    ws("ALB.Project\\shell\\open\\command",NULL,exe.c_str());
    RegCloseKey(k);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
#endif
}

// ========== Cross-platform directory helpers ==========
#ifndef _WIN32
// List files in directory matching a glob pattern (simple suffix match)
static vector<string> list_dir_files(const string& dir, const string& suffix) {
    vector<string> result;
    DIR* d = opendir(dir.c_str());
    if(!d) return result;
    struct dirent* ent;
    while((ent = readdir(d)) != NULL) {
        string name = ent->d_name;
        if(name == "." || name == "..") continue;
        // Check if it's a regular file (not directory)
        string full = dir + "/" + name;
        struct stat st;
        if(stat(full.c_str(), &st) != 0) continue;
        if(S_ISREG(st.st_mode)) {
            if(suffix.empty() || (name.size() >= suffix.size() && 
                name.substr(name.size()-suffix.size()) == suffix))
                result.push_back(name);
        }
    }
    closedir(d);
    sort(result.begin(), result.end());
    return result;
}

static bool file_exists(const string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool is_directory(const string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
#endif

// ========== Value ==========
struct Val;
struct Env;
struct Func;
using ValPtr = shared_ptr<Val>;
using EnvPtr = shared_ptr<Env>;
using Fn = function<ValPtr(vector<ValPtr>, EnvPtr)>;

struct Val {
    enum Kind { NUM, STR, FN, LIST, NIL, MAP, STRUCT, VECTOR } kind;
    double n=0; string s;
    vector<ValPtr> list;
    shared_ptr<Func> fn;
    unordered_map<string,ValPtr> map;
    // Struct fields
    string struct_type;
    unordered_map<string,ValPtr> fields;
    // Vector fields
    vector<ValPtr> vec;
    bool is_sym=false; // true=bare token(symbol), false=quoted string or non-string
    Val():kind(NIL){}
    Val(double v):kind(NUM),n(v){}
    Val(const string& v):kind(STR),s(v){}
    Val(vector<ValPtr> l):kind(LIST),list(move(l)){}
    Val(unordered_map<string,ValPtr> m):kind(MAP),map(move(m)){}
    Val(Fn f, const string& name="");
    bool truthy() const {
        switch(kind){
            case NUM:return n!=0; case STR:return !s.empty();
            case NIL:return false; case MAP:return !map.empty();
            case VECTOR:return !vec.empty();
            default:return true;
        }
    }
    string str() const;
};

struct Func {
    string name;
    Fn native;
    vector<string> params;
    vector<ValPtr> body;
    EnvPtr closure;
    bool is_native=false;
    bool is_macro=false;
    string doc_str;
    int src_line=0;
    vector<string> caps;
    vector<pair<string,ValPtr>> keyword_params; // (name, default)
    string rest_param; // &rest param name
    ValPtr macro_body; // unevaluated body for macros
    bool is_struct_ctor=false;        // true if this is a struct constructor
    vector<string> struct_fields;     // field names for struct constructors
    ValPtr call(vector<ValPtr> args, EnvPtr caller_env);
};

Val::Val(Fn f, const string& name):kind(FN){
    fn=make_shared<Func>(); fn->native=f; fn->name=name;
}

string Val::str() const {
    switch(kind){
        case NUM:{ double d=n;
            if(d==floor(d)&&fabs(d)<1e15) return to_string((long long)d);
            ostringstream o; o<<d; return o.str(); }
        case STR: return s;
        case NIL: return "nil";
        case LIST:{ ostringstream o; o<<"(";
            for(size_t i=0;i<list.size();i++){
                if(i)o<<" "; o<<list[i]->str(); }
            o<<")"; return o.str(); }
        case FN: return fn ? "<fn:"+fn->name+">" : "<fn>";
        case MAP:{ ostringstream o; o<<"{";
            bool first=true;
            for(auto& kv:map){
                if(!first)o<<", "; first=false;
                o<<kv.first<<": "<<kv.second->str();
            } o<<"}"; return o.str(); }
        case STRUCT:{ ostringstream o; o<<"<"<<struct_type<<"{";
            bool first=true;
            for(auto& kv:fields){
                if(!first)o<<", "; first=false;
                o<<kv.first<<":"<<kv.second->str();
            } o<<"}>"; return o.str(); }
        case VECTOR:{ ostringstream o; o<<"#[";
            for(size_t i=0;i<vec.size();i++){
                if(i)o<<" "; o<<vec[i]->str(); }
            o<<"]"; return o.str(); }
    }
    return "?";
}

ValPtr nil_val(){ return make_shared<Val>(); }
ValPtr num_val(double v){ return make_shared<Val>(v); }
ValPtr str_val(const string& v){ return make_shared<Val>(v); }
ValPtr make_map_val(initializer_list<pair<const string,ValPtr>> il){
    return make_shared<Val>(unordered_map<string,ValPtr>(il));
}

// Deep clone for preserving unevaluated patterns
ValPtr clone_val(ValPtr v) {
    if(!v || v->kind==Val::NIL) return nil_val();
    if(v->kind==Val::NUM) return num_val(v->n);
    if(v->kind==Val::STR) {
        auto c=str_val(v->s); c->is_sym=v->is_sym; return c;
    }
    if(v->kind==Val::LIST) {
        auto c=make_shared<Val>();
        c->kind=Val::LIST;
        for(auto& el:v->list) c->list.push_back(clone_val(el));
        return c;
    }
    if(v->kind==Val::VECTOR) {
        auto c=make_shared<Val>();
        c->kind=Val::VECTOR;
        for(auto& el:v->vec) c->vec.push_back(clone_val(el));
        return c;
    }
    if(v->kind==Val::STRUCT) {
        auto c=make_shared<Val>();
        c->kind=Val::STRUCT;
        c->struct_type=v->struct_type;
        c->fields=v->fields;
        return c;
    }
    return v; // FN, MAP shared (not mutated in patterns)
}

// ========== Structured Error ==========
struct ALBError : public exception {
    string type, msg;
    ValPtr data;
    mutable string what_str;
    ALBError(const string& t, const string& m, ValPtr d=nil_val())
        :type(t),msg(m),data(d){}
    ALBError(const string& m):type("RuntimeError"),msg(m){}
    const char* what() const noexcept override {
        what_str="["+type+"] "+msg; return what_str.c_str();
    }
    ValPtr to_val() const {
        return make_map_val({{"type",str_val(type)},{"message",str_val(msg)},{"data",data}});
    }
};

// ========== Env ==========
struct Env : enable_shared_from_this<Env> {
    unordered_map<string,ValPtr> vars;
    unordered_set<string> consts;
    unordered_map<string,ValPtr> macros; // macro name -> macro fn
    unordered_set<string> exports; // exported names (empty = export all)
    EnvPtr parent;
    Env(EnvPtr p=nullptr):parent(p){}
    ValPtr get(const string& k) {
        auto it=vars.find(k);
        if(it!=vars.end()) return it->second;
        if(parent) return parent->get(k);
        throw ALBError("NameError","Undefined: "+k);
    }
    void set(const string& k, ValPtr v){
        // Walk up scope chain to find and update existing variable
        auto it=vars.find(k);
        if(it!=vars.end()){
            if(consts.count(k)) throw ALBError("ConstError","Cannot modify const: "+k);
            it->second=v; return;
        }
        if(parent) { parent->set(k,v); return; }
        // Not found anywhere - create in current scope
        vars[k]=v;
    }
    void set_local(const string& k, ValPtr v){ vars[k]=v; }
    void set_const(const string& k, ValPtr v){ vars[k]=v; consts.insert(k); }
    bool has(const string& k) {
        if(vars.count(k)) return true;
        return parent ? parent->has(k) : false;
    }
    ValPtr get_macro(const string& k) {
        auto it=macros.find(k);
        if(it!=macros.end()) return it->second;
        if(parent) return parent->get_macro(k);
        return nullptr;
    }
};

// ========== Func call ==========
struct Break{};

ValPtr eval_expr(ValPtr expr, EnvPtr env);
string resolve_module(const string& name);

// Library helpers
string find_gpp();
bool compile_cpp_dll(const string& folder, const string& name);
void load_dll_into(const string& dll_path, EnvPtr env);

// Pattern matching helpers
static bool pat_match(ValPtr val, ValPtr pat, EnvPtr env);
static void pat_bind(ValPtr val, ValPtr pat, EnvPtr env);

static bool pat_match(ValPtr val, ValPtr pat, EnvPtr env) {
    if(!pat) return !val || val->kind==Val::NIL;
    if(pat->kind==Val::STR) {
        if(pat->is_sym) return true; // symbol = wildcard/variable
        return val && val->kind==Val::STR && !val->is_sym && val->s==pat->s;
    }
    if(pat->kind==Val::NUM) return val&&val->kind==Val::NUM&&val->n==pat->n;
    if(pat->kind==Val::LIST){
        if(!val||val->kind!=Val::LIST) {
            // Check for struct pattern: (StructName field1 field2 ...)
            if(val && val->kind==Val::STRUCT && !pat->list.empty()
               && pat->list[0]->kind==Val::STR && pat->list[0]->is_sym) {
                // Check if first element is a struct type name
                auto st = env->get(pat->list[0]->s);
                if(st && st->kind==Val::FN && st->fn && st->fn->is_struct_ctor) {
                    if(val->struct_type != pat->list[0]->s) return false;
                    // Match fields
                    size_t pi = 1;
                    // Get field order from struct constructor
                    auto field_names = st->fn->struct_fields;
                    if(field_names.size() != pat->list.size()-1) return false;
                    for(size_t i=0; i<field_names.size(); i++, pi++) {
                        if(pi >= pat->list.size()) return false;
                        auto it = val->fields.find(field_names[i]);
                        if(it == val->fields.end()) return false;
                        if(!pat_match(it->second, pat->list[pi], env)) return false;
                    }
                    return true;
                }
            }
            return false;
        }
        auto& pl=pat->list; auto& vl=val->list;
        // Check if pattern starts with 'list' keyword (list structure pattern)
        size_t pstart=0;
        if(!pl.empty()&&pl[0]->kind==Val::STR&&pl[0]->is_sym&&pl[0]->s=="list")
            pstart=1;
        size_t pi=pstart;
        for(size_t vi=0;pi<pl.size();pi++,vi++){
            if(pl[pi]->kind==Val::STR&&pl[pi]->is_sym&&pl[pi]->s=="$")
                return true;
            if(vi>=vl.size()) return false;
            if(!pat_match(vl[vi],pl[pi],env)) return false;
        }
        return vl.size()<=(pl.size()-pstart);
    }
    return false;
}

static void pat_bind(ValPtr val, ValPtr pat, EnvPtr env) {
    if(!pat) return;
    if(pat->kind==Val::STR){
        if(!pat->is_sym) return; // literal string, no binding
        if(pat->s!="_") env->set_local(pat->s, val?val:nil_val());
        return;
    }
    if(pat->kind==Val::LIST && val) {
        auto& pl=pat->list;
        // Struct pattern binding
        if(val->kind==Val::STRUCT && !pl.empty()
           && pl[0]->kind==Val::STR && pl[0]->is_sym) {
            auto st = env->get(pl[0]->s);
            if(st && st->kind==Val::FN && st->fn && st->fn->is_struct_ctor) {
                auto field_names = st->fn->struct_fields;
                for(size_t i=0; i<field_names.size() && i+1<pl.size(); i++) {
                    auto it = val->fields.find(field_names[i]);
                    if(it != val->fields.end())
                        pat_bind(it->second, pl[i+1], env);
                }
                return;
            }
        }
        if(val->kind!=Val::LIST) return;
        auto& vl=val->list;
        size_t pstart=0;
        if(!pl.empty()&&pl[0]->kind==Val::STR&&pl[0]->is_sym&&pl[0]->s=="list")
            pstart=1;
        size_t pi=pstart;
        for(size_t vi=0;pi<pl.size();pi++,vi++){
            if(pl[pi]->kind==Val::STR&&pl[pi]->is_sym&&pl[pi]->s=="$"){
                if(pi+1<pl.size()&&pl[pi+1]->kind==Val::STR&&pl[pi+1]->is_sym&&pl[pi+1]->s!="_"){
                    vector<ValPtr> rest(vl.begin()+min(vi,vl.size()),vl.end());
                    env->set_local(pl[pi+1]->s,make_shared<Val>(rest));
                }
                return;
            }
            if(vi<vl.size()) pat_bind(vl[vi],pl[pi],env);
        }
    }
}

// Macro substitution: replace symbols matching param names with arg ASTs
static ValPtr macro_subst(ValPtr expr, const unordered_map<string,ValPtr>& bindings) {
    if(!expr) return nil_val();
    if(expr->kind==Val::STR && expr->is_sym) {
        auto it=bindings.find(expr->s);
        if(it!=bindings.end()) return it->second;
        return expr;
    }
    if(expr->kind==Val::LIST) {
        auto r=make_shared<Val>();
        r->kind=Val::LIST;
        // Don't substitute the head of quasiquote-related forms
        if(!expr->list.empty() && expr->list[0]->kind==Val::STR && expr->list[0]->is_sym) {
            string& h = expr->list[0]->s;
            if(h=="unquote" || h=="unquote-splicing" || h=="quasiquote" || h=="quote") {
                // Preserve the head, only subst in sub-expressions
                r->list.push_back(expr->list[0]);
                for(size_t i=1; i<expr->list.size(); i++)
                    r->list.push_back(macro_subst(expr->list[i], bindings));
                return r;
            }
        }
        for(auto& el:expr->list) r->list.push_back(macro_subst(el,bindings));
        return r;
    }
    return expr;
}

ValPtr Func::call(vector<ValPtr> args, EnvPtr caller_env) {
    if(native) return native(args, caller_env);
    // TCO: loop for direct self-recursion
    vector<ValPtr> cur_args = args;
    while(true) {
        auto local = make_shared<Env>(closure ? closure : caller_env);
        // Bind positional params
        size_t npos = params.size();
        for(size_t i=0; i<npos; i++)
            local->set_local(params[i], i<cur_args.size() ? cur_args[i] : nil_val());
        // Bind keyword params
        size_t ai = npos;
        // Only scan for :key value pairs if function has keyword parameters
        if(!keyword_params.empty()) {
            unordered_map<string,ValPtr> kw_args;
            while(ai < cur_args.size()) {
                if(cur_args[ai]->kind==Val::STR && cur_args[ai]->is_sym
                   && !cur_args[ai]->s.empty() && cur_args[ai]->s[0]==':'
                   && ai+1 < cur_args.size()) {
                    string kname = cur_args[ai]->s.substr(1);
                    kw_args[kname] = cur_args[ai+1];
                    ai += 2;
                } else {
                    break;
                }
            }
            for(auto& kp : keyword_params) {
                auto it = kw_args.find(kp.first);
                if(it != kw_args.end())
                    local->set_local(kp.first, it->second);
                else
                    local->set_local(kp.first, kp.second ? eval_expr(kp.second, local) : nil_val());
            }
        } else {
            ai = cur_args.size();  // no keyword params, all remaining go to rest
        }
        // Bind &rest param: collect args from npos to ai (args not consumed by keywords)
        if(!rest_param.empty()) {
            vector<ValPtr> rest_args;
            if(npos < cur_args.size())
                rest_args.assign(cur_args.begin()+npos, cur_args.begin()+ai);
            local->set_local(rest_param, make_shared<Val>(rest_args));
        }
        // Execute body with TCO check
        ValPtr r = nil_val();
        for(size_t bi=0; bi<body.size(); bi++) {
            if(bi == body.size()-1 && !is_macro) {
                // Helper: check if expr is a self-recursive call, return args if so
                auto is_self_call = [&](ValPtr expr) -> ValPtr {
                    if(expr->kind==Val::LIST && !expr->list.empty()
                       && expr->list[0]->kind==Val::STR
                       && expr->list[0]->is_sym && expr->list[0]->s==name)
                        return expr;
                    return nullptr;
                };
                ValPtr self_call = is_self_call(body[bi]);
                // Also check inside if-branches: (if cond then (self-rec ...) else (self-rec ...))
                if(!self_call && body[bi]->kind==Val::LIST && !body[bi]->list.empty()
                   && body[bi]->list[0]->kind==Val::STR && body[bi]->list[0]->s=="if") {
                    ValPtr cond = eval_expr(body[bi]->list[1], local);
                    if(cond->truthy()) {
                        if(body[bi]->list.size()>2) self_call = is_self_call(body[bi]->list[2]);
                        if(self_call) {
                            vector<ValPtr> new_args;
                            for(size_t j=1; j<self_call->list.size(); j++)
                                new_args.push_back(eval_expr(self_call->list[j], local));
                            cur_args = new_args;
                            goto tco_continue;
                        }
                        return body[bi]->list.size()>2 ? eval_expr(body[bi]->list[2], local) : nil_val();
                    } else {
                        if(body[bi]->list.size()>3) self_call = is_self_call(body[bi]->list[3]);
                        if(self_call) {
                            vector<ValPtr> new_args;
                            for(size_t j=1; j<self_call->list.size(); j++)
                                new_args.push_back(eval_expr(self_call->list[j], local));
                            cur_args = new_args;
                            goto tco_continue;
                        }
                        return body[bi]->list.size()>3 ? eval_expr(body[bi]->list[3], local) : nil_val();
                    }
                }
                if(self_call) {
                    vector<ValPtr> new_args;
                    for(size_t j=1; j<self_call->list.size(); j++)
                        new_args.push_back(eval_expr(self_call->list[j], local));
                    cur_args = new_args;
                    goto tco_continue;
                }
            }
            r = eval_expr(body[bi], local);
        }
        return r;
        tco_continue:;
    }
}

// ========== Parser ==========
struct Parser {
    string src; int pos, ln;
    Parser(const string& s):src(s),pos(0),ln(1){}
    char pk(){ return pos<(int)src.size()?src[pos]:'\0'; }
    char nx(){ return pos<(int)src.size()?src[pos++]:'\0'; }
    void ws(){ while(pos<(int)src.size()&&(isspace((unsigned char)src[pos])||src[pos]=='\r'||src[pos]==';')){
        if(src[pos]==';') while(pos<(int)src.size()&&src[pos]!='\n') pos++;
        if(pos<(int)src.size()&&src[pos]=='\n') ln++;
        if(pos<(int)src.size()) pos++; }}

    ValPtr parse() {
        ws();
        if(pos>=(int)src.size()) return nil_val();
        int cur_ln = ln;
        if(pk()=='(') {
            nx(); ws();
            vector<ValPtr> list;
            while(pos<(int)src.size()&&pk()!=')')
                { list.push_back(parse()); ws(); }
            if(pk()==')') nx();
            auto v=make_shared<Val>(list);
            return v;
        }
        if(pk()=='"') {
            nx(); string s;
            while(pos<(int)src.size()&&pk()!='"') {
                if(pk()=='\\'){nx();char c=nx();
                    s+=(c=='n'?'\n':c=='t'?'\t':c=='\\'?'\\':c=='"'?'"':c);}
                else { if(pk()=='\n')ln++; s+=nx(); }
            }
            if(pk()=='"') nx();
            return str_val(s);
        }
        // Backtick: context-aware
        // `( ... )` = quasiquote (for macros), `string ${expr}` = string interpolation
        if(pk()=='`') {
            // Peek ahead: if next non-space char is '(', treat as quasiquote
            nx(); // skip `
            int after_backtick = pos;
            // Skip whitespace
            while(pos<(int)src.size() && (pk()==' ' || pk()=='\t')) nx();
            if(pk()=='(') {
                // Quasiquote mode: `(expr) -> (quasiquote expr)
                pos = after_backtick; // restore to just after `
                // Skip whitespace again
                while(pos<(int)src.size() && (pk()==' ' || pk()=='\t')) nx();
                auto v = parse(); // parse the S-expression
                vector<ValPtr> qq={str_val("quasiquote"),v};
                return make_shared<Val>(qq);
            }
            // String interpolation mode: restore and re-parse
            pos = after_backtick;
            string s;
            vector<ValPtr> parts; // for (str ... ) construction
            bool has_interp = false;
            while(pos<(int)src.size()&&pk()!='`') {
                if(pk()=='$' && pos+1<(int)src.size() && src[pos+1]=='{') {
                    // Interpolation: ${expr}
                    has_interp = true;
                    if(!s.empty()) {
                        parts.push_back(str_val(s));
                        s.clear();
                    }
                    nx(); nx(); // skip ${
                    // Parse expression until matching }
                    string expr_str;
                    int depth = 1;
                    while(pos<(int)src.size() && depth>0) {
                        if(pk()=='{') depth++;
                        else if(pk()=='}') { depth--; if(depth==0) { nx(); break; } }
                        expr_str += nx();
                    }
                    // Parse the expression
                    Parser sub(expr_str);
                    auto expr = sub.parse();
                    parts.push_back(expr);
                } else {
                    if(pk()=='\n') ln++;
                    s += nx();
                }
            }
            if(pk()=='`') nx();
            if(!has_interp) {
                return str_val(s);
            }
            // Build (str part1 part2 ...) expression
            if(!s.empty()) parts.push_back(str_val(s));
            vector<ValPtr> str_call;
            str_call.push_back(str_val("str")); str_call[0]->is_sym=true;
            for(auto& p:parts) str_call.push_back(p);
            return make_shared<Val>(str_call);
        }
        // Heredoc: <<DELIM ... DELIM
        if(pk()=='<' && pos+1<(int)src.size() && src[pos+1]=='<') {
            nx(); nx(); // skip <<
            // Read delimiter
            string delim;
            while(pos<(int)src.size() && !isspace((unsigned char)pk()))
                delim += nx();
            // Skip newline
            if(pk()=='\r') nx();
            if(pk()=='\n') { ln++; nx(); }
            // Read content until delimiter on its own line
            string content;
            while(pos<(int)src.size()) {
                // Check if current line starts with delimiter
                string line;
                while(pos<(int)src.size() && pk()!='\n') {
                    line += nx();
                }
                if(pk()=='\n') { ln++; nx(); }
                // Trim and check if line is the delimiter
                string trimmed = line;
                while(!trimmed.empty() && isspace((unsigned char)trimmed.back()))
                    trimmed.pop_back();
                if(trimmed == delim) break;
                if(!content.empty()) content += "\n";
                content += line;
            }
            return str_val(content);
        }
        if(pk()=='\'') {
            nx(); auto v=parse();
            vector<ValPtr> q={str_val("quote"),v};
            return make_shared<Val>(q);
        }
        // Comma for unquote: ,expr -> (unquote expr), ,@expr -> (unquote-splicing expr)
        if(pk()==',') {
            nx();
            if(pk()=='@') {
                nx();
                auto v=parse();
                vector<ValPtr> uq={str_val("unquote-splicing"),v};
                return make_shared<Val>(uq);
            }
            auto v=parse();
            vector<ValPtr> uq={str_val("unquote"),v};
            return make_shared<Val>(uq);
        }
        string tok;
        while(pos<(int)src.size()&&!isspace((unsigned char)pk())&&pk()!='('&&pk()!=')'&&pk()!='\r'&&pk()!=',')
            tok+=nx();
        if(tok.empty()) return nil_val();
        try { size_t e; double d=stod(tok,&e);
            if(e==tok.size()) return num_val(d); } catch(...){}
        auto v=str_val(tok); v->is_sym=true; return v;
    }
    vector<ValPtr> parse_all() {
        vector<ValPtr> exprs;
        while(true) {
            ws();
            if(pos>=(int)src.size()) break;
            exprs.push_back(parse());
        }
        return exprs;
    }
};

// ========== Built-in functions ==========
void register_builtins(EnvPtr env);
ValPtr load_file(const string& path, EnvPtr env);

// ========== Evaluator ==========
ValPtr eval_expr(ValPtr expr, EnvPtr env) {
    if(!expr || expr->kind==Val::NIL) return nil_val();
    if(expr->kind==Val::NUM) return expr;
    if(expr->kind==Val::STR) {
        if(expr->is_sym) {
            if(env->has(expr->s)) return env->get(expr->s);
        }
        return expr;
    }
    if(expr->kind!=Val::LIST || expr->list.empty()) return expr;

    auto& items = expr->list;
    string head;
    if(items[0]->kind==Val::STR) head=items[0]->s;

    // --- Special forms ---
    if(head=="let") {
        string name=items[1]->s;
        
        // Check for arrow syntax: (let name (list a b) => (body))
        if(items.size()>=5 && items[3]->kind==Val::STR 
           && items[3]->is_sym && items[3]->s=="=>") {
            // Create function from arrow syntax
            auto f=make_shared<Func>();
            f->name=name;
            // Extract params from (list a b) or (a b)
            if(items[2]->kind==Val::LIST) {
                auto& plist = items[2]->list;
                size_t start = 0;
                if(!plist.empty() && plist[0]->kind==Val::STR 
                   && plist[0]->is_sym && plist[0]->s=="list")
                    start = 1;
                for(size_t i=start;i<plist.size();i++)
                    f->params.push_back(plist[i]->s);
            }
            // Body: items[4]
            ValPtr body_expr=items[4];
            if(body_expr->kind==Val::LIST) {
                bool all_lists=true;
                for(auto& el:body_expr->list)
                    if(el->kind!=Val::LIST){all_lists=false;break;}
                if(all_lists && !body_expr->list.empty())
                    f->body=body_expr->list;
                else
                    f->body={body_expr};
            } else {
                f->body={body_expr};
            }
            // Docstring extraction
            if(f->body.size()>=2 && f->body[0]->kind==Val::STR
               && !f->body[0]->is_sym
               && f->body[1]->kind==Val::LIST
               && !env->has(f->body[0]->s)) {
                f->doc_str=f->body[0]->s;
                f->body.erase(f->body.begin());
            }
            f->closure=env;
            auto v=make_shared<Val>();
            v->kind=Val::FN; v->fn=f;
            env->set_local(name, v);
            return v;
        }
        
        ValPtr val = items.size()>2 ? eval_expr(items[2],env) : nil_val();
        env->set_local(name, val);
        return val;
    }
    // define: top-level definition (alias for let but semantically clearer)
    // (define name value) or (define (name params...) body)
    if(head=="define") {
        if(items[1]->kind==Val::LIST) {
            // (define (name params...) body) -> (let name (func (list params...) body))
            auto& sig = items[1]->list;
            if(sig.empty()) throw ALBError("SyntaxError","define: empty signature");
            string name = sig[0]->s;
            auto f=make_shared<Func>();
            f->name=name;
            for(size_t i=1;i<sig.size();i++)
                f->params.push_back(sig[i]->s);
            // Body: items[2..]
            for(size_t i=2;i<items.size();i++)
                f->body.push_back(items[i]);
            // Docstring extraction
            if(f->body.size()>=2 && f->body[0]->kind==Val::STR
               && !f->body[0]->is_sym
               && f->body[1]->kind==Val::LIST
               && !env->has(f->body[0]->s)) {
                f->doc_str=f->body[0]->s;
                f->body.erase(f->body.begin());
            }
            f->closure=env;
            auto v=make_shared<Val>();
            v->kind=Val::FN; v->fn=f;
            env->set_local(name, v);
            return v;
        } else {
            // (define name value)
            string name=items[1]->s;
            ValPtr val = items.size()>2 ? eval_expr(items[2],env) : nil_val();
            env->set_local(name, val);
            return val;
        }
    }
    // let*: sequential bindings (later ones can reference earlier ones)
    // (let* ((x 1) (y (+ x 1))) body...)
    if(head=="let*") {
        auto local=make_shared<Env>(env);
        auto& bindings=items[1]->list;
        for(auto& b:bindings) {
            if(b->kind!=Val::LIST || b->list.size()<2)
                throw ALBError("SyntaxError","let*: expected (name value) pair");
            string name=b->list[0]->s;
            ValPtr val=eval_expr(b->list[1],local);
            local->set_local(name,val);
        }
        ValPtr r=nil_val();
        for(size_t i=2;i<items.size();i++)
            r=eval_expr(items[i],local);
        return r;
    }
    // letrec: recursive bindings (variables visible in their own definitions)
    // (letrec ((even? (func ...)) (odd? (func ...))) body...)
    if(head=="letrec") {
        auto local=make_shared<Env>(env);
        auto& bindings=items[1]->list;
        // First pass: bind all names to nil
        for(auto& b:bindings) {
            if(b->kind!=Val::LIST || b->list.size()<2)
                throw ALBError("SyntaxError","letrec: expected (name value) pair");
            local->set_local(b->list[0]->s, nil_val());
        }
        // Second pass: evaluate and bind
        for(auto& b:bindings) {
            string name=b->list[0]->s;
            ValPtr val=eval_expr(b->list[1],local);
            local->set_local(name,val);
        }
        ValPtr r=nil_val();
        for(size_t i=2;i<items.size();i++)
            r=eval_expr(items[i],local);
        return r;
    }
    // cond: multi-way conditional
    // (cond (test1 expr1) (test2 expr2) ... (else exprN))
    if(head=="cond") {
        for(size_t i=1;i<items.size();i++) {
            auto& clause=items[i];
            if(clause->kind!=Val::LIST || clause->list.empty())
                throw ALBError("SyntaxError","cond: expected (test expr) clause");
            auto& test=clause->list[0];
            // Check for 'else' keyword
            if(test->kind==Val::STR && test->is_sym && test->s=="else") {
                ValPtr r=nil_val();
                for(size_t j=1;j<clause->list.size();j++)
                    r=eval_expr(clause->list[j],env);
                return r;
            }
            ValPtr tv=eval_expr(test,env);
            if(tv->truthy()) {
                ValPtr r=nil_val();
                for(size_t j=1;j<clause->list.size();j++)
                    r=eval_expr(clause->list[j],env);
                return r;
            }
        }
        return nil_val();
    }
    if(head=="set") {
        string name=items[1]->s;
        ValPtr val=eval_expr(items[2],env);
        env->set(name,val);
        return val;
    }
    if(head=="+=" || head=="-=" || head=="*=" || head=="/=") {
        string name=items[1]->s;
        ValPtr cur=env->get(name);
        ValPtr rhs=eval_expr(items[2],env);
        double result;
        if(head=="+=") {
            if(cur->kind==Val::STR || rhs->kind==Val::STR) {
                ValPtr v=str_val(cur->str()+rhs->str());
                env->set(name,v); return v;
            }
            result=cur->n+rhs->n;
        } else if(head=="-=") result=cur->n-rhs->n;
        else if(head=="*=") result=cur->n*rhs->n;
        else {
            if(rhs->n==0) throw ALBError("ArithmeticError","Division by zero in /=");
            result=cur->n/rhs->n;
        }
        ValPtr v=num_val(result);
        env->set(name,v); return v;
    }
    if(head=="func") {
        auto f=make_shared<Func>();
        int pi=1;
        
        // Check for named func syntax: (func name a b (body))
        // If first arg is symbol and second is symbol (not capability tag), it's named func
        if(pi<(int)items.size() && items[pi]->kind==Val::STR && items[pi]->is_sym
           && pi+1<(int)items.size() && items[pi+1]->kind==Val::STR 
           && items[pi+1]->is_sym && !items[pi+1]->s.empty() 
           && items[pi+1]->s[0]!=':') {
            f->name=items[pi]->s;
            pi++;
            // Extract params: all symbols until we hit a list (body)
            while(pi<(int)items.size() && items[pi]->kind==Val::STR && items[pi]->is_sym) {
                f->params.push_back(items[pi]->s);
                pi++;
            }
            // Body: everything from pi onwards
            if(pi<(int)items.size()) {
                if(pi+1>=(int)items.size() && items[pi]->kind==Val::LIST) {
                    bool all_lists=true;
                    for(auto& el:items[pi]->list)
                        if(el->kind!=Val::LIST){all_lists=false;break;}
                    if(all_lists && !items[pi]->list.empty())
                        f->body=items[pi]->list;
                    else
                        f->body={items[pi]};
                } else {
                    f->body.assign(items.begin()+pi, items.end());
                }
            }
            // Docstring extraction
            if(f->body.size()>=2 && f->body[0]->kind==Val::STR
               && !f->body[0]->is_sym
               && f->body[1]->kind==Val::LIST
               && !env->has(f->body[0]->s)) {
                f->doc_str=f->body[0]->s;
                f->body.erase(f->body.begin());
            }
            f->closure=env;
            auto v=make_shared<Val>();
            v->kind=Val::FN; v->fn=f;
            return v;
        }
        
        // Capability tags: :pure, :io, :sys, :ffi
        while(pi<(int)items.size()&&items[pi]->kind==Val::STR
              &&!items[pi]->s.empty()&&items[pi]->s[0]==':') {
            f->caps.push_back(items[pi]->s.substr(1));
            pi++;
        }
        // Params: (list x y z) or (x y z)
        if(pi<(int)items.size() && items[pi]->kind==Val::LIST) {
            auto& plist = items[pi]->list;
            // Skip 'list' keyword if present at index 0
            size_t start = 0;
            if(!plist.empty() && plist[0]->kind==Val::STR 
               && plist[0]->is_sym && plist[0]->s=="list")
                start = 1;
            bool seen_keyword=false;
            for(size_t i=start;i<plist.size();i++) {
                string ps = plist[i]->s;
                if(ps=="&rest" && i+1<plist.size()) {
                    f->rest_param=plist[++i]->s;
                    break;
                }
                if(!ps.empty() && ps[0]==':' && !seen_keyword) {
                    seen_keyword=true;
                }
                if(seen_keyword && !ps.empty() && ps[0]==':') {
                    // Keyword param: :name default_value
                    string kname=ps.substr(1);
                    ValPtr def_val=nil_val();
                    if(i+1<plist.size()) {
                        def_val=plist[++i]; // store unevaluated
                    }
                    f->keyword_params.push_back({kname, def_val});
                } else {
                    f->params.push_back(ps);
                }
            }
            pi++;
        }
        // Body: everything from pi onwards
        if(pi<(int)items.size()) {
            f->body.assign(items.begin()+pi, items.end());
        }
        // Docstring extraction: (func (params) "doc string" (body...))
        if(f->body.size()>=2 && f->body[0]->kind==Val::STR
           && !f->body[0]->is_sym
           && f->body[1]->kind==Val::LIST
           && !env->has(f->body[0]->s)) {
            f->doc_str=f->body[0]->s;
            f->body.erase(f->body.begin());
        }
        f->closure=env;
        auto v=make_shared<Val>();
        v->kind=Val::FN; v->fn=f;
        return v;
    }
    if(head=="const") {
        string name=items[1]->s;
        ValPtr val=items.size()>2 ? eval_expr(items[2],env) : nil_val();
        env->set_const(name, val);
        return val;
    }
    if(head=="if") {
        ValPtr cond=eval_expr(items[1],env);
        return cond->truthy()
            ? (items.size()>2 ? eval_expr(items[2],env) : nil_val())
            : (items.size()>3 ? eval_expr(items[3],env) : nil_val());
    }
    if(head=="when") {
        if(eval_expr(items[1],env)->truthy()) {
            ValPtr r=nil_val();
            for(size_t i=2;i<items.size();i++) r=eval_expr(items[i],env);
            return r;
        }
        return nil_val();
    }
    if(head=="while") {
        ValPtr r=nil_val();
        try {
        while(eval_expr(items[1],env)->truthy())
            try { for(size_t i=2;i<items.size();i++) r=eval_expr(items[i],env); }
            catch(Break&){ break; }
        } catch(Break&){}
        return r;
    }
    // for-i: C-style indexed loop
    // (for-i (var init cond step) body...)
    if(head=="for-i") {
        if(items.size()<2 || items[1]->kind!=Val::LIST || items[1]->list.size()<4)
            throw ALBError("SyntaxError","for-i: expected (var init cond step)");
        auto& spec = items[1]->list;
        string var = spec[0]->s;
        auto local = make_shared<Env>(env);
        ValPtr init_val = eval_expr(spec[1], env);
        local->set_local(var, init_val);
        ValPtr r = nil_val();
        try {
            while(eval_expr(spec[2], local)->truthy()) {
                try {
                    for(size_t i=2; i<items.size(); i++)
                        r = eval_expr(items[i], local);
                    eval_expr(spec[3], local); // step
                } catch(Break&){ break; }
            }
        } catch(Break&){}
        return r;
    }
    if(head=="do") {
        ValPtr r=nil_val();
        for(size_t i=1;i<items.size();i++) r=eval_expr(items[i],env);
        return r;
    }
    if(head=="def") {
        return eval_expr(make_shared<Val>(vector<ValPtr>{str_val("let"),items[1],items[2]}),env);
    }
    if(head=="try") {
        ValPtr try_body=items[1];
        ValPtr catch_clause=items.size()>2?items[2]:nullptr;
        ValPtr finally_clause=items.size()>3?items[3]:nullptr;
        ValPtr r=nil_val();
        try {
            r=eval_expr(try_body,env);
        } catch(ALBError& e) {
            if(catch_clause&&catch_clause->kind==Val::LIST
               &&catch_clause->list.size()>=3
               &&catch_clause->list[0]->kind==Val::STR
               &&catch_clause->list[0]->s=="catch"){
                auto local=make_shared<Env>(env);
                local->set_local(catch_clause->list[1]->s, e.to_val());
                for(size_t i=2;i<catch_clause->list.size();i++)
                    r=eval_expr(catch_clause->list[i],local);
            } else throw;
        } catch(exception& e) {
            if(catch_clause&&catch_clause->kind==Val::LIST
               &&catch_clause->list.size()>=3
               &&catch_clause->list[0]->kind==Val::STR
               &&catch_clause->list[0]->s=="catch"){
                auto local=make_shared<Env>(env);
                local->set_local(catch_clause->list[1]->s,
                    ALBError("RuntimeError",e.what()).to_val());
                for(size_t i=2;i<catch_clause->list.size();i++)
                    r=eval_expr(catch_clause->list[i],local);
            } else throw;
        }
        // Handle (finally body...) clause
        if(finally_clause && finally_clause->kind==Val::LIST
           && !finally_clause->list.empty()
           && finally_clause->list[0]->kind==Val::STR
           && finally_clause->list[0]->s=="finally") {
            for(size_t i=1;i<finally_clause->list.size();i++)
                eval_expr(finally_clause->list[i],env);
        } else if(finally_clause) {
            eval_expr(finally_clause,env);
        }
        return r;
    }
    if(head=="match") {
        ValPtr val=eval_expr(items[1],env);
        for(size_t i=2;i+1<items.size();i+=2){
            ValPtr pat=clone_val(items[i]); // clone before evaluation corrupts pattern
            auto local=make_shared<Env>(env);
            if(pat_match(val,pat,local)){
                pat_bind(val,pat,local);
                return eval_expr(items[i+1],local);
            }
        }
        throw ALBError("MatchError","No matching pattern for: "+val->str());
    }
    if(head=="use") {
        string name=eval_expr(items[1],env)->s;
        string path=name;
        { ifstream test(path); if(!test.good()) path=resolve_module(name); }
        if(path.empty()) throw ALBError("ModuleError","Module not found: "+name);

        auto mod_env=make_shared<Env>(env);

        if(path.size()>7 && path.substr(0,7)=="folder:") {
            // ===== Folder-based library =====
            string folder = path.substr(7);
            string init_alb = folder + "/__init__.alb";
            string dll_path = folder + "/" + name + ".dll";

            // Try auto-compile .cpp → .dll
            compile_cpp_dll(folder, name);

            // Load DLL if exists
            { ifstream test(dll_path); if(test.good()) load_dll_into(dll_path, mod_env); }

            // Load all .alb submodules (except __init__.alb and cli.alb)
            vector<string> sub_albs;
#ifdef _WIN32
            WIN32_FIND_DATAA fd;
            string search = folder + "\\*.alb";
            HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
            if(hFind != INVALID_HANDLE_VALUE) {
                do {
                    string fn = fd.cFileName;
                    if(fn != "__init__.alb" && fn != "cli.alb"
                       && !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        sub_albs.push_back(folder + "/" + fn);
                } while(FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }
#else
            vector<string> alb_files = list_dir_files(folder, ".alb");
            for(auto& fn : alb_files) {
                if(fn != "__init__.alb" && fn != "cli.alb")
                    sub_albs.push_back(folder + "/" + fn);
            }
#endif
            sort(sub_albs.begin(), sub_albs.end());
            for(auto& sf : sub_albs) load_file(sf, mod_env);

            // Load __init__.alb last (aggregates all submodules)
            { ifstream test(init_alb); if(test.good()) load_file(init_alb, mod_env); }
        } else if(path.size()>=4 && path.substr(path.size()-4)==".dll") {
            // ===== Standalone DLL =====
            load_dll_into(path, mod_env);
        } else {
            // ===== Single .alb file =====
            load_file(path, mod_env);
        }

        string prefix; vector<string> only;
        for(size_t i=2;i<items.size();i++){
            if(items[i]->kind==Val::STR&&items[i]->s==":as"&&i+1<items.size())
                { prefix=items[++i]->s; if(!prefix.empty()) prefix+="."; }
            else if(items[i]->kind==Val::STR&&items[i]->s==":only"&&i+1<items.size()){
                i++;
                if(items[i]->kind==Val::LIST)
                    for(auto& n:items[i]->list) only.push_back(n->s);
            }
        }
        for(auto& kv:mod_env->vars){
            // Respect module exports (if non-empty, only export listed names)
            if(!mod_env->exports.empty() && !mod_env->exports.count(kv.first)) continue;
            if(!only.empty()){
                bool found=false;
                for(auto& o:only) if(o==kv.first){found=true;break;}
                if(!found) continue;
            }
            env->set(prefix+kv.first,kv.second);
        }
        return nil_val();
    }
    if(head=="quote") {
        return items.size()>1 ? items[1] : nil_val();
    }
    // --- quasiquote: `(a ,b c) -> list with unquotes resolved ---
    if(head=="quasiquote") {
        if(items.size()<2) return nil_val();
        // Recursive helper to expand quasiquote template
        // Strategy: ,symbol first tries env lookup (macro param substitution),
        //           then falls back to evaluation (for computed values)
        function<ValPtr(ValPtr)> expand = [&](ValPtr tmpl) -> ValPtr {
            if(!tmpl) return nil_val();
            if(tmpl->kind!=Val::LIST) return tmpl; // atoms stay as-is
            if(tmpl->list.empty()) return tmpl; // empty list stays
            // Check for unquote: (unquote expr)
            if(tmpl->list[0]->kind==Val::STR && tmpl->list[0]->s=="unquote") {
                if(tmpl->list.size()<2) return nil_val();
                auto& inner = tmpl->list[1];
                // Simple symbol: look up in env (handles macro param substitution)
                if(inner->kind==Val::STR && inner->is_sym) {
                    if(env->has(inner->s)) {
                        ValPtr val = env->get(inner->s);
                        return val;
                    }
                    return inner; // not found → keep as symbol
                }
                // Number: return as-is
                if(inner->kind==Val::NUM) return inner;
                // List or other: keep as-is (don't evaluate — it's a template)
                return inner;
            }
            // Process list elements, handling unquote-splicing
            vector<ValPtr> result;
            for(size_t i=0; i<tmpl->list.size(); i++) {
                auto elem = tmpl->list[i];
                if(elem->kind==Val::LIST && !elem->list.empty()
                   && elem->list[0]->kind==Val::STR
                   && elem->list[0]->s=="unquote-splicing") {
                    // ,@expr - splice list
                    if(elem->list.size()<2) continue;
                    ValPtr spliced = nullptr;
                    auto& inner = elem->list[1];
                    if(inner->kind==Val::STR && inner->is_sym && env->has(inner->s)) {
                        spliced = env->get(inner->s);
                    }
                    if(!spliced) spliced = inner;
                    if(spliced->kind==Val::LIST) {
                        for(auto& s : spliced->list) result.push_back(s);
                    } else if(spliced->kind==Val::VECTOR) {
                        for(auto& s : spliced->vec) result.push_back(s);
                    }
                } else {
                    result.push_back(expand(elem));
                }
            }
            return make_shared<Val>(result);
        };
        return expand(items[1]);
    }
    if(head=="list") {
        vector<ValPtr> r;
        for(size_t i=1;i<items.size();i++) r.push_back(eval_expr(items[i],env));
        return make_shared<Val>(r);
    }
    // --- defmacro ---
    if(head=="defmacro") {
        string name=items[1]->s;
        auto f=make_shared<Func>();
        f->name=name; f->is_macro=true;
        // Extract params
        if(items[2]->kind==Val::LIST) {
            auto& plist=items[2]->list;
            size_t start=0;
            if(!plist.empty()&&plist[0]->kind==Val::STR&&plist[0]->is_sym&&plist[0]->s=="list")
                start=1;
            for(size_t i=start;i<plist.size();i++)
                f->params.push_back(plist[i]->s);
        }
        // Store body as unevaluated template
        if(items.size()>3) {
            vector<ValPtr> body_exprs(items.begin()+3, items.end());
            if(body_exprs.size()==1) f->macro_body=body_exprs[0];
            else {
                vector<ValPtr> do_body;
                do_body.push_back(str_val("do"));
                do_body.insert(do_body.end(), body_exprs.begin(), body_exprs.end());
                f->macro_body=make_shared<Val>(do_body);
            }
        } else {
            f->macro_body=nil_val();
        }
        // Create wrapper fn for storage
        auto v=make_shared<Val>();
        v->kind=Val::FN; v->fn=f;
        env->macros[name]=v;
        return nil_val();
    }
    // --- export ---
    if(head=="export") {
        for(size_t i=1;i<items.size();i++) {
            if(items[i]->kind==Val::STR)
                env->exports.insert(items[i]->s);
        }
        return nil_val();
    }
    // --- struct ---
    if(head=="struct") {
        string name=items[1]->s;
        vector<string> field_names;
        if(items[2]->kind==Val::LIST) {
            for(auto& f:items[2]->list) {
                if(f->kind==Val::STR) field_names.push_back(f->s);
            }
        }
        // Register constructor
        auto ctor=[name,field_names](vector<ValPtr> a, EnvPtr)->ValPtr{
            auto v=make_shared<Val>();
            v->kind=Val::STRUCT;
            v->struct_type=name;
            for(size_t i=0;i<field_names.size();i++)
                v->fields[field_names[i]]=i<a.size()?a[i]:nil_val();
            return v;
        };
        auto cv=make_shared<Val>();
        cv->kind=Val::FN;
        cv->fn=make_shared<Func>();
        cv->fn->name=name; cv->fn->native=ctor; cv->fn->is_native=true;
        cv->fn->is_struct_ctor=true;   // mark as struct constructor
        cv->fn->struct_fields=field_names; // store field order for pattern matching
        env->set_local(name,cv);
        // Register type predicate
        string pred=name+"?";
        auto pred_fn=[name](vector<ValPtr> a, EnvPtr)->ValPtr{
            return num_val(!a.empty()&&a[0]->kind==Val::STRUCT&&a[0]->struct_type==name?1:0);
        };
        auto pv=make_shared<Val>();
        pv->kind=Val::FN;
        pv->fn=make_shared<Func>();
        pv->fn->name=pred; pv->fn->native=pred_fn; pv->fn->is_native=true;
        env->set_local(pred,pv);
        // Register field accessors: Type-field
        for(auto& fname:field_names){
            string acc=name+"-"+fname;
            auto acc_fn=[name,fname](vector<ValPtr> a, EnvPtr)->ValPtr{
                if(a.empty()||a[0]->kind!=Val::STRUCT||a[0]->struct_type!=name)
                    throw ALBError("TypeError",name+"-"+fname+": expected "+name+" struct");
                auto it=a[0]->fields.find(fname);
                return it!=a[0]->fields.end()?it->second:nil_val();
            };
            auto av=make_shared<Val>();
            av->kind=Val::FN;
            av->fn=make_shared<Func>();
            av->fn->name=acc; av->fn->native=acc_fn; av->fn->is_native=true;
            env->set_local(acc,av);
        }
        return nil_val();
    }

    // --- Macro expansion ---
    if(items[0]->kind==Val::STR && items[0]->is_sym) {
        ValPtr macro_fn = env->get_macro(head);
        if(macro_fn && macro_fn->kind==Val::FN && macro_fn->fn->is_macro) {
            auto mf = macro_fn->fn;
            // Bind params to unevaluated args
            unordered_map<string,ValPtr> bindings;
            vector<ValPtr> uneval_args;
            for(size_t i=1;i<items.size();i++) uneval_args.push_back(items[i]);
            for(size_t i=0;i<mf->params.size();i++)
                bindings[mf->params[i]]=i<uneval_args.size()?uneval_args[i]:nil_val();
            // Substitute in macro body
            ValPtr expanded = macro_subst(mf->macro_body, bindings);
            // Evaluate expanded code (handles quasiquote)
            ValPtr result = eval_expr(expanded, env);
            // If result is a list, evaluate it again as code (macro generates code)
            if(result->kind == Val::LIST && !result->list.empty()) {
                return eval_expr(result, env);
            }
            return result;
        }
    }

    // --- Function call ---
    ValPtr fn_val = eval_expr(items[0], env);
    if(!fn_val || fn_val->kind!=Val::FN)
        throw ALBError("TypeError","Not callable: "+(fn_val?fn_val->str():"nil"));
    vector<ValPtr> args;
    for(size_t i=1;i<items.size();i++)
        args.push_back(eval_expr(items[i],env));
    return fn_val->fn->call(args, env);
}

// ========== Module loader ==========
ValPtr load_file(const string& path, EnvPtr env) {
    ifstream f(path, ios::binary);
    if(!f) throw ALBError("ModuleError","Cannot open module: "+path);
    ostringstream ss; ss<<f.rdbuf();
    string content = ss.str();
    // Strip \r characters (handles CRLF files)
    content.erase(remove(content.begin(),content.end(),'\r'),content.end());
    Parser p(content);
    auto exprs=p.parse_all();
    ValPtr r=nil_val();
    for(auto& e:exprs) r=eval_expr(e,env);
    return r;
}

// ========== Built-ins ==========
void register_builtins(EnvPtr env) {
    auto reg=[&](const string& name, Fn f){
        auto v=make_shared<Val>();
        v->kind=Val::FN;
        v->fn=make_shared<Func>();
        v->fn->name=name; v->fn->native=f; v->fn->is_native=true;
        env->set_const(name,v);
    };

    reg("print",[](vector<ValPtr> a,EnvPtr){
        for(size_t i=0;i<a.size();i++){
            if(i) cout<<" "; cout<<a[i]->str();
        }
        cout<<"\n"; return nil_val();
    });
    reg("prints",[](vector<ValPtr> a,EnvPtr){
        for(size_t i=0;i<a.size();i++){
            if(i) cout<<" "; cout<<a[i]->str();
        }
        return nil_val();
    });
    reg("input",[](vector<ValPtr> a,EnvPtr){
        if(!a.empty()) cout<<a[0]->str();
        cout.flush();
        if(alb_ide_mode){
            cerr.write("[INPUT_NEEDED]\n",15);
            cerr.flush();
        }
        string line;
        if(!getline(cin,line)) return nil_val();
        try{return num_val(stod(line));}catch(...){return str_val(line);}
    });
    reg("press",[](vector<ValPtr> a,EnvPtr){
        string want = a.empty() ? "any" : a[0]->str();
        auto vk_to_name = [](int vk)->string{
            switch(vk){
                case 0x0D: return "Return";   case 0x1B: return "Escape";
                case 0x20: return "space";    case 0x09: return "Tab";
                case 0x08: return "BackSpace";case 0x25: return "Left";
                case 0x26: return "Up";       case 0x27: return "Right";
                case 0x28: return "Down";     case 0x2E: return "Delete";
                case 0x2D: return "Insert";   case 0x24: return "Home";
                case 0x23: return "End";      case 0x21: return "Prior";
                case 0x22: return "Next";
                default:
                    if(vk>=0x30&&vk<=0x39) return string(1,(char)vk);
                    if(vk>=0x41&&vk<=0x5A){ char c=(char)(vk+32); return string(1,c); }
                    if(vk>=0x70&&vk<=0x7B) return "F"+to_string(vk-0x6F);
                    return "Unknown";
            }
        };
        if(!alb_ide_mode){
            // Console mode: wait for any key
            cout.flush();
#ifdef _WIN32
            int ch=_getch();
            if(ch==0||ch==0xE0) ch=_getch();
#else
            // POSIX: read a single key using termios
            struct termios oldt, newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag &= ~(ICANON | ECHO);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            int ch = getchar();
            if(ch == 27) { // ESC sequence
                // Check if it's an arrow key
                fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
                struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000;
                if(select(STDIN_FILENO+1, &fds, NULL, NULL, &tv) > 0) {
                    int c2 = getchar();
                    if(c2 == '[') {
                        int c3 = getchar();
                        switch(c3) {
                            case 'A': ch = 0x26; break; // Up
                            case 'B': ch = 0x28; break; // Down
                            case 'C': ch = 0x27; break; // Right
                            case 'D': ch = 0x25; break; // Left
                            case '2': getchar(); ch = 0x2D; break; // Insert
                            case '3': getchar(); ch = 0x2E; break; // Delete
                            case '1': getchar(); ch = 0x24; break; // Home
                            case '4': getchar(); ch = 0x23; break; // End
                            default: ch = 0x1B; break;
                        }
                    }
                }
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
            string name=vk_to_name(ch);
            if(want!="any"){
                string lw=want; for(auto&c:lw)c=tolower(c);
                string ln=name; for(auto&c:ln)c=tolower(c);
                return ln==lw ? num_val(1) : num_val(0);
            }
            return str_val(name);
        }
        // IDE mode: signal via stderr, read response from temp file
        cerr.write("[PRESS_KEY]\n",12);
        cerr.flush();
        const char* tf=getenv("ALB_PRESS_TMP");
        string tmpf = tf ? tf : "alb_press.tmp";
        // Clear old file
        { ofstream cl(tmpf,ios::trunc); cl<<""; }
        // Poll for key response (max 60 seconds)
        for(int i=0;i<6000;i++){
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
            ifstream f(tmpf);
            string key;
            if(f.good() && getline(f,key) && !key.empty()){
                if(want!="any"){
                    string lw=want; for(auto&c:lw)c=tolower(c);
                    string lk=key;  for(auto&c:lk)c=tolower(c);
                    return lk==lw ? num_val(1) : num_val(0);
                }
                return str_val(key);
            }
        }
        return want=="any" ? str_val("timeout") : num_val(0);
    });
    reg("type",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("nil");
        switch(a[0]->kind){
            case Val::NUM:return str_val("num");
            case Val::STR:return str_val("str");
            case Val::FN:return str_val("fn");
            case Val::LIST:return str_val("list");
            case Val::MAP:return str_val("map");
            case Val::STRUCT:return str_val(a[0]->struct_type);
            case Val::VECTOR:return str_val("vector");
            default:return str_val("nil");
        }
    });
    reg("len",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
        if(a[0]->kind==Val::STR) return num_val(a[0]->s.size());
        if(a[0]->kind==Val::LIST) return num_val(a[0]->list.size());
        if(a[0]->kind==Val::MAP) return num_val(a[0]->map.size());
        if(a[0]->kind==Val::VECTOR) return num_val(a[0]->vec.size());
        if(a[0]->kind==Val::STRUCT) return num_val(a[0]->fields.size());
        return num_val(0);
    });

    // Arithmetic
    reg("+",[](vector<ValPtr> a,EnvPtr){
        if(a.size()>=2 && (a[0]->kind==Val::STR||a[1]->kind==Val::STR))
            return str_val(a[0]->str()+a[1]->str());
        double r=0; for(auto&v:a) r+=v->n; return num_val(r);
    });
    reg("-",[](vector<ValPtr> a,EnvPtr){
        if(a.size()==1) return num_val(-a[0]->n);
        double r=a[0]->n; for(size_t i=1;i<a.size();i++) r-=a[i]->n;
        return num_val(r);
    });
    reg("*",[](vector<ValPtr> a,EnvPtr){
        double r=1; for(auto&v:a) r*=v->n; return num_val(r);
    });
    reg("/",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[1]->n==0) throw ALBError("ArithmeticError","Division error");
        return num_val(a[0]->n/a[1]->n);
    });
    reg("%",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[1]->n==0) throw ALBError("ArithmeticError","Modulo error");
        return num_val(fmod(a[0]->n,a[1]->n));
    });
    reg("^",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        return num_val(pow(a[0]->n,a[1]->n));
    });

    // Comparison
    reg("==",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->str()==a[1]->str()?1:0);});
    reg("!=",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->str()!=a[1]->str()?1:0);});
    reg("<",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->n<a[1]->n?1:0);});
    reg(">",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->n>a[1]->n?1:0);});
    reg("<=",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->n<=a[1]->n?1:0);});
    reg(">=",[](vector<ValPtr> a,EnvPtr){
        return num_val(a[0]->n>=a[1]->n?1:0);});

    // Logic
    reg("and",[](vector<ValPtr> a,EnvPtr){
        for(auto&v:a) if(!v->truthy()) return num_val(0);
        return num_val(1);
    });
    reg("or",[](vector<ValPtr> a,EnvPtr){
        for(auto&v:a) if(v->truthy()) return num_val(1);
        return num_val(0);
    });
    reg("not",[](vector<ValPtr> a,EnvPtr){
        return num_val(!a[0]->truthy()?1:0);
    });

    // Math
    reg("sqrt",[](vector<ValPtr> a,EnvPtr){return num_val(sqrt(a[0]->n));});
    reg("abs",[](vector<ValPtr> a,EnvPtr){return num_val(fabs(a[0]->n));});
    reg("floor",[](vector<ValPtr> a,EnvPtr){return num_val(floor(a[0]->n));});
    reg("ceil",[](vector<ValPtr> a,EnvPtr){return num_val(ceil(a[0]->n));});
    reg("round",[](vector<ValPtr> a,EnvPtr){return num_val(round(a[0]->n));});
    reg("sin",[](vector<ValPtr> a,EnvPtr){return num_val(sin(a[0]->n));});
    reg("cos",[](vector<ValPtr> a,EnvPtr){return num_val(cos(a[0]->n));});
    reg("tan",[](vector<ValPtr> a,EnvPtr){return num_val(tan(a[0]->n));});
    reg("asin",[](vector<ValPtr> a,EnvPtr){return num_val(asin(a[0]->n));});
    reg("acos",[](vector<ValPtr> a,EnvPtr){return num_val(acos(a[0]->n));});
    reg("atan",[](vector<ValPtr> a,EnvPtr){return num_val(atan(a[0]->n));});
    reg("atan2",[](vector<ValPtr> a,EnvPtr){return num_val(atan2(a[0]->n,a[1]->n));});
    reg("log",[](vector<ValPtr> a,EnvPtr){return num_val(log(a[0]->n));});
    reg("log2",[](vector<ValPtr> a,EnvPtr){return num_val(log2(a[0]->n));});
    reg("log10",[](vector<ValPtr> a,EnvPtr){return num_val(log10(a[0]->n));});
    reg("exp",[](vector<ValPtr> a,EnvPtr){return num_val(exp(a[0]->n));});
    reg("min",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return a.empty()?nil_val():a[0];
        return num_val(min(a[0]->n,a[1]->n));
    });
    reg("max",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return a.empty()?nil_val():a[0];
        return num_val(max(a[0]->n,a[1]->n));
    });
    reg("clamp",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3) return a.empty()?nil_val():a[0];
        double x=a[0]->n,lo=a[1]->n,hi=a[2]->n;
        return num_val(x<lo?lo:(x>hi?hi:x));
    });
    reg("random",[](vector<ValPtr> a,EnvPtr){
        static bool seeded=false;
        if(!seeded){srand((unsigned)time(NULL));seeded=true;}
        double r=(double)rand()/RAND_MAX;
        if(!a.empty()) r*=a[0]->n;
        return num_val(r);
    });
    reg("random-int",[](vector<ValPtr> a,EnvPtr){
        static bool seeded=false;
        if(!seeded){srand((unsigned)time(NULL));seeded=true;}
        if(a.size()<2) return num_val(rand());
        int lo=(int)a[0]->n, hi=(int)a[1]->n;
        if(lo>hi) swap(lo,hi);
        return num_val(lo+rand()%(hi-lo+1));
    });

    // List ops
    reg("car",[](vector<ValPtr> a,EnvPtr){
        return a[0]->list.empty()?nil_val():a[0]->list[0];});
    reg("cdr",[](vector<ValPtr> a,EnvPtr){
        auto l=a[0]->list;
        l.erase(l.begin()); return make_shared<Val>(l);});
    reg("cons",[](vector<ValPtr> a,EnvPtr){
        auto l=a[1]->list; l.insert(l.begin(),a[0]);
        return make_shared<Val>(l);});
    reg("nth",[](vector<ValPtr> a,EnvPtr){
        int i=(int)a[1]->n;
        return (i>=0&&i<(int)a[0]->list.size())?a[0]->list[i]:nil_val();});
    reg("push",[](vector<ValPtr> a,EnvPtr){
        auto l=a[0]->list; l.push_back(a[1]);
        return make_shared<Val>(l);});
    reg("range",[](vector<ValPtr> a,EnvPtr){
        vector<ValPtr> r;
        int start=a.size()>1?(int)a[0]->n:0;
        int end=a.size()>1?(int)a[1]->n:(int)a[0]->n;
        int step=a.size()>2?(int)a[2]->n:1;
        if(step==0) step=1;
        for(int i=start; step>0?i<end:i>end; i+=step)
            r.push_back(num_val(i));
        return make_shared<Val>(r);
    });

    // String ops
    reg("str",[](vector<ValPtr> a,EnvPtr){
        string r; for(auto&v:a) r+=v->str(); return str_val(r);
    });
    reg("num",[](vector<ValPtr> a,EnvPtr){
        try{return num_val(stod(a[0]->str()));}catch(...){return num_val(0);}
    });
    reg("substr",[](vector<ValPtr> a,EnvPtr){
        int s=(int)a[1]->n, l=a.size()>2?(int)a[2]->n:string::npos;
        return str_val(a[0]->s.substr(s,l));
    });
    reg("str-split",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return make_shared<Val>(vector<ValPtr>());
        string s=a[0]->str(), sep=a[1]->str();
        vector<ValPtr> result;
        if(sep.empty()){result.push_back(str_val(s));return make_shared<Val>(result);}
        size_t pos=0;
        while((pos=s.find(sep))!=string::npos){
            result.push_back(str_val(s.substr(0,pos)));
            s.erase(0,pos+sep.size());
        }
        result.push_back(str_val(s));
        return make_shared<Val>(result);
    });
    reg("str-join",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::LIST) return str_val("");
        string sep=a.size()>1?a[1]->str():"";
        string r;
        for(size_t i=0;i<a[0]->list.size();i++){
            if(i) r+=sep;
            r+=a[0]->list[i]->str();
        }
        return str_val(r);
    });
    reg("str-replace",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3) return a.empty()?str_val(""):a[0];
        string s=a[0]->str(), from=a[1]->str(), to=a[2]->str();
        if(from.empty()) return str_val(s);
        size_t pos=0;
        while((pos=s.find(from,pos))!=string::npos){
            s.replace(pos,from.size(),to);
            pos+=to.size();
        }
        return str_val(s);
    });
    reg("str-find",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(-1);
        size_t pos=a[0]->str().find(a[1]->str());
        return num_val(pos==string::npos?-1:(int)pos);
    });
    reg("str-trim",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string s=a[0]->str();
        size_t start=s.find_first_not_of(" \t\n\r");
        if(start==string::npos) return str_val("");
        size_t end=s.find_last_not_of(" \t\n\r");
        return str_val(s.substr(start,end-start+1));
    });
    reg("str-upper",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string s=a[0]->str();
        for(auto&c:s) c=toupper(c);
        return str_val(s);
    });
    reg("str-lower",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string s=a[0]->str();
        for(auto&c:s) c=tolower(c);
        return str_val(s);
    });
    reg("str-starts?",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        string s=a[0]->str(), p=a[1]->str();
        return num_val(s.size()>=p.size()&&s.substr(0,p.size())==p?1:0);
    });
    reg("str-ends?",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        string s=a[0]->str(), p=a[1]->str();
        return num_val(s.size()>=p.size()&&s.substr(s.size()-p.size())==p?1:0);
    });
    reg("str-repeat",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return str_val("");
        string s=a[0]->str();
        int n=(int)a[1]->n;
        string r;
        for(int i=0;i<n;i++) r+=s;
        return str_val(r);
    });
    reg("char-code",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->str().empty()) return num_val(0);
        return num_val((int)a[0]->str()[0]);
    });
    reg("code-char",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        return str_val(string(1,(char)(int)a[0]->n));
    });
    reg("str-reverse",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string s=a[0]->str();
        reverse(s.begin(),s.end());
        return str_val(s);
    });

    // IO ops
    reg("file-read",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        ifstream f(a[0]->str(),ios::binary);
        if(!f) throw ALBError("IOError","Cannot open file: "+a[0]->str());
        ostringstream ss; ss<<f.rdbuf();
        string content=ss.str();
        content.erase(remove(content.begin(),content.end(),'\r'),content.end());
        return str_val(content);
    });
    reg("file-write",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return nil_val();
        ofstream f(a[0]->str());
        if(!f) throw ALBError("IOError","Cannot write file: "+a[0]->str());
        f<<a[1]->str();
        return nil_val();
    });
    reg("file-append",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return nil_val();
        ofstream f(a[0]->str(),ios::app);
        if(!f) throw ALBError("IOError","Cannot append file: "+a[0]->str());
        f<<a[1]->str();
        return nil_val();
    });
    reg("file-exists?",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
        ifstream f(a[0]->str());
        return num_val(f.good()?1:0);
    });
    reg("file-delete",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
        remove(a[0]->str().c_str());
        return nil_val();
    });
    reg("file-lines",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return make_shared<Val>(vector<ValPtr>());
        ifstream f(a[0]->str(),ios::binary);
        if(!f) throw ALBError("IOError","Cannot open file: "+a[0]->str());
        vector<ValPtr> lines;
        string line;
        while(getline(f,line)){
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            lines.push_back(str_val(line));
        }
        return make_shared<Val>(lines);
    });
    reg("dir-list",[](vector<ValPtr> a,EnvPtr){
        string path=a.empty()?".":a[0]->str();
        vector<ValPtr> files;
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        HANDLE h=FindFirstFileA((path+"\\*").c_str(),&fd);
        if(h==INVALID_HANDLE_VALUE) return make_shared<Val>(files);
        do{
            string name=fd.cFileName;
            if(name!="."&&name!="..") files.push_back(str_val(name));
        }while(FindNextFileA(h,&fd));
        FindClose(h);
#else
        DIR* d = opendir(path.c_str());
        if(!d) return make_shared<Val>(files);
        struct dirent* ent;
        while((ent = readdir(d)) != NULL) {
            string name = ent->d_name;
            if(name != "." && name != "..")
                files.push_back(str_val(name));
        }
        closedir(d);
#endif
        return make_shared<Val>(files);
    });
    reg("dir-exists?",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
#ifdef _WIN32
        DWORD attr=GetFileAttributesA(a[0]->str().c_str());
        return num_val(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY)?1:0);
#else
        struct stat st;
        return num_val(stat(a[0]->str().c_str(), &st)==0 && S_ISDIR(st.st_mode) ? 1 : 0);
#endif
    });
    reg("dir-create",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
#ifdef _WIN32
        return num_val(CreateDirectoryA(a[0]->str().c_str(),NULL)?1:0);
#else
        return num_val(mkdir(a[0]->str().c_str(), 0755)==0 ? 1 : 0);
#endif
    });
    reg("path-join",[](vector<ValPtr> a,EnvPtr){
        string r;
        for(auto&v:a){
            if(!r.empty()&&r.back()!='\\'&&r.back()!='/') r+="/";
            r+=v->str();
        }
        return str_val(r);
    });
    reg("path-ext",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string p=a[0]->str();
        size_t dot=p.find_last_of('.');
        if(dot==string::npos) return str_val("");
        return str_val(p.substr(dot+1));
    });
    reg("path-name",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string p=a[0]->str();
        size_t sep=p.find_last_of("\\/");
        string name=(sep==string::npos)?p:p.substr(sep+1);
        size_t dot=name.find_last_of('.');
        if(dot!=string::npos) name=name.substr(0,dot);
        return str_val(name);
    });
    reg("time-now",[](vector<ValPtr> a,EnvPtr e){
        return num_val((double)time(NULL));
    });

    // Control
    reg("break",[](vector<ValPtr>,EnvPtr){ throw Break(); return nil_val(); });

    // Higher-order
    reg("apply",[](vector<ValPtr> a,EnvPtr e){
        if(a[1]->kind==Val::LIST)
            return a[0]->fn->call(a[1]->list, e);
        return nil_val();
    });
    reg("map",[](vector<ValPtr> a,EnvPtr e){
        vector<ValPtr> r;
        for(auto&v:a[1]->list)
            r.push_back(a[0]->fn->call({v},e));
        return make_shared<Val>(r);
    });
    reg("filter",[](vector<ValPtr> a,EnvPtr e){
        vector<ValPtr> r;
        for(auto&v:a[1]->list)
            if(a[0]->fn->call({v},e)->truthy()) r.push_back(v);
        return make_shared<Val>(r);
    });
    reg("fold",[](vector<ValPtr> a,EnvPtr e){
        ValPtr acc=a[2];
        for(auto&v:a[1]->list)
            acc=a[0]->fn->call({acc,v},e);
        return acc;
    });

    // Error handling
    reg("throw",[](vector<ValPtr> a,EnvPtr){
        if(!a.empty()&&a[0]->kind==Val::MAP){
            auto& m=a[0]->map;
            string type="Error",msg="";
            auto it=m.find("type");
            if(it!=m.end()) type=it->second->str();
            it=m.find("message");
            if(it!=m.end()) msg=it->second->str();
            auto dit=m.find("data");
            throw ALBError(type,msg,dit!=m.end()?dit->second:nil_val());
        }
        throw ALBError("Error",a.empty()?"":a[0]->str());
        return nil_val();
    });
    reg("error",[](vector<ValPtr> a,EnvPtr){
        string type=a.size()>0?a[0]->str():"Error";
        string msg=a.size()>1?a[1]->str():"";
        ValPtr data=a.size()>2?a[2]:nil_val();
        return make_map_val({{"type",str_val(type)},{"message",str_val(msg)},{"data",data}});
    });

    // Serialization: write (value -> sexpr string)
    reg("write",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("nil");
        auto w=[](ValPtr v)->string{
            if(!v||v->kind==Val::NIL) return string("nil");
            if(v->kind==Val::NUM){
                double d=v->n;
                if(d==floor(d)&&fabs(d)<1e15) return to_string((long long)d);
                ostringstream o; o<<d; return o.str();
            }
            if(v->kind==Val::STR){
                string r="\"";
                for(char c:v->s){
                    if(c=='"')r+="\\\""; else if(c=='\\')r+="\\\\";
                    else if(c=='\n')r+="\\n"; else if(c=='\t')r+="\\t";
                    else r+=c;
                }
                return r+"\"";
            }
            if(v->kind==Val::LIST){
                string r="(";
                for(size_t i=0;i<v->list.size();i++){
                    if(i)r+=" ";
                    // recursive via lambda would need capture, so inline
                    if(!v->list[i]||v->list[i]->kind==Val::NIL) r+="nil";
                    else r+=v->list[i]->str(); // simplified
                }
                return r+")";
            }
            return v->str();
        };
        return str_val(w(a[0]));
    });

    // Serialization: read (sexpr string -> value)
    reg("read",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
        string s=a[0]->str();
        int pos=0;
        function<ValPtr()> rd=[&]()->ValPtr{
            while(pos<(int)s.size()&&isspace(s[pos])) pos++;
            if(pos>=(int)s.size()) return nil_val();
            if(s[pos]=='"'){
                pos++; string r;
                while(pos<(int)s.size()&&s[pos]!='"'){
                    if(s[pos]=='\\'){pos++;
                        if(pos<(int)s.size()){
                            char c=s[pos++];
                            r+=(c=='n'?'\n':c=='t'?'\t':c);
                        }
                    } else r+=s[pos++];
                }
                if(pos<(int)s.size()) pos++;
                return str_val(r);
            }
            if(s[pos]=='('){
                pos++; vector<ValPtr> l;
                while(pos<(int)s.size()&&s[pos]!=')'){
                    while(pos<(int)s.size()&&isspace(s[pos])) pos++;
                    if(pos<(int)s.size()&&s[pos]!=')') l.push_back(rd());
                }
                if(pos<(int)s.size()) pos++;
                return make_shared<Val>(l);
            }
            string tok;
            while(pos<(int)s.size()&&!isspace(s[pos])&&s[pos]!='('&&s[pos]!=')')
                tok+=s[pos++];
            if(tok=="nil") return nil_val();
            try{size_t e; double d=stod(tok,&e);
                if(e==tok.size()) return num_val(d);}catch(...){}
            return str_val(tok);
        };
        return rd();
    });

    // Test primitive
    reg("test",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return str_val("SKIP: need 2 args");
        string actual=a[0]->str(), expected=a[1]->str();
        if(actual==expected){
            cout<<"  PASS\n"; return num_val(1);
        }
        cout<<"  FAIL: expected "<<expected<<", got "<<actual<<"\n";
        return num_val(0);
    });

    // Assert: (assert condition) or (assert condition message)
    reg("assert",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) throw ALBError("AssertionError","assert: missing condition");
        if(!a[0]->truthy()) {
            string msg = a.size()>1 ? a[1]->str() : "Assertion failed";
            throw ALBError("AssertionError", msg);
        }
        return num_val(1);
    });

    // JSON parse: (json-parse str) -> ALB value
    reg("json-parse",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
        string s = a[0]->str();
        size_t pos = 0;
        function<ValPtr()> skip_ws = [&](){
            while(pos<s.size() && isspace((unsigned char)s[pos])) pos++;
            return nullptr;
        };
        function<ValPtr()> parse_val;
        function<string()> parse_str = [&](){
            pos++; // skip "
            string r;
            while(pos<s.size() && s[pos]!='"') {
                if(s[pos]=='\\') { pos++;
                    if(pos<s.size()) {
                        char c=s[pos];
                        if(c=='n') r+='\n'; else if(c=='t') r+='\t';
                        else if(c=='r') r+='\r'; else r+=c;
                    }
                } else r+=s[pos];
                pos++;
            }
            if(pos<s.size()) pos++; // skip closing "
            return r;
        };
        parse_val = [&]()->ValPtr{
            skip_ws();
            if(pos>=s.size()) return nil_val();
            char c = s[pos];
            if(c=='"') { string str=parse_str(); return str_val(str); }
            if(c=='{') { // object -> map
                pos++;
                unordered_map<string,ValPtr> m;
                skip_ws();
                if(pos<s.size() && s[pos]=='}') { pos++; return make_shared<Val>(m); }
                while(pos<s.size()) {
                    skip_ws();
                    string key = parse_str();
                    skip_ws();
                    if(pos<s.size() && s[pos]==':') pos++;
                    ValPtr val = parse_val();
                    m[key] = val;
                    skip_ws();
                    if(pos<s.size() && s[pos]==',') { pos++; continue; }
                    if(pos<s.size() && s[pos]=='}') { pos++; break; }
                }
                return make_shared<Val>(m);
            }
            if(c=='[') { // array -> list
                pos++;
                vector<ValPtr> arr;
                skip_ws();
                if(pos<s.size() && s[pos]==']') { pos++; return make_shared<Val>(arr); }
                while(pos<s.size()) {
                    arr.push_back(parse_val());
                    skip_ws();
                    if(pos<s.size() && s[pos]==',') { pos++; continue; }
                    if(pos<s.size() && s[pos]==']') { pos++; break; }
                }
                return make_shared<Val>(arr);
            }
            if(c=='t') { pos+=4; return num_val(1); } // true
            if(c=='f') { pos+=5; return num_val(0); } // false
            if(c=='n') { pos+=4; return nil_val(); } // null
            // number
            string num_str;
            while(pos<s.size() && (isdigit(s[pos])||s[pos]=='-'||s[pos]=='.'||s[pos]=='e'||s[pos]=='E'||s[pos]=='+'))
                num_str += s[pos++];
            try { size_t e; double d=stod(num_str,&e); return num_val(d); } catch(...) {}
            return nil_val();
        };
        return parse_val();
    });

    // JSON serialize: (json-serialize val) -> JSON string
    reg("json-serialize",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("null");
        function<string(ValPtr)> to_json;
        to_json = [&](ValPtr v)->string{
            if(!v || v->kind==Val::NIL) return "null";
            if(v->kind==Val::NUM) {
                if(v->n == (long long)v->n) return to_string((long long)v->n);
                return to_string(v->n);
            }
            if(v->kind==Val::STR) {
                if(v->is_sym) {
                    // symbols: true/false/nil
                    if(v->s=="true") return "true";
                    if(v->s=="false") return "false";
                    if(v->s=="nil") return "null";
                }
                string r="\"";
                for(char c:v->s) {
                    if(c=='"') r+="\\\""; else if(c=='\\') r+="\\\\";
                    else if(c=='\n') r+="\\n"; else if(c=='\t') r+="\\t";
                    else r+=c;
                }
                return r+"\"";
            }
            if(v->kind==Val::LIST) {
                string r="[";
                for(size_t i=0;i<v->list.size();i++) {
                    if(i) r+=",";
                    r+=to_json(v->list[i]);
                }
                return r+"]";
            }
            if(v->kind==Val::VECTOR) {
                string r="[";
                for(size_t i=0;i<v->vec.size();i++) {
                    if(i) r+=",";
                    r+=to_json(v->vec[i]);
                }
                return r+"]";
            }
            if(v->kind==Val::MAP) {
                string r="{";
                bool first=true;
                for(auto& kv:v->map) {
                    if(!first) r+=","; first=false;
                    r+="\""+kv.first+"\":"+to_json(kv.second);
                }
                return r+"}";
            }
            if(v->kind==Val::STRUCT) {
                string r="{";
                bool first=true;
                for(auto& kv:v->fields) {
                    if(!first) r+=","; first=false;
                    r+="\""+kv.first+"\":"+to_json(kv.second);
                }
                return r+"}";
            }
            return "null";
        };
        return str_val(to_json(a[0]));
    });

    // Doc query
    reg("doc",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::FN)
            return str_val("Not a function");
        auto f=a[0]->fn;
        cout<<"Function: "<<f->name<<"\n";
        if(!f->params.empty()){
            cout<<"Params: (";
            for(size_t i=0;i<f->params.size();i++){
                if(i)cout<<" "; cout<<f->params[i];
            }
            cout<<")\n";
        }
        if(!f->doc_str.empty()) cout<<"Doc: "<<f->doc_str<<"\n";
        if(!f->caps.empty()){
            cout<<"Caps: ";
            for(auto& c:f->caps) cout<<":"<<c<<" ";
            cout<<"\n";
        }
        if(f->src_line>0) cout<<"Line: "<<f->src_line<<"\n";
        return nil_val();
    });

    // ========== Vector functions ==========
    reg("vector",[](vector<ValPtr> a,EnvPtr){
        auto v=make_shared<Val>(); v->kind=Val::VECTOR; v->vec=a;
        return v;
    });
    reg("vector-ref",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::VECTOR) return nil_val();
        int i=(int)a[1]->n;
        return (i>=0&&i<(int)a[0]->vec.size())?a[0]->vec[i]:nil_val();
    });
    reg("vector-set!",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3||a[0]->kind!=Val::VECTOR) return nil_val();
        int i=(int)a[1]->n;
        if(i>=0&&i<(int)a[0]->vec.size()) a[0]->vec[i]=a[2];
        return a[0];
    });
    reg("vector-push!",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::VECTOR) return nil_val();
        a[0]->vec.push_back(a[1]); return a[0];
    });
    reg("vector-pop!",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::VECTOR||a[0]->vec.empty()) return nil_val();
        ValPtr last=a[0]->vec.back(); a[0]->vec.pop_back(); return last;
    });
    reg("vector-size",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::VECTOR) return num_val(0);
        return num_val(a[0]->vec.size());
    });
    reg("vector->list",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::VECTOR) return make_shared<Val>(vector<ValPtr>());
        return make_shared<Val>(a[0]->vec);
    });
    reg("list->vector",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::LIST) return make_shared<Val>();
        auto v=make_shared<Val>(); v->kind=Val::VECTOR; v->vec=a[0]->list;
        return v;
    });

    // ========== Map functions ==========
    reg("dict",[](vector<ValPtr> a,EnvPtr){
        // (dict) -> empty map, (dict "k1" v1 "k2" v2 ...) -> map
        unordered_map<string,ValPtr> m;
        for(size_t i=0; i+1<a.size(); i+=2)
            m[a[i]->str()]=a[i+1];
        return make_shared<Val>(m);
    });
    reg("map-get",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::MAP) return nil_val();
        auto it=a[0]->map.find(a[1]->str());
        return it!=a[0]->map.end()?it->second:nil_val();
    });
    reg("map-set",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3||a[0]->kind!=Val::MAP) return nil_val();
        auto nm=a[0]->map; nm[a[1]->str()]=a[2];
        return make_shared<Val>(nm);
    });
    reg("map-del",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::MAP) return nil_val();
        auto nm=a[0]->map; nm.erase(a[1]->str());
        return make_shared<Val>(nm);
    });
    reg("map-has?",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::MAP) return num_val(0);
        return num_val(a[0]->map.count(a[1]->str())?1:0);
    });
    reg("map-keys",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::MAP) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r; for(auto& kv:a[0]->map) r.push_back(str_val(kv.first));
        return make_shared<Val>(r);
    });
    reg("map-values",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::MAP) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r; for(auto& kv:a[0]->map) r.push_back(kv.second);
        return make_shared<Val>(r);
    });
    reg("map-merge",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return a.empty()?nil_val():a[0];
        auto nm=a[0]->map;
        for(auto& kv:a[1]->map) nm[kv.first]=kv.second;
        return make_shared<Val>(nm);
    });

    // ========== Struct functions ==========
    reg("struct-get",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::STRUCT) return nil_val();
        auto it=a[0]->fields.find(a[1]->str());
        return it!=a[0]->fields.end()?it->second:nil_val();
    });
    reg("struct-set!",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3||a[0]->kind!=Val::STRUCT) return nil_val();
        a[0]->fields[a[1]->str()]=a[2];
        return a[0];
    });

    // ========== Regex functions ==========
    reg("re-match",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return nil_val();
        try {
            regex re(a[0]->str());
            smatch m;
            string s=a[1]->str();
            if(regex_search(s,m,re)){
                vector<ValPtr> r;
                for(size_t i=0;i<m.size();i++) r.push_back(str_val(m[i].str()));
                return make_shared<Val>(r);
            }
        } catch(...) {}
        return nil_val();
    });
    reg("re-find",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return nil_val();
        try {
            regex re(a[0]->str());
            smatch m;
            string s=a[1]->str();
            if(regex_search(s,m,re)) return str_val(m[0].str());
        } catch(...) {}
        return nil_val();
    });
    reg("re-replace",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<3) return a.empty()?nil_val():a[0];
        try {
            regex re(a[0]->str());
            return str_val(regex_replace(a[1]->str(),re,a[2]->str()));
        } catch(...) { return a[1]; }
    });
    reg("re-split",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r;
        try {
            regex re(a[0]->str());
            sregex_token_iterator it(a[1]->str().begin(),a[1]->str().end(),re,-1);
            sregex_token_iterator end;
            for(;it!=end;++it) r.push_back(str_val(*it));
        } catch(...) { r.push_back(a[1]); }
        return make_shared<Val>(r);
    });
    reg("re-test?",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        try { return num_val(regex_search(a[1]->str(),regex(a[0]->str()))?1:0); }
        catch(...) { return num_val(0); }
    });

    // ========== Higher-order functions ==========
    reg("sort",[](vector<ValPtr> a,EnvPtr e){
        if(a.empty()||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        auto lst=a[0]->list;
        if(a.size()>1 && a[1]->kind==Val::FN) {
            auto cmp=[&](ValPtr x,ValPtr y){
                return a[1]->fn->call({x,y},e)->truthy();
            };
            sort(lst.begin(),lst.end(),cmp);
        } else {
            sort(lst.begin(),lst.end(),[](ValPtr x,ValPtr y){ return x->n<y->n; });
        }
        return make_shared<Val>(lst);
    });
    reg("reverse",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return make_shared<Val>(vector<ValPtr>());
        if(a[0]->kind==Val::LIST){
            auto lst=a[0]->list; reverse(lst.begin(),lst.end());
            return make_shared<Val>(lst);
        }
        if(a[0]->kind==Val::STR){
            string s=a[0]->s; reverse(s.begin(),s.end()); return str_val(s);
        }
        return a[0];
    });
    reg("zip",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::LIST||a[1]->kind!=Val::LIST)
            return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r;
        size_t n=min(a[0]->list.size(),a[1]->list.size());
        for(size_t i=0;i<n;i++){
            r.push_back(make_shared<Val>(vector<ValPtr>{a[0]->list[i],a[1]->list[i]}));
        }
        return make_shared<Val>(r);
    });
    reg("unzip",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r1,r2;
        for(auto& p:a[0]->list){
            if(p->kind==Val::LIST&&p->list.size()>=2){
                r1.push_back(p->list[0]); r2.push_back(p->list[1]);
            }
        }
        return make_shared<Val>(vector<ValPtr>{make_shared<Val>(r1),make_shared<Val>(r2)});
    });
    reg("any",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::FN||a[1]->kind!=Val::LIST) return num_val(0);
        for(auto& v:a[1]->list)
            if(a[0]->fn->call({v},e)->truthy()) return num_val(1);
        return num_val(0);
    });
    reg("all",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::FN||a[1]->kind!=Val::LIST) return num_val(1);
        for(auto& v:a[1]->list)
            if(!a[0]->fn->call({v},e)->truthy()) return num_val(0);
        return num_val(1);
    });
    reg("compose",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::FN||a[1]->kind!=Val::FN) return nil_val();
        auto f=a[0]->fn, g=a[1]->fn;
        auto composed=[f,g,e](vector<ValPtr> args, EnvPtr ce)->ValPtr{
            ValPtr r=g->call(args,ce);
            return f->call({r},ce);
        };
        return make_shared<Val>(Fn(composed),"compose");
    });
    reg("pipe",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::FN||a[1]->kind!=Val::FN) return nil_val();
        auto f=a[0]->fn, g=a[1]->fn;
        auto piped=[f,g,e](vector<ValPtr> args, EnvPtr ce)->ValPtr{
            ValPtr r=f->call(args,ce);
            return g->call({r},ce);
        };
        return make_shared<Val>(Fn(piped),"pipe");
    });
    reg("partial",[](vector<ValPtr> a,EnvPtr e){
        if(a.empty()||a[0]->kind!=Val::FN) return nil_val();
        auto f=a[0]->fn;
        vector<ValPtr> bound(a.begin()+1,a.end());
        auto part=[f,bound](vector<ValPtr> args, EnvPtr ce)->ValPtr{
            vector<ValPtr> all_args=bound;
            all_args.insert(all_args.end(),args.begin(),args.end());
            return f->call(all_args,ce);
        };
        return make_shared<Val>(Fn(part),"partial");
    });
    reg("for",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::LIST||a[1]->kind!=Val::FN) return nil_val();
        for(auto& v:a[0]->list) a[1]->fn->call({v},e);
        return nil_val();
    });
    reg("each",[](vector<ValPtr> a,EnvPtr e){
        if(a.size()<2||a[0]->kind!=Val::LIST||a[1]->kind!=Val::FN) return nil_val();
        for(auto& v:a[0]->list) a[1]->fn->call({v},e);
        return nil_val();
    });
    reg("flatten",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r;
        function<void(ValPtr)> flat=[&](ValPtr v){
            if(v->kind==Val::LIST) for(auto& el:v->list) flat(el);
            else r.push_back(v);
        };
        flat(a[0]); return make_shared<Val>(r);
    });
    reg("uniq",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        vector<ValPtr> r; unordered_set<string> seen;
        for(auto& v:a[0]->list){
            string key=v->str();
            if(!seen.count(key)){ seen.insert(key); r.push_back(v); }
        }
        return make_shared<Val>(r);
    });
    reg("take",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        int n=(int)a[1]->n;
        vector<ValPtr> r(a[0]->list.begin(),a[0]->list.begin()+min((int)a[0]->list.size(),n));
        return make_shared<Val>(r);
    });
    reg("drop",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2||a[0]->kind!=Val::LIST) return make_shared<Val>(vector<ValPtr>());
        int n=min((int)a[1]->n,(int)a[0]->list.size());
        vector<ValPtr> r(a[0]->list.begin()+n,a[0]->list.end());
        return make_shared<Val>(r);
    });
    reg("copy",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
        return clone_val(a[0]);
    });

    // ========== Bit operations ==========
    reg("bit-and",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        long long r=(long long)a[0]->n;
        for(size_t i=1;i<a.size();i++) r&=(long long)a[i]->n;
        return num_val(r);
    });
    reg("bit-or",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        long long r=(long long)a[0]->n;
        for(size_t i=1;i<a.size();i++) r|=(long long)a[i]->n;
        return num_val(r);
    });
    reg("bit-xor",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        long long r=(long long)a[0]->n;
        for(size_t i=1;i<a.size();i++) r^=(long long)a[i]->n;
        return num_val(r);
    });
    reg("bit-not",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
        return num_val(~(long long)a[0]->n);
    });
    reg("bit-lshift",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        return num_val((long long)a[0]->n << (int)a[1]->n);
    });
    reg("bit-rshift",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        return num_val((long long)a[0]->n >> (int)a[1]->n);
    });

    // ========== Time functions ==========
    reg("time-format",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        time_t ts=(time_t)a[0]->n;
        string fmt=a.size()>1?a[1]->str():"%Y-%m-%d %H:%M:%S";
        struct tm* t=localtime(&ts);
        char buf[256]; strftime(buf,sizeof(buf),fmt.c_str(),t);
        return str_val(buf);
    });
    reg("time-parse",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
        struct tm t={}; string fmt=a.size()>1?a[1]->str():"%Y-%m-%d %H:%M:%S";
        // Simple parsing: try strptime-like approach
        // Fallback: just parse common format
        sscanf(a[0]->str().c_str(),"%d-%d-%d %d:%d:%d",
            &t.tm_year,&t.tm_mon,&t.tm_mday,&t.tm_hour,&t.tm_min,&t.tm_sec);
        t.tm_year-=1900; t.tm_mon-=1;
        return num_val((double)mktime(&t));
    });
    reg("time-diff",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(0);
        return num_val(a[0]->n - a[1]->n);
    });
    reg("time-sleep",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
#ifdef _WIN32
        Sleep((DWORD)a[0]->n);
#else
        usleep((useconds_t)(a[0]->n * 1000));
#endif
        return nil_val();
    });
    reg("time-now-ms",[](vector<ValPtr> a,EnvPtr){
#ifdef _WIN32
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER ul; ul.LowPart=ft.dwLowDateTime; ul.HighPart=ft.dwHighDateTime;
        // Convert from 100ns intervals since 1601 to ms since epoch
        double ms=(double)(ul.QuadPart/10000 - 11644473600000LL);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        double ms = (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
#endif
        return num_val(ms);
    });
    reg("time-local",[](vector<ValPtr> a,EnvPtr){
        time_t now=time(NULL);
        struct tm* t=localtime(&now);
        return make_map_val({
            {"year",num_val(t->tm_year+1900)},{"month",num_val(t->tm_mon+1)},
            {"day",num_val(t->tm_mday)},{"hour",num_val(t->tm_hour)},
            {"min",num_val(t->tm_min)},{"sec",num_val(t->tm_sec)},
            {"wday",num_val(t->tm_wday)},{"yday",num_val(t->tm_yday)}
        });
    });

    // ========== System functions ==========
    reg("exit",[](vector<ValPtr> a,EnvPtr){
        int code=a.empty()?0:(int)a[0]->n;
        exit(code); return nil_val();
    });
    reg("sleep",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
#ifdef _WIN32
        Sleep((DWORD)a[0]->n);
#else
        usleep((useconds_t)(a[0]->n * 1000));
#endif
        return nil_val();
    });
    reg("system",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(0);
        return num_val(system(a[0]->str().c_str()));
    });
    reg("exec",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string cmd=a[0]->str()+" 2>&1";
        string result;
        char buf[256];
#ifdef _WIN32
        FILE* pipe=_popen(cmd.c_str(),"r");
        if(!pipe) return str_val("");
        while(fgets(buf,sizeof(buf),pipe)) result+=buf;
        _pclose(pipe);
#else
        FILE* pipe=popen(cmd.c_str(),"r");
        if(!pipe) return str_val("");
        while(fgets(buf,sizeof(buf),pipe)) result+=buf;
        pclose(pipe);
#endif
        return str_val(result);
    });

    // ========== Network functions ==========
    reg("net-connect",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(-1);
#ifdef _WIN32
        WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);
#endif
        SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(s==INVALID_SOCKET) return num_val(-1);
        struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_port=htons((u_short)a[1]->n);
        addr.sin_addr.s_addr=inet_addr(a[0]->str().c_str());
        if(addr.sin_addr.s_addr==INADDR_NONE){
            struct hostent* he=gethostbyname(a[0]->str().c_str());
            if(he) addr.sin_addr=*(struct in_addr*)he->h_addr;
        }
        if(connect(s,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
            closesocket(s); return num_val(-1);
        }
        return num_val((double)s);
    });
    reg("net-send",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return num_val(-1);
        SOCKET s=(SOCKET)(long long)a[0]->n;
        string msg=a[1]->str();
        return num_val(send(s,msg.c_str(),msg.size(),0));
    });
    reg("net-recv",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        SOCKET s=(SOCKET)(long long)a[0]->n;
        int bufsize=a.size()>1?(int)a[1]->n:4096;
        vector<char> buf(bufsize+1,0);
        int n=recv(s,buf.data(),bufsize,0);
        if(n<=0) return str_val("");
        buf[n]=0; return str_val(buf.data());
    });
    reg("net-close",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return nil_val();
        SOCKET s=(SOCKET)(long long)a[0]->n;
        closesocket(s); return nil_val();
    });
    reg("net-listen",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(-1);
#ifdef _WIN32
        WSADATA wsa; WSAStartup(MAKEWORD(2,2),&wsa);
#endif
        SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(s==INVALID_SOCKET) return num_val(-1);
        struct sockaddr_in addr; addr.sin_family=AF_INET;
        addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons((u_short)a[0]->n);
        if(::bind(s,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
            closesocket(s); return num_val(-1);
        }
        listen(s,5);
        return num_val((double)s);
    });
    reg("net-accept",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return num_val(-1);
        SOCKET s=(SOCKET)(long long)a[0]->n;
        struct sockaddr_in addr; socklen_t len=sizeof(addr);
        SOCKET c=accept(s,(struct sockaddr*)&addr,&len);
        if(c==INVALID_SOCKET) return num_val(-1);
        return num_val((double)c);
    });
    reg("http-get",[](vector<ValPtr> a,EnvPtr){
        if(a.empty()) return str_val("");
        string url=a[0]->str();
        // Parse URL: http://host:port/path
        string host,path; int port=80;
        if(url.substr(0,7)=="http://") url=url.substr(7);
        size_t sl=url.find('/');
        if(sl!=string::npos){ host=url.substr(0,sl); path=url.substr(sl); }
        else { host=url; path="/"; }
        size_t cl=host.find(':');
        if(cl!=string::npos){ port=stoi(host.substr(cl+1)); host=host.substr(0,cl); }
        SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_port=htons(port);
        struct hostent* he=gethostbyname(host.c_str());
        if(!he){ closesocket(s); return str_val(""); }
        addr.sin_addr=*(struct in_addr*)he->h_addr;
        if(connect(s,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
            closesocket(s); return str_val("");
        }
        string req="GET "+path+" HTTP/1.1\r\nHost: "+host+"\r\nConnection: close\r\n\r\n";
        send(s,req.c_str(),req.size(),0);
        string resp; char buf[4096]; int n;
        while((n=recv(s,buf,sizeof(buf)-1,0))>0){ buf[n]=0; resp+=buf; }
        closesocket(s);
        // Extract body after \r\n\r\n
        size_t hdr=resp.find("\r\n\r\n");
        return str_val(hdr!=string::npos?resp.substr(hdr+4):resp);
    });
    reg("http-post",[](vector<ValPtr> a,EnvPtr){
        if(a.size()<2) return str_val("");
        string url=a[0]->str(), data=a[1]->str();
        string host,path; int port=80;
        if(url.substr(0,7)=="http://") url=url.substr(7);
        size_t sl=url.find('/');
        if(sl!=string::npos){ host=url.substr(0,sl); path=url.substr(sl); }
        else { host=url; path="/"; }
        size_t cl=host.find(':');
        if(cl!=string::npos){ port=stoi(host.substr(cl+1)); host=host.substr(0,cl); }
        SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_port=htons(port);
        struct hostent* he=gethostbyname(host.c_str());
        if(!he){ closesocket(s); return str_val(""); }
        addr.sin_addr=*(struct in_addr*)he->h_addr;
        if(connect(s,(struct sockaddr*)&addr,sizeof(addr))==SOCKET_ERROR){
            closesocket(s); return str_val("");
        }
        string req="POST "+path+" HTTP/1.1\r\nHost: "+host+
            "\r\nContent-Type: application/x-www-form-urlencoded"+
            "\r\nContent-Length: "+to_string(data.size())+
            "\r\nConnection: close\r\n\r\n"+data;
        send(s,req.c_str(),req.size(),0);
        string resp; char buf[4096]; int n;
        while((n=recv(s,buf,sizeof(buf)-1,0))>0){ buf[n]=0; resp+=buf; }
        closesocket(s);
        size_t hdr=resp.find("\r\n\r\n");
        return str_val(hdr!=string::npos?resp.substr(hdr+4):resp);
    });

    // Constants
    env->set_const("pi", num_val(3.14159265358979323846));
    env->set_const("e",  num_val(2.71828182845904523536));
    env->set_const("real", num_val(1));
    env->set_const("unrl", num_val(0));
    env->set_const("rl", num_val(1));    // short alias for real/true
    env->set_const("ul", num_val(0));    // short alias for unrl/false
    env->set_const("nil", nil_val());
    env->set_const("true", num_val(1));
    env->set_const("false", num_val(0));
}

// ========== Evaluator helpers ==========
ValPtr run_code(const string& code, EnvPtr env, bool debug=false) {
    Parser p(code);
    auto exprs=p.parse_all();
    if(debug){
        cout<<"--- AST ---\n";
        for(auto& e:exprs) cout<<e->str()<<"\n";
        cout<<"-----------\n";
    }
    ValPtr r=nil_val();
    for(auto& e:exprs) {
        try {
            r=eval_expr(e,env);
        } catch(ALBError& err) {
            cerr<<"Error "<<err.type<<": "<<err.msg<<"\n";
            throw;
        }
    }
    return r;
}

// ========== Bytecode System (v0.1.1) ==========
// Bytecode = serialized AST in binary format
// Format: ALBC header + serialized expressions

static const uint32_t ALBC_MAGIC = 0x43424C41; // "ALBC" in little-endian
static const uint16_t ALBC_FORMAT_VERSION = 1;

struct BytecodeHeader {
    uint32_t magic;
    uint16_t format_version;
    uint16_t alb_major, alb_minor, alb_patch;
    uint32_t flags;
    uint32_t num_exprs;
    uint32_t checksum;
};

// Simple checksum (XOR-based)
static uint32_t compute_checksum(const vector<uint8_t>& data) {
    uint32_t cs = 0xDEADBEEF;
    for(size_t i = 0; i < data.size(); i++) {
        cs ^= ((uint32_t)data[i]) << ((i % 4) * 8);
        cs = (cs << 1) | (cs >> 31); // rotate left
    }
    return cs;
}

// Node type tags for serialization
enum NodeType : uint8_t {
    NODE_NIL    = 0x00,
    NODE_NUM    = 0x01,
    NODE_STR    = 0x02,  // quoted string
    NODE_SYM    = 0x03,  // symbol (bare token)
    NODE_LIST   = 0x04,
    NODE_VECTOR = 0x05
};

// Write helpers
static void write_u8(vector<uint8_t>& buf, uint8_t v) { buf.push_back(v); }
static void write_u16(vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF);
}
static void write_u32(vector<uint8_t>& buf, uint32_t v) {
    for(int i = 0; i < 4; i++) { buf.push_back((v >> (i*8)) & 0xFF); }
}
static void write_double(vector<uint8_t>& buf, double v) {
    uint64_t bits; memcpy(&bits, &v, 8);
    for(int i = 0; i < 8; i++) buf.push_back((bits >> (i*8)) & 0xFF);
}
static void write_string(vector<uint8_t>& buf, const string& s) {
    write_u32(buf, (uint32_t)s.size());
    for(char c : s) buf.push_back((uint8_t)c);
}

// Read helpers
struct ByteReader {
    const uint8_t* data;
    size_t pos, size;
    ByteReader(const uint8_t* d, size_t sz) : data(d), pos(0), size(sz) {}
    bool has(size_t n) const { return pos + n <= size; }
    uint8_t read_u8() {
        if(!has(1)) throw ALBError("BytecodeError", "Unexpected end of bytecode");
        return data[pos++];
    }
    uint16_t read_u16() {
        if(!has(2)) throw ALBError("BytecodeError", "Unexpected end of bytecode");
        uint16_t v = data[pos] | ((uint16_t)data[pos+1] << 8);
        pos += 2; return v;
    }
    uint32_t read_u32() {
        if(!has(4)) throw ALBError("BytecodeError", "Unexpected end of bytecode");
        uint32_t v = 0;
        for(int i = 0; i < 4; i++) v |= ((uint32_t)data[pos+i]) << (i*8);
        pos += 4; return v;
    }
    double read_double() {
        if(!has(8)) throw ALBError("BytecodeError", "Unexpected end of bytecode");
        uint64_t bits = 0;
        for(int i = 0; i < 8; i++) bits |= ((uint64_t)data[pos+i]) << (i*8);
        pos += 8;
        double v; memcpy(&v, &bits, 8); return v;
    }
    string read_string() {
        uint32_t len = read_u32();
        if(!has(len)) throw ALBError("BytecodeError", "Unexpected end of bytecode");
        string s((const char*)(data + pos), len);
        pos += len; return s;
    }
};

// Serialize an AST node to binary
static void serialize_node(vector<uint8_t>& buf, ValPtr v) {
    if(!v || v->kind == Val::NIL) {
        write_u8(buf, NODE_NIL);
        return;
    }
    switch(v->kind) {
    case Val::NUM:
        write_u8(buf, NODE_NUM);
        write_double(buf, v->n);
        break;
    case Val::STR:
        write_u8(buf, v->is_sym ? NODE_SYM : NODE_STR);
        write_string(buf, v->s);
        break;
    case Val::LIST:
        write_u8(buf, NODE_LIST);
        write_u32(buf, (uint32_t)v->list.size());
        for(auto& child : v->list) serialize_node(buf, child);
        break;
    case Val::VECTOR:
        write_u8(buf, NODE_VECTOR);
        write_u32(buf, (uint32_t)v->vec.size());
        for(auto& child : v->vec) serialize_node(buf, child);
        break;
    default:
        // FN, MAP, STRUCT cannot be serialized (runtime values)
        write_u8(buf, NODE_NIL);
        break;
    }
}

// Deserialize a binary node back to AST
static ValPtr deserialize_node(ByteReader& r) {
    uint8_t tag = r.read_u8();
    switch(tag) {
    case NODE_NIL:
        return nil_val();
    case NODE_NUM:
        return num_val(r.read_double());
    case NODE_STR: {
        string s = r.read_string();
        return str_val(s);
    }
    case NODE_SYM: {
        string s = r.read_string();
        auto v = str_val(s);
        v->is_sym = true;
        return v;
    }
    case NODE_LIST: {
        uint32_t count = r.read_u32();
        vector<ValPtr> items;
        items.reserve(count);
        for(uint32_t i = 0; i < count; i++)
            items.push_back(deserialize_node(r));
        return make_shared<Val>(items);
    }
    case NODE_VECTOR: {
        uint32_t count = r.read_u32();
        auto v = make_shared<Val>();
        v->kind = Val::VECTOR;
        v->vec.reserve(count);
        for(uint32_t i = 0; i < count; i++)
            v->vec.push_back(deserialize_node(r));
        return v;
    }
    default:
        throw ALBError("BytecodeError", "Unknown node type tag: " + to_string((int)tag));
    }
}

// Optional: simple XOR obfuscation for string constants
static void obfuscate_buffer(vector<uint8_t>& buf, uint8_t key) {
    if(key == 0) return;
    for(size_t i = 0; i < buf.size(); i++)
        buf[i] ^= key;
}

// Compile source code to bytecode file (.albc)
static bool compile_to_bytecode(const string& src_file, const string& out_file,
                                 bool obfuscate = false) {
    // Read source
    ifstream f(src_file, ios::binary);
    if(!f) { cerr << "Cannot open source: " << src_file << "\n"; return false; }
    ostringstream ss; ss << f.rdbuf();
    string code = ss.str();
    f.close();
    code.erase(remove(code.begin(), code.end(), '\r'), code.end());

    // Parse to AST
    Parser p(code);
    auto exprs = p.parse_all();
    if(exprs.empty()) {
        cerr << "Warning: empty source file\n";
    }

    // Serialize AST to binary
    vector<uint8_t> data_buf;
    for(auto& e : exprs) serialize_node(data_buf, e);

    // Compute checksum on original (unobfuscated) data
    uint32_t checksum = compute_checksum(data_buf);

    // Optional obfuscation
    uint8_t obf_key = 0;
    if(obfuscate) {
        obf_key = (uint8_t)(time(NULL) & 0xFF);
        if(obf_key == 0) obf_key = 0x42;
        obfuscate_buffer(data_buf, obf_key);
    }

    // Build header
    BytecodeHeader hdr;
    hdr.magic = ALBC_MAGIC;
    hdr.format_version = ALBC_FORMAT_VERSION;
    hdr.alb_major = 0; hdr.alb_minor = 1; hdr.alb_patch = 1;
    hdr.flags = obfuscate ? 0x01 : 0x00;
    hdr.num_exprs = (uint32_t)exprs.size();
    hdr.checksum = checksum;  // always checksum of original data

    // Write output file
    ofstream out(out_file, ios::binary);
    if(!out) { cerr << "Cannot write: " << out_file << "\n"; return false; }
    out.write((const char*)&hdr, sizeof(hdr));
    if(obfuscate) out.write((const char*)&obf_key, 1);
    out.write((const char*)data_buf.data(), data_buf.size());
    out.close();

    cout << "Compiled " << src_file << " -> " << out_file
         << " (" << exprs.size() << " expressions, "
         << (sizeof(hdr) + (obfuscate?1:0) + data_buf.size()) << " bytes)\n";
    return true;
}

// Load bytecode file back to AST expressions
static vector<ValPtr> load_bytecode(const string& bc_file) {
    ifstream f(bc_file, ios::binary);
    if(!f) throw ALBError("BytecodeError", "Cannot open: " + bc_file);
    // Read entire file
    f.seekg(0, ios::end);
    size_t file_size = (size_t)f.tellg();
    f.seekg(0, ios::beg);
    vector<uint8_t> file_data(file_size);
    f.read((char*)file_data.data(), file_size);
    f.close();

    if(file_size < sizeof(BytecodeHeader))
        throw ALBError("BytecodeError", "File too small to be valid bytecode");

    // Parse header
    BytecodeHeader hdr;
    memcpy(&hdr, file_data.data(), sizeof(hdr));

    if(hdr.magic != ALBC_MAGIC)
        throw ALBError("BytecodeError", "Invalid bytecode file (bad magic)");
    if(hdr.format_version != ALBC_FORMAT_VERSION)
        throw ALBError("BytecodeError",
            "Bytecode format version mismatch: expected " + to_string(ALBC_FORMAT_VERSION)
            + ", got " + to_string(hdr.format_version));

    size_t data_offset = sizeof(hdr);
    uint8_t obf_key = 0;
    bool obfuscated = (hdr.flags & 0x01) != 0;
    if(obfuscated) {
        if(data_offset >= file_size)
            throw ALBError("BytecodeError", "Bytecode file truncated");
        obf_key = file_data[data_offset++];
    }

    // Extract data portion
    size_t data_size = file_size - data_offset;
    vector<uint8_t> data_buf(file_data.begin() + data_offset, file_data.end());

    // Deobfuscate
    if(obfuscated) obfuscate_buffer(data_buf, obf_key);

    // Verify checksum
    uint32_t cs = compute_checksum(data_buf);
    if(cs != hdr.checksum)
        throw ALBError("BytecodeError", "Bytecode checksum mismatch (file may be corrupted)");

    // Deserialize expressions
    ByteReader reader(data_buf.data(), data_buf.size());
    vector<ValPtr> exprs;
    for(uint32_t i = 0; i < hdr.num_exprs; i++)
        exprs.push_back(deserialize_node(reader));

    return exprs;
}

// Check if file is bytecode (by extension or magic)
static bool is_bytecode_file(const string& path) {
    if(path.size() > 5 && path.substr(path.size()-5) == ".albc") return true;
    // Check magic bytes
    ifstream f(path, ios::binary);
    if(!f) return false;
    uint32_t magic = 0;
    f.read((char*)&magic, 4);
    return magic == ALBC_MAGIC;
}

// Run a bytecode file
static ValPtr run_bytecode_file(const string& bc_file, EnvPtr env) {
    auto exprs = load_bytecode(bc_file);
    ValPtr r = nil_val();
    for(auto& e : exprs) r = eval_expr(e, env);
    return r;
}

// ========== Simple ZIP writer (for alb package) ==========
// Minimal ZIP implementation using STORE method (no compression)
// Enough for packaging .albc + runtime files

struct ZipEntry {
    string name;
    vector<uint8_t> data;
};

static uint32_t zip_crc32(const uint8_t* data, size_t len) {
    // CRC32 lookup table
    static uint32_t table[256];
    static bool table_init = false;
    if(!table_init) {
        for(uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for(int j = 0; j < 8; j++)
                c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        table_init = true;
    }
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

static bool write_zip(const string& zip_path, const vector<ZipEntry>& entries) {
    ofstream f(zip_path, ios::binary);
    if(!f) { cerr << "Cannot create: " << zip_path << "\n"; return false; }

    vector<uint32_t> local_offsets;
    // Write local file headers + data
    for(size_t i = 0; i < entries.size(); i++) {
        local_offsets.push_back((uint32_t)f.tellp());
        uint32_t crc = zip_crc32(entries[i].data.data(), entries[i].data.size());
        uint32_t sz = (uint32_t)entries[i].data.size();
        // Local file header signature
        uint32_t sig = 0x04034b50;
        f.write((const char*)&sig, 4);
        uint16_t ver = 20; // version needed
        f.write((const char*)&ver, 2);
        uint16_t flags = 0;
        f.write((const char*)&flags, 2);
        uint16_t method = 0; // STORE (no compression)
        f.write((const char*)&method, 2);
        uint16_t mod_time = 0, mod_date = 0;
        f.write((const char*)&mod_time, 2);
        f.write((const char*)&mod_date, 2);
        f.write((const char*)&crc, 4);
        f.write((const char*)&sz, 4); // compressed size
        f.write((const char*)&sz, 4); // uncompressed size
        uint16_t name_len = (uint16_t)entries[i].name.size();
        f.write((const char*)&name_len, 2);
        uint16_t extra_len = 0;
        f.write((const char*)&extra_len, 2);
        f.write(entries[i].name.c_str(), name_len);
        f.write((const char*)entries[i].data.data(), sz);
    }

    // Write central directory
    uint32_t cd_offset = (uint32_t)f.tellp();
    for(size_t i = 0; i < entries.size(); i++) {
        uint32_t crc = zip_crc32(entries[i].data.data(), entries[i].data.size());
        uint32_t sz = (uint32_t)entries[i].data.size();
        uint32_t cd_sig = 0x02014b50;
        f.write((const char*)&cd_sig, 4);
        uint16_t ver_made = 20;
        f.write((const char*)&ver_made, 2);
        uint16_t ver_need = 20;
        f.write((const char*)&ver_need, 2);
        uint16_t flags = 0;
        f.write((const char*)&flags, 2);
        uint16_t method = 0;
        f.write((const char*)&method, 2);
        uint16_t mod_time = 0, mod_date = 0;
        f.write((const char*)&mod_time, 2);
        f.write((const char*)&mod_date, 2);
        f.write((const char*)&crc, 4);
        f.write((const char*)&sz, 4);
        f.write((const char*)&sz, 4);
        uint16_t name_len = (uint16_t)entries[i].name.size();
        f.write((const char*)&name_len, 2);
        uint16_t extra_len = 0;
        f.write((const char*)&extra_len, 2);
        uint16_t comment_len = 0;
        f.write((const char*)&comment_len, 2);
        uint16_t disk_start = 0;
        f.write((const char*)&disk_start, 2);
        uint16_t int_attr = 0;
        f.write((const char*)&int_attr, 2);
        uint32_t ext_attr = 0;
        f.write((const char*)&ext_attr, 4);
        f.write((const char*)&local_offsets[i], 4);
        f.write(entries[i].name.c_str(), name_len);
    }
    uint32_t cd_size = (uint32_t)f.tellp() - cd_offset;

    // End of central directory
    uint32_t eocd_sig = 0x06054b50;
    f.write((const char*)&eocd_sig, 4);
    uint16_t disk_num = 0;
    f.write((const char*)&disk_num, 2);
    f.write((const char*)&disk_num, 2);
    uint16_t num_entries = (uint16_t)entries.size();
    f.write((const char*)&num_entries, 2);
    f.write((const char*)&num_entries, 2);
    f.write((const char*)&cd_size, 4);
    f.write((const char*)&cd_offset, 4);
    uint16_t comment_len = 0;
    f.write((const char*)&comment_len, 2);

    f.close();
    return true;
}

// Read a file into a byte vector
static vector<uint8_t> read_file_bytes(const string& path) {
    ifstream f(path, ios::binary);
    if(!f) throw ALBError("PackageError", "Cannot read file: " + path);
    f.seekg(0, ios::end);
    size_t sz = (size_t)f.tellg();
    f.seekg(0, ios::beg);
    vector<uint8_t> data(sz);
    f.read((char*)data.data(), sz);
    return data;
}

// Package command: compile + bundle into zip
static int package_app(const string& src_file, const string& out_zip,
                       bool obfuscate = false) {
    // Step 1: Compile to bytecode
    string bc_name = src_file;
    if(bc_name.size() > 4 && bc_name.substr(bc_name.size()-4) == ".alb")
        bc_name = bc_name.substr(0, bc_name.size()-4);
    bc_name += ".albc";

    cout << "[package] Compiling " << src_file << "...\n";
    if(!compile_to_bytecode(src_file, bc_name, obfuscate))
        return 1;

    // Step 2: Build ZIP entries
    vector<ZipEntry> entries;

    // Add bytecode
    ZipEntry bc_entry;
    // Use just the filename part in the zip
    size_t slash = bc_name.find_last_of("\\/");
    bc_entry.name = (slash != string::npos) ? bc_name.substr(slash+1) : bc_name;
    bc_entry.data = read_file_bytes(bc_name);
    entries.push_back(bc_entry);

    // Add runtime exe if available
#ifdef _WIN32
    string runtime_exe = get_exe_dir() + "\\ALB.exe";
    string rt_zip_name = "ALB.exe";
#elif defined(__APPLE__)
    string runtime_exe = get_exe_dir() + "/ALB";
    string rt_zip_name = "ALB";
#else
    string runtime_exe = get_exe_dir() + "/ALB";
    string rt_zip_name = "ALB";
#endif
    { ifstream test(runtime_exe);
      if(test.good()) {
          ZipEntry rt_entry;
          rt_entry.name = rt_zip_name;
          rt_entry.data = read_file_bytes(runtime_exe);
          entries.push_back(rt_entry);
      }
    }

    // Generate run script
    string run_bat = "@echo off\nALB.exe " + bc_entry.name + " %*\n";
    ZipEntry bat_entry;
    bat_entry.name = "run.bat";
    bat_entry.data.assign(run_bat.begin(), run_bat.end());
    entries.push_back(bat_entry);

    string run_sh = "#!/bin/sh\n./ALB " + bc_entry.name + " \"$@\"\n";
    ZipEntry sh_entry;
    sh_entry.name = "run.sh";
    sh_entry.data.assign(run_sh.begin(), run_sh.end());
    entries.push_back(sh_entry);

    // Generate manifest.json
    string manifest = "{\n"
        "  \"name\": \"" + src_file + "\",\n"
        "  \"entry\": \"" + bc_entry.name + "\",\n"
        "  \"alb_version\": \"" + __version__ + "\",\n"
        "  \"format_version\": " + to_string(ALBC_FORMAT_VERSION) + "\n"
        "}\n";
    ZipEntry mf_entry;
    mf_entry.name = "manifest.json";
    mf_entry.data.assign(manifest.begin(), manifest.end());
    entries.push_back(mf_entry);

    // Write ZIP
    cout << "[package] Creating " << out_zip << "...\n";
    if(!write_zip(out_zip, entries)) return 1;

    // Clean up temp .albc
#ifdef _WIN32
    DeleteFileA(bc_name.c_str());
#else
    unlink(bc_name.c_str());
#endif

    cout << "[package] Done! Package: " << out_zip << "\n";
    for(auto& e : entries)
        cout << "  " << e.name << " (" << e.data.size() << " bytes)\n";
    return 0;
}

// ========== Library helpers ==========

// Find g++ compiler
string find_gpp() {
    const char* gpp = getenv("ALB_GPP");
    if(gpp) return string(gpp);
#ifdef _WIN32
    vector<string> candidates = {
        // Bundled minimal MinGW (highest priority)
        get_exe_dir() + "\\dll_min\\bin\\g++.exe",
        get_exe_dir() + "\\..\\dll_min\\bin\\g++.exe",
        // External MinGW installations
        get_exe_dir() + "\\mingw\\bin\\g++.exe",
        get_exe_dir() + "\\..\\mingw\\bin\\g++.exe",
        get_exe_dir() + "\\..\\mingw64\\bin\\g++.exe",
        "D:\\Program Files (x86)\\Dev-Cpp\\MinGW64\\bin\\g++.exe",
        "C:\\MinGW\\bin\\g++.exe",
        "C:\\msys64\\mingw64\\bin\\g++.exe",
    };
    for(auto& c : candidates) {
        ifstream test(c);
        if(test.good()) return c;
    }
    // Try from PATH
    if(getenv("PATH")) {
        string path_env = getenv("PATH");
        stringstream ss(path_env);
        string dir;
        while(getline(ss, dir, ';')) {
            string full = dir + "\\g++.exe";
            ifstream t(full);
            if(t.good()) return full;
        }
    }
#else
    // POSIX: try `which g++` first
    FILE* which = popen("which g++ 2>/dev/null", "r");
    if(which) {
        char buf[1024];
        if(fgets(buf, sizeof(buf), which)) {
            string path(buf);
            while(!path.empty() && (path.back()=='\n' || path.back()=='\r')) path.pop_back();
            pclose(which);
            if(!path.empty()) return path;
        }
        pclose(which);
    }
    // Check standard paths
    vector<string> candidates = {
        "/usr/bin/g++",
        "/usr/local/bin/g++",
        "/opt/homebrew/bin/g++",  // macOS Apple Silicon
        "/usr/bin/c++",
    };
    for(auto& c : candidates) {
        ifstream test(c);
        if(test.good()) return c;
    }
#endif
    return "";
}

// Get file modification time
static long long get_mtime(const string& path) {
#ifdef _WIN32
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, 0, NULL);
    if(h == INVALID_HANDLE_VALUE) return 0;
    FILETIME ft;
    GetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    return ((long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
#else
    struct stat st;
    if(stat(path.c_str(), &st) != 0) return 0;
    return (long long)st.st_mtime;
#endif
}

// Compile .cpp files in folder to shared library (DLL/SO/DYLIB)
bool compile_cpp_dll(const string& folder, const string& name) {
    // Find all .cpp files in folder
    vector<string> cpp_files;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    string search = folder + "\\*.cpp";
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if(hFind == INVALID_HANDLE_VALUE) return false;
    do {
        if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            cpp_files.push_back(folder + "\\" + fd.cFileName);
    } while(FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    cpp_files = list_dir_files(folder, ".cpp");
    for(auto& f : cpp_files) f = folder + "/" + f;
#endif
    if(cpp_files.empty()) return false;

#ifdef _WIN32
    string dll_path = folder + "\\" + name + ".dll";
#elif defined(__APPLE__)
    string dll_path = folder + "/" + name + ".dylib";
#else
    string dll_path = folder + "/" + name + ".so";
#endif
    // Check if recompilation needed
    long long dll_time = get_mtime(dll_path);
    bool needs_recompile = (dll_time == 0);
    if(!needs_recompile) {
        for(auto& f : cpp_files) {
            if(get_mtime(f) > dll_time) { needs_recompile = true; break; }
        }
    }
    if(!needs_recompile) return true;

    string gpp = find_gpp();
    if(gpp.empty()) {
#ifdef _WIN32
        cerr << "[ALB] Pre-compiled DLL not found and g++ not installed.\n"
             << "[ALB] Install MinGW g++ or use a pre-compiled " << name << ".dll\n";
#else
        cerr << "[ALB] Pre-compiled shared library not found and g++ not installed.\n"
             << "[ALB] Please install g++:\n"
             << "  Ubuntu/Debian: sudo apt-get install g++\n"
             << "  macOS: xcode-select --install\n"
             << "  Or use a pre-compiled " << name << ".so/.dylib\n";
#endif
        return false;
    }

    // Build command
    string cmd = "\"" + gpp + "\" -shared -fPIC -std=c++11 -O2 -o \"" + dll_path + "\"";
    for(auto& f : cpp_files) cmd += " \"" + f + "\"";
#ifdef _WIN32
    cmd += " -lshlwapi -luser32 -lgdi32 -lkernel32 -lcomctl32 -lcomdlg32 -lole32 -static-libgcc -static-libstdc++ 2>&1";
    // If using bundled dll_min, prepend its bin dir to PATH
    bool using_bundled = (gpp.find("dll_min") != string::npos);
    if(using_bundled) {
        string dll_min_bin = gpp.substr(0, gpp.rfind("\\bin\\")) + "\\bin";
        string old_path = getenv("PATH") ? getenv("PATH") : "";
        string new_path = dll_min_bin + ";" + old_path;
        SetEnvironmentVariableA("PATH", new_path.c_str());
    }
    string full_cmd = "\"" + cmd + "\"";
#elif defined(__APPLE__)
    // macOS: g++ is actually clang++, needs SDK path and dynamic lookup
    {
        string sdk_path;
        FILE* p = popen("xcrun --show-sdk-path 2>/dev/null", "r");
        if(p) { char buf[1024]; if(fgets(buf,sizeof(buf),p)) { sdk_path=buf; while(!sdk_path.empty()&&(sdk_path.back()=='\n'||sdk_path.back()=='\r')) sdk_path.pop_back(); } pclose(p); }
        string mac_cmd;
        if(!sdk_path.empty())
            mac_cmd = "\"" + gpp + "\" -isysroot \"" + sdk_path + "\" -shared -fPIC -std=c++11 -O2 -undefined dynamic_lookup -o \"" + dll_path + "\"";
        else
            mac_cmd = "\"" + gpp + "\" -shared -fPIC -std=c++11 -O2 -undefined dynamic_lookup -o \"" + dll_path + "\"";
        for(auto& f : cpp_files) mac_cmd += " \"" + f + "\"";
        mac_cmd += " -ldl 2>&1";
        cmd = mac_cmd;
    }
    string full_cmd = cmd;
#else
    cmd += " -ldl 2>&1";
    string full_cmd = cmd;
#endif

    cout << "[ALB] Compiling " << name << " library...\n";
    cout.flush();

    int ret = system(full_cmd.c_str());
    if(ret != 0) {
        cerr << "[ALB] Compilation failed for " << name << "\n";
        return false;
    }
    cout << "[ALB] " << name << " compiled successfully.\n";
    return true;
}

// ========== GUI Callback Support ==========
static EnvPtr g_gui_env = nullptr;  // Global env for GUI callbacks

// Callback function injected into GUI DLLs
// DLL calls this to invoke ALB functions
static const char* alb_gui_callback(const char* func_name, int argc, const char* argv[]) {
    if(!g_gui_env) return "nil";
    try {
        // Look up the function in the environment
        ValPtr fn_val = g_gui_env->get(string(func_name));
        if(!fn_val || fn_val->kind != Val::FN) return "nil";
        
        // Build arguments list
        vector<ValPtr> args;
        for(int i = 0; i < argc; i++) {
            args.push_back(str_val(argv[i]));
        }
        
        // Call the function
        ValPtr result = fn_val->fn->call(args, g_gui_env);
        
        // Return result as string (static buffer)
        static string result_buf;
        result_buf = result ? result->str() : "nil";
        return result_buf.c_str();
    } catch(ALBError& err) {
        cerr << "[GUI Callback Error] " << func_name << ": " << err.msg << "\n";
        return "nil";
    } catch(exception& ex) {
        cerr << "[GUI Callback Error] " << func_name << ": " << ex.what() << "\n";
        return "nil";
    }
}

// Load shared library (DLL/SO/DYLIB) and register its functions into env
void load_dll_into(const string& dll_path, EnvPtr env) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dll_path.c_str());
    if(!hMod) throw ALBError("FFIError", "Cannot load DLL: " + dll_path);
    typedef const char** (*alb_get_fn)();
    auto alb_get_functions = (alb_get_fn)GetProcAddress(hMod, "alb_get_functions");
    if(!alb_get_functions) {
        FreeLibrary(hMod);
        throw ALBError("FFIError", "DLL missing alb_get_functions: " + dll_path);
    }
#else
    void* hMod = dlopen(dll_path.c_str(), RTLD_LAZY);
    if(!hMod) throw ALBError("FFIError", string("Cannot load shared library: ") + dll_path + " (" + dlerror() + ")");
    typedef const char** (*alb_get_fn)();
    auto alb_get_functions = (alb_get_fn)dlsym(hMod, "alb_get_functions");
    if(!alb_get_functions) {
        dlclose(hMod);
        throw ALBError("FFIError", "Shared library missing alb_get_functions: " + dll_path);
    }
#endif
    const char** funcs = alb_get_functions();
    for(int i = 0; funcs[i] != NULL; i += 2) {
        string fname = funcs[i];
        typedef const char* (*dll_fn_t)(int, const char*[]);
        dll_fn_t dll_fn = (dll_fn_t)funcs[i + 1];
        if(!dll_fn) break;
        auto wrapper = [dll_fn](vector<ValPtr> a, EnvPtr) -> ValPtr {
            vector<const char*> cargs;
            for(auto& x : a) cargs.push_back(x->str().c_str());
            const char* result = dll_fn((int)cargs.size(), cargs.data());
            return result ? str_val(result) : nil_val();
        };
        auto v = make_shared<Val>();
        v->kind = Val::FN;
        v->fn = make_shared<Func>();
        v->fn->name = fname; v->fn->native = wrapper; v->fn->is_native = true;
        env->set_local(fname, v);
    }
    
    // Optional: inject ALB callback into DLL for event handling
    typedef void (*alb_set_callback_fn)(const char* (*)(const char*, int, const char*[]));
#ifdef _WIN32
    auto alb_set_callback = (alb_set_callback_fn)GetProcAddress(hMod, "alb_set_callback");
#else
    auto alb_set_callback = (alb_set_callback_fn)dlsym(hMod, "alb_set_callback");
#endif
    if(alb_set_callback) {
        g_gui_env = env;
        alb_set_callback(alb_gui_callback);
    }
}

string resolve_module(const string& name) {
    // Build list of lib base directories to search
    vector<string> lib_bases;
    lib_bases.push_back("lib");
    string exe_dir = get_exe_dir();
    string exe_lib = exe_dir + "/lib";
    if(exe_lib != "lib") lib_bases.push_back(exe_lib);
    // Also check parent of exe dir (for IDE scenario: exe in ALB IDE/, libs in ../lib/)
    string parent_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));
    if(!parent_dir.empty()) {
        string parent_lib = parent_dir + "/lib";
        if(parent_lib != "lib" && parent_lib != exe_lib)
            lib_bases.push_back(parent_lib);
    }

    for(auto& lb : lib_bases) {
        string folder = lb + "/" + name;
        { ifstream test(folder + "/__init__.alb");
          if(test.good()) return "folder:" + folder; }
        { ifstream test(folder + "/cli.alb");
          if(test.good()) return "folder:" + folder; }
#ifdef _WIN32
        { WIN32_FIND_DATAA fd;
          string search = folder + "\\*.cpp";
          HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
          if(hFind != INVALID_HANDLE_VALUE) { FindClose(hFind); return "folder:" + folder; } }
#else
        { auto cpp_files = list_dir_files(folder, ".cpp");
          if(!cpp_files.empty()) return "folder:" + folder; }
#endif
    }

    // Single file search in all lib bases
    for(auto& lb : lib_bases) {
        vector<string> paths = { lb + "/" + name + ".alb" };
#ifdef _WIN32
        paths.push_back(lb + "/" + name + ".dll");
#elif defined(__APPLE__)
        paths.push_back(lb + "/" + name + ".dylib");
#else
        paths.push_back(lb + "/" + name + ".so");
#endif
        for(auto& p : paths) { ifstream f(p); if(f.good()) return p; }
    }
    // Backward-compatible: relative to CWD
    vector<string> cwd_paths = { name, name + ".alb" };
#ifdef _WIN32
    cwd_paths.push_back(name + ".dll");
#elif defined(__APPLE__)
    cwd_paths.push_back(name + ".dylib");
#else
    cwd_paths.push_back(name + ".so");
#endif
    for(auto& p : cwd_paths) { ifstream f(p); if(f.good()) return p; }
    return "";
}

// ========== REPL ==========
void run_repl(EnvPtr env) {
    cout<<"ALB v"<<__version__<<" REPL - type (help) or Ctrl+Z to quit\n";
    string line;
    while(true) {
        cout<<"alb> ";
        if(!getline(cin,line)) break;
        if(line=="(exit)"||line=="(quit)") break;
        if(line.empty()||line[0]==';') continue;
        int depth=0;
        for(char c:line) { if(c=='(')depth++; if(c==')')depth--; }
        while(depth>0) {
            cout<<"...  ";
            string more;
            if(!getline(cin,more)) break;
            line+="\n"+more;
            for(char c:more){if(c=='(')depth++;if(c==')')depth--;}
        }
        try {
            Parser p(line);
            auto exprs=p.parse_all();
            for(auto& e:exprs) {
                auto r=eval_expr(e,env);
                if(r && r->kind!=Val::NIL)
                    cout<<"=> "<<r->str()<<"\n";
            }
        } catch(ALBError& err) {
            cerr<<"Error "<<err.type<<": "<<err.msg<<"\n";
        } catch(exception& ex) {
            cerr<<"Error: "<<ex.what()<<"\n";
        }
    }
}

// ========== Package Manager ==========

// Get the lib directory path
string get_lib_dir() {
    string exe_dir = get_exe_dir();
    string lib_path = exe_dir + "/lib";
#ifdef _WIN32
    CreateDirectoryA(lib_path.c_str(), NULL);
#else
    { string cmd_mkdir = "mkdir -p \"" + lib_path + "\""; system(cmd_mkdir.c_str()); }
#endif
    return lib_path;
}

// Download file from URL
bool download_file(const string& url, const string& dest) {
    cout << "Downloading: " << url << "\n";
#ifdef _WIN32
    HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), dest.c_str(), 0, NULL);
    if(FAILED(hr)) {
        cerr << "Download failed: " << url << "\n";
        return false;
    }
#else
    // Use curl or wget
    string cmd = "curl -L -o \"" + dest + "\" \"" + url + "\" 2>/dev/null";
    if(system(cmd.c_str()) != 0) {
        cmd = "wget -O \"" + dest + "\" \"" + url + "\" 2>/dev/null";
        if(system(cmd.c_str()) != 0) {
            cerr << "Download failed: " << url << " (need curl or wget)\n";
            return false;
        }
    }
#endif
    cout << "Downloaded to: " << dest << "\n";
    return true;
}

// Check if string starts with prefix
static bool starts_with(const string& s, const string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Check if string ends with suffix
static bool ends_with(const string& s, const string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size()-suffix.size(), suffix.size(), suffix) == 0;
}

// Extract archive to target directory
bool extract_archive(const string& archive, const string& target) {
    // Create target directory (recursive)
#ifdef _WIN32
    CreateDirectoryA(target.c_str(), NULL);
#else
    // mkdir -p equivalent
    string cmd_mkdir = "mkdir -p \"" + target + "\"";
    system(cmd_mkdir.c_str());
#endif

    string cmd;
    if(ends_with(archive, ".zip")) {
#ifdef _WIN32
        // tar supports .zip on Win10+
        cmd = "tar -xf \"" + archive + "\" -C \"" + target + "\"";
#else
        // macOS/Linux: use unzip
        cmd = "unzip -o \"" + archive + "\" -d \"" + target + "\"";
#endif
    } else if(ends_with(archive, ".tar.gz") || ends_with(archive, ".tgz")) {
        cmd = "tar -xzf \"" + archive + "\" -C \"" + target + "\"";
    } else if(ends_with(archive, ".tar")) {
        cmd = "tar -xf \"" + archive + "\" -C \"" + target + "\"";
    } else {
        cerr << "Unsupported archive format: " << archive << "\n";
        cerr << "Supported: .zip, .tar.gz, .tgz, .tar\n";
        return false;
    }

    cout << "Extracting to: " << target << "\n";
    int ret = system(cmd.c_str());
    if(ret != 0) {
        cerr << "Extraction failed\n";
        return false;
    }
    return true;
}

// Get package name from path/URL
string pkg_name_from_source(const string& source) {
    string name = source;
    // Remove trailing slash
    while(!name.empty() && (name.back() == '/' || name.back() == '\\'))
        name.pop_back();
    // Get last component
    size_t pos = name.find_last_of("/\\");
    if(pos != string::npos) name = name.substr(pos + 1);
    // Remove extension
    if(ends_with(name, ".tar.gz")) name = name.substr(0, name.size() - 7);
    else if(ends_with(name, ".tgz")) name = name.substr(0, name.size() - 4);
    else if(ends_with(name, ".zip")) name = name.substr(0, name.size() - 4);
    else if(ends_with(name, ".tar")) name = name.substr(0, name.size() - 4);
    return name;
}

// Install a package
int pkg_install(const string& source, bool set_dll) {
    string lib_dir = get_lib_dir();
    bool is_url = starts_with(source, "http://") || starts_with(source, "https://");

    string archive_path;
    string pkg_name = pkg_name_from_source(source);

    if(is_url) {
        // Download to temp file
#ifdef _WIN32
        char tmp[MAX_PATH];
        GetTempPathA(MAX_PATH, tmp);
        string tmp_dir(tmp);
#else
        const char* tmp_env = getenv("TMPDIR");
        string tmp_dir = tmp_env ? string(tmp_env) : "/tmp/";
        if(!tmp_dir.empty() && tmp_dir.back() != '/') tmp_dir += '/';
#endif
        string ext = ".zip";
        if(ends_with(source, ".tar.gz") || ends_with(source, ".tgz")) ext = ".tar.gz";
        else if(ends_with(source, ".tar")) ext = ".tar";
        archive_path = tmp_dir + "alb_pkg_" + pkg_name + ext;
        if(!download_file(source, archive_path)) return 1;
    } else {
        archive_path = source;
        ifstream test(archive_path);
        if(!test.good()) {
            cerr << "File not found: " << archive_path << "\n";
            return 1;
        }
    }

    // Extract to lib/pkg_name/
    string target = lib_dir + "/" + pkg_name;

    // If target exists, ask to overwrite
    bool exists = false;
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(target.c_str());
    exists = (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    exists = (stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#endif
    if(exists) {
        cout << "Package '" << pkg_name << "' already exists. Overwrite? [y/N]: ";
        string ans;
        getline(cin, ans);
        if(ans != "y" && ans != "Y") {
            cout << "Cancelled.\n";
#ifdef _WIN32
            if(is_url) DeleteFileA(archive_path.c_str());
#else
            if(is_url) unlink(archive_path.c_str());
#endif
            return 0;
        }
#ifdef _WIN32
        string rm_cmd = "rmdir /s /q \"" + target + "\"";
#else
        string rm_cmd = "rm -rf \"" + target + "\"";
#endif
        system(rm_cmd.c_str());
    }

    // Create temp extraction directory
    string tmp_extract = target + "_tmp";
    // Create target directory (recursive on POSIX)
#ifdef _WIN32
    CreateDirectoryA(tmp_extract.c_str(), NULL);
#else
    { string cmd_mkdir = "mkdir -p \"" + tmp_extract + "\""; system(cmd_mkdir.c_str()); }
#endif

    if(!extract_archive(archive_path, tmp_extract)) {
#ifdef _WIN32
        if(is_url) DeleteFileA(archive_path.c_str());
#else
        if(is_url) unlink(archive_path.c_str());
#endif
        return 1;
    }

    // Check if archive extracted to a subdirectory (common for GitHub zips)
    vector<string> entries;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    string search = tmp_extract + "\\*";
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            string name = fd.cFileName;
            if(name != "." && name != "..")
                entries.push_back(name);
        } while(FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    entries = list_dir_files(tmp_extract, "");
    // Also include directories
    DIR* d = opendir(tmp_extract.c_str());
    if(d) {
        struct dirent* ent;
        entries.clear();
        while((ent = readdir(d)) != NULL) {
            string name = ent->d_name;
            if(name != "." && name != "..")
                entries.push_back(name);
        }
        closedir(d);
    }
#endif

    string final_target = target;
    if(entries.size() == 1) {
        string inner = tmp_extract + "/" + entries[0];
        bool is_dir = false;
#ifdef _WIN32
        DWORD inner_attr = GetFileAttributesA(inner.c_str());
        is_dir = (inner_attr != INVALID_FILE_ATTRIBUTES && (inner_attr & FILE_ATTRIBUTE_DIRECTORY));
#else
        struct stat st2;
        is_dir = (stat(inner.c_str(), &st2) == 0 && S_ISDIR(st2.st_mode));
#endif
        if(is_dir) {
            rename(inner.c_str(), target.c_str());
        } else {
            rename(tmp_extract.c_str(), target.c_str());
        }
    } else {
        rename(tmp_extract.c_str(), target.c_str());
    }

    // Clean up temp extraction directory if still exists
#ifdef _WIN32
    string rm_cmd2 = "rmdir /s /q \"" + tmp_extract + "\" 2>nul";
#else
    string rm_cmd2 = "rm -rf \"" + tmp_extract + "\" 2>/dev/null";
#endif
    system(rm_cmd2.c_str());

    // Clean up downloaded file
#ifdef _WIN32
    if(is_url) DeleteFileA(archive_path.c_str());
#else
    if(is_url) unlink(archive_path.c_str());
#endif

    cout << "Package '" << pkg_name << "' installed to: " << final_target << "\n";

    // If set_dll=true and package has .cpp files, try to compile
    if(set_dll) {
        bool has_cpp = false;
#ifdef _WIN32
        string cpp_search = final_target + "\\*.cpp";
        WIN32_FIND_DATAA fd2;
        HANDLE h2 = FindFirstFileA(cpp_search.c_str(), &fd2);
        has_cpp = (h2 != INVALID_HANDLE_VALUE);
        if(has_cpp) FindClose(h2);
#else
        auto cpp_files = list_dir_files(final_target, ".cpp");
        has_cpp = !cpp_files.empty();
#endif
        if(has_cpp) {
            cout << "[set_dll] C++ extension detected, compiling...\n";
            string gpp = find_gpp();
            if(gpp.empty()) {
                cerr << "[set_dll] Warning: g++ not found. Install compiler or set ALB_GPP.\n";
            } else {
                if(compile_cpp_dll(final_target, pkg_name)) {
                    cout << "[set_dll] Compiled " << pkg_name << " successfully.\n";
                } else {
                    cerr << "[set_dll] Compilation failed for " << pkg_name << ".\n";
                }
            }
        } else {
            cout << "[set_dll] No .cpp files found, skipping compilation.\n";
        }
    }

    cout << "Use with: (use \"" << pkg_name << "\")\n";
    return 0;
}

// List installed packages
int pkg_list() {
    string lib_dir = get_lib_dir();
    cout << "Installed packages in " << lib_dir << ":\n\n";

    int count = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    string search = lib_dir + "\\*";
    HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
    if(hFind == INVALID_HANDLE_VALUE) {
        cout << "  (none)\n";
        return 0;
    }
    do {
        string name = fd.cFileName;
        if(name == "." || name == "..") continue;
        string full = lib_dir + "\\" + name;
        DWORD attr = GetFileAttributesA(full.c_str());
        if(attr & FILE_ATTRIBUTE_DIRECTORY) {
            string init = full + "\\__init__.alb";
            ifstream test(init);
            bool has_init = test.good();
            string cpp_search = full + "\\*.cpp";
            WIN32_FIND_DATAA fd2;
            HANDLE h2 = FindFirstFileA(cpp_search.c_str(), &fd2);
            bool has_cpp = (h2 != INVALID_HANDLE_VALUE);
            if(has_cpp) FindClose(h2);
            if(has_init || has_cpp) {
                cout << "  " << name;
                if(has_cpp) cout << " [C++ ext]";
                cout << "\n";
                count++;
            }
        } else if(ends_with(name, ".alb")) {
            cout << "  " << name.substr(0, name.size()-4) << " [single file]\n";
            count++;
        }
    } while(FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* d = opendir(lib_dir.c_str());
    if(!d) {
        cout << "  (none)\n";
        return 0;
    }
    struct dirent* ent;
    while((ent = readdir(d)) != NULL) {
        string name = ent->d_name;
        if(name == "." || name == "..") continue;
        string full = lib_dir + "/" + name;
        struct stat st;
        if(stat(full.c_str(), &st) != 0) continue;
        if(S_ISDIR(st.st_mode)) {
            string init = full + "/__init__.alb";
            ifstream test(init);
            bool has_init = test.good();
            auto cpp_files = list_dir_files(full, ".cpp");
            bool has_cpp = !cpp_files.empty();
            if(has_init || has_cpp) {
                cout << "  " << name;
                if(has_cpp) cout << " [C++ ext]";
                cout << "\n";
                count++;
            }
        } else if(ends_with(name, ".alb")) {
            cout << "  " << name.substr(0, name.size()-4) << " [single file]\n";
            count++;
        }
    }
    closedir(d);
#endif

    if(count == 0) cout << "  (none)\n";
    return 0;
}

// Remove a package
int pkg_remove(const string& name) {
    string lib_dir = get_lib_dir();
#ifdef _WIN32
    string target = lib_dir + "\\" + name;
    DWORD attr = GetFileAttributesA(target.c_str());
    if(attr == INVALID_FILE_ATTRIBUTES) {
        cerr << "Package not found: " << name << "\n";
        return 1;
    }
    if(attr & FILE_ATTRIBUTE_DIRECTORY) {
        cout << "Remove package '" << name << "'? [y/N]: ";
        string ans;
        getline(cin, ans);
        if(ans != "y" && ans != "Y") {
            cout << "Cancelled.\n";
            return 0;
        }
        string cmd = "rmdir /s /q \"" + target + "\"";
        system(cmd.c_str());
    } else {
        DeleteFileA(target.c_str());
    }
#else
    string target = lib_dir + "/" + name;
    struct stat st;
    if(stat(target.c_str(), &st) != 0) {
        cerr << "Package not found: " << name << "\n";
        return 1;
    }
    if(S_ISDIR(st.st_mode)) {
        cout << "Remove package '" << name << "'? [y/N]: ";
        string ans;
        getline(cin, ans);
        if(ans != "y" && ans != "Y") {
            cout << "Cancelled.\n";
            return 0;
        }
        string cmd = "rm -rf \"" + target + "\"";
        system(cmd.c_str());
    } else {
        unlink(target.c_str());
    }
#endif

    cout << "Package '" << name << "' removed.\n";
    return 0;
}

// ========== Help System ==========
static unordered_map<string,pair<string,string>> get_builtin_docs() {
    unordered_map<string,pair<string,string>> d;
    // sig, desc
    d["print"]  = {"(print arg1 ...)", "Print values followed by newline"};
    d["prints"] = {"(prints arg1 ...)", "Print values without newline"};
    d["input"]  = {"(input prompt?)", "Read line from stdin, returns number or string"};
    d["press"]  = {"(press key?)", "Wait for key press (any, Return, Escape, space, ...)"};
    d["type"]   = {"(type val)", "Return type name: nil/number/string/list/function"};
    d["len"]    = {"(len val)", "Return length of string or list"};
    // Arithmetic
    d["+"]  = {"(+ a b ...)", "Add numbers"};
    d["-"]  = {"(- a b)", "Subtract b from a"};
    d["*"]  = {"(* a b ...)", "Multiply numbers"};
    d["/"]  = {"(/ a b)", "Divide a by b"};
    d["%"]  = {"(% a b)", "Modulo (remainder)"};
    d["^"]  = {"(^ base exp)", "Power"};
    // Math
    d["sqrt"]={"(sqrt n)","Square root"}; d["abs"]={"(abs n)","Absolute value"};
    d["floor"]={"(floor n)","Floor"}; d["ceil"]={"(ceil n)","Ceiling"};
    d["round"]={"(round n)","Round to nearest integer"};
    d["sin"]={"(sin n)","Sine (radians)"}; d["cos"]={"(cos n)","Cosine (radians)"};
    d["tan"]={"(tan n)","Tangent (radians)"};
    d["asin"]={"(asin n)","Arcsine"}; d["acos"]={"(acos n)","Arccosine"};
    d["atan"]={"(atan n)","Arctangent"}; d["atan2"]={"(atan2 y x)","Two-arg arctangent"};
    d["log"]={"(log n)","Natural log"}; d["log2"]={"(log2 n)","Log base 2"};
    d["log10"]={"(log10 n)","Log base 10"}; d["exp"]={"(exp n)","e^n"};
    d["min"]={"(min a b)","Minimum"}; d["max"]={"(max a b)","Maximum"};
    d["clamp"]={"(clamp val lo hi)","Clamp value to range"};
    d["random"]={"(random)","Random float in [0,1)"};
    d["random-int"]={"(random-int lo hi)","Random integer in [lo,hi]"};
    // Compare
    d["=="]={"(== a b)","Equal"}; d["!="]={"(!= a b)","Not equal"};
    d["<"]={"(< a b)","Less than"}; d[">"]={"(> a b)","Greater than"};
    d["<="]={"(<= a b)","Less or equal"}; d[">="]={"(>= a b)","Greater or equal"};
    // Logic
    d["and"]={"(and a b)","Logical AND"}; d["or"]={"(or a b)","Logical OR"};
    d["not"]={"(not a)","Logical NOT"};
    // List
    d["list"]={"(list v1 v2 ...)","Create list"};
    d["car"]={"(car lst)","First element"}; d["cdr"]={"(cdr lst)","Rest of list"};
    d["cons"]={"(cons v lst)","Prepend value to list"};
    d["nth"]={"(nth lst i)","Nth element (0-based)"};
    d["push"]={"(push lst v)","Append value, returns new list"};
    d["range"]={"(range n) / (range start end)","Generate number list"};
    // Function
    d["map"]={"(map fn lst)","Apply fn to each element"};
    d["filter"]={"(filter fn lst)","Keep elements where fn returns truthy"};
    d["fold"]={"(fold fn init lst)","Reduce list with accumulator"};
    d["apply"]={"(apply fn args-list)","Call fn with args from list"};
    // String
    d["str"]={"(str v1 ...)","Concatenate values as string"};
    d["num"]={"(num v)","Convert to number"};
    d["substr"]={"(substr s start len?)","Extract substring"};
    d["str-split"]={"(str-split s delim)","Split string into list"};
    d["str-join"]={"(str-join lst delim?)","Join list into string"};
    d["str-replace"]={"(str-replace s old new)","Replace all occurrences"};
    d["str-find"]={"(str-find s sub)","Find index of substring (-1 if not found)"};
    d["str-trim"]={"(str-trim s)","Trim whitespace from both ends"};
    d["str-upper"]={"(str-upper s)","Convert to uppercase"};
    d["str-lower"]={"(str-lower s)","Convert to lowercase"};
    d["str-starts?"]={"(str-starts? s prefix)","Check if starts with prefix"};
    d["str-ends?"]={"(str-ends? s suffix)","Check if ends with suffix"};
    d["str-repeat"]={"(str-repeat s n)","Repeat string n times"};
    d["str-reverse"]={"(str-reverse s)","Reverse string"};
    d["char-code"]={"(char-code s)","ASCII/Unicode code of first char"};
    d["code-char"]={"(code-char n)","Char from code"};
    // File
    d["file-read"]={"(file-read path)","Read entire file as string"};
    d["file-write"]={"(file-write path content)","Write content to file (overwrite)"};
    d["file-append"]={"(file-append path content)","Append content to file"};
    d["file-exists?"]={"(file-exists? path)","Check if file exists"};
    d["file-delete"]={"(file-delete path)","Delete file"};
    d["file-lines"]={"(file-lines path)","Read file as list of lines"};
    // Dir
    d["dir-list"]={"(dir-list path)","List directory entries"};
    d["dir-exists?"]={"(dir-exists? path)","Check if directory exists"};
    d["dir-create"]={"(dir-create path)","Create directory"};
    // Path
    d["path-join"]={"(path-join a b)","Join path components"};
    d["path-ext"]={"(path-ext path)","Get file extension"};
    d["path-name"]={"(path-name path)","Get filename without directory"};
    // Other
    d["time-now"]={"(time-now)","Current Unix timestamp (seconds)"};
    d["break"]={"(break)","Break out of while loop"};
    d["throw"]={"(throw val)","Raise error"};
    d["error"]={"(error msg)","Raise error with message"};
    d["write"]={"(write val)","Serialize value to string"};
    d["read"]={"(read s)","Deserialize string to value"};
    d["test"]={"(test actual expected)","Compare values, report pass/fail"};
    d["assert"]={"(assert cond) / (assert cond msg)","Assert condition is truthy, throw if not"};
    d["json-parse"]={"(json-parse str)","Parse JSON string to ALB value"};
    d["json-serialize"]={"(json-serialize val)","Convert ALB value to JSON string"};
    d["doc"]={"(doc fn)","Show function documentation"};
    // Special forms
    d["let"]={"(let name val)","Define variable (also: let name (func ...) for functions)"};
    d["const"]={"(const name val)","Define immutable binding"};
    d["set"]={"(set name val)","Update existing variable"};
    d["+="]={"(+= name val)","Compound add (also -=, *=, /=)"};
    d["if"]={"(if cond then else?)","Conditional expression"};
    d["when"]={"(when cond body...)","Execute body if cond is truthy (no else)"};
    d["while"]={"(while cond body...)","Loop while cond is truthy"};
    d["do"]={"(do expr...)","Block: evaluate expressions, return last"};
    d["func"]={"(func (list params...) \"doc\" body...)","Create function"};
    d["try"]={"(try body (catch e handler))","Error handling"};
    d["use"]={"(use \"mod\") / (use \"m\" :as p) / (use \"m\" :only (f g))","Import module"};
    d["defmacro"]={"(defmacro name (params...) body)","Define macro (code transformer)"};
    d["export"]={"(export name1 name2 ...)","Mark names for module export"};
    d["struct"]={"(struct Name (field1 field2 ...))","Define struct type with constructor and accessors"};
    d["define"]={"(define name value) / (define (name params...) body)","Top-level definition (alias for let with function shorthand)"};
    d["let*"]={"(let* ((x 1) (y (+ x 1))) body...)","Sequential bindings (later ones can reference earlier)"};
    d["letrec"]={"(letrec ((even? ...) (odd? ...)) body...)","Recursive bindings (mutually recursive functions)"};
    d["cond"]={"(cond (test1 expr1) (test2 expr2) ... (else exprN))","Multi-way conditional"};
    d["for-i"]={"(for-i (var init cond step) body...)","C-style indexed loop"};
    d["match"]={"(match val pat1 body1 ...)","Pattern match: values, lists (list ...), structs (Type ...), wildcards (_), rest ($)"};
    // Vector
    d["vector"]={"(vector v1 v2 ...)","Create vector (O(1) random access)"};
    d["vector-ref"]={"(vector-ref v i)","Get vector element at index i"};
    d["vector-set!"]={"(vector-set! v i val)","Set vector element at index i"};
    d["vector-push!"]={"(vector-push! v val)","Append to vector"};
    d["vector-pop!"]={"(vector-pop! v)","Remove and return last element"};
    d["vector-size"]={"(vector-size v)","Vector size"};
    d["vector->list"]={"(vector->list v)","Convert vector to list"};
    d["list->vector"]={"(list->vector lst)","Convert list to vector"};
    // Map
    d["dict"]={"(dict) / (dict k1 v1 k2 v2 ...)","Create map from key-value pairs"};
    d["map-get"]={"(map-get m key)","Get value from map"};
    d["map-set"]={"(map-set m key val)","Set value in map (returns new map)"};
    d["map-del"]={"(map-del m key)","Delete key from map"};
    d["map-has?"]={"(map-has? m key)","Check if key exists"};
    d["map-keys"]={"(map-keys m)","List of all keys"};
    d["map-values"]={"(map-values m)","List of all values"};
    d["map-merge"]={"(map-merge m1 m2)","Merge two maps"};
    // Struct
    d["struct-get"]={"(struct-get obj \"field\")","Get struct field by name"};
    d["struct-set!"]={"(struct-set! obj \"field\" val)","Set struct field by name"};
    // Regex
    d["re-match"]={"(re-match pattern str)","Return regex match list or nil"};
    d["re-find"]={"(re-find pattern str)","Return first regex match"};
    d["re-replace"]={"(re-replace pattern str replacement)","Replace all regex matches"};
    d["re-split"]={"(re-split pattern str)","Split string by regex"};
    d["re-test?"]={"(re-test? pattern str)","Test if regex matches (returns 0/1)"};
    // Higher-order
    d["sort"]={"(sort lst fn?)","Sort list (optional comparison fn)"};
    d["reverse"]={"(reverse lst)","Reverse list or string"};
    d["zip"]={"(zip lst1 lst2)","Zip two lists into pairs"};
    d["unzip"]={"(unzip lst)","Unzip pairs into two lists"};
    d["any"]={"(any fn lst)","True if any element satisfies fn"};
    d["all"]={"(all fn lst)","True if all elements satisfy fn"};
    d["compose"]={"(compose f g)","Function composition (f . g)"};
    d["pipe"]={"(pipe f g)","Function pipe (f then g)"};
    d["partial"]={"(partial fn arg1 ...)","Partial application"};
    d["for"]={"(for lst fn)","Execute fn for each element (side effects)"};
    d["each"]={"(each lst fn)","Alias for for"};
    d["flatten"]={"(flatten lst)","Flatten nested lists"};
    d["uniq"]={"(uniq lst)","Remove duplicates"};
    d["take"]={"(take n lst)","Take first n elements"};
    d["drop"]={"(drop n lst)","Drop first n elements"};
    d["copy"]={"(copy val)","Deep copy value"};
    // Bit ops
    d["bit-and"]={"(bit-and a b)","Bitwise AND"};
    d["bit-or"]={"(bit-or a b)","Bitwise OR"};
    d["bit-xor"]={"(bit-xor a b)","Bitwise XOR"};
    d["bit-not"]={"(bit-not a)","Bitwise NOT"};
    d["bit-lshift"]={"(bit-lshift a n)","Left shift"};
    d["bit-rshift"]={"(bit-rshift a n)","Right shift"};
    // Time
    d["time-format"]={"(time-format ts fmt?)","Format timestamp to string"};
    d["time-parse"]={"(time-parse str fmt?)","Parse time string to timestamp"};
    d["time-diff"]={"(time-diff a b)","Difference in seconds"};
    d["time-sleep"]={"(time-sleep ms)","Sleep for milliseconds"};
    d["time-now-ms"]={"(time-now-ms)","Current time in milliseconds"};
    d["time-local"]={"(time-local)","Local time as map {year month day hour min sec}"};
    // System
    d["exit"]={"(exit code?)","Exit program with code (default 0)"};
    d["sleep"]={"(sleep ms)","Sleep for milliseconds"};
    d["system"]={"(system cmd)","Run system command, return exit code"};
    d["exec"]={"(exec cmd)","Run system command, return output string"};
    // Network
    d["net-connect"]={"(net-connect host port)","TCP connect, return socket handle"};
    d["net-send"]={"(net-send sock msg)","Send data on socket"};
    d["net-recv"]={"(net-recv sock bufsize?)","Receive data from socket"};
    d["net-close"]={"(net-close sock)","Close socket"};
    d["net-listen"]={"(net-listen port)","TCP listen on port"};
    d["net-accept"]={"(net-accept sock)","Accept TCP connection"};
    d["http-get"]={"(http-get url)","HTTP GET request, return body"};
    d["http-post"]={"(http-post url data)","HTTP POST request, return body"};
    return d;
}

static void show_help_topic(const string& topic) {
    // 1. Check built-in docs
    auto docs = get_builtin_docs();
    auto it = docs.find(topic);
    if(it != docs.end()) {
        cout << "  " << it->second.first << "\n";
        cout << "    " << it->second.second << "\n";
        return;
    }
    // 2. Check library module
    string path = topic;
    { ifstream test(path); if(!test.good()) path = resolve_module(topic); }
    if(!path.empty()) {
        string folder;
        if(path.size()>7 && path.substr(0,7)=="folder:")
            folder = path.substr(7);
        else {
            size_t sl = path.find_last_of("/\\");
            if(sl != string::npos) folder = path.substr(0, sl);
        }
        if(!folder.empty()) {
            cout << "Library: " << topic << "\n" << string(50,'-') << "\n";
            // Print __init__.alb header comments
            string init_path = folder + "/__init__.alb";
            {
                ifstream inf(init_path);
                if(inf) {
                    string line;
                    while(getline(inf, line)) {
                        size_t f = line.find_first_not_of(" \t");
                        if(f != string::npos && line[f]==';') {
                            string c = line.substr(f+1);
                            if(!c.empty() && c[0]==' ') c = c.substr(1);
                            cout << "  " << c << "\n";
                        } else if(!line.empty() && line.find_first_not_of(" \t\r\n")!=string::npos) break;
                    }
                }
            }
            // Scan .alb files for function/const definitions
            cout << "\nFunctions:\n";
            vector<DirEntry> entries;
            scan_dir_entries(folder, entries);
            {
                vector<string> files;
                for(auto& e : entries) {
                    if(!e.is_dir && e.name.size()>=4 && e.name.substr(e.name.size()-4)==".alb"
                       && e.name != "__init__.alb" && e.name != "cli.alb")
                        files.push_back(folder + "/" + e.name);
                }
                sort(files.begin(), files.end());
                for(auto& fp : files) {
                    ifstream inf(fp);
                    if(!inf) continue;
                    string src((istreambuf_iterator<char>(inf)), istreambuf_iterator<char>());
                    size_t pos = 0;
                    int depth = 0;
                    while(pos < src.size()) {
                        // Track paren depth to only match top-level definitions
                        if(src[pos]=='(' && depth==0) {
                            // Check for (let or (const at depth 0
                            size_t save = pos;
                            if(pos+5 <= src.size() && src.substr(pos,5)=="(let "
                               && src[pos+5]!=' ') {
                                pos += 5;
                                goto parse_let;
                            }
                            if(pos+7 <= src.size() && src.substr(pos,7)=="(const "
                               && src[pos+7]!=' ') {
                                pos += 7;
                                goto parse_const;
                            }
                            depth++;
                            pos++;
                            continue;
                        }
                        if(src[pos]=='(') depth++;
                        else if(src[pos]==')') { if(depth>0) depth--; }
                        else if(src[pos]==';') {
                            // Skip comment line
                            while(pos < src.size() && src[pos]!='\n') pos++;
                            continue;
                        } else if(src[pos]=='"') {
                            // Skip string literal
                            pos++;
                            while(pos < src.size() && src[pos]!='"') {
                                if(src[pos]=='\\') pos++;
                                pos++;
                            }
                        }
                        pos++;
                        continue;
                    parse_let: {
                            while(pos < src.size() && (src[pos]==' '||src[pos]=='\t')) pos++;
                            size_t ns = pos;
                            while(pos < src.size() && src[pos]!=' ' && src[pos]!='(' && src[pos]!='\n') pos++;
                            string name = src.substr(ns, pos-ns);
                            while(pos < src.size() && (src[pos]==' '||src[pos]=='\t')) pos++;
                            if(pos+5 <= src.size() && src.substr(pos,5)=="(func") {
                                pos += 5;
                                while(pos < src.size() && (src[pos]==' '||src[pos]=='\t')) pos++;
                                string params_str;
                                if(pos < src.size() && src[pos]=='(') {
                                    int d2 = 0; size_t ps = pos;
                                    for(; pos < src.size(); pos++) {
                                        if(src[pos]=='(') d2++;
                                        else if(src[pos]==')') { d2--; if(d2==0) { pos++; break; } }
                                    }
                                    string raw = src.substr(ps+1, pos-ps-2);
                                    size_t li = raw.find("list ");
                                    if(li==0 || (li!=string::npos && raw[li-1]=='('))
                                        raw = raw.substr(raw.find("list ")+5);
                                    string cl;
                                    for(char c : raw) if(c!='(' && c!=')') cl += c;
                                    params_str = cl;
                                }
                                { string t; bool sp=false;
                                  for(char c : params_str) {
                                    if(c==' '||c=='\t'||c=='\n'||c=='\r') { if(!t.empty()&&!sp){t+=' ';sp=true;} }
                                    else { t+=c; sp=false; }
                                  }
                                  params_str = t;
                                }
                                while(pos < src.size() && (src[pos]==' '||src[pos]=='\t'||src[pos]=='\n'||src[pos]=='\r')) pos++;
                                string doc;
                                if(pos < src.size() && src[pos]=='"') {
                                    pos++;
                                    while(pos < src.size() && src[pos]!='"') {
                                        if(src[pos]=='\\' && pos+1<src.size()) {
                                            char nx=src[pos+1];
                                            if(nx=='n') doc+='\n'; else if(nx=='t') doc+='\t';
                                            else doc+=nx; pos+=2;
                                        } else { doc+=src[pos]; pos++; }
                                    }
                                    if(pos < src.size()) pos++;
                                }
                                cout << "  (" << name;
                                if(!params_str.empty()) cout << " " << params_str;
                                cout << ")";
                                if(!doc.empty()) cout << "  " << doc;
                                cout << "\n";
                            }
                            // Skip to matching close paren
                            int d2 = 1;
                            while(pos < src.size() && d2 > 0) {
                                if(src[pos]=='(') d2++;
                                else if(src[pos]==')') d2--;
                                else if(src[pos]=='"') {
                                    pos++;
                                    while(pos < src.size() && src[pos]!='"') {
                                        if(src[pos]=='\\') pos++;
                                        pos++;
                                    }
                                }
                                pos++;
                            }
                            continue;
                        }
                    parse_const: {
                            while(pos < src.size() && (src[pos]==' '||src[pos]=='\t')) pos++;
                            size_t ns = pos;
                            while(pos < src.size() && src[pos]!=' ' && src[pos]!='\n' && src[pos]!=')') pos++;
                            string name = src.substr(ns, pos-ns);
                            while(pos < src.size() && (src[pos]==' '||src[pos]=='\t')) pos++;
                            size_t vs = pos;
                            int d2 = 0;
                            while(pos < src.size()) {
                                if(src[pos]=='(') d2++;
                                else if(src[pos]==')') { if(d2==0) break; d2--; }
                                else if(d2==0 && (src[pos]=='\n'||src[pos]==';')) break;
                                pos++;
                            }
                            string val = src.substr(vs, pos-vs);
                            while(!val.empty() && (val.back()==' '||val.back()=='\t'||val.back()=='\r')) val.pop_back();
                            cout << "  (const " << name;
                            if(!val.empty()) cout << " " << val;
                            cout << ")\n";
                            // Skip to matching close paren
                            int d3 = 1;
                            while(pos < src.size() && d3 > 0) {
                                if(src[pos]=='(') d3++;
                                else if(src[pos]==')') d3--;
                                else if(src[pos]=='"') {
                                    pos++;
                                    while(pos < src.size() && src[pos]!='"') {
                                        if(src[pos]=='\\') pos++;
                                        pos++;
                                    }
                                }
                                pos++;
                            }
                            continue;
                        }
                    }
                }
            }
            return;
        }
    }
    // 3. List available libraries
    cout << "Unknown topic: " << topic << "\n\n";
    cout << "Available libraries:\n";
    string exe_dir = get_exe_dir();
    vector<string> lib_bases = {"lib", exe_dir+"/lib"};
    string parent_dir = exe_dir.substr(0, exe_dir.find_last_of("\\/"));
    if(!parent_dir.empty()) {
        string pl = parent_dir+"/lib";
        if(pl!="lib" && pl!=exe_dir+"/lib") lib_bases.push_back(pl);
    }
    unordered_set<string> shown;
    for(auto& lb : lib_bases) {
        vector<DirEntry> entries2;
        scan_dir_entries(lb, entries2);
        for(auto& e : entries2) {
            string n = e.name;
            if(!e.is_dir) continue;
            if(shown.count(n)) continue;
            string ip = lb+"/"+n+"/__init__.alb";
            if(ifstream(ip).good()) { cout << "  " << n << "\n"; shown.insert(n); }
        }
    }
    cout << "\nUse ALB -h <name> to see details.\n";
}

static void show_help_list() {
    cout << "ALB v" << __version__ << " - All Built-in Functions\n" << string(50,'=') << "\n\n";
    struct Cat { const char* name; vector<string> fns; };
    Cat cats[] = {
        {"I/O", {"print","prints","input","press","type","len"}},
        {"Arithmetic", {"+","-","*","/","%","^"}},
        {"Math", {"sqrt","abs","floor","ceil","round","sin","cos","tan",
                   "asin","acos","atan","atan2","log","log2","log10","exp",
                   "min","max","clamp","random","random-int"}},
        {"Compare", {"==","!=","<",">","<=",">="}},
        {"Logic", {"and","or","not"}},
        {"List", {"list","car","cdr","cons","nth","push","range"}},
        {"Vector", {"vector","vector-ref","vector-set!","vector-push!",
                     "vector-pop!","vector-size","vector->list","list->vector"}},
        {"Map", {"dict","map-get","map-set","map-del","map-has?","map-keys","map-values","map-merge"}},
        {"Struct", {"struct-get","struct-set!"}},
        {"Function", {"map","filter","fold","apply"}},
        {"Higher-order", {"sort","reverse","zip","unzip","any","all",
                          "compose","pipe","partial","for","each",
                          "flatten","uniq","take","drop","copy"}},
        {"Regex", {"re-match","re-find","re-replace","re-split","re-test?"}},
        {"String", {"str","num","substr","str-split","str-join","str-replace",
                     "str-find","str-trim","str-upper","str-lower",
                     "str-starts?","str-ends?","str-repeat","str-reverse",
                     "char-code","code-char"}},
        {"Bit Ops", {"bit-and","bit-or","bit-xor","bit-not","bit-lshift","bit-rshift"}},
        {"File I/O", {"file-read","file-write","file-append","file-exists?",
                       "file-delete","file-lines"}},
        {"Directory", {"dir-list","dir-exists?","dir-create"}},
        {"Path", {"path-join","path-ext","path-name"}},
        {"Time", {"time-now","time-format","time-parse","time-diff",
                   "time-sleep","time-now-ms","time-local"}},
        {"Network", {"net-connect","net-send","net-recv","net-close",
                      "net-listen","net-accept","http-get","http-post"}},
        {"System", {"break","throw","error","write","read","test","doc",
                     "exit","sleep","system","exec"}},
    };
    auto docs = get_builtin_docs();
    for(auto& cat : cats) {
        cout << "[" << cat.name << "]\n";
        for(auto& fn : cat.fns) {
            auto it = docs.find(fn);
            if(it != docs.end()) {
                printf("  %-30s  %s\n", it->second.first.c_str(), it->second.second.c_str());
            }
        }
        cout << "\n";
    }
    cout << "[Special Forms]\n";
    for(auto& sf : {"let","const","set","+=","if","when","while","do","func",
                     "match","try","use","throw","break",
                     "defmacro","export","struct","define","let*","letrec","cond","for-i"}) {
        auto it = docs.find(sf);
        if(it != docs.end()) {
            printf("  %-30s  %s\n", it->second.first.c_str(), it->second.second.c_str());
        }
    }
    cout << "\n[Constants]  pi, e, nil, true, false, real, unrl, rl, ul\n";
    cout << "\nUse ALB -h <name> for details, or ALB -h <lib> for library help.\n";
}

// ========== Main ==========
void show_help() {
    cout<<"ALB v"<<__version__<<" - A micro Lisp language\n\n"
        <<"Usage:\n"
        <<"  ALB                 Start REPL (interactive)\n"
        <<"  ALB <file.alb>      Run file as interpreter\n"
        <<"  ALB -c <code>       Execute code string\n"
        <<"  ALB -m <module>     Run a module (searches .alb, lib/)\n"
        <<"  ALB -e <file.alb>   Execute file, then enter REPL\n"
        <<"  ALB -d <file.alb>   Debug mode (show AST before run)\n"
        <<"  ALB -b <file.alb>   Build standalone executable\n"
        <<"  ALB -b <file> -o <out> Build with custom output name\n"
        <<"  ALB -b <file> --embed  Build with libraries bundled into exe\n"
        <<"  ALB --register      Register .alb file association\n"
        <<"  ALB -v              Show version\n"
        <<"  ALB -h              Show this help\n"
        <<"  ALB -h <name>       Show help for function or library\n"
        <<"  ALB -h --list       List all built-in functions\n\n"
        <<"Bytecode & Distribution (v0.1.1):\n"
        <<"  ALB compile <file.alb>             Compile to bytecode (.albc)\n"
        <<"  ALB compile <file> --obfuscate     Compile with string obfuscation\n"
        <<"  ALB run <file.albc>                Run bytecode file\n"
        <<"  ALB run <file.alb>                 Auto-detect and run source or bytecode\n"
        <<"  ALB package <file.alb>             Package into distribution zip\n"
        <<"  ALB package <file> -o <out.zip>    Package with custom output name\n"
        <<"  ALB package <file> --obfuscate     Package with obfuscation enabled\n\n"
        <<"Package Manager:\n"
        <<"  ALB -m install <url|file> [true]   Install package, optionally compile C++ ext\n"
        <<"  ALB -m list                        List installed packages\n"
        <<"  ALB -m remove <name>               Remove installed package\n\n"
        <<"Syntax:\n"
        <<"  (f arg1 arg2 ...)       function call\n"
        <<"  (let x 42)              define variable\n"
        <<"  (const x 42)            define immutable variable\n"
        <<"  (set x 99)              update variable\n"
        <<"  (+= x 1)                compound assignment (-=, *=, /=)\n"
        <<"  (let f (func (list x) ( define function\n"
        <<"    (body...)))           :pure :io :sys capability tags\n"
        <<"  (if cond a b)           conditional\n"
        <<"  (when cond body)        conditional (no else)\n"
        <<"  (while cond body)       loop\n"
        <<"  (do expr...)            block\n"
        <<"  (match val pat1 body1   pattern matching\n"
        <<"    pat2 body2 ...)       _ = wildcard, $ = rest\n"
        <<"  (try body               error handling\n"
        <<"    (catch e handler))\n"
        <<"  (throw value)           raise structured error\n"
        <<"  (use \"mod\")             import module\n"
        <<"  (use \"m\" :as p)         import with prefix\n"
        <<"  (use \"m\" :only (f g))   selective import\n"
        <<"  ; comment\n\n"
        <<"Built-ins:\n"
        <<"  I/O:     print, prints, input, type, len\n"
        <<"  Math:    +, -, *, /, %, ^, sqrt, abs, floor, ceil, round\n"
        <<"  Compare: ==, !=, <, >, <=, >=\n"
        <<"  Logic:   and, or, not\n"
        <<"  List:    list, car, cdr, cons, nth, push, range\n"
        <<"  Func:    map, filter, fold, apply\n"
        <<"  String:  str, num, substr\n"
        <<"  Error:   try, throw, error\n"
        <<"  Serial:  write, read\n"
        <<"  Test:    test, assert\n"
        <<"  Meta:    doc, type\n"
        <<"  JSON:    json-parse, json-serialize\n"
        <<"  Const:   pi, e, real, unrl, nil\n\n"
        <<"Library Structure (lib/<name>/):\n"
        <<"  __init__.alb   Entry point, aggregates submodules\n"
        <<"  <name>.cpp     C++ extension (auto-compiled to DLL)\n"
        <<"  module.alb     Submodule files (auto-loaded by use)\n"
        <<"  cli.alb        CLI entry point (for ALB -m <name>)\n";
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    // Detect IDE mode
    if(getenv("ALB_IDE_MODE")) {
        alb_ide_mode = true;
        setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered stdout for pipe mode
    }
    auto_add_to_path();
    auto_file_assoc();
    
    auto env=make_shared<Env>();
    register_builtins(env);

    enum { REPL_MODE, FILE_MODE, CODE_MODE, MODULE_MODE, EXEC_REPL_MODE, DEBUG_MODE,
           PKG_INSTALL_MODE, PKG_LIST_MODE, PKG_REMOVE_MODE, BUILD_MODE,
           COMPILE_MODE, RUN_BC_MODE, PACKAGE_MODE } mode = REPL_MODE;
    string arg;
    bool arg_set_dll = false;
    string build_output;
    bool build_embed = false;
    bool compile_obfuscate = false;
    string package_output;

    if(argc==1) {
        mode=REPL_MODE;
    } else {
        for(int i=1;i<argc;i++){
            string a=argv[i];
            if(a=="-c")      { mode=CODE_MODE;      if(i+1<argc) arg=argv[++i]; else {cerr<<"-c requires code\n";return 1;} }
            else if(a=="-m") {
                // Check for package manager subcommands
                if(i+1<argc && string(argv[i+1])=="install") {
                    mode=PKG_INSTALL_MODE;
                    i++; // skip "install"
                    if(i+1<argc) arg=argv[++i];
                    else {cerr<<"-m install requires URL or file path\n";return 1;}
                    // Optional: set_dll parameter (default false)
                    if(i+1<argc) {
                        string next=argv[i+1];
                        if(next=="true"||next=="1") { arg_set_dll=true; i++; }
                        else if(next=="false"||next=="0") { arg_set_dll=false; i++; }
                    }
                } else if(i+1<argc && string(argv[i+1])=="list") {
                    mode=PKG_LIST_MODE;
                    i++;
                } else if(i+1<argc && string(argv[i+1])=="remove") {
                    mode=PKG_REMOVE_MODE;
                    i++;
                    if(i+1<argc) arg=argv[++i];
                    else {cerr<<"-m remove requires package name\n";return 1;}
                } else {
                    mode=MODULE_MODE;
                    if(i+1<argc) arg=argv[++i];
                    else {cerr<<"-m requires module\n";return 1;}
                }
            }
            else if(a=="-e") { mode=EXEC_REPL_MODE;  if(i+1<argc) arg=argv[++i]; else {cerr<<"-e requires file\n";return 1;} }
            else if(a=="-d") { mode=DEBUG_MODE;      if(i+1<argc) arg=argv[++i]; else {cerr<<"-d requires file\n";return 1;} }
            else if(a=="-b") {
                mode=BUILD_MODE;
                if(i+1<argc) arg=argv[++i]; else {cerr<<"-b requires source file\n";return 1;}
                // Optional flags: -o output, --embed
                while(i+1<argc && argv[i+1][0]=='-') {
                    string next=argv[i+1];
                    if(next=="-o") {
                        i++; // skip -o
                        build_output=argv[++i];
                    } else if(next=="--embed") {
                        build_embed=true;
                        i++;
                    } else break;
                }
            }
            else if(a=="compile") {
                mode=COMPILE_MODE;
                if(i+1<argc) arg=argv[++i]; else {cerr<<"compile requires source file\n";return 1;}
                // Optional: --obfuscate
                while(i+1<argc && string(argv[i+1]).substr(0,2)=="--") {
                    string next=argv[i+1];
                    if(next=="--obfuscate") { compile_obfuscate=true; i++; }
                    else break;
                }
            }
            else if(a=="run") {
                mode=RUN_BC_MODE;
                if(i+1<argc) arg=argv[++i]; else {cerr<<"run requires .albc file\n";return 1;}
            }
            else if(a=="package") {
                mode=PACKAGE_MODE;
                if(i+1<argc) arg=argv[++i]; else {cerr<<"package requires source file\n";return 1;}
                // Optional: -o output, --obfuscate
                while(i+1<argc && argv[i+1][0]=='-') {
                    string next=argv[i+1];
                    if(next=="-o") { i++; package_output=argv[++i]; }
                    else if(next=="--obfuscate") { compile_obfuscate=true; i++; }
                    else break;
                }
            }
            else if(a=="-v"||a=="--version") { cout<<"ALB v"<<__version__<<"\n"; return 0; }
            else if(a=="--register"){ auto_file_assoc(); cout<<"File association registered.\n"; return 0; }
            else if(a=="-h"||a=="--help") {
                if(i+1 < argc && string(argv[i+1])=="--list") {
                    i++;
                    show_help_list();
                } else if(i+1 < argc && argv[i+1][0] != '-') {
                    show_help_topic(argv[++i]);
                } else {
                    show_help();
                }
                return 0;
            }
            else if(a[0]=='-') { cerr<<"Unknown option: "<<a<<"\nUse -h for help\n"; return 1; }
            else { mode=FILE_MODE; arg=a; }
        }
    }

    try {
        switch(mode) {
        case REPL_MODE:
            run_repl(env);
            break;

        case FILE_MODE:
        case DEBUG_MODE: {
            // Auto-detect bytecode files
            if(is_bytecode_file(arg)) {
                run_bytecode_file(arg, env);
            } else {
                ifstream f(arg, ios::binary);
                if(!f){cerr<<"Cannot open: "<<arg<<"\n";return 1;}
                ostringstream ss; ss<<f.rdbuf();
                string code=ss.str();
                code.erase(remove(code.begin(),code.end(),'\r'),code.end());
                run_code(code, env, mode==DEBUG_MODE);
            }
            break;
        }

        case CODE_MODE:
            run_code(arg, env);
            break;

        case MODULE_MODE: {
            string path=resolve_module(arg);
            if(path.empty()){cerr<<"Module not found: "<<arg<<"\n";return 1;}

            if(path.size()>7 && path.substr(0,7)=="folder:") {
                // Folder-based module
                string folder = path.substr(7);
                string init_alb = folder + "/__init__.alb";
                string cli_alb = folder + "/cli.alb";
                string dll_path = folder + "/" + arg + ".dll";

                compile_cpp_dll(folder, arg);
                { ifstream test(dll_path); if(test.good()) load_dll_into(dll_path, env); }

                // Load all submodules
                vector<DirEntry> entries;
                scan_dir_entries(folder, entries);
                vector<string> sub_albs;
                for(auto& e : entries) {
                    if(!e.is_dir && e.name.size()>=4 && e.name.substr(e.name.size()-4)==".alb"
                       && e.name != "__init__.alb" && e.name != "cli.alb")
                        sub_albs.push_back(folder + "/" + e.name);
                }
                sort(sub_albs.begin(), sub_albs.end());
                for(auto& sf : sub_albs) {
                    ifstream sf2(sf, ios::binary);
                    if(sf2) {
                        ostringstream ss; ss<<sf2.rdbuf();
                        string c=ss.str();
                        c.erase(remove(c.begin(),c.end(),'\r'),c.end());
                        run_code(c, env);
                    }
                }

                // Load __init__.alb
                { ifstream test(init_alb, ios::binary);
                  if(test.good()) {
                      ostringstream ss; ss<<test.rdbuf();
                      string c=ss.str();
                      c.erase(remove(c.begin(),c.end(),'\r'),c.end());
                      run_code(c, env);
                  }
                }

                // Run cli.alb as entry point
                { ifstream cli(cli_alb, ios::binary);
                  if(!cli) { cerr<<"No cli.alb found in: "<<folder<<"\n"; return 1; }
                  ostringstream ss; ss<<cli.rdbuf();
                  string code=ss.str();
                  code.erase(remove(code.begin(),code.end(),'\r'),code.end());
                  run_code(code, env);
                }
            } else {
                ifstream f(path, ios::binary);
                if(!f){cerr<<"Cannot open: "<<path<<"\n";return 1;}
                ostringstream ss; ss<<f.rdbuf();
                string code=ss.str();
                code.erase(remove(code.begin(),code.end(),'\r'),code.end());
                run_code(code, env);
            }
            break;
        }

        case EXEC_REPL_MODE: {
            ifstream f(arg, ios::binary);
            if(!f){cerr<<"Cannot open: "<<arg<<"\n";return 1;}
            ostringstream ss; ss<<f.rdbuf();
            string code=ss.str();
            code.erase(remove(code.begin(),code.end(),'\r'),code.end());
            run_code(code, env);
            cout<<"[loaded "<<arg<<", entering REPL]\n";
            run_repl(env);
            break;
        }

        case PKG_INSTALL_MODE:
            return pkg_install(arg, arg_set_dll);

        case PKG_LIST_MODE:
            return pkg_list();

        case PKG_REMOVE_MODE:
            return pkg_remove(arg);

        case BUILD_MODE: {
            // Compile ALB source to standalone executable
            string src_file = arg;
            string out_file = build_output;
            if(out_file.empty()) {
                // Default: same name as source
                out_file = src_file;
                if(out_file.size()>4 && out_file.substr(out_file.size()-4)==".alb")
                    out_file = out_file.substr(0, out_file.size()-4);
#ifdef _WIN32
                out_file += ".exe";
#endif
            }

            // Read ALB source
            ifstream src_f(src_file, ios::binary);
            if(!src_f) { cerr<<"Cannot open source: "<<src_file<<"\n"; return 1; }
            ostringstream src_ss; src_ss<<src_f.rdbuf();
            string alb_src = src_ss.str();
            src_f.close();

            // Read runtime template
#ifdef _WIN32
            string runtime_path = get_exe_dir() + "\\alb_runtime.cpp";
            ifstream rt_f(runtime_path, ios::binary);
            if(!rt_f) {
                runtime_path = get_exe_dir() + "\\..\\alb_runtime.cpp";
                rt_f.open(runtime_path, ios::binary);
            }
#else
            string runtime_path = get_exe_dir() + "/alb_runtime.cpp";
            ifstream rt_f(runtime_path, ios::binary);
            if(!rt_f) {
                runtime_path = get_exe_dir() + "/../alb_runtime.cpp";
                rt_f.open(runtime_path, ios::binary);
            }
#endif
            if(!rt_f) { cerr<<"Runtime template not found: alb_runtime.cpp\n"; return 1; }
            ostringstream rt_ss; rt_ss<<rt_f.rdbuf();
            string runtime_tpl = rt_ss.str();
            rt_f.close();

            // ALB source is embedded in a C++ raw string literal R"ALB_SOURCE(...)"
            // so no escaping is needed — raw strings don't process escape sequences.
            // The only forbidden sequence is )ALB_SOURCE" which is virtually impossible
            // in real ALB code.

            // === Embed mode: bundle used libraries ===
            string embed_map_str;
            string embed_dll_data_str;  // static arrays (file scope)
            string embed_dll_init_str;  // push_back calls (inside constructor)
            if(build_embed) {
                cout<<"[embed] Scanning for (use ...) declarations...\n";
                // Scan for (use "name" patterns
                vector<string> used_modules;
                size_t scan_pos = 0;
                while(scan_pos < alb_src.size()) {
                    // Find (use
                    size_t use_pos = alb_src.find("(use", scan_pos);
                    if(use_pos == string::npos) break;
                    // Skip whitespace after "use"
                    size_t q1 = alb_src.find('"', use_pos+4);
                    if(q1 == string::npos) { scan_pos = use_pos+4; continue; }
                    size_t q2 = alb_src.find('"', q1+1);
                    if(q2 == string::npos) { scan_pos = q1+1; continue; }
                    string mod_name = alb_src.substr(q1+1, q2-q1-1);
                    used_modules.push_back(mod_name);
                    scan_pos = q2+1;
                }

                if(used_modules.empty()) {
                    cout<<"[embed] No (use ...) found, building without embedded modules.\n";
                } else {
                    cout<<"[embed] Found modules: ";
                    for(auto& m : used_modules) cout<<m<<" ";
                    cout<<"\n";

                    // Resolve and read each module
                    vector<pair<string,string>> bundled;
                    vector<pair<string,string>> bundled_dlls;  // name -> file path
                    for(auto& mod_name : used_modules) {
                        string path = mod_name;
                        { ifstream test(path); if(!test.good()) path = resolve_module(mod_name); }
                        if(path.empty()) {
                            cerr<<"[embed] WARNING: Module not found: "<<mod_name<<" (will rely on runtime)\n";
                            continue;
                        }

                        string module_src;
                        if(path.size()>7 && path.substr(0,7)=="folder:") {
                            // Folder-based library: read all .alb files
                            string folder = path.substr(7);
                            cout<<"[embed]   "<<mod_name<<" -> folder "<<folder<<"\n";
#ifdef _WIN32
                            WIN32_FIND_DATAA fd;
                            // Scan .alb files
                            string search = folder + "\\*.alb";
                            HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
                            vector<string> sub_albs;
                            if(hFind != INVALID_HANDLE_VALUE) {
                                do {
                                    string fn = fd.cFileName;
                                    if(fn != "__init__.alb" && fn != "cli.alb"
                                       && !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                                        sub_albs.push_back(folder + "/" + fn);
                                } while(FindNextFileA(hFind, &fd));
                                FindClose(hFind);
                            }
#else
                            vector<string> sub_albs;
                            { auto alb_files = list_dir_files(folder, ".alb");
                              for(auto& fn : alb_files)
                                if(fn != "__init__.alb" && fn != "cli.alb")
                                    sub_albs.push_back(folder + "/" + fn);
                            }
#endif
                            sort(sub_albs.begin(), sub_albs.end());
                            // Read sub-modules
                            for(auto& sf : sub_albs) {
                                ifstream mf(sf, ios::binary);
                                if(mf) { ostringstream ss; ss<<mf.rdbuf(); module_src += ss.str() + "\n"; }
                            }
                            // Read __init__.alb last
                            string init_alb = folder + "/__init__.alb";
                            ifstream mf(init_alb, ios::binary);
                            if(mf) { ostringstream ss; ss<<mf.rdbuf(); module_src += ss.str() + "\n"; }

                            // Scan for shared library files
#ifdef _WIN32
                            {
                                string search2 = folder + "\\*.dll";
                                HANDLE hFind2 = FindFirstFileA(search2.c_str(), &fd);
                                if(hFind2 != INVALID_HANDLE_VALUE) {
                                    do {
                                        string fn = fd.cFileName;
                                        if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                                            bundled_dlls.push_back({fn, folder + "/" + fn});
                                    } while(FindNextFileA(hFind2, &fd));
                                    FindClose(hFind2);
                                }
                            }
#else
#ifdef __APPLE__
                            { auto so_files = list_dir_files(folder, ".dylib");
                              for(auto& fn : so_files) bundled_dlls.push_back({fn, folder + "/" + fn}); }
#else
                            { auto so_files = list_dir_files(folder, ".so");
                              for(auto& fn : so_files) bundled_dlls.push_back({fn, folder + "/" + fn}); }
#endif
#endif
                        } else {
                            // Single file
                            cout<<"[embed]   "<<mod_name<<" -> "<<path<<"\n";
                            ifstream mf(path, ios::binary);
                            if(mf) { ostringstream ss; ss<<mf.rdbuf(); module_src = ss.str(); }
                        }
                        bundled.push_back({mod_name, module_src});
                    }

                    // Generate C++ assignment statements for struct constructor
                    for(auto& kv : bundled) {
                        // Escape for C++ string literal
                        string escaped;
                        for(char c : kv.second) {
                            if(c=='\\') escaped += "\\\\";
                            else if(c=='"') escaped += "\\\"";
                            else if(c=='\n') escaped += "\\n";
                            else if(c=='\r') {} // skip \r
                            else escaped += c;
                        }
                        embed_map_str += "EMBEDDED_MODULES[\"" + kv.first + "\"] = \"" + escaped + "\";\n        ";
                    }
                    cout<<"[embed] Bundled "<<bundled.size()<<" module(s).\n";
                    
                    // Generate DLL byte arrays
                    if(!bundled_dlls.empty()) {
                        for(auto& dll : bundled_dlls) {
                            ifstream dll_f(dll.second, ios::binary);
                            if(!dll_f) {
                                cerr<<"[embed] WARNING: Cannot read DLL: "<<dll.second<<"\n";
                                continue;
                            }
                            vector<unsigned char> data((istreambuf_iterator<char>(dll_f)), istreambuf_iterator<char>());
                            cout<<"[embed]   DLL: "<<dll.first<<" ("<<data.size()<<" bytes)\n";
                            
                            // Generate byte array at file scope
                            string arr_name = "dll_data_" + dll.first.substr(0, dll.first.size()-4);
                            for(char& c : arr_name) if(c=='.') c='_';
                            embed_dll_data_str += "static const unsigned char " + arr_name + "[] = {";
                            for(size_t i=0; i<data.size(); i++) {
                                if(i%16==0) embed_dll_data_str += "\n    ";
                                char buf[8];
                                sprintf(buf, "0x%02x,", data[i]);
                                embed_dll_data_str += buf;
                            }
                            embed_dll_data_str += "\n};\n";
                            // push_back inside constructor
                            embed_dll_init_str += "EMBEDDED_DLLS.push_back({\"" + dll.first + "\", " + arr_name + ", sizeof(" + arr_name + ")});\n        ";
                        }
                        cout<<"[embed] Embedded "<<bundled_dlls.size()<<" DLL(s).\n";
                    }
                }
            }

            // Replace embedded modules placeholder
            string emb_placeholder = "// __EMBEDDED_MODULES_INIT__";
            size_t emb_pos = runtime_tpl.find(emb_placeholder);
            if(emb_pos != string::npos) {
                runtime_tpl = runtime_tpl.substr(0, emb_pos) + embed_map_str + runtime_tpl.substr(emb_pos + emb_placeholder.size());
            }

            // Replace DLL data placeholder (file scope arrays)
            string dll_data_placeholder = "// __EMBEDDED_DLL_DATA__";
            size_t dll_data_pos = runtime_tpl.find(dll_data_placeholder);
            if(dll_data_pos != string::npos) {
                runtime_tpl = runtime_tpl.substr(0, dll_data_pos) + embed_dll_data_str + runtime_tpl.substr(dll_data_pos + dll_data_placeholder.size());
            }

            // Replace DLL init placeholder (inside constructor)
            string dll_init_placeholder = "// __EMBEDDED_DLLS_INIT__";
            size_t dll_init_pos = runtime_tpl.find(dll_init_placeholder);
            if(dll_init_pos != string::npos) {
                runtime_tpl = runtime_tpl.substr(0, dll_init_pos) + embed_dll_init_str + runtime_tpl.substr(dll_init_pos + dll_init_placeholder.size());
            }

            // Replace ALB source placeholder
            string placeholder = "__ALB_SOURCE_PLACEHOLDER__";
            size_t pos = runtime_tpl.find(placeholder);
            if(pos == string::npos) {
                cerr<<"Invalid runtime template\n";
                return 1;
            }
            string cpp_src = runtime_tpl.substr(0, pos) + alb_src + runtime_tpl.substr(pos + placeholder.size());

            // Write combined C++ file
            string cpp_file = out_file;
#ifdef _WIN32
            if(cpp_file.size()>4 && cpp_file.substr(cpp_file.size()-4)==".exe")
                cpp_file = cpp_file.substr(0, cpp_file.size()-4);
#endif
            cpp_file += "_build.cpp";

            ofstream cpp_f(cpp_file, ios::binary);
            if(!cpp_f) { cerr<<"Cannot write: "<<cpp_file<<"\n"; return 1; }
            cpp_f << cpp_src;
            cpp_f.close();

            // Find g++ compiler
            string gpp = find_gpp();
            if(gpp.empty()) {
#ifdef _WIN32
                cerr<<"g++ not found. Install MinGW or set ALB_GPP.\n";
#else
                cerr<<"g++ not found. Please install:\n";
                cerr<<"  Ubuntu/Debian: sudo apt-get install g++\n";
                cerr<<"  macOS: xcode-select --install\n";
                cerr<<"  Or set ALB_GPP environment variable.\n";
#endif
#ifdef _WIN32
                DeleteFileA(cpp_file.c_str());
#else
                unlink(cpp_file.c_str());
#endif
                return 1;
            }

            // Build command
#ifdef _WIN32
            string cmd = "\"" + gpp + "\" -std=c++11 -O2 -o \"" + out_file + "\" \"" + cpp_file + "\" 2>&1";
#elif defined(__APPLE__)
            // macOS: get SDK path for clang++
            string sdk_path;
            { FILE* p = popen("xcrun --show-sdk-path 2>/dev/null", "r");
              if(p) { char buf[1024]; if(fgets(buf,sizeof(buf),p)) { sdk_path=buf; while(!sdk_path.empty()&&(sdk_path.back()=='\n'||sdk_path.back()=='\r')) sdk_path.pop_back(); } pclose(p); } }
            string cmd;
            if(!sdk_path.empty())
                cmd = "\"" + gpp + "\" -isysroot \"" + sdk_path + "\" -std=c++11 -O2 -o \"" + out_file + "\" \"" + cpp_file + "\" 2>&1";
            else
                cmd = "\"" + gpp + "\" -std=c++11 -O2 -o \"" + out_file + "\" \"" + cpp_file + "\" 2>&1";
#else
            string cmd = "\"" + gpp + "\" -std=c++11 -O2 -o \"" + out_file + "\" \"" + cpp_file + "\" 2>&1";
#endif
            cout<<"Compiling "<<src_file<<" -> "<<out_file<<"...\n";
            cout.flush();

#ifdef _WIN32
            // If using bundled dll_min, prepend its bin dir to PATH
            bool using_bundled = (gpp.find("dll_min") != string::npos);
            if(using_bundled) {
                string dll_min_bin = gpp.substr(0, gpp.rfind("\\bin\\")) + "\\bin";
                string old_path = getenv("PATH") ? getenv("PATH") : "";
                string new_path = dll_min_bin + ";" + old_path;
                SetEnvironmentVariableA("PATH", new_path.c_str());
            }

            // Set TEMP/TMP to a drive with free space
            {
                string out_dir;
                size_t sl = out_file.find_last_of("/\\");
                if(sl != string::npos) out_dir = out_file.substr(0, sl);
                else out_dir = get_exe_dir();
                SetEnvironmentVariableA("TEMP", out_dir.c_str());
                SetEnvironmentVariableA("TMP", out_dir.c_str());
            }

            // Wrap entire command in extra quotes for cmd.exe /S handling
            string full_cmd = "\"" + cmd + "\"";
#else
            string full_cmd = cmd;
#endif
            int ret = system(full_cmd.c_str());

            // Clean up temp C++ file
#ifdef _WIN32
            DeleteFileA(cpp_file.c_str());
#else
            unlink(cpp_file.c_str());
#endif

            if(ret != 0) {
                cerr<<"Compilation failed.\n";
                return 1;
            }

            cout<<"Build successful: "<<out_file<<"\n";
            return 0;
        }

        case COMPILE_MODE: {
            string out_file = arg;
            if(out_file.size()>4 && out_file.substr(out_file.size()-4)==".alb")
                out_file = out_file.substr(0, out_file.size()-4);
            out_file += ".albc";
            if(!compile_to_bytecode(arg, out_file, compile_obfuscate))
                return 1;
            return 0;
        }

        case RUN_BC_MODE: {
            if(!is_bytecode_file(arg)) {
                // Auto-detect: if it's a .alb source file, run it normally
                ifstream f(arg, ios::binary);
                if(!f){cerr<<"Cannot open: "<<arg<<"\n";return 1;}
                ostringstream ss; ss<<f.rdbuf();
                string code=ss.str();
                code.erase(remove(code.begin(),code.end(),'\r'),code.end());
                run_code(code, env);
            } else {
                run_bytecode_file(arg, env);
            }
            return 0;
        }

        case PACKAGE_MODE: {
            string out_zip = package_output;
            if(out_zip.empty()) {
                out_zip = arg;
                if(out_zip.size()>4 && out_zip.substr(out_zip.size()-4)==".alb")
                    out_zip = out_zip.substr(0, out_zip.size()-4);
                out_zip += ".zip";
            }
            return package_app(arg, out_zip, compile_obfuscate);
        }
        }
    } catch(ALBError& err) {
        cerr<<"Error "<<err.type<<": "<<err.msg<<"\n";
        return 1;
    } catch(exception& ex) {
        cerr<<"Error: "<<ex.what()<<"\n";
        return 1;
    }
    return 0;
}
