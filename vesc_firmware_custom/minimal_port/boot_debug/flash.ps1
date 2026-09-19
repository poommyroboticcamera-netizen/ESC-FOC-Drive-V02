param(
    [string]$Programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
    [switch]$UnderReset
)
$ErrorActionPreference = 'Stop'
$imagePath = Join-Path $PSScriptRoot 'output\custom_v02_boot_debug.hex'
$manifestPath = Join-Path $PSScriptRoot 'output\sha256.json'
if (!(Test-Path -LiteralPath $Programmer)) { throw 'STM32CubeProgrammer CLI not found; provide -Programmer.' }
$expected = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json |
    Where-Object File -EQ 'custom_v02_boot_debug.hex'
if ((Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash -ne $expected.Hash) {
    throw 'HEX hash mismatch. Rebuild and verify the artifact before flashing.'
}
Write-Output 'STM32F405RG boot/debug ONLY: no motor rotation; gates remain LOW. Flash base 0x08000000.'
$connection = @('-c','port=SWD','freq=1000','mode=NORMAL')
if ($UnderReset) { $connection = @('-c','port=SWD','freq=1000','mode=UR','reset=HWrst') }
& $Programmer @connection '-w' $imagePath '-v' '-rst'
if ($LASTEXITCODE) { throw 'Flash/verification failed. Inspect STM32CubeProgrammer output.' }
