@echo off
@echo building fanplayer jni library using ndk...
@echo.

set pwd=%~dp0

set PATH=%PATH%;%NDK_HOME%
call ndk-build

xcopy %pwd%..\libs %pwd%..\apk\app\src\main\jniLibs\ /s /y
rd %pwd%..\libs %pwd%..\obj /s /q

@echo.
@echo build fanplayer jni library done !
@echo.

pause

