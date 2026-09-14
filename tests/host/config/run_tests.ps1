$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$testBinary = Join-Path ([System.IO.Path]::GetTempPath()) "mock_mcu_part1_config_tests.exe"

$sources = @(
    (Join-Path $PSScriptRoot "test_part1_config.c"),
    (Join-Path $projectRoot "drivers\can\config\com\com_cfg.c"),
    (Join-Path $projectRoot "drivers\can\config\pdur\pdur_cfg.c"),
    (Join-Path $projectRoot "drivers\can\config\canif\canif_cfg.c"),
    (Join-Path $projectRoot "drivers\can\config\can\Can_Cfg.c"),
    (Join-Path $projectRoot "drivers\can\config\node_cfg.c")
)

& gcc -std=c99 -Wall -Wextra -Werror -pedantic @sources -o $testBinary
if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}

& $testBinary
exit $LASTEXITCODE
