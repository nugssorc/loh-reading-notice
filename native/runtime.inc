typedef void *P;
int _fltused = 1;
void *memset(void *d, int c, uptr n) { u8 *p=d; while(n--) *p++=(u8)c; return d; }
void *memcpy(void *d, const void *s, uptr n) { u8 *p=d; const u8 *q=s; while(n--) *p++=*q++; return d; }
static int eq(const char *a,const char *b) { if(!a||!b)return 0;while(*a&&*a==*b){a++;b++;}return *a==*b; }
static int has(const char *s,const char *t) { int i,j;if(!s||!t)return 0;for(i=0;s[i];i++){for(j=0;t[j]&&s[i+j]==t[j];j++){}if(!t[j])return 1;}return 0; }
static void wc(wch *d,const wch *s){while(*d)d++;while((*d++=*s++));}
static HANDLE log_file=INVALID_HANDLE;
static void logmsg(const char *fmt,...) {
    char b[1000];int n;u32 w;__builtin_va_list ap;
    if(log_file==INVALID_HANDLE)return;
    __builtin_va_start(ap,fmt);n=wvsprintfA(b,fmt,ap);__builtin_va_end(ap);
    if(n<0||n>990)return;b[n++]='\r';b[n++]='\n';WriteFile(log_file,b,n,&w,0);
}
#define IL(ret,n,args) static ret (*n) args;
IL(P,il2cpp_domain_get,(void))
IL(P*,il2cpp_domain_get_assemblies,(P,uptr*))
IL(P,il2cpp_assembly_get_image,(P))
IL(P,il2cpp_class_from_name,(P,const char*,const char*))
IL(P,il2cpp_class_from_type,(P))
IL(P,il2cpp_class_get_parent,(P))
IL(const char*,il2cpp_class_get_name,(P))
IL(P,il2cpp_class_get_methods,(P,P*))
IL(P,il2cpp_class_get_fields,(P,P*))
IL(P,il2cpp_class_get_field_from_name,(P,const char*))
IL(P,il2cpp_field_get_type,(P))
IL(u32,il2cpp_field_get_offset,(P))
IL(int,il2cpp_field_get_flags,(P))
IL(void,il2cpp_field_static_get_value,(P,void*))
IL(P,il2cpp_method_get_param,(P,u32))
IL(P,il2cpp_method_get_return_type,(P))
IL(u32,il2cpp_method_get_param_count,(P))
IL(const char*,il2cpp_method_get_name,(P))
IL(u8,il2cpp_method_is_generic,(P))
IL(char*,il2cpp_type_get_name,(P))
IL(P,il2cpp_object_get_class,(P))
IL(P,il2cpp_object_unbox,(P))
IL(P,il2cpp_runtime_invoke,(P,P,P*,P*))
IL(uptr,il2cpp_image_get_class_count,(P))
IL(P,il2cpp_image_get_class,(P,uptr))
IL(const char*,il2cpp_image_get_name,(P))
IL(P,il2cpp_thread_current,(void))
IL(void,il2cpp_free,(P))
IL(u32,il2cpp_gchandle_new,(P,int))
IL(P,il2cpp_gchandle_get_target,(u32))
IL(void,il2cpp_gchandle_free,(u32))
IL(P,il2cpp_object_new,(P))
IL(P,il2cpp_string_new,(const char*))
static int load_api(HANDLE ga){int ok=1;
#define L(n) n=(void*)GetProcAddress(ga,#n);if(!n){logmsg("export missing: " #n);ok=0;}
L(il2cpp_domain_get) L(il2cpp_domain_get_assemblies) L(il2cpp_assembly_get_image)
L(il2cpp_class_from_name) L(il2cpp_class_from_type) L(il2cpp_class_get_parent)
L(il2cpp_class_get_name) L(il2cpp_class_get_methods) L(il2cpp_class_get_fields)
L(il2cpp_class_get_field_from_name) L(il2cpp_field_get_type) L(il2cpp_field_get_offset)
L(il2cpp_field_get_flags) L(il2cpp_field_static_get_value) L(il2cpp_method_get_param)
L(il2cpp_method_get_return_type) L(il2cpp_method_get_param_count) L(il2cpp_method_get_name)
L(il2cpp_method_is_generic) L(il2cpp_type_get_name) L(il2cpp_object_get_class)
L(il2cpp_object_unbox) L(il2cpp_runtime_invoke) L(il2cpp_image_get_class_count)
L(il2cpp_image_get_class) L(il2cpp_image_get_name) L(il2cpp_thread_current)
L(il2cpp_free) L(il2cpp_gchandle_new) L(il2cpp_gchandle_get_target) L(il2cpp_gchandle_free)
L(il2cpp_object_new) L(il2cpp_string_new)
#undef L
return ok;}
static int errors;
static P invoke(P m,P o,P *a){P e=0,r;if(!m)return 0;r=il2cpp_runtime_invoke(m,o,a,&e);if(e){if(errors++<6)logmsg("managed exception: %s",il2cpp_method_get_name(m));return 0;}return r;}
static int ibool(P m,P o){P r=invoke(m,o,0);return r?*(u8*)il2cpp_object_unbox(r):0;}
static P images[512];
static int nimages;
static P klass(const char *ns,const char *name){int i;for(i=0;i<nimages;i++){P k=il2cpp_class_from_name(images[i],ns,name);if(k)return k;}return 0;}
static P method(P k,const char *name,int argc){for(;k;k=il2cpp_class_get_parent(k)){P it=0,m;while((m=il2cpp_class_get_methods(k,&it)))if(eq(name,il2cpp_method_get_name(m))&&(int)il2cpp_method_get_param_count(m)==argc&&!il2cpp_method_is_generic(m))return m;}return 0;}
static P field(P k,const char *name){return k?il2cpp_class_get_field_from_name(k,name):0;}
static int stat(P f){return f&&(il2cpp_field_get_flags(f)&0x10);}
static P typeclass(P f){return f?il2cpp_class_from_type(il2cpp_field_get_type(f)):0;}
static P bytype(P k,const char *type,int isstatic){P it=0,f,found=0;int count=0;while(k&&(f=il2cpp_class_get_fields(k,&it))){char *s;int match;if(!!stat(f)!=isstatic)continue;s=il2cpp_type_get_name(il2cpp_field_get_type(f));match=has(s,type);if(s)il2cpp_free(s);if(match){found=f;count++;}}return count==1?found:0;}
static P get(P o,P f){return o&&f?*(P*)((u8*)o+il2cpp_field_get_offset(f)):0;}
static int geti(P o,P f){return o&&f?*(int*)((u8*)o+il2cpp_field_get_offset(f)):0;}
static P sget(P f){P v=0;if(f)il2cpp_field_static_get_value(f,&v);return v;}
static int sint(P f){int v=0;if(f)il2cpp_field_static_get_value(f,&v);return v;}
static int eval(P k,const char *name){P f=field(k,name);return f?sint(f):-1;}
static int alive(P p){return p&&*(P*)((u8*)p+0x10);}
/* Managed ABI: List<T>, reference arrays, System.String. Not game field offsets. */
static int length(P l){int n=l?*(int*)((u8*)l+0x18):0;return n>=0&&n<=100000?n:0;}
static P at(P l,int i){P a=l?*(P*)((u8*)l+0x10):0;return a&&i>=0&&i<length(l)&&(uptr)i<*(uptr*)((u8*)a+0x18)?((P*)((u8*)a+0x20))[i]:0;}
static P arrayat(P a,int i){return a&&i>=0&&(uptr)i<*(uptr*)((u8*)a+0x18)?((P*)((u8*)a+0x20))[i]:0;}
static int stringeq(P a,P b){int i,n;if(a==b)return 1;if(!a||!b)return 0;n=*(int*)((u8*)a+0x10);if(n!=*(int*)((u8*)b+0x10)||n<0||n>1000)return 0;for(i=0;i<n;i++)if(((wch*)((u8*)a+0x14))[i]!=((wch*)((u8*)b+0x14))[i])return 0;return 1;}
static void utf8(P s,char *out,int cap){int n;out[0]=0;if(!s)return;n=WideCharToMultiByte(65001,0,(wch*)((u8*)s+0x14),*(int*)((u8*)s+0x10),out,cap-1,0,0);if(n>0)out[n]=0;}
static int lastarg(const char *s,char *out){int i,a=-1,b=-1,n=0;for(i=0;s&&s[i];i++){if(s[i]=='<'||s[i]==',')a=i;if(s[i]=='>')b=i;}if(a<0||b<=a)return 0;for(i=a+1;i<b;i++){if(s[i]=='.')n=0;else if(s[i]!=' '&&n<95)out[n++]=s[i];}out[n]=0;return n>0;}

static int typename_is(P t,const char *expected){char *s=il2cpp_type_get_name(t);int r=eq(s,expected);if(s)il2cpp_free(s);return r;}
static P sentinel;static u32 sentinel_handle;
