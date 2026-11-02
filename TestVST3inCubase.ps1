# Script to test VST3 loading in Cubase 15
# Run after extracting the ZIP file

Param()

$ErrorActionPreference = "Stop"

Write-Host "=== Testing VST3 Loading in Cubase 15 ==="
Write-Host "Step 1: Extract VST3 from ZIP"
Write-Host "  Location: C:\Users\jaisw\OneDrive\Desktop\Project Seven\ProjectSeven-1.2.0-win64.zip"
Write-Host "  Extract to: C:\Program Files\Project Seven VST3"

Write-Host "`nStep 2: Copy to VST3 scan folder"
Write-Host "  Check VST3ScanProgramPicker availability"
$ScanCommand = Get-Command Vst3ScanProgramPicker -ErrorAction SilentlyContinue
if ($ScanCommand)
{
    Write-Host "  Vst3ScanProgramPicker found: $($ScanCommand.Source)"
    Write-Host "  Use this to force VST3 scan"
}
else
{
    Write-Host "  Vst3ScanProgramPicker not found in PATH"
    Write-Host "  Try: Start-Process 'C:\Program Files\Steinberg\Cubase 15\Vst3ScanProgramPicker.exe'"
}

Write-Host "`nStep 3: Verify DLL exists and is valid"
$Vst3Path = "C:\Program Files\Project Seven VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3"
if (Test-Path $Vst3Path)
{
    Write-Host "  VST3 DLL found: $Vst3Path"
    Write-Host "  Size: $([math]::Round((Get-Item $Vst3Path).Length / 1KB, 2)) KB"
    
    # Get file hash for verification
    $Hash = Get-FileHash $Vst3Path -Algorithm SHA256
    Write-Host "  SHA256: $($Hash.Hash)"
}
else
{
    Write-Host "  WARNING: VST3 DLL not found at expected location"
    Write-Host "  Check if extraction was successful"
    Write-Host "  Expected: C:\Program Files\Project Seven VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3"
}

Write-Host "`nStep 4: Check dependencies and runtime"
Write-Host "  Running dumpbin to check dependencies..."
$VSWhere = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"
$Vst3Dll = "C:\Program Files\Project Seven VST3\Project Seven.vst3\Contents\x86_64-win\Project Seven.vst3"

if (Test-Path $VSWhere -and Test-Path $Vst3Dll)
{
    Write-Host "  Executing: $VSWhere /dependents $Vst3Dll"
    $Output = & $VSWhere /dependents $Vst3Dll 2>&1
    Write-Host "  Dependencies found:"
    $Output | Select-String "\.dll" | ForEach-Object { Write-Host "    $_" }
}
else
{
    Write-Host "  WARNING: Could not verify dependencies"
    if (!(Test-Path $VSWhere)) { Write-Host "    dumpbin.exe not found" }
    if (!(Test-Path $Vst3Dll)) { Write-Host "    VST3 DLL not found" }
}

Write-Host "`nStep 5: Launch Cubase 15 and check VST3 manager"
Write-Host "  Open Cubase 15"
Write-Host "  Go to: Studio > Manage VST Plug-ins..."
Write-Host "  Look for 'Project Seven' in the list"
Write-Host "  If not listed, use 'Scan VST Plug-ins...' button"

Write-Host "`nStep 6: Test loading the plugin"
Write-Host "  Create a new empty Cubase project"
Write-Host "  Add an instrument track"
Write-Host "  Select 'Project Seven' as the instrument"
Write-Host "  If it loads successfully, test with MIDI notes"

Write-Host "`n=== Diagnostic Complete ==="
Write-Host "If the plugin still doesn't load in Cubase, please check:"
Write-Host "  1. Cubase 15 version is up to date"
Write-Host "  2. VST3 folder path is correct in Cubase preferences"
Write-Host "  3. Windows Event Viewer for any plugin loading errors"
Write-Host "  4. Try running Cubase as Administrator once to test"
