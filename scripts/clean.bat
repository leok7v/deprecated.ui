@echo off
pushd ..
taskkill /f /im mspdbsrv.exe 2> nul 1> nul
rmdir /q /s bin              2> nul 1> nul
rmdir /q /s build            2> nul 1> nul
rmdir /q /s msvc2022\.vs     2> nul 1> nul
del /q samples\version.h
popd

