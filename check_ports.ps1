Start-Sleep -Seconds 8
$usb = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Manufacturer -like '*USB*' -or $_.Name -like '*CH*' -or $_.Name -like '*WCH*' -or $_.Name -like '*COM*' }
foreach ($d in $usb) { Write-Host ('USB: ' + $d.Name + ' | status=' + $d.Status) }
Write-Host ('---- serial ports ----')
$p = ([System.IO.Ports.SerialPort]::GetPortNames()) -join ','
Write-Host ('PORTS=[' + $p + ']')
