static volatile int stopping;
static HANDLE hwnd;static P oldproc;static volatile int state;static int in_tick;
#ifdef LOH_MOD_MANAGER
#ifdef MODULE_INFO
/* Read by the manager window straight from the DLL file (5_모드매니저 docs/모드_약속.md). */
__declspec(dllexport) const char LOH_ModInfo[]=MODULE_INFO;
#endif
/* Called by the manager worker: reads scalar status only, no game API calls. */
__declspec(dllexport) int LOH_ModManagerState(void){
    if(stopping)return 4;if(state<0)return 3;if(!state)return 1;
#ifdef MODULE_DETAIL_STATE
    return MODULE_DETAIL_STATE;
#else
    return 2;
#endif
}
#endif
static i64 API wndproc(HANDLE h,u32 msg,uptr w,i64 l){
    if(msg==0x10||msg==0x82)stopping=1;
    if(msg==WM_MOD){if(stopping||in_tick)return 0;in_tick=1;
        if(!state&&il2cpp_thread_current()){state=resolve()?1:-1;logmsg(state==1?READY_TEXT:"DISABLED — game metadata mismatch, no UI changes");}
        if(state==1&&alive(sentinel))tick();in_tick=0;return 0;}
    return CallWindowProcW(oldproc,h,msg,w,l);
}
static int API enumwin(HANDLE h,i64 lp){u32 pid=0;wch name[64];GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(h)&&GetClassNameW(h,name,64)&&lstrcmpiW(name,L"UnityWndClass")==0){*(HANDLE*)lp=h;return 0;}return 1;}
static u32 API worker(void *arg){HANDLE ga=0;int i;(void)arg;
    for(i=0;i<2400;i++){ga=GetModuleHandleA("GameAssembly.dll");EnumWindows(enumwin,(i64)&hwnd);if(ga&&hwnd)break;Sleep(100);}
    if(!ga||!hwnd||!load_api(ga)){logmsg("startup failed: game window/API unavailable");return 1;}
    /* Worker never calls IL2CPP, even during startup. Unity's main thread owns all calls. */
    oldproc=(P)SetWindowLongPtrW(hwnd,-4,(i64)wndproc);if(!oldproc){logmsg("window attach failed %u",GetLastError());return 1;}
    for(;;){if(stopping)break;PostMessageW(hwnd,WM_MOD,0,0);Sleep(50);}return 0;
}
int API DllMain(HANDLE h,u32 why,P unused){(void)unused;if(why==1){static wch path[800];u32 n,w;HANDLE thread;DisableThreadLibraryCalls(h);CreateMutexW(0,0,MODULE_MUTEX);if(GetLastError()==183)return 0;n=GetModuleFileNameW(h,path,700);while(n&&path[n-1]!='\\')n--;path[n]=0;wc(path,MODULE_LOG);log_file=CreateFileW(path,0x40000000,3,0,2,0x80,0);if(log_file!=INVALID_HANDLE)WriteFile(log_file,"\xEF\xBB\xBF",3,&w,0);logmsg(MODULE_BANNER " pid=%u",GetCurrentProcessId());thread=CreateThread(0,0,worker,0,0,0);if(thread)CloseHandle(thread);}return 1;}
