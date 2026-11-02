$files = Get-ChildItem -Path 'C:\Users\jaisw\OneDrive\Desktop\Project Seven\build\CMakeFiles' -Recurse -Filter '*ProjectSeven_vst3*' -File
foreach ($f in $files) {
    Write-Host "File: $($f.FullName)"
    Get-Content $f.FullName | Select-Object -First 20
    Write-Host ""
}
