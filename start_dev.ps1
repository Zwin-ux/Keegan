param(
    [switch]$SkipExe
)

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$aiRoot = Join-Path $repoRoot "ai_radio"
$serverDir = Join-Path $aiRoot "server"
$webDir = Join-Path $aiRoot "web"
$dataDir = Join-Path $serverDir "data"
$exePath = Join-Path $aiRoot "build\\Release\\keegan_patched.exe"

if (!(Test-Path $dataDir)) {
    New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
}

Write-Host "Starting registry (http://localhost:8090) ..."
Start-Process -FilePath "python" -ArgumentList "registry_server.py" -WorkingDirectory $serverDir

Write-Host "Starting web UI (http://localhost:5173) ..."
Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "npm run dev" -WorkingDirectory $webDir

if (-not $SkipExe -and (Test-Path $exePath)) {
    Write-Host "Starting EXE ..."
    Start-Process -FilePath $exePath -WorkingDirectory $aiRoot
} else {
    Write-Host "EXE not found (build it with CMake) or SkipExe is set."
}

Write-Host "Done."
