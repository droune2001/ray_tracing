@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

set path=D:\dev\ray_tracing\bin;%path%
set path=D:\dev\4coder;%path%
set path=D:\dev\openseeit;%path%

REM pour rediriger un flux dans une DOSKY, utiliser $g (=greater)
DOSKEY ls=dir
DOSKEY os=openseeit.exe $*
DOSKEY run=pushd ..\data $T main.exe $* $T openseeit.exe out.png $T popd
DOSKEY brun=pushd ..\data $T call ..\src\build.bat $T main.exe $* $T openseeit.exe out.png $T popd

cd D:\dev\ray_tracing\bin
