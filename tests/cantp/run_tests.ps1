param(
    [string]$HostCompiler = 'gcc',
    [string]$ArmCompiler = 'E:/NXP/S32DS/S32DS/build_tools/gcc_v6.3/gcc-6.3-arm32-eabi/bin/arm-none-eabi-gcc.exe'
)

$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskOutput = Join-Path $taskRoot 'build/cantp_phase3'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskLog = Join-Path $taskOutput 'verification.log'

$taskCanTp = Join-Path $taskRoot 'drivers/can/cantp/Cantp.c'
$taskCanTpConfig = Join-Path $taskRoot 'drivers/can/cantp/Cantp_Cfg.c'
$taskPduR = Join-Path $taskRoot 'drivers/can/pdur/PduR.c'
$taskPduRConfig = Join-Path $taskRoot 'drivers/can/pdur/PduR_Cfg.c'
$taskNodeApp = Join-Path $taskRoot 'app/node_app.c'
$taskPhase1Test = Join-Path $PSScriptRoot 'test_cantp_phase1.c'
$taskPhase2Test = Join-Path $PSScriptRoot 'test_cantp_phase2.c'
$taskPhase3Test = Join-Path $PSScriptRoot 'test_cantp_phase3.c'
$taskRoutingTest = Join-Path $PSScriptRoot 'test_cantp_routing.c'
$taskPhase1Exe = Join-Path $taskOutput 'test_cantp_phase1.exe'
$taskPhase2Exe = Join-Path $taskOutput 'test_cantp_phase2.exe'
$taskPhase3Exe = Join-Path $taskOutput 'test_cantp_phase3.exe'
$taskRoutingExe = Join-Path $taskOutput 'test_cantp_routing.exe'

'CanTp Phase-3 verification' | Tee-Object -FilePath $taskLog
& $HostCompiler -std=c99 -Wall -Wextra -Werror -g `
    $taskPhase1Test $taskCanTp $taskCanTpConfig -o $taskPhase1Exe 2>&1 |
    Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-1 test compilation failed: $LASTEXITCODE" }
& $taskPhase1Exe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-1 tests failed: $LASTEXITCODE" }

& $HostCompiler -std=c99 -Wall -Wextra -Werror -g `
    $taskPhase2Test $taskCanTp $taskCanTpConfig -o $taskPhase2Exe 2>&1 |
    Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-2 test compilation failed: $LASTEXITCODE" }
& $taskPhase2Exe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-2 tests failed: $LASTEXITCODE" }

& $HostCompiler -std=c99 -Wall -Wextra -Werror -g `
    $taskPhase3Test $taskCanTp $taskCanTpConfig -o $taskPhase3Exe 2>&1 |
    Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-3 test compilation failed: $LASTEXITCODE" }
& $taskPhase3Exe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp Phase-3 tests failed: $LASTEXITCODE" }

& $HostCompiler -std=c99 -Wall -Wextra -Werror -g `
    $taskRoutingTest $taskNodeApp $taskPduR $taskPduRConfig $taskCanTpConfig `
    -o $taskRoutingExe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp routing test compilation failed: $LASTEXITCODE" }
& $taskRoutingExe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp routing tests failed: $LASTEXITCODE" }

foreach ($taskSource in @($taskCanTp, $taskCanTpConfig, $taskPduR,
                           $taskPduRConfig, $taskNodeApp)) {
    $taskObjectName = ($taskSource.Substring($taskRoot.Length + 1) `
        -replace '[\\/]', '_' -replace '\.c$', '.o')
    $taskObject = Join-Path $taskOutput $taskObjectName
    & $ArmCompiler -std=c99 -mcpu=cortex-m4 -mthumb -Wall -Wextra -Werror `
        -g3 -c $taskSource -o $taskObject 2>&1 |
        Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "ARM compilation failed for ${taskSource}: $LASTEXITCODE"
    }
}

'PASS: CanTp Phase-3 host tests and strict ARM object compilation completed.' |
    Tee-Object -FilePath $taskLog -Append
