# Regional Data Sources

> Country and region-specific economic statistics and development data

## Overview

Access official statistics and data from specific countries and regional development organizations. These sources provide localized economic indicators, demographic data, and development statistics not available in global databases.

## Country-Specific Statistics

| Country/Region | File Name | API Key | Agency |
|----------------|-----------|---------|--------|
| 🇯🇵 **Japan** | `estat_japan_api.py` | 🔑 Required | e-Stat - Statistics Bureau of Japan |
| 🇸🇪 **Sweden** | `scb_data.py` | ❌ No | Statistics Sweden (SCB) |
| 🇪🇸 **Spain** | `spain_data.py` | ❌ No | National Statistics Institute (INE) |

## Regional Development Organizations

| Organization | File Name | API Key | Coverage |
|--------------|-----------|---------|----------|
| 🌍 **Open Africa** | `openafrica_api.py` | ❌ No | African open data portal - 54 countries |
| 🏦 **Asian Development Bank** | `adb_data.py` | ❌ No | Asian economic development data |

## Data Categories

| Category | Sources | Examples |
|----------|---------|----------|
| **Demographics** | All sources | Population, age distribution, migration |
| **Economic** | Japan, Sweden, Spain, ADB | GDP, inflation, employment, wages |
| **Social** | All sources | Education, health, welfare |
| **Development** | Open Africa, ADB | Infrastructure, poverty, aid |
| **Trade** | Japan, ADB | Import/export, trade balance |

## API Key Setup

```bash
# Japan e-Stat (required)
export ESTAT_JAPAN_API_KEY="your_key_here"  # Get from: https://www.e-stat.go.jp/api/
```

## Usage Examples

```python
# Japan statistics
from estat_japan_api import get_statistics
population = get_statistics(stats_code='00200521', category='population')

# Sweden statistics
from scb_data import get_data
gdp = get_data(table='BE0101', variables={'Region': 'SE', 'Year': '2020'})

# Spain statistics
from spain_data import get_indicator
unemployment = get_indicator('unemployment_rate', start_date='2020-01-01')

# Open Africa data
from openafrica_api import search_datasets
kenya_data = search_datasets(query='Kenya GDP', limit=10)

# Asian Development Bank
from adb_data import get_key_indicators
asia_gdp = get_key_indicators(country='CHN', indicator='GDP_GROWTH')
```

## Key Features by Source

### e-Stat Japan
- **Official government statistics portal**
- 800+ statistical surveys
- Census data (population, housing)
- Economic indicators (GDP, trade)
- Social statistics (labor, education)
- Regional breakdowns (prefectures, cities)
- API access with registration

### Statistics Sweden (SCB)
- **Nordic statistical authority**
- Population and demographics
- Labor market statistics
- National accounts
- Price indices
- Trade statistics
- Open API (no key needed)

### Spain INE
- **National Statistics Institute**
- Population census
- Economic indicators
- Consumer Price Index (CPI)
- Labor force survey
- Regional statistics (autonomous communities)

### Open Africa
- **Largest open data portal for Africa**
- 54 African countries
- Government datasets
- NGO and UN data
- Development indicators
- CKAN-based platform

### Asian Development Bank (ADB)
- **Development finance institution**
- 46 member countries (Asia-Pacific)
- Key economic indicators
- Development effectiveness
- Project data
- Poverty statistics

## Data Quality & Coverage

| Source | Countries | Update Frequency | Historical Depth |
|--------|-----------|------------------|------------------|
| e-Stat Japan | Japan | Monthly/Quarterly | Decades |
| SCB Sweden | Sweden | Monthly | 50+ years |
| Spain INE | Spain | Monthly/Quarterly | 20+ years |
| Open Africa | 54 African | Varies | Varies |
| ADB | 46 Asia-Pacific | Annual/Quarterly | 20+ years |

## Regional Focus Areas

### Japan (e-Stat)
- Population aging and demographics
- Industrial production
- International trade
- Regional economic disparities
- Technology and innovation

### Sweden (SCB)
- Welfare statistics
- Environmental data
- Gender equality metrics
- Innovation indicators
- Quality of life measures

### Spain (INE)
- Tourism statistics
- Real estate market
- Regional GDP
- Migration data
- Agricultural production

### Africa (Open Africa)
- Development goals (SDGs)
- Agricultural data
- Health indicators
- Infrastructure
- Governance

### Asia (ADB)
- Economic growth
- Poverty reduction
- Infrastructure development
- Financial sector
- Climate and environment

## Technical Details

- **Protocol**: REST API (all sources)
- **Format**: JSON, CSV, XML
- **Authentication**: API key (Japan only), others open
- **Rate Limits**: Generous (Japan: 1000/day, others unlimited)
- **Language**: English + native languages

---

**Total Sources**: 5 (3 countries + 2 regional orgs) | **Free Access**: 4 sources | **Coverage**: 100+ countries | **Last Updated**: 2025-12-28
