@echo off
cd /d "C:\Users\jaisw\OneDrive\Desktop\Project Seven\build"
dir /s /b *ProjectSeven*link.txt 2>nul
dir /s /b *ProjectSeven*.txt 2>nul | findstr /i link
