@echo off
rem gnu-win32 cannot understand real dos names starting with C:\...  
echo Starting Oz demo.
echo This may take some time...
rem %OZHOME%\platform\win32-i486\ozemulator -f - < %OZHOME%\demo\rundemo
%OZHOME%\platform\win32-i486\ozemulator -f %OZHOME%\demo\rundemo

