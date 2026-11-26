@echo off
setlocal
set "MAMA_PYTHON=%LocalAppData%\Programs\Python\Python314\python.exe"
if exist "%MAMA_PYTHON%" (
  "%MAMA_PYTHON%" "%~dp0Tools/BuildMama.py"
) else (
  py -3 "%~dp0Tools/BuildMama.py"
)
set "MAMA_RESULT=%ERRORLEVEL%"
pause
exit /b %MAMA_RESULT%
