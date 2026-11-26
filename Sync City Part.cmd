@echo off
setlocal
set "MAMA_PYTHON=%LocalAppData%\Programs\Python\Python314\python.exe"
if exist "%MAMA_PYTHON%" (
  "%MAMA_PYTHON%" "%~dp0Tools/SyncStreetscape.py" %*
) else (
  py -3 "%~dp0Tools/SyncStreetscape.py" %*
)
set "MAMA_RESULT=%ERRORLEVEL%"
exit /b %MAMA_RESULT%
