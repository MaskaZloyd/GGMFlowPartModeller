param(
  [Parameter(Mandatory = $true)]
  [string]$CommandLine,

  [string]$Arch = "x64"
)

$candidates = @()

if ($env:VSDEVCMD -and (Test-Path -LiteralPath $env:VSDEVCMD)) {
  $candidates += $env:VSDEVCMD
}

if ($env:ProgramFiles) {
  $candidates += Join-Path $env:ProgramFiles "Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
  $candidates += Join-Path $env:ProgramFiles "Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat"
  $candidates += Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
  $candidates += Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
}

$programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
if ($programFilesX86) {
  $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path -LiteralPath $vswhere) {
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $installPath) {
      $candidates += Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    }
  }
}

$vsDevCmd = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $vsDevCmd) {
  Write-Error "Could not find VsDevCmd.bat. Install Visual Studio/Build Tools with C++ workload or set VSDEVCMD."
  exit 1
}

$cmd = "call `"$vsDevCmd`" -arch=$Arch >NUL && $CommandLine"
& cmd.exe /S /C $cmd
exit $LASTEXITCODE
