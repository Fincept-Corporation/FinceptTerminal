# Setup Python 3.12.8 for Windows (Embedded)

$PYTHON_VERSION = "3.12.8"
$PYTHON_URL = "https://www.python.org/ftp/python/$PYTHON_VERSION/python-$PYTHON_VERSION-embed-amd64.zip"
$GET_PIP_URL = "https://bootstrap.pypa.io/get-pip.py"
$RESOURCES_DIR = "$PSScriptRoot\..\src-tauri\resources"
$PYTHON_DIR = "$RESOURCES_DIR\python-windows"

Write-Host "Setting up Python $PYTHON_VERSION for Windows..." -ForegroundColor Cyan

# Clean existing installation
if (Test-Path $PYTHON_DIR) {
    Write-Host "Removing existing Python installation..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $PYTHON_DIR
}

# Download Python embeddable
Write-Host "Downloading Python embeddable package..." -ForegroundColor Green
$pythonZip = "$env:TEMP\python-$PYTHON_VERSION-embed-amd64.zip"
Invoke-WebRequest -Uri $PYTHON_URL -OutFile $pythonZip

# Extract Python
Write-Host "Extracting Python..." -ForegroundColor Green
Expand-Archive -Path $pythonZip -DestinationPath $PYTHON_DIR -Force
Remove-Item $pythonZip

# Enable site-packages
Write-Host "Enabling site-packages..." -ForegroundColor Green
$pthFile = "$PYTHON_DIR\python312._pth"
(Get-Content $pthFile) -replace '#import site', 'import site' | Set-Content $pthFile

# Download and install pip
Write-Host "Installing pip..." -ForegroundColor Green
$getPip = "$env:TEMP\get-pip.py"
Invoke-WebRequest -Uri $GET_PIP_URL -OutFile $getPip
& "$PYTHON_DIR\python.exe" $getPip
Remove-Item $getPip

# Install requirements
Write-Host "Installing Python dependencies..." -ForegroundColor Green
& "$PYTHON_DIR\python.exe" -m pip install -r "$RESOURCES_DIR\requirements.txt"

Write-Host "Python setup complete!" -ForegroundColor Green
Write-Host "Testing installation..." -ForegroundColor Cyan

# Test imports
& "$PYTHON_DIR\python.exe" -c "import pandas, numpy, yfinance; print(f'✓ Pandas: {pandas.__version__} | NumPy: {numpy.__version__} | YFinance: {yfinance.__version__}')"

Write-Host "All done!" -ForegroundColor Green
