$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$executable = Join-Path $projectRoot 'Vehicle_ECU_Simulator/dist/OJM_DDE_Virtual_ECU.exe'
$helpProcess = Start-Process -FilePath $executable -ArgumentList '--help' -WindowStyle Hidden -PassThru
if (-not $helpProcess.WaitForExit(15000)) { throw 'EXE --help timeout' }
if ($helpProcess.ExitCode -ne 0) { throw "EXE --help exit $($helpProcess.ExitCode)" }
$demoProcess = Start-Process -FilePath $executable -ArgumentList '--demo' -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds(15)
$windowProcess = $null
try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $demoProcess.Refresh()
        if ($demoProcess.HasExited) { throw "DEMO exited before its window: $($demoProcess.ExitCode)" }
        # A one-file PyInstaller process spawns a child with the application.
        $childProcesses = Get-CimInstance Win32_Process -Filter "ParentProcessId = $($demoProcess.Id)"
        $candidateIds = @($demoProcess.Id) + @($childProcesses | Select-Object -ExpandProperty ProcessId)
        foreach ($candidateId in $candidateIds) {
            $candidate = Get-Process -Id $candidateId -ErrorAction SilentlyContinue
            if ($candidate -and $candidate.MainWindowTitle -like 'OJM Systems DDE 6.0.5*') {
                $windowProcess = $candidate
                break
            }
        }
        if ($windowProcess) { break }
        Start-Sleep -Milliseconds 200
    }
    if (-not $windowProcess) { throw 'DEMO did not create the expected Tk window' }
    $title = $windowProcess.MainWindowTitle
    if (-not $windowProcess.CloseMainWindow()) { throw 'Could not close the test-owned DEMO window' }
    if (-not $demoProcess.WaitForExit(10000)) { throw 'DEMO did not exit cleanly' }
    if ($demoProcess.ExitCode -ne 0) { throw "DEMO exit $($demoProcess.ExitCode)" }
    $report = @{status='PASS'; help_exit=0; demo_window_title=$title; demo_exit=0; can_port_opened=$false}
    $report | ConvertTo-Json | Set-Content (Join-Path $projectRoot 'validation/exe_startup.json')
    $report | ConvertTo-Json
} finally {
    # Only processes created by this test are eligible for cleanup.
    if ($windowProcess -and -not $windowProcess.HasExited) { Stop-Process -Id $windowProcess.Id }
    if (-not $demoProcess.HasExited) { Stop-Process -Id $demoProcess.Id }
}
