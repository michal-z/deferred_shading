@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

cd ..\vc2015
msbuild /nologo /verbosity:quiet /p:Configuration=Debug eneida.sln
cd ..\src
