<#
 Bouwt NoidGPT en maakt:
   dist\NoidGPT-Setup.exe     (Inno Setup installer)
   dist\NoidGPT-Portable.zip  (uitpakken en starten)

 Vereisten: Visual Studio (C++ workload) en Inno Setup 6 (https://jrsoftware.org/isdl.php)
 Gebruik:   powershell -ExecutionPolicy Bypass -File .\Build-Release.ps1
#>
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dist = Join-Path $root 'dist'
$app  = Join-Path $dist 'app'
$port = Join-Path $dist 'NoidGPT-Portable'

# --- MSBuild zoeken via vswhere ---
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe niet gevonden. Is Visual Studio geinstalleerd?' }
$msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild niet gevonden.' }

Write-Host '==> Bouwen (Release x64)...' -ForegroundColor Cyan
& $msbuild (Join-Path $root 'NoidGPT\NoidGPT.vcxproj') /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'Build mislukt.' }

$exe = Join-Path $root 'NoidGPT\x64\Release\NoidGPT.exe'
if (-not (Test-Path $exe)) { $exe = Join-Path $root 'x64\Release\NoidGPT.exe' }
if (-not (Test-Path $exe)) { throw "NoidGPT.exe niet gevonden na build." }

# --- dist opruimen ---
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $app, $port | Out-Null
Copy-Item $exe $app

# --- Portable ---
Copy-Item $exe $port
Set-Content -Path (Join-Path $port 'portable.txt') -Value 'Portable modus: alle data (Ollama + modellen) komt in de map "data" naast NoidGPT.exe.'
Set-Content -Path (Join-Path $port 'noidgpt.ini') -Value "[NoidGPT]`r`n; Modelkeuze via NoidGPT. Beschikbaar: qwen3.5:4b-q4_K_M, llama3.1:8b, phi4-mini, llama3.2:3b`r`nModel=qwen3.5:4b-q4_K_M"
Compress-Archive -Path (Join-Path $port '*') -DestinationPath (Join-Path $dist 'NoidGPT-Portable.zip') -Force
Remove-Item $port -Recurse -Force
Write-Host '==> Portable zip klaar: dist\NoidGPT-Portable.zip' -ForegroundColor Green

# --- Inno Setup ---
$iscc = @(
  "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
  "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
  "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc) {
  Write-Host '==> Installer bouwen met Inno Setup...' -ForegroundColor Cyan
  & $iscc (Join-Path $root 'installer\NoidGPT.iss')
  if ($LASTEXITCODE -ne 0) { throw 'Inno Setup mislukt.' }
  Write-Host '==> Installer klaar: dist\NoidGPT-Setup.exe' -ForegroundColor Green
} else {
  Write-Warning 'Inno Setup 6 niet gevonden - installer overgeslagen. Installeer het en run dit script opnieuw.'
}
Remove-Item $app -Recurse -Force -ErrorAction SilentlyContinue
