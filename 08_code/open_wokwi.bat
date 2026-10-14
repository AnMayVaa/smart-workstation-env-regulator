@echo off
rem Open the room node and desk node in separate VS Code windows.
rem In each window press F1 and choose "Wokwi: Start Simulator".
cd /d "%~dp0"
start "" code -n "%~dp0room_node"
timeout /t 2 >nul
start "" code -n "%~dp0desk_node"
if /i "%1"=="2" (timeout /t 2 >nul & start "" code -n "%~dp0desk_node_2")
