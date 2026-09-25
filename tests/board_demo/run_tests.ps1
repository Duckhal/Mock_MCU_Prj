param(
    [string]$HostCompiler = 'gcc',
    [string]$ArmCompiler = 'E:/NXP/S32DS/S32DS/build_tools/gcc_v6.3/gcc-6.3-arm32-eabi/bin/arm-none-eabi-gcc.exe'
)

$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskOutput = Join-Path $taskRoot 'build/uart_cantp_echo'
$taskLog = Join-Path $taskOutput 'verification.log'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null

'UART CanTp multi-board application verification' | Tee-Object -FilePath $taskLog
$taskTests = @(
    (Join-Path $PSScriptRoot 'test_app.c'),
    (Join-Path $PSScriptRoot 'test_adc_led_mapping.c'),
    (Join-Path $PSScriptRoot 'test_buttons.c'),
    (Join-Path $PSScriptRoot 'test_com_update_filter.c'),
    (Join-Path $PSScriptRoot 'test_tx.c'),
    (Join-Path $PSScriptRoot 'test_rx.c'),
    (Join-Path $PSScriptRoot 'test_canif_mapping.c'),
    (Join-Path $PSScriptRoot 'test_uart_cantp_echo.c'),
    (Join-Path $PSScriptRoot 'test_uart_cantp_loopback_mode.c'),
    (Join-Path $PSScriptRoot 'test_uart_cantp_echo_timeout.c'),
    (Join-Path $taskRoot 'tests/com_stack/test_main_scheduler.c')
)

foreach ($taskTest in $taskTests) {
    $taskName = [IO.Path]::GetFileNameWithoutExtension($taskTest)
    $taskExe = Join-Path $taskOutput ($taskName + '.exe')
    & $HostCompiler -std=c99 -Wall -Wextra -Werror `
        "-I$(Join-Path $taskRoot 'include')" $taskTest -o $taskExe 2>&1 |
        Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Host compilation failed for ${taskTest}: $LASTEXITCODE"
    }
    & $taskExe 2>&1 | Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Host test failed for ${taskTest}: $LASTEXITCODE"
    }
}

$taskComTests = @(
    @{
        Source = Join-Path $taskRoot 'tests/com/test_com.c'
        Extra = Join-Path $taskRoot 'drivers/can/com/Com.c'
        Name = 'test_com'
    },
    @{
        Source = Join-Path $taskRoot 'tests/com/test_com_config.c'
        Extra = $null
        Name = 'test_com_config'
    }
)
foreach ($taskComTest in $taskComTests) {
    $taskExe = Join-Path $taskOutput ($taskComTest.Name + '.exe')
    $taskSources = @($taskComTest.Source)
    if ($taskComTest.Extra) {
        $taskSources += $taskComTest.Extra
    }
    $taskSources += Join-Path $taskRoot 'drivers/can/com/Com_Cfg.c'
    & $HostCompiler -std=c99 -Wall -Wextra -Werror $taskSources -o $taskExe 2>&1 |
        Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Host compilation failed for $($taskComTest.Source): $LASTEXITCODE"
    }
    & $taskExe 2>&1 | Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "Host test failed for $($taskComTest.Source): $LASTEXITCODE"
    }
}

$taskArmSources = @(
    (Join-Path $taskRoot 'app/app.c'),
    (Join-Path $taskRoot 'system/System.c'),
    (Join-Path $taskRoot 'src/main.c'),
    (Join-Path $taskRoot 'src/cantp_loopback_test.c'),
    (Join-Path $taskRoot 'drivers/can/com/Com.c')
)
foreach ($taskSource in $taskArmSources) {
    $taskObjectName = ($taskSource.Substring($taskRoot.Length + 1) `
        -replace '[\\/]', '_' -replace '\.c$', '.o')
    $taskObject = Join-Path $taskOutput $taskObjectName
    & $ArmCompiler -std=c99 -mcpu=cortex-m4 -mthumb -Wall -Wextra -Werror `
        -g3 "-I$(Join-Path $taskRoot 'include')" -c $taskSource `
        -o $taskObject 2>&1 | Tee-Object -FilePath $taskLog -Append
    if ($LASTEXITCODE -ne 0) {
        throw "ARM compilation failed for ${taskSource}: $LASTEXITCODE"
    }
}

'PASS: UART CanTp multi-board host tests and strict ARM compilation completed.' |
    Tee-Object -FilePath $taskLog -Append
