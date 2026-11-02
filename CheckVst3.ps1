# VST3 loading diagnostic script
param()

$ErrorActionPreference = "Stop"

$Vst3List = @(
    "C:\Users\jaisw\OneDrive\Desktop\Project Seven\build\ProjectSeven_artefacts\Release\VST3\Project Seven.vst3"
)

foreach ($Path in $Vst3List)
{
    if (!(Test-Path $Path)) { continue }

    Write-Host "=== Path: $Path ==="
    try
    {
        $Counter = New-Object System.Security.Cryptography.SHA256Managed
        $Bytes = [IO.File]::ReadAllBytes($Path)
        $Hash = $Counter.ComputeHash($Bytes)
        $Counter.Dispose()

        Write-Host "  Size: $($Bytes.Length) bytes"
        Write-Host "  SHA256: $([BitConverter]::ToString($Hash).Replace('-','') -join '')"
    }
    finally
    {
        if ($Counter -and $Counter -is [System.IDisposable])
        {
            $Counter.Dispose()
        }
    }

    Write-Host ""
}

Write-Host "Done"
