# perf driver: rewrite perf_config -> compile -> flash -> capture serial
param(
  [Parameter(Mandatory)][string]$Tag,
  [Parameter(Mandatory)][int]$Lines,
  [Parameter(Mandatory)][long]$Speed,
  [int]$Capture = 70
)
$ErrorActionPreference = 'Continue'
$root = 'D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo'
$ac   = 'C:\Users\Administrator\Tools\arduino-cli\arduino-cli.exe'
$sketch = "$root\LVGL_Demo.ino"
$bp   = "$root\build"
$cfg  = "$root\src\bsp\perf_config.h"

Write-Host ("===== [" + $Tag + "] lines=" + $Lines + " speed=" + $Speed + " =====")

$tagLine = '#define PERF_TEST_TAG    "' + $Tag + '"'
$cfgLines = @(
  '#ifndef BSP_PERF_CONFIG_H',
  '#define BSP_PERF_CONFIG_H',
  ('#define PERF_BUF_LINES   ' + $Lines),
  ('#define PERF_SPI_SPEED   ' + $Speed + 'UL'),
  $tagLine,
  '#endif'
)
$cfgLines | Set-Content -Path $cfg -Encoding ASCII
Write-Host ("perf_config.h -> tag=" + $Tag + " lines=" + $Lines + " speed=" + $Speed)

Write-Host "----- COMPILE -----"
& $ac compile -b WCH:ch32v:CH32V30x_EVT -p COM9 --warnings none --build-path $bp $sketch 2>&1 | Select-String 'error:|undefined reference|multiple definition|Sketch uses|Global variables'
Write-Host ("compile exit = " + $LASTEXITCODE)
if ($LASTEXITCODE -ne 0) { Write-Host "COMPILE FAILED"; exit 1 }

Write-Host "----- FLASH -----"
powershell -NoProfile -ExecutionPolicy Bypass -File "$root\upload_no_verify.ps1" | Out-Null
Write-Host ("openocd exit = " + $LASTEXITCODE)

Write-Host ("----- CAPTURE serial " + $Capture + "s -----")

# wait for COM9 to re-enumerate (WCH-Link flash can drop the CH340 UART)
$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports -notcontains 'COM9') {
  Write-Host '### COM9 missing, trying USB reset ###'
  pnputil /restart-device 'USB\VID_05C6&PID_90BB' | Out-Null
  for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Seconds 2
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports -contains 'COM9') { break }
  }
}
Start-Sleep -Seconds 3   # let CH340 driver settle after re-enumeration
$port = New-Object System.IO.Ports.SerialPort('COM9', 115200, 'None', 8, 'One')
$port.ReadTimeout = 150
$port.Open()
$fs = [System.IO.File]::Create("$root\serial_$Tag.txt")
$buf = New-Object byte[] 4096
$bootSeen = $false
$got = 0
$end = (Get-Date).AddSeconds($Capture)
while ((Get-Date) -lt $end) {
  if (-not $port.IsOpen) { break }
  $n = 0
  try { $n = $port.Read($buf, 0, $buf.Length) } catch { $n = 0 }
  if ($n -gt 0) {
    $fs.Write($buf, 0, $n)
    $got += $n
    $txt = [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
    if (-not $bootSeen -and $txt -match 'perf test') { $bootSeen = $true; Write-Host '### BOOT seen ###' }
    if ($txt -match 'Weighted FPS') { Write-Host '### WEIGHTED FPS seen ###' }
  }
}
$fs.Close()
if ($port.IsOpen) { $port.Close() }
Write-Host ("===== [" + $Tag + "] DONE bootSeen=" + $bootSeen + " bytes=" + $got + " =====")
