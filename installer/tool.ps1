param(
    [ValidateSet('Install','Remove','Check')][string]$Action = 'Install',
    [string]$GamePath = '',
    [string]$PayloadPath = $PSScriptRoot      # 묶음의 _files\tool.ps1 로 들어가므로 자기 폴더가 곧 _files
)
# 독서 알림 정리 손 설치기(설치.cmd·제거.cmd 가 부름). 2_독박육아\모드공통\공개설치.ps1 을 이 모드 하나에 맞춰 줄인 것.
# 게임 폴더에 LOH_ReadingNotice\ 폴더 + 입구 하나(이미 우리 입구가 있는 자리를 먼저, 없으면 version → winhttp → winmm → hid 중 빈 자리).
# 다른 모드의 파일·설치 기록 없는 폴더는 바꾸지 않는다. 바꾸기 전에 묶음 파일 해시를 모두 검사하고, 실패하면 백업으로 되돌린다.
$ErrorActionPreference = 'Stop'
$Name = 'LOH_ReadingNotice'; $Marker = 'LOH_ReadingNotice entry 1'
$Slots = @('version.dll','winhttp.dll','winmm.dll','hid.dll')
function Say([string]$t) { Write-Host $t }
function Fail([string]$t) { Write-Host ''; Write-Host "  [멈춤] $t" -ForegroundColor Yellow; Write-Host ''; exit 1 }
function File-Hash([string]$Path) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToUpper() }
function Find-Game {
    if ($GamePath) { $g = [IO.Path]::GetFullPath($GamePath).TrimEnd('\'); if (!(Test-Path -LiteralPath (Join-Path $g 'ThreeKingdom.exe'))) { Fail '지정한 게임 폴더에 ThreeKingdom.exe 가 없습니다.' }; return $g }
    $here = $PSScriptRoot
    for ($k = 0; $k -lt 6 -and $here; $k++) { if (Test-Path -LiteralPath (Join-Path $here 'ThreeKingdom.exe')) { return $here }; $here = Split-Path -Parent $here }
    $roots = @()
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        try { $v = Get-ItemProperty -Path $key -ErrorAction Stop; if ($v.SteamPath) { $roots += $v.SteamPath }; if ($v.InstallPath) { $roots += $v.InstallPath } } catch {} }
    $libs = @()
    foreach ($r in $roots) { $r = $r -replace '/', '\'; $libs += $r
        $vdf = Join-Path $r 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) { foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += ($m.Groups[1].Value -replace '\\\\', '\') } } }
    foreach ($l in ($libs | Select-Object -Unique)) { $g = Join-Path $l 'steamapps\common\LegendOfHeros'; if (Test-Path -LiteralPath (Join-Path $g 'ThreeKingdom.exe')) { return [IO.Path]::GetFullPath($g).TrimEnd('\') } }
    Say '  게임 폴더를 자동으로 찾지 못했습니다.'
    $p = (Read-Host '  영웅입지전 게임 폴더(ThreeKingdom.exe 가 있는 폴더) 경로를 붙여 넣고 엔터').Trim('"', ' ')
    if ($p -and (Test-Path -LiteralPath (Join-Path $p 'ThreeKingdom.exe'))) { return [IO.Path]::GetFullPath($p).TrimEnd('\') }
    Fail '그 폴더에 ThreeKingdom.exe 가 없습니다.'
}
function Checked-Path([string]$Relative) {
    $Full = [IO.Path]::GetFullPath((Join-Path $Game $Relative))
    if (!$Full.StartsWith($Game + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail '게임 폴더 밖의 경로입니다.' }
    $Current = $Full
    while ($Current -and $Current.Length -gt $Game.Length) {
        if ((Test-Path -LiteralPath $Current) -and ((Get-Item -LiteralPath $Current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { Fail "연결 경로는 바꾸지 않습니다: $Current" }
        $Current = [IO.Path]::GetDirectoryName($Current)
    }
    return $Full
}
function Is-Ours([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    return [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Path)).Contains($Marker)
}
function Owned-Slots { @($Slots | Where-Object { Is-Ours (Join-Path $Game $_) }) }

$PayloadPath = [IO.Path]::GetFullPath($PayloadPath)
$Manifest = Get-Content -LiteralPath (Join-Path $PayloadPath 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ([string]$Manifest.product -ne $Name) { Fail "알 수 없는 묶음입니다: $($Manifest.product)" }
$Title = [string]$Manifest.title; $Version = [string]$Manifest.version
Say ''; Say "  ==== $Title $Version — $(@{Install='설치';Remove='제거';Check='확인'}[$Action]) ===="
$Game = Find-Game; Say "  게임 폴더: $Game"
$Directory = Checked-Path $Name
$Prev = $null
if (Test-Path -LiteralPath $Directory) {
    $RecordPath = Join-Path $Directory '설치기록.json'
    if (Test-Path -LiteralPath $RecordPath -PathType Leaf) { $Prev = Get-Content -LiteralPath $RecordPath -Raw -Encoding UTF8 | ConvertFrom-Json; if ($Prev.Name -ne $Name) { Fail "다른 모드의 폴더입니다: $Directory" } }
    else { Fail "설치 기록이 없는 기존 폴더는 바꾸지 않습니다: $Directory`n         (모드매니저로 설치했다면 모드매니저에서 지우거나 업데이트하세요)" }
}
if ($Action -eq 'Check') {
    Say "  설치됨: $(if ($Prev) { "예 ($($Prev.Version), 입구 $($Prev.Entry))" } else { '아니요' })"
    Say "  우리 입구: $(if ((Owned-Slots).Count) { (Owned-Slots) -join ', ' } else { '없음' })"
    foreach ($s in $Slots) { $p = Join-Path $Game $s; if ((Test-Path -LiteralPath $p) -and !(Is-Ours $p)) { Say "  $s : 다른 모드가 쓰는 중" } }
    exit 0
}
if (Get-Process -Name 'ThreeKingdom' -ErrorAction SilentlyContinue) { Fail '게임이 켜져 있습니다. 게임을 완전히 끈 뒤 다시 실행하세요.' }

$Entry = $null
if ($Action -eq 'Install') {
    $Owned = @(Owned-Slots)
    if ($Owned.Count) { $Entry = $Owned[0] }
    else { foreach ($s in $Slots) { if (!(Test-Path -LiteralPath (Join-Path $Game $s))) { $Entry = $s; break }; Say "  - $s 자리: 다른 모드가 쓰는 중 → 건너뜀" } }
    if (!$Entry) { Fail '입구로 쓸 빈 자리가 없습니다(version / winhttp / winmm / hid 가 모두 다른 모드에 쓰이는 중). 이럴 때는 모드매니저로 설치하세요.' }
    Say "  입구 자리: $Entry"
    foreach ($File in @($Manifest.files) + @($Manifest.entries)) {      # 바꾸기 전에 묶음 전체 검사
        $Source = [IO.Path]::GetFullPath((Join-Path $PayloadPath $File.src))
        if (!$Source.StartsWith($PayloadPath + '\', [StringComparison]::OrdinalIgnoreCase)) { Fail '잘못된 배포 파일 경로입니다.' }
        if (!(Test-Path -LiteralPath $Source -PathType Leaf) -or (File-Hash $Source) -ne ([string]$File.sha256).ToUpper()) { Fail "설치 파일 '$($File.src)' 이(가) 없거나 받은 것과 다릅니다. 압축을 다시 풀어 주세요(백신이 지웠다면 허용 뒤 다시)." }
    }
} elseif (!$Prev) { Say '  이 모드는 손 설치되어 있지 않습니다.'; exit 0 }

$Touched = @($Directory) + @($Slots | ForEach-Object { Join-Path $Game $_ } | Where-Object { Is-Ours $_ })
if ($Entry) { $Touched += (Join-Path $Game $Entry) }
$Backup = Checked-Path ('.loh-reading-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $Backup -Force | Out-Null
$Snapshots = @()
foreach ($Path in @($Touched | Select-Object -Unique)) {
    $Saved = Join-Path $Backup $Path.Substring($Game.Length + 1); $Existed = Test-Path -LiteralPath $Path
    if ($Existed) { New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Saved)) -Force | Out-Null; Copy-Item -LiteralPath $Path -Destination $Saved -Recurse -Force }
    $Snapshots += [pscustomobject]@{ Path=$Path; Saved=$Saved; Existed=$Existed }
}
try {
    if ($Action -eq 'Install') {
        $Keep = @(); if (Test-Path -LiteralPath $Directory) { $Keep = @(Get-ChildItem -LiteralPath $Directory -Filter '*.ini' -File | ForEach-Object { [pscustomobject]@{ Name=$_.Name; Bytes=[IO.File]::ReadAllBytes($_.FullName) } }) }
        if (Test-Path -LiteralPath $Directory) { Remove-Item -LiteralPath $Directory -Recurse -Force }      # 옛 판 파일이 남지 않게(로그는 백업에)
        New-Item -ItemType Directory -Path $Directory -Force | Out-Null
        Get-ChildItem -LiteralPath (Join-Path $PayloadPath $Name) | Copy-Item -Destination $Directory -Recurse -Force
        foreach ($k in $Keep) { [IO.File]::WriteAllBytes((Join-Path $Directory $k.Name), $k.Bytes) }      # 고쳐 둔 메모장 설정은 다시 설치해도 그대로
        [ordered]@{ Name=$Name; Version=$Version; Entry=$Entry; Installed=(Get-Date -Format o) } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Directory '설치기록.json') -Encoding UTF8
        $EntryPath = Join-Path $Game $Entry; $New = Join-Path $PayloadPath ('entry\' + $Entry)
        if (!(Test-Path -LiteralPath $EntryPath) -or (File-Hash $EntryPath) -ne (File-Hash $New)) { Copy-Item -LiteralPath $New -Destination $EntryPath -Force }
        Say ''; Say "  설치 끝: $Title $Version. 게임을 켜면 바로 적용됩니다."
    } else {
        Remove-Item -LiteralPath $Directory -Recurse -Force
        foreach ($s in Owned-Slots) { Remove-Item -LiteralPath (Join-Path $Game $s) -Force }
        Say ''; Say "  제거 끝: $Title (입구 파일도 지웠습니다)"
    }
} catch {
    foreach ($Snapshot in $Snapshots) {
        if (Test-Path -LiteralPath $Snapshot.Path) { Remove-Item -LiteralPath $Snapshot.Path -Recurse -Force }
        if ($Snapshot.Existed) { Copy-Item -LiteralPath $Snapshot.Saved -Destination $Snapshot.Path -Recurse -Force }
    }
    Fail "중간에 실패해 원래대로 되돌렸습니다: $($_.Exception.Message)"
}
Say "  백업: $Backup"; Say ''
