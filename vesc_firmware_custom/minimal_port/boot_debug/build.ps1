param([string]$ToolchainBin = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$ToolchainBin) {
    $gccCommand = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    if ($gccCommand) { $ToolchainBin = Split-Path $gccCommand.Source }
    else {
        $gccRoot = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\STMicroelectronics\tools\xpack-arm-none-eabi-gcc'
        $gccFile = Get-ChildItem -LiteralPath $gccRoot -Filter arm-none-eabi-gcc.exe -Recurse |
            Sort-Object FullName -Descending | Select-Object -First 1
        if (!$gccFile) { throw 'Provide -ToolchainBin pointing to ARM GNU bin directory.' }
        $ToolchainBin = $gccFile.DirectoryName
    }
}
if (!$OutputDirectory) { $OutputDirectory = Join-Path $PSScriptRoot 'output' }
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$gcc = Join-Path $ToolchainBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $ToolchainBin 'arm-none-eabi-objcopy.exe'
$size = Join-Path $ToolchainBin 'arm-none-eabi-size.exe'
$objdump = Join-Path $ToolchainBin 'arm-none-eabi-objdump.exe'
$elf = Join-Path $outputPath 'custom_v02_boot_debug.elf'
$arguments = @('-mcpu=cortex-m4','-mthumb','-mfloat-abi=soft','-Os','-g3',
    '-std=c11','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections',
    '-Wall','-Wextra','-Werror','-nostdlib','-Wl,--gc-sections','-Wl,--build-id=none','-Wl,-z,noexecstack',
    "-Wl,-Map,$(Join-Path $outputPath 'custom_v02_boot_debug.map')",
    '-T',(Join-Path $PSScriptRoot 'stm32f405.ld'))
foreach ($dir in @('board','drivers','communication')) { $arguments += "-I$(Join-Path $PSScriptRoot $dir)" }
foreach ($file in @('startup.S','main.c','board/board.c','drivers/gate_driver.c','communication/debug_mailbox.c')) {
    $arguments += Join-Path $PSScriptRoot $file
}
$arguments += @('-o',$elf)
& $gcc @arguments
if ($LASTEXITCODE) { throw 'Compile/link failed.' }
foreach ($format in @(@('binary','bin'),@('ihex','hex'))) {
    & $objcopy -O $format[0] $elf (Join-Path $outputPath ('custom_v02_boot_debug.'+$format[1]))
    if ($LASTEXITCODE) { throw 'objcopy failed.' }
}
$sizeReport = & $size $elf
if ($LASTEXITCODE) { throw 'size failed.' }
$disassembly = & $objdump -d $elf
if ($LASTEXITCODE) { throw 'objdump failed.' }
$disassembly | Set-Content -LiteralPath (Join-Path $outputPath 'disassembly.txt') -Encoding utf8
$sizeReport | Set-Content -LiteralPath (Join-Path $outputPath 'size.txt') -Encoding utf8
$version = & $gcc --version
$version | Set-Content -LiteralPath (Join-Path $outputPath 'toolchain.txt') -Encoding utf8
Get-ChildItem -LiteralPath $outputPath -File | Where-Object Extension -In '.bin','.hex','.elf' |
    Get-FileHash -Algorithm SHA256 | Select-Object Hash,@{Name='File';Expression={Split-Path $_.Path -Leaf}} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputPath 'sha256.json') -Encoding utf8
$sizeReport
Write-Output "Built boot/debug-only artifacts in $outputPath"
