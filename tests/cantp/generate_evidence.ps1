param(
    [string]$HostCompiler = 'gcc',
    [string]$ArmCompiler = 'E:/NXP/S32DS/S32DS/build_tools/gcc_v6.3/gcc-6.3-arm32-eabi/bin/arm-none-eabi-gcc.exe'
)

$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskEvidence = Join-Path $taskRoot 'evidence/cantp_phase3/raw'
$taskBuild = Join-Path $taskRoot 'build/cantp_phase3'
New-Item -ItemType Directory -Force -Path $taskEvidence | Out-Null

$taskRunner = Join-Path $PSScriptRoot 'run_tests.ps1'
$taskVerification = & powershell -ExecutionPolicy Bypass -File $taskRunner `
    -HostCompiler $HostCompiler -ArmCompiler $ArmCompiler 2>&1
$taskVerification | Set-Content -Encoding UTF8 `
    (Join-Path $taskEvidence 'full_verification.log')
if ($LASTEXITCODE -ne 0) {
    throw "CanTp verification failed: $LASTEXITCODE"
}

$taskProtocol = & (Join-Path $taskBuild 'test_cantp_phase2.exe') 2>&1
$taskProtocol | Set-Content -Encoding UTF8 `
    (Join-Path $taskEvidence 'protocol_trace.log')
if ($LASTEXITCODE -ne 0) {
    throw "CanTp protocol evidence run failed: $LASTEXITCODE"
}

$taskPhase3 = & (Join-Path $taskBuild 'test_cantp_phase3.exe') 2>&1
$taskPhase3 | Set-Content -Encoding UTF8 `
    (Join-Path $taskEvidence 'phase3_trace.log')
if ($LASTEXITCODE -ne 0) {
    throw "CanTp Phase-3 evidence run failed: $LASTEXITCODE"
}

$taskRouting = & (Join-Path $taskBuild 'test_cantp_routing.exe') 2>&1
$taskRouting | Set-Content -Encoding UTF8 `
    (Join-Path $taskEvidence 'routing_trace.log')
if ($LASTEXITCODE -ne 0) {
    throw "CanTp routing evidence run failed: $LASTEXITCODE"
}

$taskCommit = (& git -C $taskRoot rev-parse HEAD).Trim()
$taskDirty = if ((& git -C $taskRoot status --porcelain).Count -eq 0) {
    'clean'
} else {
    'modified'
}
$taskHostVersion = (& $HostCompiler --version | Select-Object -First 1)
$taskArmVersion = (& $ArmCompiler --version | Select-Object -First 1)
$taskMetadata = @(
    "Generated: $((Get-Date).ToString('yyyy-MM-ddTHH:mm:ssK'))",
    "Repository HEAD: $taskCommit",
    "Working tree: $taskDirty",
    "Host compiler: $taskHostVersion",
    "ARM compiler: $taskArmVersion",
    'Tick resolution: 1 ms virtual monotonic tick',
    'Data CAN ID: 0x650; FC CAN ID: 0x658; DLC: 8',
    'Source SHA-256:'
)
$taskHashFiles = @(
    'drivers/can/cantp/Cantp.c',
    'drivers/can/cantp/Cantp.h',
    'drivers/can/cantp/Cantp_Cfg.c',
    'drivers/can/cantp/Cantp_Types.h',
    'tests/cantp/test_cantp_phase1.c',
    'tests/cantp/test_cantp_phase2.c',
    'tests/cantp/test_cantp_phase3.c',
    'tests/cantp/test_cantp_routing.c',
    'tests/cantp/run_tests.ps1',
    'tests/cantp/generate_evidence.ps1'
)
foreach ($taskRelative in $taskHashFiles) {
    $taskHash = Get-FileHash -Algorithm SHA256 (Join-Path $taskRoot $taskRelative)
    $taskMetadata += "  $taskRelative $($taskHash.Hash)"
}
$taskMetadata | Set-Content -Encoding UTF8 `
    (Join-Path $taskEvidence 'environment.txt')

'PASS: generated CanTp T01-T14 evidence artifacts through Phase 3.'
