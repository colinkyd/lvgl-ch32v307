Write-Host '===== ALL USB devices ====='
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -like '*USB*' -or $_.Name -like '*CH340*' -or $_.Name -like '*WCH*' -or $_.Name -like '*VCP*' -or $_.Name -like '*Serial*' -or $_.Name -like '*通信*' } | ForEach-Object { Write-Host ($_.Name + '  [' + $_.Status + ']  ' + $_.DeviceID) }
