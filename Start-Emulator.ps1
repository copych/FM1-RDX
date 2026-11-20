param([int]$Port=8788,[switch]$Rebuild,[switch]$NoBrowser)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
if($Rebuild -or !(Test-Path -LiteralPath 'app/build/emu/app.wasm')) {
    & wsl -d Ubuntu -- bash tools/build-emu.sh
    if($LASTEXITCODE -ne 0){throw 'Emulator build failed'}
}
$url="http://127.0.0.1:$Port/"
$health=$null
try {$health=Invoke-RestMethod -Uri ($url+'__rdx_emu_health') -TimeoutSec 2} catch {}
$expectedRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'app/build/emu'))
if(!$health -or $health.app -ne 'FM1-RDX emulator' -or $health.root -ne $expectedRoot) {
    New-Item -ItemType Directory -Force 'build' | Out-Null
    $node=(Get-Command node.exe -ErrorAction Stop).Source
    $script=Join-Path $PSScriptRoot 'tools/serve-emu.mjs'
    $process=Start-Process -FilePath $node -ArgumentList @(('"'+$script+'"'),"$Port") -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput 'build/emu-server.log' -RedirectStandardError 'build/emu-server-error.log'
    $process.Id | Set-Content -LiteralPath 'build/emu-server.pid'
    for($attempt=0;$attempt -lt 30;$attempt++) {
        Start-Sleep -Milliseconds 100
        try {$health=Invoke-RestMethod -Uri ($url+'__rdx_emu_health') -TimeoutSec 1;break} catch {}
        if($process.HasExited){throw 'Emulator server failed; see build/emu-server-error.log'}
    }
    if(!$health -or $health.app -ne 'FM1-RDX emulator' -or $health.root -ne $expectedRoot){throw 'Could not start the emulator server on this port'}
}
Write-Host "FM1-RDX emulator: $url"
if(!$NoBrowser){Start-Process $url}
