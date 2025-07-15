# Use the official Python image with Python 3.11
FROM python:3.11-slim

# Set GDAL version (used by osmnx, shapely, etc.)
ENV GDAL_VERSION=3.6.3

# Set environment variables
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV PYTHONPATH=/app/finceptTerminal

# Set working directory
WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    gdal-bin \
    libgdal-dev \
    libpq-dev \
    libmariadb-dev \
    python3-dev \
    build-essential \
    pkg-config \
    qt5-default \
    libqt5webkit5-dev \
    libqt5webengine5 \
    libqt5webenginewidgets5 \
    curl \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install PDM for pyproject.toml-based builds
RUN pip install --no-cache-dir pdm

# Copy pyproject and lock file (if exists)
COPY pyproject.toml ./
COPY pdm.lock ./

# Install dependencies with PDM (without dev-deps)
RUN pdm install --no-editable --prod

# Copy the entire project
COPY . .

# Default command to run the app
CMD ["pdm", "run", "python", "-m", "fincept_terminal.FinceptTerminalStart"]
