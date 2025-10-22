# Contributing to Fincept Terminals

Welcome to the Fincept Terminal community! Your contributions help build the future of open-source financial technology.

## Project Architecture Overview

Fincept Terminal uses a **Hybrid MVP (Model-View-Presenter) + Services** architecture designed for massive scalability and modular development. This architecture allows thousands of developers to contribute independently without breaking existing functionality.

### Why This Architecture?

- **Modularity**: Add new features without affecting existing code
- **Team Collaboration**: Different teams can work on different layers simultaneously  
- **Easy Testing**: Each layer can be tested in isolation
- **Beginner Friendly**: Clear separation makes it easy to understand and contribute
- **Bloomberg Scale**: Designed to handle thousands of data sources and features

## Directory Structure

```
/fincept_terminal
├── /models               # Data structures and entities
│   ├── /analytics
│   │   └── analytics_models.py
│   ├── /dashboard
│   │   └── dashboard_models.py
│   ├── /portfolio
│   │   └── portfolio_models.py
│   ├── /watchlist
│   │   └── watchlist_models.py
│   └── /shared          # Models used by 2+ modules
│       └── common_models.py
├── /services            # Data fetching and processing
│   ├── /analytics
│   │   └── analytics_service.py
│   ├── /dashboard
│   │   └── dashboard_service.py
│   ├── /portfolio
│   │   └── portfolio_service.py
│   ├── /watchlist
│   │   └── watchlist_service.py
│   └── /shared          # Services used by 2+ modules
│       └── api_service.py
├── /presenters          # Business logic and coordination
│   ├── /analytics
│   │   └── analytics_presenter.py
│   ├── /dashboard
│   │   └── dashboard_presenter.py
│   ├── /portfolio
│   │   └── portfolio_presenter.py
│   ├── /watchlist
│   │   └── watchlist_presenter.py
│   └── /shared          # Inter-module communication
│       └── event_bus.py
├── /views               # User interface components
│   ├── /analytics
│   │   └── analytics_view.py
│   ├── /dashboard
│   │   └── dashboard_view.py
│   ├── /portfolio
│   │   └── portfolio_view.py
│   ├── /watchlist
│   │   └── watchlist_view.py
│   └── /shared          # Reusable UI components
│       ├── chart_components.py
│       └── common_widgets.py
├── /utils               # Global utilities (logging, validation, etc.)
│   ├── logging.py
│   ├── validators.py
│   └── formatters.py
└── /core                # Framework core and base classes
    ├── base_tab.py
    ├── base_service.py
    └── app_config.py
```

## Architecture Layers

### 1. Models Layer (`/models`)
**Purpose**: Define data structures, entities, and business objects

**What goes here**:
- Data classes and dataclasses
- Entity definitions (Stock, Portfolio, News, etc.)
- Configuration objects
- Enums and constants

**Example**:
```python
# models/dashboard/stock_data.py
@dataclass
class StockData:
    symbol: str
    price: float
    change_pct: float
    volume: int
```

**Contribution Guidelines**:
- Use dataclasses for simple data containers
- Include validation methods
- Add type hints to all fields
- Document all fields with docstrings

### 2. Services Layer (`/services`)
**Purpose**: Handle data fetching, API calls, and data processing

**What goes here**:
- External API integrations
- Data fetching logic
- Data transformation and processing
- Caching mechanisms
- Error handling for data operations

**Example**:
```python
# services/dashboard/market_data_service.py
class MarketDataService:
    def fetch_stock_data(self, symbols: List[str]) -> Dict[str, StockData]:
        # API calls and data processing
```

**Contribution Guidelines**:
- Handle all errors gracefully with fallback data
- Use concurrent programming for multiple API calls
- Include comprehensive logging
- Write data validation logic
- Create mock/fallback data for testing

### 3. Presenters Layer (`/presenters`)
**Purpose**: Business logic, state management, and coordination between views and services

**What goes here**:
- Business rules and logic
- State management
- Event handling
- Data transformation for UI
- Inter-module communication

**Example**:
```python
# presenters/dashboard/dashboard_presenter.py
class DashboardPresenter:
    def __init__(self, view, service):
        self.view = view
        self.service = service
    
    def refresh_data(self):
        data = self.service.fetch_all_data()
        self.view.update_display(data)
```

**Contribution Guidelines**:
- Keep business logic here, not in views
- Manage application state
- Handle user interactions
- Coordinate between multiple services if needed

### 4. Views Layer (`/views`)
**Purpose**: User interface and presentation logic only

**What goes here**:
- DearPyGui UI components
- Chart creation and rendering
- Event callbacks (but not logic)
- UI updates and animations

**Example**:
```python
# views/dashboard/dashboard_view.py
class DashboardView(BaseTab):
    def create_content(self):
        # DearPyGui UI creation only
        dpg.add_table(...)
    
    def update_stock_table(self, stocks):
        # Update UI with new data
```

**Contribution Guidelines**:
- Only DearPyGui code goes here
- No business logic in views
- Event handlers should call presenter methods
- Focus on user experience and visual design

## How to Contribute

### Adding a New Feature

1. **Choose Your Layer**: Decide which layer your contribution affects
2. **Create Module Folders**: Add folders in relevant layers
3. **Follow Naming Conventions**: Use clear, descriptive names

**Example: Adding a "News Analysis" feature**
```
/app
├── /models/news_analysis/
│   ├── news_models.py
│   └── sentiment_models.py
├── /services/news_analysis/  
│   ├── news_service.py
│   └── sentiment_service.py
├── /presenters/news_analysis/
│   └── news_presenter.py
└── /views/news_analysis/
    └── news_view.py
```

### Partial Contributions Welcome

You don't need to implement all layers! Contributions can be:

**UI Only**: Work in `/views` folder
- Design new charts or interfaces
- Improve existing UI components
- Add new visualization types

**Data Only**: Work in `/services` folder  
- Add new data sources (crypto exchanges, news APIs)
- Improve data processing algorithms
- Add new market indicators

**Business Logic Only**: Work in `/presenters` folder
- Add new analysis algorithms
- Implement trading strategies  
- Create data aggregation logic

**Models Only**: Work in `/models` folder
- Define new data structures
- Add validation logic
- Create configuration objects

### Inter-Module Communication

When features need to communicate:

1. **Add to `/shared` folders**: Common code goes in shared directories
2. **Use Presenter Layer**: Inter-module communication happens in presenters
3. **Avoid Direct Dependencies**: Modules should not directly import each other

### File Naming Conventions

- **Models**: `*_models.py` (e.g., `stock_models.py`)
- **Services**: `*_service.py` (e.g., `market_data_service.py`)  
- **Presenters**: `*_presenter.py` (e.g., `dashboard_presenter.py`)
- **Views**: `*_view.py` (e.g., `analytics_view.py`)

### Code Quality Standards

1. **Type Hints**: Use type hints for all function parameters and returns
2. **Docstrings**: Document all classes and public methods
3. **Error Handling**: Include comprehensive error handling
4. **Logging**: Use the shared logging system for debugging
5. **Testing**: Write unit tests for your contributions

### Getting Started

1. **Fork the Repository**
2. **Choose a Feature**: Pick something you want to work on
3. **Create Module Structure**: Add necessary folders
4. **Start Small**: Begin with models or a simple service
5. **Submit PR**: Submit your contribution for review

### Examples for New Contributors

**Easy Contributions**:
- Add a new stock exchange data source
- Create a new chart type for existing data
- Add a new economic indicator
- Improve error messages

**Medium Contributions**:
- Add crypto portfolio tracking
- Implement technical analysis indicators
- Create news sentiment analysis
- Add export/import functionality

**Advanced Contributions**:
- Add real-time streaming data
- Implement machine learning features
- Create plugin system for external modules
- Add advanced backtesting capabilities

## Questions?

- Read existing code in `/app` to understand patterns
- Check `/utils` for shared utilities you can use
- Look at `/core` for base classes to inherit from
- Submit an issue if you need clarification
