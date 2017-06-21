@echo off

REM call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set path=C:\home\dev\ray_tracing\bin;%path%
set path=C:\home\4coder;%path%
set path=C:\home\openseeit;%path%

DOSKEY ls=dir
DOSKEY os=openseeit.exe $*
DOSKEY run=main.exe $g test.ppm $T openseeit.exe test.ppm
DOSKEY brun=call ..\src\build.bat $T main.exe $g test.ppm $T openseeit.exe test.ppm

cd C:\home\dev\ray_tracing\bin
