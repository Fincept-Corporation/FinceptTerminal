$result = & 'C:\Python313\python.exe' 'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\test_quantlib_batch1.py' 2>&1
$result | Out-File 'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ps_output.txt' -Encoding UTF8
Write-Host "Done, exit $LASTEXITCODE"
