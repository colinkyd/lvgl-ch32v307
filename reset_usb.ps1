# try to reset USB composite device (board) to re-enumerate
$devs = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -like 'USB\VID_05C6*' }
foreach ($d in $devs) {
  Write-Host ('RESET: ' + $d.Name + ' instance=' + $d.InstanceID)
}
# disable + enable
$wmi = [wmiclass]'Win32_PnPEntity'
foreach ($d in $devs) {
  try { $d.Disable() } catch { Write-Host ('disable err: ' + $_.Exception.Message) }
}
Start-Sleep -Seconds 3
foreach ($d in $devs) {
  try { $d.Enable() } catch { Write-Host ('enable err: ' + $_.Exception.Message) }
}
Start-Sleep -Seconds 6
$p = ([System.IO.Ports.SerialPort]::GetPortNames()) -join ','
Write-Host ('AFTER RESET PORTS=[' + $p + ']')
