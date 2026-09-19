# standalone serial capture: open COM9, wait for data, dump to file
param([int]$Seconds = 12, [string]$Out = 'serial_diag.txt')
$root = 'D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo'
$port = New-Object System.IO.Ports.SerialPort('COM9', 115200, 'None', 8, 'One')
$port.ReadTimeout = 250
$port.Open()
Write-Host ("open port... IsOpen=" + $port.IsOpen)
$fs = [System.IO.File]::Create("$root\$Out")
$buf = New-Object byte[] 4096
$got = 0
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  $n = 0
  try { $n = $port.Read($buf, 0, $buf.Length) } catch { $n = 0 }
  if ($n -gt 0) {
    $fs.Write($buf, 0, $n)
    $got += $n
    $txt = [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
    Write-Host ("### +$n bytes: " + ($txt -replace "[`r`n]", ' | '))
  }
}
$fs.Close()
if ($port.IsOpen) { $port.Close() }
Write-Host ("DONE total bytes=" + $got)
