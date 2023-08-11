ECHO OFF
SET PREPAREDEVENV="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

CALL %PREPAREDEVENV%
cl.exe /EHsc /std:c++17 winhost.cpp 
DEL *.obj