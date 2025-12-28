# Satellite & Geospatial Data Sources

> Earth observation, satellite tracking, and geospatial intelligence data

## Overview

Access satellite imagery, tracking data, and geospatial intelligence from NASA, ESA, and specialized providers. These sources enable environmental monitoring, maritime tracking, climate analysis, and satellite position tracking.

## Satellite & Geospatial Providers

| Provider | File Name | API Key | Coverage |
|----------|-----------|---------|----------|
| 🛰️ **N2YO** | `n2yo_satellite_data.py` | 🔑 Required | Real-time satellite tracking - 20K+ satellites |
| 🌍 **NASA GIBS** | `nasa_gibs_api.py` | ❌ No | Global Imagery Browse Services - Earth imagery |
| 🛰️ **Sentinel Hub** | `sentinelhub_data.py` | 🔑 Required | Sentinel satellite imagery (ESA) |
| 🌊 **OSCAR** | `oscar_data.py` | ❌ No | Ocean Surface Current Analysis Real-time |

## Data Categories

| Category | Sources | Applications |
|----------|---------|--------------|
| **Satellite Tracking** | N2YO | ISS tracking, satellite positions, TLE data |
| **Earth Imagery** | NASA GIBS, Sentinel Hub | Land use, vegetation, climate monitoring |
| **Oceanographic** | OSCAR | Ocean currents, maritime navigation, climate |
| **Environmental** | All sources | Climate research, disaster monitoring |

## API Key Setup

```bash
# N2YO satellite tracking (required)
export N2YO_API_KEY="your_key_here"  # Get from: https://www.n2yo.com/api/

# Sentinel Hub (required)
export SENTINELHUB_CLIENT_ID="your_client_id"
export SENTINELHUB_CLIENT_SECRET="your_client_secret"
# Get from: https://apps.sentinel-hub.com/dashboard/
```

## Usage Examples

```python
# N2YO - Track satellites
from n2yo_satellite_data import get_satellite_positions
iss = get_satellite_positions(satellite_id=25544)  # ISS

# NASA GIBS - Earth imagery
from nasa_gibs_api import get_imagery
imagery = get_imagery(
    product='MODIS_Terra_CorrectedReflectance_TrueColor',
    date='2024-01-15',
    bbox=[-180, -90, 180, 90]
)

# Sentinel Hub - Satellite imagery
from sentinelhub_data import get_sentinel_image
image = get_sentinel_image(
    bbox=[13.0, 45.0, 13.5, 45.5],  # Venice, Italy
    time_range=('2024-01-01', '2024-01-31'),
    layer='TRUE-COLOR'
)

# OSCAR - Ocean currents
from oscar_data import get_ocean_currents
currents = get_ocean_currents(
    lat_min=20, lat_max=50,
    lon_min=-80, lon_max=-40,
    date='2024-01-15'
)
```

## Key Features by Source

### N2YO (Satellite Tracking)
- **Real-time satellite positions**
- 20,000+ tracked objects
- ISS tracking
- Satellite passes predictions
- Two-Line Element (TLE) data
- Radio amateur satellites
- Visual passes calculation
- Satellite categories (weather, communications, military)

### NASA GIBS (Global Imagery)
- **Daily global imagery**
- 1000+ imagery products
- Multiple satellites (MODIS, VIIRS, Landsat)
- Near real-time (3-hour delay)
- Historical archive (2000+)
- True color imagery
- Scientific products (NDVI, temperature, snow cover)
- Free, unlimited access

### Sentinel Hub (ESA)
- **Sentinel satellite constellation**
- Sentinel-1 (radar)
- Sentinel-2 (optical - 10m resolution)
- Sentinel-3 (ocean/land monitoring)
- Sentinel-5P (atmospheric monitoring)
- Custom processing
- Cloud detection
- Time-series analysis

### OSCAR (Ocean Currents)
- **Near-surface ocean currents**
- 1/3 degree resolution
- 5-day temporal resolution
- Global coverage (60°S to 60°N)
- Velocity components (u, v)
- Historical data (1993+)
- Climate research quality
- Free NetCDF data

## Satellite Products

### NASA GIBS Products

| Product | Description | Resolution |
|---------|-------------|------------|
| **MODIS_Terra_TrueColor** | True color composite | 250m |
| **VIIRS_SNPP_CorrectedReflectance** | VIIRS true color | 375m |
| **MODIS_Aqua_Chlorophyll_A** | Ocean chlorophyll | 4km |
| **ASTER_GDEM_Color** | Digital elevation model | 30m |

### Sentinel Bands

| Sentinel-2 Band | Description | Resolution |
|-----------------|-------------|------------|
| **B02** | Blue | 10m |
| **B03** | Green | 10m |
| **B04** | Red | 10m |
| **B08** | NIR | 10m |
| **B11** | SWIR | 20m |

## Use Cases

### Climate Monitoring
- **Track vegetation changes** (NDVI from NASA GIBS)
- **Ice sheet monitoring** (Sentinel-1 radar)
- **Ocean temperature** (MODIS sea surface temp)
- **Deforestation tracking** (Sentinel-2 time series)

### Maritime Intelligence
- **Ocean current forecasting** (OSCAR)
- **Ship routing optimization** (combine currents + satellite tracking)
- **Fishing grounds identification** (chlorophyll maps)

### Satellite Operations
- **ISS visibility** (N2YO passes)
- **Communication satellite tracking** (geostationary satellites)
- **Collision avoidance** (TLE data)

### Disaster Response
- **Flood mapping** (Sentinel-1 before/after)
- **Fire monitoring** (MODIS thermal anomalies)
- **Hurricane tracking** (visible imagery)

## Data Quality & Coverage

| Source | Update Frequency | Spatial Resolution | Temporal Coverage |
|--------|------------------|-------------------|-------------------|
| N2YO | Real-time (seconds) | N/A (position data) | Current |
| NASA GIBS | 3 hours - daily | 250m - 5km | 2000 - present |
| Sentinel Hub | 5 days | 10m - 60m | 2015 - present |
| OSCAR | 5 days | 1/3 degree (~37km) | 1993 - present |

## Technical Details

- **Protocol**: REST API (all)
- **Format**: JSON (metadata), GeoTIFF/PNG (imagery), NetCDF (OSCAR)
- **Authentication**: API keys (N2YO, Sentinel Hub)
- **Rate Limits**: Varies by provider
- **Data Size**: Imagery can be large (MB to GB per request)

## Advanced Features

### N2YO Categories
```python
# Get satellites by category
from n2yo_satellite_data import get_category_satellites
weather_sats = get_category_satellites(category='weather')
```

### Sentinel Processing
```python
# Custom band combinations
from sentinelhub_data import custom_script
ndvi = custom_script(bands=['B08', 'B04'], formula='(B08-B04)/(B08+B04)')
```

### OSCAR Time Series
```python
# Get current trends
from oscar_data import get_current_trends
trends = get_current_trends(region='gulf_stream', years=10)
```

---

**Total Sources**: 4 providers | **Free Access**: 2 sources | **Coverage**: Global | **Applications**: Climate, Maritime, Disaster Response | **Last Updated**: 2025-12-28
