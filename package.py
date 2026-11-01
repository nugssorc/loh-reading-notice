r"""배포 묶음 만들기(공통지식 9절 '우리 모드 공개판 기준').
  python package.py   → dist\독서알림정리_v<판>\ (압축 전 폴더)
                         dist\loh-reading-notice-<판>.zip          비밀번호 없음 = GitHub 릴리스 첨부(모드매니저 '지금 업데이트'용)
                         dist\독서알림정리_v<판>.zip               비밀번호 1234 = 디시·드라이브용(+ 배포_올릴것 사본)
압축 안: 독서알림정리_v<판>\{읽어주세요.txt, 설치.cmd, 제거.cmd,
          _files\{manifest.json, tool.ps1(손 설치기), entry\{version,winhttp,winmm,hid}.dll, LOH_ReadingNotice\{dll, 설정 ini, loh-mod.json}}}
  모드매니저는 manifest 의 files 만 쓰고(entries·tool.ps1·cmd 는 안 씀), 설치.cmd 는 게임 폴더에 모드 폴더 + 빈 입구 자리 하나.
먼저 build.py 로 DLL 을 만든다. 버전은 src\reading.c 의 MODULE_VERSION 에서 읽는다."""
from pathlib import Path
import hashlib, io, json, os, re, shutil, struct, time, zipfile, zlib

ROOT = Path(__file__).resolve().parent
MODULE = 'LOH_ReadingNotice'
REPO = 'nugssorc/loh-reading-notice'
GAME_BUILD = '26159462'
UPLOAD = ROOT.parent / '배포_올릴것'
TITLE = '독서 알림 정리'
PAGE = 'https://gall.dcinside.com/mgallery/board/view/?id=san14&no=79055'      # 디시 글(1.0.0 부터). 새 판은 이 글을 고쳐 올림(제목 판 번호도)
SLOTS = ['version', 'winhttp', 'winmm', 'hid']
# RVA 는 GAME_BUILD 판 GameAssembly.dll 기준(2026-11-01 역어셈블). 모드매니저 겹침 비교용.
CHANGES = [
    {'kind': 'hook', 'target': 'UI_FinishReadingUI.OpenUI', 'address': '0x26A4BC0', 'note': '독서 완료 창 → 로그 줄, 닫기 대신 누름'},
    {'kind': 'hook', 'target': 'UI_ConfirmContributeUI.OpenUI', 'address': '0x29C7280', 'note': '책 보상 아이템 창 → 로그 줄, 확인 대신 누름'},
    {'kind': 'hook', 'target': '책 진행 보상 함수(진행도 단계)', 'address': '0x1761A90', 'note': '책 보상인지 표시만'},
    {'kind': 'hook', 'target': '책 진행 보상 함수(다른 경로)', 'address': '0x175D140', 'note': '책 보상인지 표시만'},
]
# 최신이 맨 위. 릴리스 설명(패치노트)도 여기서 만든다.
NOTES = [
    {'version': '1.0.0', 'date': '2026-11-01',
     'text': '첫 공개판: 책 진행도 25/50/75/100% 알림창(주인공)과 책 보상 아이템 획득 창(주인공·동행)을 클릭 없이 화면 구석 로그 줄로. '
             '매일 읽기 경험치 때는 표시하지 않게 고쳐 약 보상 등 다른 아이템 창은 그대로 뜸. 설치.cmd 손 설치와 모드매니저 설치 둘 다 됨'},
    {'version': '0.1.0', 'date': '2026-11-01', 'text': '시험판'},
]
SETTINGS = ('﻿; 독서 알림 정리 설정 — 메모장으로 고쳐 저장한 뒤 게임을 다시 켜면 적용됩니다.\r\n'
            '[ReadingNotice]\r\n'
            '; 1 = 책 진행도 25/50/75/100% 창(주인공)을 띄우지 않고 회색 로그 줄로, 0 = 원래대로 창\r\n'
            'ReadingWindow=1\r\n'
            "; 1 = 책 보상 '아이템 획득' 창(주인공·동행)을 자동 확인하고 회색 로그 줄로, 0 = 원래대로 창\r\n"
            'ItemWindow=1\r\n')
README = """독서 알림 정리 {v} — 영웅입지전 모드

책을 들고 시간을 보내면 뜨는 알림창을 클릭 없이 화면 구석 로그(날짜와 함께 쌓이는 회색 글)로 바꿉니다.
- 책 진행도 25/50/75/100% 창(주인공): 창 대신 로그 한 줄. 경험치 등 보상은 그대로 들어옵니다.
- 책 보상 '아이템 획득' 창(주인공·동행): 자동으로 '확인'해 가방에 넣고 로그 한 줄. 아이템 창이 아주 잠깐 보일 수 있습니다.
- 약 보상 등 책이 아닌 아이템 창은 그대로 뜹니다.

설치 — 둘 중 편한 쪽 하나
1) 설치.cmd (모드매니저 없이)
   - 게임을 완전히 끄고 압축을 푼 뒤(비밀번호 압축이면 1234) 「설치.cmd」를 더블클릭합니다.
   - 게임 폴더는 스팀 설치 위치에서 자동으로 찾습니다(못 찾으면 ThreeKingdom.exe 가 있는 폴더 경로를 묻습니다).
   - 게임 폴더에 LOH_ReadingNotice 폴더와, version / winhttp / winmm / hid 중 비어 있는 자리 하나에 작은 입구 파일을 둡니다.
     다른 모드가 쓰는 파일은 건드리지 않습니다. 넷 다 차 있으면 설치를 멈춥니다.
     이때는 아래 2) 모드매니저로 등록해서 쓰세요(여러 모드가 입구 하나를 같이 씀).
     모드매니저 사용법: https://gall.dcinside.com/mgallery/board/view/?id=san14&no=78820
   - 지우기: 「제거.cmd」. 설치·제거 전 상태는 게임 폴더의 .loh-reading-backups 에 남습니다.
   - 새 버전은 새 압축의 설치.cmd 를 다시 실행하면 됩니다(고쳐 둔 설정은 그대로).
2) 모드매니저
   - 게임을 끄고 모드매니저 창에 압축(비밀번호 압축이면 푼 폴더)을 끌어다 놓은 뒤 '설치'.
   - 모드매니저로 설치하면 새 버전이 나올 때 '업데이트 확인' → '지금 업데이트'로 바로 받을 수 있습니다.
   ※ 둘 중 하나로만 설치하세요(둘 다 깔려도 한 번만 동작하지만 정리가 번거로워집니다).

설정
- 모드 폴더의 독서알림_설정.ini 를 메모장으로 열어 ReadingWindow / ItemWindow 를 0 으로 바꾸면 그 창은 원래대로 뜹니다.
  손 설치는 게임 폴더의 LOH_ReadingNotice 폴더, 모드매니저는 '설정 열기' 버튼. 저장 뒤 게임을 다시 켜면 적용됩니다.

알아 둘 것
- 게임 파일(GameAssembly.dll 등)은 바꾸지 않습니다. 인터넷 전송 없음, 치트 기능 없음.
- 게임 빌드 {game} 에서 확인했습니다. 게임이 업데이트되어 맞는 곳을 못 찾으면 그 기능만 쉬고 원래 창이 뜹니다.
- 기록: 모드 폴더의 LOH_ReadingNotice.log
- 소스 공개, MIT-0: https://github.com/{repo}
- 설명·문의: {page}
"""

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

# ---- 비밀번호 zip(ZipCrypto, 탐색기·반디집·7-Zip 이 엶) — 2_독박육아 installer\make_zip.py 와 같은 방식 ----
CRCT = []
for i in range(256):
    c = i
    for _ in range(8): c = (c >> 1) ^ 0xEDB88320 if c & 1 else c >> 1
    CRCT.append(c)
class Crypt:
    def __init__(self, pw):
        self.k = [0x12345678, 0x23456789, 0x34567890]
        for b in pw: self.enc(bytes([b]), plain_update=True)
    def enc(self, data, plain_update=False):
        out = bytearray(len(data)); k0, k1, k2 = self.k
        for i, p in enumerate(data):
            t = (k2 | 2) & 0xFFFF; out[i] = p ^ (((t * (t ^ 1)) >> 8) & 0xFF)
            k0 = CRCT[(k0 ^ p) & 0xFF] ^ (k0 >> 8)
            k1 = ((k1 + (k0 & 0xFF)) * 134775813 + 1) & 0xFFFFFFFF
            k2 = CRCT[(k2 ^ (k1 >> 24)) & 0xFF] ^ (k2 >> 8)
        self.k = [k0, k1, k2]; return bytes(out)
def dos_time(ts):
    t = time.localtime(ts); y = max(t.tm_year, 1980)
    return (t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec // 2), ((y - 1980) << 9) | (t.tm_mon << 5) | t.tm_mday
def zip_password(files, out, pw=b'1234'):
    buf = io.BytesIO(); cd = []
    for p, arc in files:
        raw = p.read_bytes(); crc = zlib.crc32(raw) & 0xFFFFFFFF
        co = zlib.compressobj(9, zlib.DEFLATED, -15); comp = co.compress(raw) + co.flush(); method = 8
        if len(comp) >= len(raw): comp, method = raw, 0
        c = Crypt(pw); body = c.enc(bytes(bytearray(os.urandom(11)) + bytes([crc >> 24]))) + c.enc(comp)
        tm, dt = dos_time(p.stat().st_mtime); nm = arc.encode('utf-8'); flags = 0x0801; off = buf.tell()
        buf.write(struct.pack('<IHHHHHIIIHH', 0x04034B50, 20, flags, method, tm, dt, crc, len(body), len(raw), len(nm), 0) + nm + body)
        cd.append(struct.pack('<IHHHHHHIIIHHHHHII', 0x02014B50, 20, 20, flags, method, tm, dt, crc, len(body), len(raw), len(nm), 0, 0, 0, 0, 0x20, off) + nm)
    cdo = buf.tell(); cdb = b''.join(cd); buf.write(cdb)
    buf.write(struct.pack('<IHHHHIIH', 0x06054B50, 0, 0, len(cd), len(cd), len(cdb), cdo, 0))
    out.write_bytes(buf.getvalue())
    z = zipfile.ZipFile(out)                          # 다시 풀어 원본과 대조, 틀린 비밀번호는 막히는지
    for (p, arc), zi in zip(files, z.infolist()):
        assert zi.filename == arc and zi.flag_bits & 1 and z.read(zi, pwd=pw) == p.read_bytes(), arc
        try: z.read(zi, pwd=b'wrong'); raise SystemExit('틀린 비밀번호로 풀림: ' + arc)
        except (RuntimeError, zlib.error, zipfile.BadZipFile): pass

def main():
    version = re.search(r'#define MODULE_VERSION "([^"]+)"', (ROOT / 'src/reading.c').read_text('utf-8')).group(1)
    assert NOTES[0]['version'] == version, 'NOTES 맨 위를 새 판으로'
    dll = ROOT / 'build' / f'{MODULE}.dll'
    if not dll.exists() or f'version={version};'.encode() not in dll.read_bytes(): raise SystemExit('build.py 를 먼저 돌리세요(DLL 판이 다름)')
    name = f'독서알림정리_v{version}'
    dist = ROOT / 'dist'; top = dist / name; files = top / '_files'; mod = files / MODULE
    if top.exists(): shutil.rmtree(top)
    mod.mkdir(parents=True)
    shutil.copy2(dll, mod / dll.name)
    (mod / '독서알림_설정.ini').write_bytes(SETTINGS.encode('utf-16-le'))
    info = {'format': 1, 'name': '독서 알림 정리', 'version': version, 'page': PAGE, 'github': REPO, 'settings': ['독서알림_설정.ini'],
            'game': GAME_BUILD, 'changes': CHANGES, 'notes': NOTES}
    (mod / 'loh-mod.json').write_text(json.dumps(info, ensure_ascii=False, indent=2), 'utf-8')
    (top / '읽어주세요.txt').write_text(README.format(v=version, repo=REPO, game=GAME_BUILD, page=PAGE).replace('\n', '\r\n'), 'utf-8-sig')
    entries = [{'src': f'{MODULE}\\{p.name}', 'dst': f'{MODULE}\\{p.name}', 'sha256': sha(p)} for p in sorted(mod.iterdir())]
    (files / 'entry').mkdir()
    for slot in SLOTS:
        e = ROOT / 'build' / 'entry' / f'{slot}.dll'
        assert b'LOH_ReadingNotice entry 1' in e.read_bytes(), slot
        shutil.copy2(e, files / 'entry' / e.name)
    (files / 'tool.ps1').write_text((ROOT / 'installer' / 'tool.ps1').read_text('utf-8-sig'), 'utf-8-sig')   # PowerShell 5.1 은 BOM 이 있어야 한글을 읽음
    slots = [{'name': f'{s}.dll', 'src': f'entry\\{s}.dll', 'sha256': sha(files / 'entry' / f'{s}.dll')} for s in SLOTS]
    (files / 'manifest.json').write_text(json.dumps({'product': MODULE, 'version': version, 'title': TITLE, 'files': entries,
                                                     'entries': slots}, ensure_ascii=False, indent=2), 'utf-8')
    for action, fname, say in [('Install', '설치.cmd', '설치'), ('Remove', '제거.cmd', '제거')]:
        cmd = (f'@echo off\r\nchcp 65001 >nul\r\ncd /d "%~dp0"\r\necho.\r\necho  {TITLE} {version} {say}를 시작합니다. (게임은 꺼 두세요)\r\n'
               f'powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0_files\\tool.ps1" -Action {action}\r\necho.\r\npause\r\n')
        (top / fname).write_bytes(cmd.encode('utf-8'))
    items = [(p, p.relative_to(dist).as_posix()) for p in sorted(top.rglob('*')) if p.is_file()]
    plain = dist / f'loh-reading-notice-{version}.zip'
    with zipfile.ZipFile(plain, 'w', zipfile.ZIP_DEFLATED) as zf:
        for p, arc in items: zf.write(p, arc)
    locked = dist / f'{name}.zip'
    zip_password(items, locked)
    notes = '\n'.join(f"## {n['version']} ({n['date']})\n- {n['text']}" for n in NOTES[:1])
    (dist / f'release-notes-{version}.md').write_text(notes + '\n', 'utf-8')
    UPLOAD.mkdir(exist_ok=True); shutil.copy2(locked, UPLOAD / locked.name)
    for z in (plain, locked): print(z, sha(z))
    print('사본:', UPLOAD / locked.name)

if __name__ == '__main__':
    main()
