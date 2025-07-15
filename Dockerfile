# Use the official Python image with Python 3.11
FROM python:3.11-slim

ENV GDAL_VERSION=3.6.3
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV PYTHONPATH=/app/finceptTerminal

WORKDIR /app

# Install system dependencies (REMOVED qt5-default)
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    gdal-bin \
    libgdal-dev \
    libpq-dev \
    libmariadb-dev \
    python3-dev \
    build-essential \
    pkg-config \
    libqt5webkit5-dev \
    libqt5webengine5 \
    libqt5webenginewidgets5 \
    curl \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Install PDM for pyproject.toml-based dependency management
RUN pip install --no-cache-dir pdm

# Copy pyproject only
COPY pyproject.toml ./

# Install dependencies via PDM
RUN pdm install --no-editable --prod

# Copy the full project
COPY . .

# Run the app
CMD ["pdm", "run", "python", "-m", "fincept_terminal.FinceptTerminalStart"]
