$ocdBin = "C:\Users\Administrator\AppData\Local\Arduino15\packages\WCH\tools\openocd\1.0.0\bin"
$ocd    = "$ocdBin\openocd.exe"
$cfg    = "$ocdBin\wch-riscv.cfg"
$elf    = "D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo\build\LVGL_Demo.ino.elf"

Write-Host "=== stale openocd procs ==="
Get-Process openocd -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "killing $($_.Id)"; Stop-Process -Id $_.Id -Force }
Start-Sleep -Milliseconds 500

if (-not (Test-Path $elf)) { Write-Host "ELF NOT FOUND: $elf"; exit 1 }
Write-Host "ELF OK: $elf"

# openocd 的 Tcl 把反斜杠当转义字符，路径转正斜杠
$elfF = $elf -replace '\\','/'

& $ocd -f $cfg -c init -c halt -c "program $elfF; wlink_reset_resume; exit;"
Write-Host "=== OPENOCD EXIT = $LASTEXITCODE ==="
