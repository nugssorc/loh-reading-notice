/* 손 설치(설치.cmd)용 입구: 게임 폴더의 version/winhttp/winmm/hid 중 빈 자리에 놓이고,
   진짜 시스템 DLL 로 호출을 넘기면서 LOH_ReadingNotice\LOH_ReadingNotice.dll 하나만 불러온다.
   모드매니저로 설치한 경우에는 쓰지 않는다(모드매니저 입구가 불러옴). 2_독박육아\모드공통\native\entry.c 와 같은 꼴. */
#include "win.h"
W HANDLE API LoadLibraryW(const wch *);
static wch root[600];
__declspec(dllexport) const char *LOH_ReadingNotice_Entry(void){return "LOH_ReadingNotice entry 1";}
static u32 API boot(void *unused){wch path[800];int i=0,j=0;static const wch rel[]=L"LOH_ReadingNotice\\LOH_ReadingNotice.dll";(void)unused;
    while(root[i]&&i<590){path[i]=root[i];i++;}
    while(rel[j]&&i<799)path[i++]=rel[j++];path[i]=0;
    if(GetFileAttributesW(path)!=0xffffffff)LoadLibraryW(path);
    return 0;
}
int API DllMain(HANDLE h,u32 reason,void *unused){(void)unused;
    if(reason==1){wch exe[600];u32 n,k;DisableThreadLibraryCalls(h);
        n=GetModuleFileNameW(0,exe,600);k=n;while(k&&exe[k-1]!='\\')k--;
        if(n&&lstrcmpiW(exe+k,L"ThreeKingdom.exe")==0){
            n=GetModuleFileNameW(h,root,590);while(n&&root[n-1]!='\\')n--;root[n]=0;
            if(n){HANDLE th=CreateThread(0,0,boot,0,0,0);if(th)CloseHandle(th);}
        }
    }return 1;
}
