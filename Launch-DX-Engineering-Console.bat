@echo off
setlocal
set "REPO_ROOT=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%tools\dx-engineering-console.ps1" -RepositoryRoot "%REPO_ROOT%"
if errorlevel 1 (
  echo.
  echo DX-Extractor Engineering Console failed to start. See the message above.
  pause
)
endlocal
