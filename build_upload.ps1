$ac  = "C:\Users\Administrator\Tools\arduino-cli\arduino-cli.exe"
$sketch = "D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo\LVGL_Demo.ino"
$bp  = "D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo\build"
$log = "D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo\build_upload.log"

& $ac compile -b WCH:ch32v:CH32V30x_EVT -p COM9 --warnings none --build-path $bp $sketch 2>&1 | Tee-Object -FilePath $log
$code = $LASTEXITCODE
Write-Host "=== COMPILE EXIT = $code ==="
if ($code -ne 0) { exit $code }

& $ac upload  -b WCH:ch32v:CH32V30x_EVT -p COM9 --build-path $bp $sketch 2>&1 | Tee-Object -FilePath $log -Append
$code = $LASTEXITCODE
Write-Host "=== UPLOAD EXIT = $code ==="
exit $code
