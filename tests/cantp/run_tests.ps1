param(
    [string]$HostCompiler = 'gcc',
    [string]$ArmCompiler = 'E:/NXP/S32DS/S32DS/build_tools/gcc_v6.3/gcc-6.3-arm32-eabi/bin/arm-none-eabi-gcc.exe'
)

$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskOutput = Join-Path $taskRoot 'build/cantp_phase1'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
$taskLog = Join-Path $taskOutput 'verification.log'

$taskCanTp = Join-Path $taskRoot 'drivers/can/cantp/Cantp.c'
$taskCanTpConfig = Join-Path $taskRoot 'drivers/can/cantp/Cantp_Cfg.c'
$taskPduR = Join-Path $taskRoot 'drivers/can/pdur/PduR.c'
$taskPduRConfig = Join-Path $taskRoot 'drivers/can/pdur/PduR_Cfg.c'
$taskNodeApp = Join-Path $taskRoot 'app/node_app.c'
$taskProtocolTest = Join-Path $PSScriptRoot 'test_cantp_phase1.c'
$taskRoutingTest = Join-Path $PSScriptRoot 'test_cantp_routing.c'
$taskProtocolExe = Join-Path $taskOutput 'test_cantp_phase1.exe'
$taskRoutingExe = Join-Path $taskOutput 'test_cantp_routing.exe'

'CanTp Phase-1 verification' | Tee-Object -FilePath $taskLog
& $HostCompiler -std=c99 -Wall -Wextra -Werror -g `
    $taskProtocolTest $taskCanTp $taskCanTpConfig -o $taskProtocolExe 2>&1 |
    Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp protocol test compilation failed: $LASTEXITCODE" }
& $taskProtocolExe 2>&1 | Tee-Object -FilePath $taskLog -Append
if ($LASTEXITCODE -ne 0) { throw "CanTp protocol tests failed: $LASTEXITCODE" }

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

'PASS: CanTp Phase-1 host tests and strict ARM object compilation completed.' |
    Tee-Object -FilePath $taskLog -Append
