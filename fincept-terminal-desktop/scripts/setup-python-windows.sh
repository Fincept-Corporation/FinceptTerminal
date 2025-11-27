#!/bin/bash
# Setup Python 3.12.8 for Windows (Embedded) - Bash version for Git Bash/WSL

PYTHON_VERSION="3.12.8"
PYTHON_URL="https://www.python.org/ftp/python/$PYTHON_VERSION/python-$PYTHON_VERSION-embed-amd64.zip"
GET_PIP_URL="https://bootstrap.pypa.io/get-pip.py"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESOURCES_DIR="$SCRIPT_DIR/../src-tauri/resources"
PYTHON_DIR="$RESOURCES_DIR/python-windows"

echo "Setting up Python $PYTHON_VERSION for Windows..."

# Clean existing installation
if [ -d "$PYTHON_DIR" ]; then
    echo "Removing existing Python installation..."
    rm -rf "$PYTHON_DIR"
fi

# Download Python embeddable
echo "Downloading Python embeddable package..."
curl -L -o "/tmp/python-$PYTHON_VERSION-embed-amd64.zip" "$PYTHON_URL"

# Extract Python
echo "Extracting Python..."
unzip -q "/tmp/python-$PYTHON_VERSION-embed-amd64.zip" -d "$PYTHON_DIR"
rm "/tmp/python-$PYTHON_VERSION-embed-amd64.zip"

# Enable site-packages
echo "Enabling site-packages..."
sed -i 's/#import site/import site/' "$PYTHON_DIR/python312._pth"

# Download and install pip
echo "Installing pip..."
curl -L -o "/tmp/get-pip.py" "$GET_PIP_URL"
"$PYTHON_DIR/python.exe" "/tmp/get-pip.py"
rm "/tmp/get-pip.py"

# Install requirements
echo "Installing Python dependencies..."
"$PYTHON_DIR/python.exe" -m pip install -r "$RESOURCES_DIR/requirements.txt"

echo "Python setup complete!"
echo "Testing installation..."

# Test imports
"$PYTHON_DIR/python.exe" -c "import pandas, numpy, yfinance; print('Pandas:', pandas.__version__, '| NumPy:', numpy.__version__, '| YFinance:', yfinance.__version__)"

echo "All done!"
