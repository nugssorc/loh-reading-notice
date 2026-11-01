/* 독서 알림 정리 0.1.0 (시험판).
 * 책 진행 25/50/75/100% 때 뜨는 두 창을 클릭 없이 화면 구석 회색 로그 줄로 바꾼다.
 *  1) 독서 완료 창 UI_FinishReadingUI(주인공만): 게임이 연 직후 글자를 읽어 로그로 보내고 '닫기'를 대신 누른다.
 *     경험치 등 보상은 이 창과 따로(창을 띄운 다음 책 보상 함수가) 들어온다.
 *  2) 아이템 획득 창 UI_ConfirmContributeUI(책 보상 화제, 주인공·동행): '확인'(ButtonClosePress)을 눌러야
 *     가방에 들어가므로, 다음 틀에 모드가 '확인'을 대신 누른다. 책 보상인지는 책 보상 함수(감춤 이름, 기계어로 찾음)가
 *     그 장수에게 막 불렸는지로 가린다(약 보상 등 다른 아이템 창은 그대로 둠).
 * 1.0.0: 매일 읽기 경험치 목록(책의 5번째 보상 목록)으로 불릴 때는 표시하지 않음 — 단계 목록 4개(25/50/75/100%)일 때만.
 * 근거: 현재상태.md, 2026-11-01 GameAssembly(빌드 26159462) 역어셈블. */
#include "win.h"
#include "runtime.inc"
#define WM_MOD (0x8000+0x6C1)
#define MODULE_MUTEX L"LOH_ReadingNotice_Loaded"
#define MODULE_LOG L"LOH_ReadingNotice.log"
#define MODULE_VERSION "1.0.0"
#define MODULE_BANNER "LOH_ReadingNotice " MODULE_VERSION
#define READY_TEXT "READY " MODULE_VERSION " — 독서 알림 정리 준비 완료"
#define MODULE_INFO "LOH_ModInfo 1;id=reading;name=독서 알림 정리;version=" MODULE_VERSION ";pause=0"
#define MODULE_DETAIL_STATE (fr_ready&&cc_ready&&log_ready?2:5)
static volatile int stopping;static HANDLE hwnd;static volatile int state;   /* lifecycle.inc 가 다시 선언(같은 tentative 정의) */
W u32 API GetPrivateProfileIntW(const wch *, const wch *, int, const wch *);
static P game_image;static u8 *ga_lo,*ga_hi;
static u32 (*method_flags)(P,u32*);
static P M_active,M_getGO,M_getText,M_queueAdd;
static P K_hero,K_book,F_heroId,M_name[2];
static int fr_ready,cc_ready,log_ready,hero_ctx=-1;
static int opt_reading=1,opt_items=1;

/* ---------- 설정: DLL 옆 독서알림_설정.ini (없으면 기본값으로 만듦, UTF-16 이라 메모장으로 열림) ---------- */
static const wch settings_text[]=L"\xFEFF; 독서 알림 정리 설정 — 메모장으로 고쳐 저장한 뒤 게임을 다시 켜면 적용됩니다.\r\n"
    L"[ReadingNotice]\r\n"
    L"; 1 = 책 진행도 25/50/75/100% 창(주인공)을 띄우지 않고 회색 로그 줄로, 0 = 원래대로 창\r\n"
    L"ReadingWindow=1\r\n"
    L"; 1 = 책 보상 '아이템 획득' 창(주인공·동행)을 자동 확인하고 회색 로그 줄로, 0 = 원래대로 창\r\n"
    L"ItemWindow=1\r\n";
static void load_settings(void){wch path[800];HANDLE h=GetModuleHandleW(L"LOH_ReadingNotice.dll");u32 n=h?GetModuleFileNameW(h,path,700):0,w;int i;
    static const wch name[]=L"독서알림_설정.ini";
    if(!n)return;while(n&&path[n-1]!=0x5C)n--;for(i=0;name[i];i++)path[n++]=name[i];path[n]=0;
    if(GetFileAttributesW(path)==0xFFFFFFFFu){HANDLE f=CreateFileW(path,0x40000000,1,0,1,0x80,0);
        if(f!=INVALID_HANDLE){WriteFile(f,settings_text,sizeof settings_text-2,&w,0);CloseHandle(f);logmsg("settings: created 독서알림_설정.ini with defaults");}}
    opt_reading=GetPrivateProfileIntW(L"ReadingNotice",L"ReadingWindow",1,path)!=0;opt_items=GetPrivateProfileIntW(L"ReadingNotice",L"ItemWindow",1,path)!=0;
    logmsg("settings: ReadingWindow=%d ItemWindow=%d",opt_reading,opt_items);
}

/* ---------- 작은 도구 ---------- */
static int active(P o){P go;if(!alive(o))return 0;go=invoke(M_getGO,o,0);return alive(go)&&ibool(M_active,go);}
static int isstatic(P m){u32 i=0;return method_flags&&(method_flags(m,&i)&0x10);}
static u8 *code(P m){u8 *f=m?*(u8**)m:0;return f>=ga_lo&&f<ga_hi?f:0;}
/* 함수 f 안(int3 두 개를 만나면 끝으로 봄)에서 target 을 부르는 call rel32 의 위치. 없으면 -1 */
static int find_call(u8 *f,int from,u8 *target){int i;
    for(i=from;i<0x4000&&f+i+5<ga_hi;i++){if(f[i]==0xCC&&f[i+1]==0xCC)break;if(f[i]==0xE8&&f+i+5+*(int*)(f+i+1)==target)return i;}
    return -1;}
/* 함수 앞 5바이트 이상을 덮을 때 옮겨 적을 명령 길이. 위치와 상관없는 머리 명령만 받는다
   (push reg · mov [rsp+imm8],reg · sub rsp,imm). 다른 명령이면 0 → 그 기능은 쉰다. */
static int prolen(u8 *f){int n=0,k;
    while(n<5){u8 *p=f+n;
        if(p[0]>=0x50&&p[0]<=0x57)k=1;
        else if((p[0]==0x40||p[0]==0x41)&&p[1]>=0x50&&p[1]<=0x57)k=2;
        else if((p[0]==0x48||p[0]==0x4C)&&p[1]==0x89&&(p[2]&0xC7)==0x44&&p[3]==0x24)k=5;
        else if(p[0]==0x48&&p[1]==0x83&&p[2]==0xEC)k=4;
        else if(p[0]==0x48&&p[1]==0x81&&p[2]==0xEC)k=7;
        else{logmsg("hook: unknown prologue at %p: %02x %02x %02x %02x",f,p[0],p[1],p[2],p[3]);return 0;}
        n+=k;}
    return n;}
static int hook(u8 *f,P wrapper,P *original){u8 *stub=0,*t;uptr address;u32 old;i64 rel;int i,n;
    if(!f||!(n=prolen(f)))return 0;
    for(address=((uptr)f-0x10000000)&~(uptr)0xffff;address>(uptr)f-0x70000000;address-=0x10000){stub=VirtualAlloc((P)address,0x1000,0x3000,0x40);if(stub)break;}
    if(!stub)return 0;rel=(i64)stub-(i64)(f+5);if(rel>0x7fffffff||rel< -0x7fffffffLL){VirtualFree(stub,0,0x8000);return 0;}
    stub[0]=0xff;stub[1]=0x25;*(u32*)(stub+2)=0;*(P*)(stub+6)=wrapper;
    t=stub+32;memcpy(t,f,n);t[n]=0xff;t[n+1]=0x25;*(u32*)(t+n+2)=0;*(P*)(t+n+6)=f+n;
    if(!VirtualProtect(f,n,0x40,&old)){VirtualFree(stub,0,0x8000);return 0;}
    *original=t;f[0]=0xe9;*(int*)(f+1)=(int)rel;for(i=5;i<n;i++)f[i]=0x90;
    VirtualProtect(f,n,old,&old);FlushInstructionCache(GetCurrentProcess(),f,n);return 1;
}
static int slen(const char *s){int n=0;while(s[n])n++;return n;}
static int live(void){return !stopping&&state==1&&alive(sentinel);}

/* 글 모으기(UTF-8). 줄바꿈은 빈칸으로. */
typedef struct{char s[900];int n;} Buf;
static void put(Buf *b,const char *t){while(t&&*t&&b->n<(int)sizeof b->s-1){char c=*t++;b->s[b->n++]=(c=='\r'||c=='\n')?' ':c;}b->s[b->n]=0;}
static void puttext(Buf *b,P text){char s[600];P str;if(!alive(text))return;str=invoke(M_getText,text,0);utf8(str,s,sizeof s);put(b,s);}
static void putname(Buf *b,P hero){char best[200]="",s[200];int i;
    for(i=0;i<2;i++){u8 f0=0,f1=0;P a[2]={&f0,&f1},r;if(!M_name[i]||!hero)continue;r=invoke(M_name[i],hero,a);utf8(r,s,sizeof s);if(slen(s)>slen(best))memcpy(best,s,slen(s)+1);}
    put(b,best);}

/* ---------- 회색 로그 줄: 게임의 로그 추가 함수(감춤 이름, UI_TipsCornerUI.Refresh 를 부르는 static void(string,string)) ---------- */
static P M_addLog;static P F_centerQueue;
static void add_log(Buf *b,P heroId){P a[2];logmsg("log: %s",b->s);
    a[0]=il2cpp_string_new(b->s);
    if(M_addLog){a[1]=heroId?heroId:il2cpp_string_new("");invoke(M_addLog,0,a);return;}
    if(F_centerQueue&&M_queueAdd){P q=sget(F_centerQueue);if(q)invoke(M_queueAdd,q,a);}   /* 대신 가운데 안내 줄 */
}
static int resolve_log(void){P kc=klass("","UI_TipsCornerUI"),kt=klass("","UI_TipsCenterRowUI"),refresh;u8 *target;uptr i;int found=0;
    refresh=method(kc,"Refresh",0);target=code(refresh);
    if(target)for(i=0;i<il2cpp_image_get_class_count(game_image);i++){P k=il2cpp_image_get_class(game_image,i),it=0,m;
        while((m=il2cpp_class_get_methods(k,&it))){u8 *f;
            if(il2cpp_method_get_param_count(m)!=2||!isstatic(m)||!typename_is(il2cpp_method_get_return_type(m),"System.Void"))continue;
            if(!typename_is(il2cpp_method_get_param(m,0),"System.String")||!typename_is(il2cpp_method_get_param(m,1),"System.String"))continue;
            if((f=code(m))&&find_call(f,0,target)>=0){if(!found++)M_addLog=m;}}}
    if(found!=1){M_addLog=0;logmsg("corner log: %d candidates — using center tips instead",found);}
    else logmsg("corner log: READY (%s)",il2cpp_method_get_name(M_addLog));
    F_centerQueue=field(kt,"m_queueTips");
    if(!M_addLog&&F_centerQueue){P q=sget(F_centerQueue);M_queueAdd=q?method(il2cpp_object_get_class(q),"Enqueue",1):0;}
    return M_addLog||M_queueAdd;
}

/* ---------- 1) 독서 완료 창 ---------- */
static P F_frSelf,F_frName,F_frBook,F_frProgress,F_frEffect,M_frOK;
typedef void (*FrOpen)(P,P,int,P);static FrOpen fr_orig;
static u32 fr_hero;static int fr_wait;static u32 fr_since;
static int fr_finish(void){P ui=sget(F_frSelf),hero=fr_hero?il2cpp_gchandle_get_target(fr_hero):0,str;Buf b;b.n=0;b.s[0]=0;
    if(!active(ui))return 0;
    str=invoke(M_getText,get(ui,F_frBook),0);if(!str||*(int*)((u8*)str+0x10)==0)return 0;   /* 글자를 아직 안 채움(처음 만드는 창) */
    puttext(&b,get(ui,F_frName));put(&b," ");puttext(&b,get(ui,F_frBook));put(&b," ");puttext(&b,get(ui,F_frProgress));put(&b," — ");puttext(&b,get(ui,F_frEffect));
    add_log(&b,hero?get(hero,F_heroId):0);invoke(M_frOK,ui,0);return 1;}
static void fr_open(P hero,P book,int stage,P mi){int had;
    if(!live()||!opt_reading){fr_orig(hero,book,stage,mi);return;}
    had=alive(sget(F_frSelf));fr_orig(hero,book,stage,mi);
    logmsg("reading window: stage=%d existing=%d",stage,had);
    if(fr_hero)il2cpp_gchandle_free(fr_hero);fr_hero=hero?il2cpp_gchandle_new(hero,0):0;
    if(had&&fr_finish()){fr_wait=0;return;}
    fr_wait=1;fr_since=GetTickCount();PostMessageW(hwnd,WM_MOD,0,0);   /* 처음 만드는 창은 다음 틀에 Start 가 글자를 채움 */
}
static int resolve_reading(void){P k=klass("","UI_FinishReadingUI"),m=method(k,"OpenUI",3);
    if(!m||!isstatic(m))return 0;
    K_hero=il2cpp_class_from_type(il2cpp_method_get_param(m,0));K_book=il2cpp_class_from_type(il2cpp_method_get_param(m,1));
    if(!K_hero||!K_book||!typename_is(il2cpp_method_get_param(m,2),"System.Int32"))return 0;
    F_frSelf=bytype(k,"UI_FinishReadingUI",1);F_frName=field(k,"m_textName");F_frBook=field(k,"m_textBookName");F_frProgress=field(k,"m_textProgress");F_frEffect=field(k,"m_textEffect");M_frOK=method(k,"ButtonOKPress",0);
    if(!F_frSelf||!F_frName||!F_frBook||!F_frProgress||!F_frEffect||!M_frOK)return 0;
    return hook(code(m),(P)fr_open,(P*)&fr_orig);
}

/* ---------- 책 보상 함수: 책 클래스에서 독서 완료 창 OpenUI 를 부른 바로 뒤의 (장수, List, float, bool) 호출 ---------- */
typedef struct{u32 id,hero,until;int left;} Pending;
static Pending pend[16];
/* 책의 보상 목록 칸: 보상 함수 둘째 인자와 같은 List 형인 칸을 선언 순서(=오프셋 순)로. 앞 4개 = 진행도 25/50/75/100%,
   5번째 = 매일 읽기 경험치(이 판 +0x68~+0x80 / +0x88). 매일 목록에는 아이템이 없고 하루마다 불리므로 표시하지 않는다. */
static P F_stage[4];static int nstage;
static int stage_of(P book,P list){int i;for(i=0;i<nstage;i++)if(list&&get(book,F_stage[i])==list)return i;return -1;}
typedef void (*Reward)(P,P,P,float,u8,P);static Reward reward_orig[2];
static void note_reward(P book,P hero,P list){int stage;
    if(live()&&opt_items&&hero&&(stage=stage_of(book,list))>=0){P id=get(hero,F_heroId);int i,slot=0;u32 now=GetTickCount();
        for(i=0;i<16;i++)if(!pend[i].id||(int)(now-pend[i].until)>0){slot=i;break;}else if(pend[i].until<pend[slot].until)slot=i;
        if(pend[slot].id){il2cpp_gchandle_free(pend[slot].id);il2cpp_gchandle_free(pend[slot].hero);}
        pend[slot].id=il2cpp_gchandle_new(id,0);pend[slot].hero=il2cpp_gchandle_new(hero,0);pend[slot].until=now+20000;pend[slot].left=3;
        {char s[96];utf8(id,s,sizeof s);logmsg("book reward: hero=%s stage=%d events=%d",s,stage+1,length(list));}}
}
static void reward0(P self,P hero,P list,float f,u8 flag,P mi){note_reward(self,hero,list);reward_orig[0](self,hero,list,f,flag,mi);}
static void reward1(P self,P hero,P list,float f,u8 flag,P mi){note_reward(self,hero,list);reward_orig[1](self,hero,list,f,flag,mi);}
/* 책 클래스 함수 중 독서 완료 창 OpenUI 를 부르는 곳마다, 바로 뒤(0x80바이트 안)의 첫 보상 함수 호출을 모은다.
   2026-11-01 판: EIHNGOGOCDJ 네 곳 → GLJBLLEEAHK, JOIIAGJMHPF 한 곳 → FPFDHNNOBHP. 서로 다른 것은 둘까지. */
static int find_reward(u8 *frOpen,u8 **found){P it=0,m;u8 *cp[64];int nc=0,nf=0,hits=0,bad=0;
    while((m=il2cpp_class_get_methods(K_book,&it))&&nc<64){
        if(il2cpp_method_get_param_count(m)!=4||isstatic(m)||!typename_is(il2cpp_method_get_return_type(m),"System.Void"))continue;
        if(il2cpp_class_from_type(il2cpp_method_get_param(m,0))!=K_hero||!typename_is(il2cpp_method_get_param(m,2),"System.Single")||!typename_is(il2cpp_method_get_param(m,3),"System.Boolean"))continue;
        if(code(m))cp[nc++]=code(m);
        if(!nstage){P jt=0,q,lt=il2cpp_method_get_param(m,1);char *want=il2cpp_type_get_name(lt);   /* 보상 목록 칸(선언 순서) */
            while((q=il2cpp_class_get_fields(K_book,&jt))&&nstage<4)if(!stat(q)&&typename_is(il2cpp_field_get_type(q),want))F_stage[nstage++]=q;
            if(want)il2cpp_free(want);}}
    it=0;while((m=il2cpp_class_get_methods(K_book,&it))){u8 *f=code(m);int at=0;
        while(f&&(at=find_call(f,at,frOpen))>=0){int j,k;u8 *next=0;at+=5;
            for(j=at;j<at+0x80&&!next;j++)if(f[j]==0xE8){u8 *t=f+j+5+*(int*)(f+j+1);for(k=0;k<nc;k++)if(t==cp[k])next=t;}
            if(!next){bad++;continue;}hits++;
            for(k=0;k<nf&&found[k]!=next;k++){}if(k==nf){if(nf==2){bad++;continue;}found[nf++]=next;}}}
    logmsg("book reward: %d candidates, %d call sites, %d functions, %d mismatched",nc,hits,nf,bad);
    logmsg("book stage lists: %d (+0x%x … +0x%x)",nstage,nstage?il2cpp_field_get_offset(F_stage[0]):0,nstage?il2cpp_field_get_offset(F_stage[nstage-1]):0);
    return hits&&!bad&&nstage==4?nf:0;
}

/* ---------- 2) 아이템 획득 창 ---------- */
static P F_ccSelf,F_ccBoxes,F_boxName,F_boxNum,M_ccClose;
typedef void (*CcOpen)(int,P,P,P,P,P,P,u8,int,u8,int,P,u8,P);static CcOpen cc_orig;
static u32 cc_hero;static int cc_wait;static u32 cc_since;
static void cc_open(int a0,P a1,P a2,P a3,P a4,P a5,P a6,u8 a7,int a8,u8 a9,int ctx,P ctxId,u8 a12,P mi){int i;u32 now;
    cc_orig(a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,ctx,ctxId,a12,mi);
    if(!live()||!opt_items||ctx!=hero_ctx||!ctxId)return;
    now=GetTickCount();
    for(i=0;i<16;i++)if(pend[i].id&&pend[i].left>0&&(int)(now-pend[i].until)<0&&stringeq(ctxId,il2cpp_gchandle_get_target(pend[i].id))){
        pend[i].left--;if(cc_hero)il2cpp_gchandle_free(cc_hero);cc_hero=il2cpp_gchandle_new(il2cpp_gchandle_get_target(pend[i].hero),0);
        cc_wait=1;cc_since=now;logmsg("item window from book reward → auto confirm");PostMessageW(hwnd,WM_MOD,0,0);return;}
}
static int cc_finish(int force){P ui=sget(F_ccSelf),boxes,hero=cc_hero?il2cpp_gchandle_get_target(cc_hero):0;int i,n=0;Buf b;b.n=0;b.s[0]=0;
    if(!active(ui))return 0;boxes=get(ui,F_ccBoxes);
    putname(&b,hero);put(&b," 책 보상: ");
    for(i=0;i<length(boxes)&&i<12;i++){P box=at(boxes,i);P s;if(!active(box))continue;s=invoke(M_getText,get(box,F_boxName),0);if(!s||*(int*)((u8*)s+0x10)==0)continue;
        if(n++)put(&b,", ");puttext(&b,get(box,F_boxName));puttext(&b,get(box,F_boxNum));}
    if(!n&&!force)return 0;                      /* 칸을 아직 안 채움 */
    if(!n)put(&b,"아이템 획득");
    add_log(&b,hero?get(hero,F_heroId):0);invoke(M_ccClose,ui,0);return 1;}
static int resolve_items(u8 *frOpen){P k=klass("","UI_ConfirmContributeUI"),kb=klass("","UI_ConfirmContributeBoxUI"),m=method(k,"OpenUI",13),ke;u8 *rw[2];int nr,i;
    static const P wraps[2]={(P)reward0,(P)reward1};
    if(!m||!isstatic(m)||!kb||!frOpen||!K_hero||!K_book)return 0;
    ke=il2cpp_class_from_type(il2cpp_method_get_param(m,10));hero_ctx=eval(ke,"Hero");
    if(hero_ctx<0||!field(ke,"Dialog")||!typename_is(il2cpp_method_get_param(m,11),"System.String"))return 0;
    F_ccSelf=bytype(k,"UI_ConfirmContributeUI",1);F_ccBoxes=bytype(k,"List<UI_ConfirmContributeBoxUI>",0);F_boxName=field(kb,"m_textName");F_boxNum=field(kb,"m_textNum");M_ccClose=method(k,"ButtonClosePress",0);
    if(!F_ccSelf||!F_ccBoxes||!F_boxName||!F_boxNum||!M_ccClose)return 0;
    nr=find_reward(frOpen,rw);if(!nr)return 0;
    for(i=0;i<nr;i++)if(!prolen(rw[i]))return 0;          /* 다 걸 수 있을 때만 건다 */
    if(!prolen(code(m)))return 0;
    for(i=0;i<nr;i++)if(!hook(rw[i],wraps[i],(P*)&reward_orig[i]))return 0;
    return hook(code(m),(P)cc_open,(P*)&cc_orig);
}

static int resolve(void){
    uptr n=0,i;P *as=il2cpp_domain_get_assemblies(il2cpp_domain_get(),&n),it,f,m;HANDLE ga=GetModuleHandleA("GameAssembly.dll");u8 *fr;int ns=0;
    for(i=0;i<n&&nimages<512;i++){P im=il2cpp_assembly_get_image(as[i]);images[nimages++]=im;if(eq(il2cpp_image_get_name(im),"Assembly-CSharp.dll"))game_image=im;}
    method_flags=(void*)GetProcAddress(ga,"il2cpp_method_get_flags");
    ga_lo=(u8*)ga;ga_hi=ga_lo+*(u32*)(ga_lo+*(int*)(ga_lo+0x3c)+0x50);
    if(!game_image||!method_flags)return 0;
    M_active=method(klass("UnityEngine","GameObject"),"get_activeInHierarchy",0);M_getGO=method(klass("UnityEngine","Component"),"get_gameObject",0);M_getText=method(klass("UnityEngine.UI","Text"),"get_text",0);
    if(!M_active||!M_getGO||!M_getText)return 0;
    load_settings();
    fr_ready=resolve_reading();logmsg("reading window: %s",fr_ready?"READY":"unavailable — 원래 창 그대로");
    if(K_hero){/* 장수 ID = 맨 앞 string 칸(+0x30), 이름 = string (bool,bool) 둘(전체 이름·성) 중 긴 것 */
        it=0;while((f=il2cpp_class_get_fields(K_hero,&it)))if(!stat(f)&&typename_is(il2cpp_field_get_type(f),"System.String")&&(!F_heroId||il2cpp_field_get_offset(f)<il2cpp_field_get_offset(F_heroId)))F_heroId=f;
        it=0;while((m=il2cpp_class_get_methods(K_hero,&it)))if(il2cpp_method_get_param_count(m)==2&&!isstatic(m)&&typename_is(il2cpp_method_get_return_type(m),"System.String")&&typename_is(il2cpp_method_get_param(m,0),"System.Boolean")&&typename_is(il2cpp_method_get_param(m,1),"System.Boolean")){if(ns<2)M_name[ns]=m;ns++;}
        logmsg("hero: id field +0x%x, name methods %d",F_heroId?il2cpp_field_get_offset(F_heroId):0,ns);if(ns!=2)M_name[0]=M_name[1]=0;}
    fr=code(method(klass("","UI_FinishReadingUI"),"OpenUI",3));
    /* 후킹 뒤에도 methodPointer·책 함수 속 call 대상 주소는 그대로 */
    cc_ready=F_heroId&&resolve_items(fr);logmsg("item window: %s",cc_ready?"READY":"unavailable — 원래 창 그대로");
    log_ready=resolve_log();
    {P kg=klass("UnityEngine","GameObject"),args[1],ctor=method(kg,".ctor",1),keep=method(klass("UnityEngine","Object"),"DontDestroyOnLoad",1);if(!ctor||!keep)return 0;sentinel=il2cpp_object_new(kg);args[0]=il2cpp_string_new("LOH_ReadingNotice_Lifetime");invoke(ctor,sentinel,args);if(!alive(sentinel))return 0;args[0]=sentinel;invoke(keep,0,args);sentinel_handle=il2cpp_gchandle_new(sentinel,0);}
    return (fr_ready||cc_ready)&&errors==0;
}
static void tick(void){u32 now=GetTickCount();
    if(fr_wait){if(fr_finish())fr_wait=0;else if(now-fr_since>3000){fr_wait=0;logmsg("reading window: text not ready in 3s, left open");}}
    if(cc_wait){if(cc_finish(now-cc_since>1500))cc_wait=0;else if(now-cc_since>3000){cc_wait=0;logmsg("item window: not active in 3s, left alone");}}
}
#include "lifecycle.inc"
