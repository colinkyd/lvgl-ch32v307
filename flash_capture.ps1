# flash -> immediate capture (back-to-back, catches full benchmark round + Weighted FPS)
param([string]$Tag='B1', [int]$Capture=32)
$root = 'D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo'
Write-Host ("===== [$Tag] FLASH =====")
powershell -NoProfile -ExecutionPolicy Bypass -File "$root\upload_no_verify.ps1"
Write-Host ("openocd exit = " + $LASTEXITCODE)
Write-Host ("===== [$Tag] CAPTURE " + $Capture + "s (immediate) =====")
$port = New-Object System.IO.Ports.SerialPort('COM9', 115200, 'None', 8, 'One')
$port.ReadTimeout = 150
$port.Open()
$fs = [System.IO.File]::Create("$root\serial_$Tag.txt")
$buf = New-Object byte[] 4096
$end = (Get-Date).AddSeconds($Capture)
$got = 0
while ((Get-Date) -lt $end) {
  $n = 0
  try { $n = $port.Read($buf, 0, $buf.Length) } catch { $n = 0 }
  if ($n -gt 0) {
    $fs.Write($buf, 0, $n)
    $got += $n
  }
}
$fs.Close()
if ($port.IsOpen) { $port.Close() }
Write-Host ("===== [$Tag] captured " + $got + " bytes =====")
