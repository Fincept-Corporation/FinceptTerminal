#!/bin/bash

# AI Prediction Dependencies Installer for FinceptTerminal
# This script installs all required Python libraries for the prediction module

set -e

echo "=========================================="
echo "AI Prediction Module - Dependency Installer"
echo "=========================================="
echo ""

# Check Python version
echo "Checking Python version..."
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo "✓ Python version: $PYTHON_VERSION"
echo ""

# Check pip
echo "Checking pip..."
pip3 --version > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ pip is installed"
else
    echo "✗ pip not found. Installing pip..."
    sudo apt-get install -y python3-pip
fi
echo ""

# Upgrade pip
echo "Upgrading pip..."
pip3 install --upgrade pip
echo ""

# Install dependencies
echo "Installing dependencies..."
echo "This may take 5-10 minutes..."
echo ""

# Core dependencies
echo "[1/9] Installing numpy and pandas..."
pip3 install numpy>=1.24.0 pandas>=2.0.0

echo "[2/9] Installing scipy..."
pip3 install scipy>=1.10.0

echo "[3/9] Installing statsmodels (ARIMA/SARIMA)..."
pip3 install statsmodels>=0.14.0

echo "[4/9] Installing prophet (Facebook Prophet)..."
pip3 install prophet>=1.1.4

echo "[5/9] Installing scikit-learn (ML utilities)..."
pip3 install scikit-learn>=1.3.0

echo "[6/9] Installing xgboost (Gradient Boosting)..."
pip3 install xgboost>=2.0.0

echo "[7/9] Installing arch (GARCH models)..."
pip3 install arch>=6.2.0

echo "[8/9] Installing yfinance (Market data)..."
pip3 install yfinance>=0.2.28

echo "[9/9] Installing TensorFlow (LSTM - Optional, large download)..."
read -p "Install TensorFlow? This is ~500MB (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    pip3 install tensorflow>=2.13.0 keras>=2.13.0
    echo "✓ TensorFlow installed"
else
    echo "⊘ TensorFlow skipped (LSTM predictions will not be available)"
fi

echo ""
echo "=========================================="
echo "Installation Complete!"
echo "=========================================="
echo ""

# Verify installations
echo "Verifying installations..."
python3 << 'EOF'
import sys

packages = {
    'numpy': 'NumPy',
    'pandas': 'Pandas',
    'scipy': 'SciPy',
    'statsmodels': 'Statsmodels',
    'prophet': 'Prophet',
    'sklearn': 'Scikit-learn',
    'xgboost': 'XGBoost',
    'arch': 'ARCH',
    'yfinance': 'yfinance'
}

optional = {
    'tensorflow': 'TensorFlow',
    'keras': 'Keras'
}

print("\nCore Libraries:")
all_ok = True
for module, name in packages.items():
    try:
        __import__(module)
        print(f"  ✓ {name}")
    except ImportError:
        print(f"  ✗ {name} - FAILED")
        all_ok = False

print("\nOptional Libraries:")
for module, name in optional.items():
    try:
        __import__(module)
        print(f"  ✓ {name}")
    except ImportError:
        print(f"  ⊘ {name} - Not installed")

if all_ok:
    print("\n✓ All core dependencies installed successfully!")
else:
    print("\n✗ Some dependencies failed to install. Please check errors above.")
    sys.exit(1)
EOF

echo ""
echo "=========================================="
echo "Next Steps:"
echo "=========================================="
echo "1. Run test suite: python3 test_predictions.py"
echo "2. Use the prediction module in FinceptTerminal"
echo ""
echo "For help, see README.md"
echo ""
