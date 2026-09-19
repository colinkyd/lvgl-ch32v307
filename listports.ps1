$ports = [System.IO.Ports.SerialPort]::GetPortNames()
foreach ($p in $ports) { Write-Host ('port: ' + $p) }
Write-Host ('COM9 present: ' + @($ports).Contains('COM9'))
# 试打开 COM9
try {
  $port = New-Object System.IO.Ports.SerialPort('COM9', 115200, 'None', 8, 'One')
  $port.ReadTimeout = 200
  $port.Open()
  Write-Host ('COM9 open: ' + $port.IsOpen)
  $port.Close()
} catch {
  Write-Host ('COM9 open error: ' + $_.Exception.Message)
}
