color 0A
setlocal
set workdir=%~dp0
PATH=%PATH%;%workdir%;%USERPROFILE%\.platformio\penv\Scripts;
echo off
set PYTHONHOME=%USERPROFILE%\Python
set PYTHONPATH=%USERPROFILE%\Python
@chcp 1251>nul
mode con: cols=88 lines=40
cls 

:m1
Echo  #------------------------------------------#-----------------------------------------# 
Echo  *                COMMANDS                  *  (Russian)      �������                 *
Echo  #------------------------------------------#-----------------------------------------# 
Echo  *              GIT commands                *              ������ � GIT               *
Echo  *  Switch to MASTER branch          - (1)  *  ������������� �� ����� MASTER          *
Echo  *  Switch to DEV branch             - (2)  *  ������������� �� ����� DEV             *
Echo  #------------------------------------------#-----------------------------------------# 
echo  *  Reset changes in local repo!     - (3)  *  �������� ��������� � ��������� ����!   *
Echo  *  WARNING! This will revert all changes!  *  ��������! ��� ������� ��� ���������!   *
Echo  #------------------------------------------#-----------------------------------------# 
Echo  *             Build and flash              *            ������ � ��������            *
Echo  *  Build - Esp8266 160MHz           - (4)  *  ������� ��� Esp8266 �� 160���          *
Echo  *  Build - Esp8266 80MHz            - (5)  *  ������� ��� Esp8266 �� 80���           *
Echo  *  Build - Esp32                    - (6)  *  ������� ��� Esp32                      *
Echo  *                                          *                                         *
Echo  *  Build and upload - Esp8266@160   - (7)  *  ������� � ������� - Esp8266 �� 160���  *
Echo  *  Build and upload - Esp8266@80    - (8)  *  ������� � ������� - Esp8266 �� 80���   *
Echo  *  Build and upload - Esp32         - (9)  *  ������� � ������� - Esp32              *
Echo  *                                          *                                         *
Echo  *            File System                   *           �������� �������              *
Echo  *  Update FS data from framework    - (u)  *  �������� ����� �� �� ����������        *
Echo  *  Build File System image          - (b)  *  ������� ����� �������� �������         *
Echo  *  Build and upload File System     - (f)  *  ������� � ������� �������� �������     *
Echo  *                                          *                                         *
Echo  *  Erase Flash                      - (e)  *  ������� ���� �����������               *
Echo  *                                          *                                         *
Echo  *  Clean up temp files .pio         - (c)  *  �������� ��������� ����� .pio          *
Echo  *  Update libs and PIO Core         - (g)  *  �������� ���������� � ����� PIO Core   *
Echo  *------------------------------------------#-----------------------------------------*
Echo  *  CMD window                       - (m)  *  ������� ���� ���������� ������ CMD     *
Echo  *------------------------------------------#-----------------------------------------*
Echo  *  Remove Platformio installation   - (R)  *  ��������� ������� Platformio � ��      *
Echo  #------------------------------------------#-----------------------------------------#
Echo.
Set /p choice="Your choice (��� �����): "


if "%choice%"=="1" call update-DEV-from-Git.cmd 1
if "%choice%"=="2" call update-DEV-from-Git.cmd 2
if "%choice%"=="3" call update-DEV-from-Git.cmd 3
if "%choice%"=="4" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --environment esp8266@160)
if "%choice%"=="5" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --environment esp8266)
if "%choice%"=="6" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --environment esp32)
if "%choice%"=="7" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target upload --environment esp8266@160)
if "%choice%"=="8" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target upload --environment esp8266)
if "%choice%"=="9" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target upload --environment esp32)
if "%choice%"=="u" (
	cd %workdir%\resources\
	start respack.cmd
	cd %workdir%
)
if "%choice%"=="b" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target buildfs --environment esp8266@160)
if "%choice%"=="f" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target uploadfs --environment esp8266@160)
if "%choice%"=="e" ("%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run --target erase --environment esp8266@160)
if "%choice%"=="c" (
	pio system prune -f
	rmdir /S /Q %workdir%\.pio
)
if "%choice%"=="g" (
	"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" upgrade
	"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" update
)
if "%choice%"=="m" (start cmd)
if "%choice%"=="R" (rmdir /S "%USERPROFILE%\.platformio")

Echo.
Echo.
Echo.
pause
del %workdir%\resources\.wget-hsts
cls
goto m1


exit