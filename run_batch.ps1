# batch: buffer sweep (B2/B3/B4 @36MHz) + SPI sweep (S8/S16/S24/S48 @10lines)
# sync invocation, each group: config->compile->flash->capture 130s
$root = 'D:\administrator\Desktop\Hermes_Workshop\CH32V307\LVGL_Demo'
$pt   = "$root\perf_test.ps1"
$cap  = 130

$groups = @(
  @{Tag='B3';  Lines=20;  Speed=36000000},
  @{Tag='B4';  Lines=40;  Speed=36000000},
  @{Tag='S8';  Lines=10;  Speed=8000000},
  @{Tag='S16'; Lines=10;  Speed=16000000},
  @{Tag='S24'; Lines=10;  Speed=24000000},
  @{Tag='S48'; Lines=10;  Speed=48000000}
)

$batchStart = Get-Date
Write-Host ("===== BATCH START {0} ({1} groups) =====" -f $batchStart.ToString('HH:mm:ss'), $groups.Count)

foreach ($g in $groups) {
  $t0 = Get-Date
  Write-Host ("----- {0} START {1} -----" -f $g.Tag, $t0.ToString('HH:mm:ss'))
  & powershell -NoProfile -ExecutionPolicy Bypass -File $pt -Tag $g.Tag -Lines $g.Lines -Speed $g.Speed -Capture $cap
  $t1 = Get-Date
  Write-Host ("----- {0} DONE exit={1} {2} -----" -f $g.Tag, $LASTEXITCODE, $t1.ToString('HH:mm:ss'))
}

$elapsed = (Get-Date) - $batchStart
Write-Host ("===== BATCH END {0} total={1}min =====" -f (Get-Date).ToString('HH:mm:ss'), [math]::Round($elapsed.TotalMinutes,1))
