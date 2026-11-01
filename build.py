r"""독서 알림 정리 DLL 빌드 → build\LOH_ReadingNotice.dll (모드매니저 export 포함).
런타임 뼈대(native\)는 2_독박육아\모드공통\native 에서 복사해 온 것."""
from pathlib import Path
import hashlib, os, shutil, subprocess, sys, tempfile

ROOT = Path(__file__).resolve().parent
NATIVE = ROOT / 'native'
ZIG = ROOT / '.tools/zig-x86_64-windows-0.14.1/zig.exe'
MODULE = 'LOH_ReadingNotice'
CF = ['-target', 'x86_64-windows-msvc', '-nostdlib', '-nostdinc', '-O2',
      '-ffreestanding', '-fno-stack-protector', '-mno-stack-arg-probe',
      '-fno-builtin', '-Wall', '-Wextra', '-Wno-pointer-sign',
      '-Wno-misleading-indentation', '-Wno-unused-function', '-Wno-unused-parameter',
      '-finput-charset=UTF-8', '-DLOH_MOD_MANAGER=1', '-I', '.']

def build():
    out = ROOT / 'build'
    out.mkdir(exist_ok=True)
    tmp_root = out / '_tmp'
    tmp_root.mkdir(exist_ok=True)
    env = dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(tmp_root / 'zig'))
    with tempfile.TemporaryDirectory(prefix='reading_', dir=tmp_root) as tmp:
        work = Path(tmp)
        def run(args):
            r = subprocess.run([str(ZIG), *map(str, args)], cwd=work, env=env,
                               capture_output=True, text=True, encoding='utf-8', errors='replace')
            if r.stdout or r.stderr: print(r.stdout + r.stderr)
            if r.returncode: raise SystemExit(r.returncode)
        for p in [*NATIVE.glob('*.h'), *NATIVE.glob('*.inc'), *(ROOT / 'src').glob('*')]:
            if p.is_file(): shutil.copy2(p, work / p.name)
        for name in ['kernel32', 'user32']:
            shutil.copy2(NATIVE / 'lib' / f'{name}.def', work / f'{name}.def')
            run(['dlltool', '-m', 'i386:x86-64', '-d', f'{name}.def', '-l', f'{name}.lib'])
        run(['cc', *CF, '-c', 'reading.c', '-o', 'mod.obj'])
        run(['lld-link', '/dll', '/nodefaultlib', '/entry:DllMain', f'/out:{MODULE}.dll',
             'mod.obj', 'kernel32.lib', 'user32.lib', '/Brepro'])
        shutil.copy2(work / f'{MODULE}.dll', out / f'{MODULE}.dll')
        # 손 설치(설치.cmd)용 입구 넷: 진짜 시스템 DLL 로 넘기는 내보내기 표 + 모드 DLL 불러오기
        for p in [*NATIVE.glob('entry*'), NATIVE / 'fix_exports.py', NATIVE / 'sys_exports.json']:
            shutil.copy2(p, work / p.name)
        run(['cc', *CF, '-c', 'entry.c', '-o', 'entry.obj'])
        (out / 'entry').mkdir(exist_ok=True)
        for slot in ['version', 'hid', 'winhttp', 'winmm']:
            run(['lld-link', '/dll', '/nodefaultlib', '/entry:DllMain', f'/def:entry_{slot}.def',
                 f'/out:{slot}.dll', 'entry.obj', 'kernel32.lib', '/Brepro'])
            r = subprocess.run([sys.executable, 'fix_exports.py', f'{slot}.dll', slot, 'sys_exports.json'],
                               cwd=work, capture_output=True, text=True, encoding='utf-8', errors='replace')
            print(r.stdout + r.stderr, end='')
            if r.returncode: raise SystemExit(r.returncode)
            shutil.copy2(work / f'{slot}.dll', out / 'entry' / f'{slot}.dll')
    for p in [out / f'{MODULE}.dll', *sorted((out / 'entry').glob('*.dll'))]:
        print(p.relative_to(out), hashlib.sha256(p.read_bytes()).hexdigest())

if __name__ == '__main__':
    build()
