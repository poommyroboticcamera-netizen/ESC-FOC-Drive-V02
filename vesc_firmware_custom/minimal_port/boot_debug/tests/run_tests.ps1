param([string]$HostGcc = '')
$ErrorActionPreference='Stop'
if (!$HostGcc) {
    $HostGcc = Join-Path $PSScriptRoot '..\..\..\tmp\w64devkit\extracted\w64devkit\bin\gcc.exe'
}
$rootPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe = Join-Path $rootPath 'output\test_fail_closed.exe'
$hostBin = Split-Path ([IO.Path]::GetFullPath($HostGcc))
$argsList = @("-B$hostBin/",'-std=c11','-O2','-Wall','-Wextra','-Werror','-DPORTING_HOST_TEST',
    "-I$rootPath\board","-I$rootPath\drivers","-I$rootPath\communication",
    "$PSScriptRoot\test_fail_closed.c","$rootPath\board\board.c",
    "$rootPath\drivers\gate_driver.c","$rootPath\communication\debug_mailbox.c",'-o',$exe)
& $HostGcc @argsList
if ($LASTEXITCODE) { throw 'Host test compile failed.' }
$result = & $exe
if ($LASTEXITCODE) { throw 'Fail-closed tests failed.' }
$result | Set-Content -LiteralPath (Join-Path $rootPath 'output\host_test_result.txt') -Encoding utf8
$result
