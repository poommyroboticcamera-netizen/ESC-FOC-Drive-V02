@echo off
cd /d "%~dp0"
python serve_app.py
if errorlevel 1 pause
