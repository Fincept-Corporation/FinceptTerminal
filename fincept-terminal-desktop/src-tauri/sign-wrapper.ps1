param($FilePath)

# Log what we're checking
Write-Host "Checking file: $FilePath"

# Match any file in python directory
if ($FilePath -like "*\python\*" -or $FilePath -like "*/python/*") {
    Write-Host "SKIPPED Python file"
    exit 0
}

# Sign other files
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Write-Host "Signing with relic..."
& relic sign --file $FilePath --key azure --config "$scriptDir\relic.conf"
exit $LASTEXITCODE