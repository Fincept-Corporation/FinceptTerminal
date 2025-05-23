# Use the official Python image with Python 3.11
FROM python:3.11-slim

# set gdal version as some packages requires gdal.
ENV GDAL_VERSION=3.6.3

# Set environment variables
ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV PYTHONPATH=/app/finceptTerminal

# Set the working directory
WORKDIR /app/finceptTerminal

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

# Set the working directory
WORKDIR /app

# Install system dependencies for psycopg2, mysqlclient, and other build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    gdal-bin \
    libgdal-dev \
    libpq-dev \
    libmariadb-dev \
    python3-dev \
    build-essential \
    pkg-config \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Copy the requirements file
COPY requirements.txt .


# Remove 'pywin32' from requirements.txt and install dependencies
RUN grep -v 'pywin32' requirements.txt > filtered-requirements.txt && \
    pip install --no-cache-dir -r filtered-requirements.txt

RUN pip install --no-cache-dir -r requirements.txt


# Copy the rest of the application code
COPY . .

# Specify the command to run your application

CMD ["python", "fincept_terminal/cli.py"]

CMD ["python", "-m", "fincept_terminal.FinceptTerminalStart"]

