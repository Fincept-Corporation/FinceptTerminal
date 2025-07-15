import requests
import json
import pandas as pd
import os
import time
from datetime import datetime
import logging
from typing import Dict, List, Optional
import sqlite3
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import hashlib
import re


class ProductionDBNomicsScraper:
    def __init__(self, base_url="https://api.db.nomics.world/v22",
                 max_workers=10, delay=0.05, output_dir="db_nomics_production"):
        """
        Production-ready DB Nomics scraper with separate tables per series

        Args:
            base_url: Base URL for DB Nomics API
            max_workers: Maximum number of concurrent threads
            delay: Delay between requests (seconds)
            output_dir: Directory to save all data
        """
        self.base_url = base_url
        self.max_workers = max_workers
        self.delay = delay
        self.output_dir = output_dir

        # Create output directory
        os.makedirs(output_dir, exist_ok=True)

        # Setup comprehensive logging
        self.setup_logging()

        # Session for requests
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'DBNomics-Production-Scraper/2.0'
        })

        # Thread lock for database operations
        self.lock = threading.Lock()

        # Initialize SQLite database
        self.init_database()

        # Statistics tracking
        self.stats = {
            'providers': 0,
            'datasets': 0,
            'series': 0,
            'observations': 0,
            'tables_created': 0,
            'errors': 0,
            'start_time': datetime.now(),
            'data_freshness_checks': 0
        }

        # Track created tables to avoid recreation
        self.created_tables = set()

    def setup_logging(self):
        """Setup comprehensive logging with proper encoding"""
        log_file = f'{self.output_dir}/production_scraper.log'

        # Create formatter
        formatter = logging.Formatter(
            '%(asctime)s - %(levelname)s - %(threadName)s - %(message)s'
        )

        # File handler with UTF-8 encoding
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setFormatter(formatter)
        file_handler.setLevel(logging.INFO)

        # Console handler with UTF-8 encoding
        console_handler = logging.StreamHandler()
        console_handler.setFormatter(formatter)
        console_handler.setLevel(logging.INFO)

        # Setup logger
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)
        self.logger.addHandler(file_handler)
        self.logger.addHandler(console_handler)

    def init_database(self):
        """Initialize SQLite database with production schema"""
        self.db_path = f"{self.output_dir}/db_nomics_production.db"

        with sqlite3.connect(self.db_path) as conn:
            # Providers table
            conn.execute('''
                CREATE TABLE IF NOT EXISTS providers (
                    code TEXT PRIMARY KEY,
                    name TEXT,
                    region TEXT,
                    website TEXT,
                    terms_of_use TEXT,
                    indexed_at TEXT,
                    scraped_at TEXT,
                    status TEXT DEFAULT 'active'
                )
            ''')

            # Datasets table
            conn.execute('''
                CREATE TABLE IF NOT EXISTS datasets (
                    provider_code TEXT,
                    dataset_code TEXT,
                    name TEXT,
                    description TEXT,
                    nb_series INTEGER,
                    indexed_at TEXT,
                    scraped_at TEXT,
                    status TEXT DEFAULT 'active',
                    PRIMARY KEY (provider_code, dataset_code)
                )
            ''')

            # Series metadata table (master catalog)
            conn.execute('''
                CREATE TABLE IF NOT EXISTS series_metadata (
                    series_id TEXT PRIMARY KEY,
                    provider_code TEXT,
                    dataset_code TEXT,
                    series_code TEXT,
                    series_name TEXT,
                    frequency TEXT,
                    table_name TEXT UNIQUE,
                    first_period TEXT,
                    last_period TEXT,
                    record_count INTEGER DEFAULT 0,
                    data_hash TEXT,
                    raw_json_metadata TEXT,
                    indexed_at TEXT,
                    first_scraped_at TEXT,
                    last_scraped_at TEXT,
                    status TEXT DEFAULT 'active',
                    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
                )
            ''')

            # Data freshness tracking
            conn.execute('''
                CREATE TABLE IF NOT EXISTS data_freshness (
                    series_id TEXT,
                    check_timestamp TEXT,
                    data_available BOOLEAN,
                    response_time_ms INTEGER,
                    record_count INTEGER,
                    last_period TEXT,
                    error_message TEXT,
                    PRIMARY KEY (series_id, check_timestamp)
                )
            ''')

            # Dimensions tracking (for series metadata)
            conn.execute('''
                CREATE TABLE IF NOT EXISTS series_dimensions (
                    series_id TEXT,
                    dimension_key TEXT,
                    dimension_value TEXT,
                    scraped_at TEXT,
                    PRIMARY KEY (series_id, dimension_key)
                )
            ''')

            # Processing log for tracking operations
            conn.execute('''
                CREATE TABLE IF NOT EXISTS processing_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    operation_type TEXT,
                    series_id TEXT,
                    table_name TEXT,
                    records_processed INTEGER,
                    processing_time_ms INTEGER,
                    status TEXT,
                    error_message TEXT,
                    timestamp TEXT DEFAULT CURRENT_TIMESTAMP
                )
            ''')

            # Create indexes for performance
            conn.execute('CREATE INDEX IF NOT EXISTS idx_series_provider ON series_metadata(provider_code)')
            conn.execute('CREATE INDEX IF NOT EXISTS idx_series_dataset ON series_metadata(dataset_code)')
            conn.execute('CREATE INDEX IF NOT EXISTS idx_freshness_series ON data_freshness(series_id)')

            conn.commit()

        self.logger.info("Production database initialized successfully")

    def generate_table_name(self, provider_code: str, dataset_code: str, series_code: str) -> str:
        """Generate clean table name for series data"""

        # Clean identifiers by removing special characters
        def clean_identifier(text: str) -> str:
            # Replace dots, dashes, and other special chars with underscores
            cleaned = re.sub(r'[^a-zA-Z0-9_]', '_', text.lower())
            # Remove multiple consecutive underscores
            cleaned = re.sub(r'_+', '_', cleaned)
            # Remove leading/trailing underscores
            cleaned = cleaned.strip('_')
            return cleaned

        clean_provider = clean_identifier(provider_code)
        clean_dataset = clean_identifier(dataset_code)
        clean_series = clean_identifier(series_code)

        # Create base table name
        base_name = f"data_{clean_provider}_{clean_dataset}_{clean_series}"

        # Handle long names by truncating and adding hash
        if len(base_name) > 55:  # Leave room for hash
            name_hash = hashlib.md5(base_name.encode()).hexdigest()[:8]
            truncated = base_name[:45]
            table_name = f"{truncated}_{name_hash}"
        else:
            table_name = base_name

        return table_name

    def create_series_table(self, table_name: str) -> bool:
        """Create individual table for series data with optimized structure"""
        if table_name in self.created_tables:
            return True

        try:
            with sqlite3.connect(self.db_path) as conn:
                # Create the data table
                conn.execute(f'''
                    CREATE TABLE IF NOT EXISTS {table_name} (
                        period TEXT PRIMARY KEY,
                        period_start_date TEXT,
                        period_end_date TEXT,
                        value REAL,
                        value_original TEXT,
                        currency TEXT,
                        unit TEXT,
                        status TEXT,
                        revision_number INTEGER DEFAULT 1,
                        first_fetched_at TEXT,
                        last_updated_at TEXT DEFAULT CURRENT_TIMESTAMP,
                        data_quality_score REAL DEFAULT 1.0
                    )
                ''')

                # Create indexes for fast queries
                conn.execute(f'''
                    CREATE INDEX IF NOT EXISTS idx_{table_name}_period 
                    ON {table_name}(period)
                ''')

                conn.execute(f'''
                    CREATE INDEX IF NOT EXISTS idx_{table_name}_start_date 
                    ON {table_name}(period_start_date)
                ''')

                conn.execute(f'''
                    CREATE INDEX IF NOT EXISTS idx_{table_name}_value 
                    ON {table_name}(value) WHERE value IS NOT NULL
                ''')

                conn.commit()

            self.created_tables.add(table_name)
            self.stats['tables_created'] += 1
            self.logger.debug(f"Created table: {table_name}")
            return True

        except Exception as e:
            self.logger.error(f"Failed to create table {table_name}: {e}")
            return False

    def make_request(self, endpoint: str, params: Dict = None) -> Optional[Dict]:
        """Make API request with enhanced error handling and metrics"""
        url = f"{self.base_url}{endpoint}"
        start_time = time.time()

        try:
            time.sleep(self.delay)  # Rate limiting
            response = self.session.get(url, params=params, timeout=30)
            response.raise_for_status()

            response_time = int((time.time() - start_time) * 1000)
            self.logger.debug(f"Request to {endpoint} completed in {response_time}ms")

            return response.json()

        except requests.exceptions.RequestException as e:
            response_time = int((time.time() - start_time) * 1000)
            self.logger.error(f"Request failed for {url} after {response_time}ms: {e}")
            self.stats['errors'] += 1
            return None

    def log_data_freshness(self, series_id: str, success: bool, response_time: int,
                           record_count: int = 0, last_period: str = None, error: str = None):
        """Log data freshness check results"""
        try:
            with sqlite3.connect(self.db_path) as conn:
                conn.execute('''
                    INSERT INTO data_freshness 
                    (series_id, check_timestamp, data_available, response_time_ms, 
                     record_count, last_period, error_message)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    series_id, datetime.now().isoformat(), success, response_time,
                    record_count, last_period, error
                ))
                conn.commit()

            self.stats['data_freshness_checks'] += 1

        except Exception as e:
            self.logger.error(f"Failed to log freshness for {series_id}: {e}")

    def log_processing_operation(self, operation_type: str, series_id: str, table_name: str,
                                 records_processed: int, processing_time: int,
                                 status: str, error_message: str = None):
        """Log processing operations for audit trail"""
        try:
            with sqlite3.connect(self.db_path) as conn:
                conn.execute('''
                    INSERT INTO processing_log 
                    (operation_type, series_id, table_name, records_processed, 
                     processing_time_ms, status, error_message)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    operation_type, series_id, table_name, records_processed,
                    processing_time, status, error_message
                ))
                conn.commit()

        except Exception as e:
            self.logger.error(f"Failed to log operation: {e}")

    def get_series_data(self, provider_code: str, dataset_code: str, series_code: str) -> Optional[Dict]:
        """Get data for a specific series - OPTIMIZED VERSION"""
        series_id = f"{provider_code}/{dataset_code}/{series_code}"
        start_time = time.time()

        # SIMPLIFIED LOGGING - less verbose
        data = self.make_request(
            f"/series/{provider_code}/{dataset_code}/{series_code}",
            params={'observations': 1}
        )

        response_time = int((time.time() - start_time) * 1000)

        if data and 'series' in data and data['series']['docs']:
            series_info = data['series']['docs'][0]

            # Quick check for data availability
            has_period_data = 'period' in series_info and len(series_info.get('period', [])) > 0
            has_obs_data = 'observations' in series_info and len(series_info.get('observations', [])) > 0

            data_count = 0
            if has_period_data:
                data_count = len(series_info.get('period', []))
            elif has_obs_data:
                data_count = len(series_info.get('observations', []))

            self.log_data_freshness(series_id, True, response_time, data_count, None)

            # Only log if there's actually data
            if data_count > 0:
                self.logger.debug(f"Found {data_count} data points for {series_id}")

            return data
        else:
            self.log_data_freshness(series_id, False, response_time, 0, None, "No data returned")
            return None

    def save_series_data_to_dedicated_table(self, provider_code: str, dataset_code: str,
                                            series_data: Dict) -> bool:
        """Save series data to its dedicated table"""
        if not series_data or 'series' not in series_data:
            return False

        start_time = time.time()

        try:
            for series in series_data['series']['docs']:
                series_code = series.get('series_code', '')
                series_id = f"{provider_code}/{dataset_code}/{series_code}"

                # Generate table name
                table_name = self.generate_table_name(provider_code, dataset_code, series_code)

                # Create table if needed
                if not self.create_series_table(table_name):
                    continue

                # Extract series metadata
                series_name = series.get('series_name', series_code)
                frequency = series.get('@frequency', 'unknown')
                dimensions = series.get('dimensions', {})

                # Process observations
                observations = series.get('observations', [])
                records_processed = 0
                first_period = None
                last_period = None

                with sqlite3.connect(self.db_path) as conn:
                    # Insert/update observations
                    for obs in observations:
                        period = obs.get('period')
                        value = obs.get('value')
                        period_start = obs.get('period_start_day', period)

                        if period and value is not None:
                            # Convert value to float if possible
                            numeric_value = None
                            try:
                                numeric_value = float(value)
                            except (ValueError, TypeError):
                                pass

                            # Check if record exists
                            existing = conn.execute(
                                f"SELECT period FROM {table_name} WHERE period = ?",
                                (period,)
                            ).fetchone()

                            if existing:
                                # Update existing record
                                conn.execute(f'''
                                    UPDATE {table_name} 
                                    SET value = ?, value_original = ?, period_start_date = ?,
                                        last_updated_at = ?, revision_number = revision_number + 1
                                    WHERE period = ?
                                ''', (numeric_value, str(value), period_start,
                                      datetime.now().isoformat(), period))
                            else:
                                # Insert new record
                                conn.execute(f'''
                                    INSERT INTO {table_name}
                                    (period, period_start_date, value, value_original, 
                                     first_fetched_at, last_updated_at)
                                    VALUES (?, ?, ?, ?, ?, ?)
                                ''', (period, period_start, numeric_value, str(value),
                                      datetime.now().isoformat(), datetime.now().isoformat()))

                            records_processed += 1

                            if first_period is None:
                                first_period = period
                            last_period = period

                    # Calculate data hash for change detection
                    obs_data = json.dumps([obs for obs in observations], sort_keys=True)
                    data_hash = hashlib.md5(obs_data.encode()).hexdigest()

                    # Update series metadata
                    conn.execute('''
                        INSERT OR REPLACE INTO series_metadata
                        (series_id, provider_code, dataset_code, series_code, series_name,
                         frequency, table_name, first_period, last_period, record_count,
                         data_hash, raw_json_metadata, indexed_at, last_scraped_at, updated_at)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    ''', (
                        series_id, provider_code, dataset_code, series_code, series_name,
                        frequency, table_name, first_period, last_period, records_processed,
                        data_hash, json.dumps(series), series.get('indexed_at', ''),
                        datetime.now().isoformat(), datetime.now().isoformat()
                    ))

                    # Save dimensions
                    for dim_key, dim_value in dimensions.items():
                        conn.execute('''
                            INSERT OR REPLACE INTO series_dimensions 
                            (series_id, dimension_key, dimension_value, scraped_at)
                            VALUES (?, ?, ?, ?)
                        ''', (series_id, dim_key, str(dim_value), datetime.now().isoformat()))

                    conn.commit()

                # Log the operation
                processing_time = int((time.time() - start_time) * 1000)
                self.log_processing_operation(
                    'series_data_save', series_id, table_name, records_processed,
                    processing_time, 'success'
                )

                self.stats['series'] += 1
                self.stats['observations'] += records_processed

                self.logger.info(f"Saved {records_processed} observations for {series_id} in table {table_name}")

            return True

        except Exception as e:
            processing_time = int((time.time() - start_time) * 1000)
            error_msg = str(e)

            self.log_processing_operation(
                'series_data_save', f"{provider_code}/{dataset_code}/ERROR",
                'N/A', 0, processing_time, 'error', error_msg
            )

            self.logger.error(f"❌ Error saving series data: {error_msg}")
            self.stats['errors'] += 1
            return False

    def get_providers(self) -> List[Dict]:
        """Get all providers from the API"""
        self.logger.info("Fetching all providers...")

        data = self.make_request("/providers")
        if not data or 'providers' not in data:
            self.logger.error("Failed to fetch providers")
            return []

        providers = data['providers']['docs']
        self.logger.info(f"Found {len(providers)} providers")
        return providers

    def get_datasets(self, provider_code: str) -> List[Dict]:
        """Get all datasets for a provider"""
        self.logger.info(f"Fetching datasets for provider: {provider_code}")

        data = self.make_request(f"/datasets/{provider_code}")
        if not data or 'datasets' not in data:
            self.logger.error(f"Failed to fetch datasets for {provider_code}")
            return []

        datasets = data['datasets']['docs']
        self.logger.info(f"Found {len(datasets)} datasets for {provider_code}")
        return datasets

    def get_series_list(self, provider_code: str, dataset_code: str) -> List[Dict]:
        """Get all series for a dataset"""
        self.logger.info(f"Fetching series for {provider_code}/{dataset_code}")

        data = self.make_request(f"/series/{provider_code}/{dataset_code}")
        if not data or 'series' not in data:
            self.logger.error(f"Failed to fetch series for {provider_code}/{dataset_code}")
            return []

        series = data['series']['docs']
        self.logger.info(f"Found {len(series)} series for {provider_code}/{dataset_code}")
        return series

    def save_providers(self, providers: List[Dict]):
        """Save providers to database"""
        with sqlite3.connect(self.db_path) as conn:
            for provider in providers:
                conn.execute('''
                    INSERT OR REPLACE INTO providers 
                    (code, name, region, website, terms_of_use, indexed_at, scraped_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    provider.get('code', ''),
                    provider.get('name', ''),
                    provider.get('region', ''),
                    provider.get('website', ''),
                    provider.get('terms_of_use', ''),
                    provider.get('indexed_at', ''),
                    datetime.now().isoformat()
                ))
            conn.commit()

        self.stats['providers'] += len(providers)
        self.logger.info(f"Saved {len(providers)} providers")

    def save_datasets(self, provider_code: str, datasets: List[Dict]):
        """Save datasets to database"""
        with sqlite3.connect(self.db_path) as conn:
            for dataset in datasets:
                conn.execute('''
                    INSERT OR REPLACE INTO datasets 
                    (provider_code, dataset_code, name, description, nb_series, indexed_at, scraped_at)
                    VALUES (?, ?, ?, ?, ?, ?, ?)
                ''', (
                    provider_code,
                    dataset.get('code', ''),
                    dataset.get('name', ''),
                    dataset.get('description', ''),
                    dataset.get('nb_series', 0),
                    dataset.get('indexed_at', ''),
                    datetime.now().isoformat()
                ))
            conn.commit()

        self.stats['datasets'] += len(datasets)
        self.logger.info(f"Saved {len(datasets)} datasets for {provider_code}")

    def process_series(self, provider_code: str, dataset_code: str, series_code: str):
        """Process a single series with enhanced error handling"""
        series_id = f"{provider_code}/{dataset_code}/{series_code}"

        try:
            series_data = self.get_series_data(provider_code, dataset_code, series_code)
            if series_data:
                with self.lock:
                    success = self.save_series_data_to_dedicated_table(
                        provider_code, dataset_code, series_data
                    )
                    if not success:
                        self.logger.error(f"Failed to save data for {series_id}")
            else:
                self.logger.warning(f"No data retrieved for {series_id}")

        except Exception as e:
            error_msg = f"Error processing series {series_id}: {e}"
            self.logger.error(error_msg)
            self.stats['errors'] += 1

            # Log the error operation
            self.log_processing_operation(
                'series_process', series_id, 'N/A', 0, 0, 'error', error_msg
            )

    def scrape_complete_data(self, max_providers: int = None,
                             max_datasets_per_provider: int = None,
                             target_providers: List[str] = None):
        """
        Scrape complete data with production-ready approach

        Args:
            max_providers: Limit number of providers (for testing)
            max_datasets_per_provider: Limit datasets per provider (for testing)
            target_providers: Specific provider codes to process
        """
        self.logger.info("Starting production DB Nomics data scraping...")

        # Step 1: Get all providers
        providers = self.get_providers()
        if not providers:
            self.logger.error("No providers found. Exiting.")
            return

        # Filter providers if specified
        if target_providers:
            providers = [p for p in providers if p.get('code') in target_providers]
            self.logger.info(f"Filtered to {len(providers)} target providers")

        # Save providers
        self.save_providers(providers)

        # Limit providers for testing
        if max_providers:
            providers = providers[:max_providers]
            self.logger.info(f"Limited to {len(providers)} providers for testing")

        # Step 2: Process each provider
        total_providers = len(providers)
        for i, provider in enumerate(providers, 1):
            provider_code = provider.get('code', '')
            self.logger.info(f"Processing provider {i}/{total_providers}: {provider_code}")

            # Get datasets for this provider
            datasets = self.get_datasets(provider_code)
            if not datasets:
                self.logger.warning(f"No datasets found for {provider_code}")
                continue

            # Save datasets
            self.save_datasets(provider_code, datasets)

            # Limit datasets for testing
            if max_datasets_per_provider:
                datasets = datasets[:max_datasets_per_provider]

            # Step 3: Process each dataset
            total_datasets = len(datasets)
            for j, dataset in enumerate(datasets, 1):
                dataset_code = dataset.get('code', '')
                self.logger.info(f"Processing dataset {j}/{total_datasets}: {provider_code}/{dataset_code}")

                # Get series list for this dataset
                series_list = self.get_series_list(provider_code, dataset_code)
                if not series_list:
                    self.logger.warning(f"No series found for {provider_code}/{dataset_code}")
                    continue

                # Step 4: Process all series with threading
                series_codes = [s.get('series_code', '') for s in series_list]
                total_series = len(series_codes)

                self.logger.info(f"Processing {total_series} series with {self.max_workers} workers")

                with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
                    futures = [
                        executor.submit(self.process_series, provider_code, dataset_code, series_code)
                        for series_code in series_codes
                    ]

                    # Wait for all series to complete with progress tracking
                    completed = 0
                    for future in as_completed(futures):
                        try:
                            future.result()
                            completed += 1
                            if completed % 10 == 0:  # Log progress every 10 series
                                self.logger.info(f"Progress: {completed}/{total_series} series completed")
                        except Exception as e:
                            self.logger.error(f"Error in thread: {e}")
                            self.stats['errors'] += 1

                self.logger.info(f"Completed dataset: {provider_code}/{dataset_code}")

            self.logger.info(f"Completed provider: {provider_code}")

        self.logger.info("Complete scraping finished!")
        self.print_production_statistics()

    def get_series_data_easy(self, provider_code: str, dataset_code: str,
                             series_code: str, start_date: str = None,
                             end_date: str = None) -> pd.DataFrame:
        """
        Easy data extraction for production use

        Args:
            provider_code: Provider code
            dataset_code: Dataset code
            series_code: Series code
            start_date: Optional start date filter (YYYY-MM-DD)
            end_date: Optional end date filter (YYYY-MM-DD)

        Returns:
            DataFrame with the series data
        """
        series_id = f"{provider_code}/{dataset_code}/{series_code}"

        try:
            # Get table name from metadata
            with sqlite3.connect(self.db_path) as conn:
                result = conn.execute('''
                    SELECT table_name, series_name, frequency, record_count 
                    FROM series_metadata WHERE series_id = ?
                ''', (series_id,)).fetchone()

                if not result:
                    self.logger.warning(f"Series {series_id} not found in database")
                    return pd.DataFrame()

                table_name, series_name, frequency, record_count = result

                # Build query with optional date filtering
                query = f"SELECT * FROM {table_name}"
                params = []

                if start_date or end_date:
                    conditions = []
                    if start_date:
                        conditions.append("period_start_date >= ?")
                        params.append(start_date)
                    if end_date:
                        conditions.append("period_start_date <= ?")
                        params.append(end_date)
                    query += " WHERE " + " AND ".join(conditions)

                query += " ORDER BY period_start_date"

                df = pd.read_sql_query(query, conn, params=params)

                if not df.empty:
                    # Add metadata as attributes
                    df.attrs['series_id'] = series_id
                    df.attrs['series_name'] = series_name
                    df.attrs['frequency'] = frequency
                    df.attrs['total_records'] = record_count

                    self.logger.info(f"Retrieved {len(df)} records for {series_id}")
                else:
                    self.logger.warning(f"No data found for {series_id} with given filters")

                return df

        except Exception as e:
            self.logger.error(f"Error retrieving data for {series_id}: {e}")
            return pd.DataFrame()

    def get_all_series_catalog(self) -> pd.DataFrame:
        """Get complete catalog of all stored series"""
        with sqlite3.connect(self.db_path) as conn:
            df = pd.read_sql_query('''
                SELECT 
                    series_id,
                    provider_code,
                    dataset_code, 
                    series_code,
                    series_name,
                    frequency,
                    table_name,
                    first_period,
                    last_period,
                    record_count,
                    last_scraped_at,
                    status
                FROM series_metadata
                WHERE status = 'active'
                ORDER BY last_scraped_at DESC
            ''', conn)

        self.logger.info(f"Retrieved catalog of {len(df)} series")
        return df

    def export_series_to_csv(self, provider_code: str, dataset_code: str,
                             series_code: str, output_path: str = None) -> str:
        """Export specific series to CSV"""
        df = self.get_series_data_easy(provider_code, dataset_code, series_code)

        if df.empty:
            raise ValueError(f"No data found for series {provider_code}/{dataset_code}/{series_code}")

        if not output_path:
            safe_name = f"{provider_code}_{dataset_code}_{series_code}".replace('/', '_').replace('.', '_')
            output_path = f"{self.output_dir}/{safe_name}_{datetime.now().strftime('%Y%m%d')}.csv"

        df.to_csv(output_path, index=False)
        self.logger.info(f"Exported {len(df)} records to {output_path}")
        return output_path

    def check_data_freshness_report(self, hours_threshold: int = 24) -> pd.DataFrame:
        """Generate comprehensive data freshness report"""
        with sqlite3.connect(self.db_path) as conn:
            threshold_time = datetime.now().timestamp() - (hours_threshold * 3600)
            threshold_iso = datetime.fromtimestamp(threshold_time).isoformat()

            df = pd.read_sql_query('''
                SELECT 
                    sm.series_id,
                    sm.provider_code,
                    sm.dataset_code,
                    sm.series_code,
                    sm.series_name,
                    sm.record_count,
                    sm.last_scraped_at,
                    sm.last_period,
                    CASE 
                        WHEN sm.last_scraped_at < ? THEN 'STALE'
                        ELSE 'FRESH'
                    END as freshness_status,
                    df.response_time_ms,
                    df.error_message
                FROM series_metadata sm
                LEFT JOIN (
                    SELECT series_id, response_time_ms, error_message,
                           ROW_NUMBER() OVER (PARTITION BY series_id ORDER BY check_timestamp DESC) as rn
                    FROM data_freshness
                ) df ON sm.series_id = df.series_id AND df.rn = 1
                WHERE sm.status = 'active'
                ORDER BY sm.last_scraped_at ASC
            ''', conn, params=(threshold_iso,))

        self.logger.info(f"Generated freshness report for {len(df)} series")
        return df

    def update_stale_series(self, hours_threshold: int = 24, max_updates: int = 50) -> Dict:
        """Update series that haven't been refreshed recently"""
        freshness_report = self.check_data_freshness_report(hours_threshold)
        stale_series = freshness_report[freshness_report['freshness_status'] == 'STALE'].head(max_updates)

        results = {
            'total_stale': len(freshness_report[freshness_report['freshness_status'] == 'STALE']),
            'attempted_updates': len(stale_series),
            'successful_updates': 0,
            'failed_updates': 0,
            'errors': []
        }

        self.logger.info(f"Updating {len(stale_series)} stale series...")

        for _, row in stale_series.iterrows():
            try:
                series_data = self.get_series_data(
                    row['provider_code'],
                    row['dataset_code'],
                    row['series_code']
                )

                if series_data:
                    success = self.save_series_data_to_dedicated_table(
                        row['provider_code'],
                        row['dataset_code'],
                        series_data
                    )

                    if success:
                        results['successful_updates'] += 1
                        self.logger.info(f"Updated {row['series_id']}")
                    else:
                        results['failed_updates'] += 1
                        results['errors'].append(f"Failed to save {row['series_id']}")
                else:
                    results['failed_updates'] += 1
                    results['errors'].append(f"No data retrieved for {row['series_id']}")

                # Rate limiting between updates
                time.sleep(self.delay * 2)

            except Exception as e:
                results['failed_updates'] += 1
                error_msg = f"Error updating {row['series_id']}: {str(e)}"
                results['errors'].append(error_msg)
                self.logger.error(error_msg)

        self.logger.info(f"Update completed: {results['successful_updates']}/{results['attempted_updates']} successful")
        return results

    def get_database_statistics(self) -> Dict:
        """Get comprehensive database statistics"""
        with sqlite3.connect(self.db_path) as conn:
            stats = {}

            # Basic counts
            stats['total_providers'] = conn.execute("SELECT COUNT(*) FROM providers").fetchone()[0]
            stats['total_datasets'] = conn.execute("SELECT COUNT(*) FROM datasets").fetchone()[0]
            stats['total_series'] = \
            conn.execute("SELECT COUNT(*) FROM series_metadata WHERE status='active'").fetchone()[0]
            stats['total_tables'] = len(self.created_tables)

            # Data volume
            total_observations = conn.execute("SELECT SUM(record_count) FROM series_metadata").fetchone()[0] or 0
            stats['total_observations'] = total_observations

            # Database size
            db_size_mb = os.path.getsize(self.db_path) / (1024 * 1024)
            stats['database_size_mb'] = round(db_size_mb, 2)

            # Freshness statistics
            now = datetime.now()
            one_day_ago = (now.timestamp() - 86400)
            one_week_ago = (now.timestamp() - 604800)

            fresh_24h = conn.execute('''
                SELECT COUNT(*) FROM series_metadata 
                WHERE last_scraped_at > ? AND status='active'
            ''', (datetime.fromtimestamp(one_day_ago).isoformat(),)).fetchone()[0]

            fresh_week = conn.execute('''
                SELECT COUNT(*) FROM series_metadata 
                WHERE last_scraped_at > ? AND status='active'
            ''', (datetime.fromtimestamp(one_week_ago).isoformat(),)).fetchone()[0]

            stats['fresh_24h'] = fresh_24h
            stats['fresh_week'] = fresh_week
            stats['stale_24h'] = stats['total_series'] - fresh_24h
            stats['stale_week'] = stats['total_series'] - fresh_week

            # Error statistics
            error_count = conn.execute('''
                SELECT COUNT(*) FROM processing_log WHERE status='error'
            ''').fetchone()[0]
            stats['total_errors'] = error_count

            # Top providers by series count
            top_providers = conn.execute('''
                SELECT provider_code, COUNT(*) as series_count
                FROM series_metadata 
                WHERE status='active'
                GROUP BY provider_code 
                ORDER BY series_count DESC 
                LIMIT 10
            ''').fetchall()
            stats['top_providers'] = dict(top_providers)

        return stats

    def print_production_statistics(self):
        """Print comprehensive production statistics"""
        db_stats = self.get_database_statistics()
        runtime = datetime.now() - self.stats['start_time']

        self.logger.info("=" * 80)
        self.logger.info("PRODUCTION SCRAPING STATISTICS")
        self.logger.info("=" * 80)
        self.logger.info(f"Runtime: {runtime}")
        self.logger.info(f"Providers: {db_stats['total_providers']}")
        self.logger.info(f"Datasets: {db_stats['total_datasets']}")
        self.logger.info(f"Series: {db_stats['total_series']}")
        self.logger.info(f"Data Tables: {db_stats['total_tables']}")
        self.logger.info(f"Observations: {db_stats['total_observations']:,}")
        self.logger.info(f"Database Size: {db_stats['database_size_mb']} MB")
        self.logger.info(f"Fresh (24h): {db_stats['fresh_24h']}")
        self.logger.info(f"Stale (24h): {db_stats['stale_24h']}")
        self.logger.info(f"Errors: {db_stats['total_errors']}")
        self.logger.info(f"Freshness Checks: {self.stats['data_freshness_checks']}")

        self.logger.info("\nTop Providers by Series Count:")
        for provider, count in list(db_stats['top_providers'].items())[:5]:
            self.logger.info(f"   {provider}: {count:,} series")

        self.logger.info("=" * 80)

    def create_production_dashboard_data(self) -> Dict:
        """Create data for production monitoring dashboard"""
        db_stats = self.get_database_statistics()
        freshness_report = self.check_data_freshness_report(24)

        dashboard_data = {
            'summary': db_stats,
            'freshness_distribution': {
                'fresh': len(freshness_report[freshness_report['freshness_status'] == 'FRESH']),
                'stale': len(freshness_report[freshness_report['freshness_status'] == 'STALE'])
            },
            'recent_errors': [],
            'performance_metrics': {
                'tables_created': self.stats['tables_created'],
                'avg_response_time': 0,  # Could calculate from freshness data
                'success_rate': 0  # Could calculate from processing log
            },
            'top_series_by_observations': []
        }

        # Get recent errors
        with sqlite3.connect(self.db_path) as conn:
            recent_errors = conn.execute('''
                SELECT operation_type, series_id, error_message, timestamp
                FROM processing_log 
                WHERE status='error' 
                ORDER BY timestamp DESC 
                LIMIT 10
            ''').fetchall()

            dashboard_data['recent_errors'] = [
                {
                    'operation': error[0],
                    'series_id': error[1],
                    'error': error[2],
                    'timestamp': error[3]
                } for error in recent_errors
            ]

            # Get top series by record count
            top_series = conn.execute('''
                SELECT series_id, series_name, record_count, table_name
                FROM series_metadata 
                WHERE status='active'
                ORDER BY record_count DESC 
                LIMIT 10
            ''').fetchall()

            dashboard_data['top_series_by_observations'] = [
                {
                    'series_id': series[0],
                    'series_name': series[1],
                    'record_count': series[2],
                    'table_name': series[3]
                } for series in top_series
            ]

        return dashboard_data

    def test_single_series_save(self, provider_code: str, dataset_code: str, series_code: str):
        """Test saving a single series to verify data is being stored correctly"""
        series_id = f"{provider_code}/{dataset_code}/{series_code}"
        self.logger.info(f"=== TESTING SERIES SAVE: {series_id} ===")

        # Fetch the data
        data = self.get_series_data(provider_code, dataset_code, series_code)

        if data:
            # Save the data
            success = self.save_series_data_to_dedicated_table(provider_code, dataset_code, data)

            if success:
                # Verify it was saved by querying the database
                table_name = self.generate_table_name(provider_code, dataset_code, series_code)

                with sqlite3.connect(self.db_path) as conn:
                    # Check metadata
                    metadata = conn.execute('''
                        SELECT record_count, first_period, last_period 
                        FROM series_metadata WHERE series_id = ?
                    ''', (series_id,)).fetchone()

                    if metadata:
                        record_count, first_period, last_period = metadata
                        self.logger.info(
                            f"✅ Metadata saved: {record_count} records from {first_period} to {last_period}")

                        # Check actual data in table
                        data_count = conn.execute(f"SELECT COUNT(*) FROM {table_name}").fetchone()[0]

                        # Get sample data
                        sample_data = conn.execute(f'''
                            SELECT period, value, value_original 
                            FROM {table_name} 
                            ORDER BY period 
                            LIMIT 5
                        ''').fetchall()

                        self.logger.info(f"✅ Actual data in table: {data_count} records")
                        self.logger.info("Sample data:")
                        for period, value, value_orig in sample_data:
                            self.logger.info(f"  {period}: {value} (original: {value_orig})")

                        # Check for zero values specifically
                        zero_count = conn.execute(f'''
                            SELECT COUNT(*) FROM {table_name} WHERE value = 0.0
                        ''').fetchone()[0]

                        if zero_count > 0:
                            self.logger.info(f"✅ Found {zero_count} zero values (correctly saved)")

                            zero_samples = conn.execute(f'''
                                SELECT period, value_original FROM {table_name} 
                                WHERE value = 0.0 LIMIT 3
                            ''').fetchall()

                            for period, value_orig in zero_samples:
                                self.logger.info(f"  Zero value example: {period} = {value_orig}")

                        return True
                    else:
                        self.logger.error("❌ No metadata found after save")
                        return False
            else:
                self.logger.error("❌ Failed to save series data")
                return False
        else:
            self.logger.error("❌ Failed to fetch series data")
            return False

    def debug_single_series(self, provider_code: str, dataset_code: str, series_code: str):
        """Debug a single series to understand the data structure"""
        series_id = f"{provider_code}/{dataset_code}/{series_code}"
        self.logger.info(f"=== DEBUGGING SERIES: {series_id} ===")

        # Test different API endpoints and parameters
        test_params = [
            {'observations': 1},
            {'observations': 'true'},
            {},  # No parameters
            {'observations': 1, 'format': 'json'},
        ]

        for i, params in enumerate(test_params):
            self.logger.info(f"--- Test {i + 1}: params = {params} ---")

            data = self.make_request(
                f"/series/{provider_code}/{dataset_code}/{series_code}",
                params=params
            )

            if data and 'series' in data and data['series']['docs']:
                series_info = data['series']['docs'][0]

                self.logger.info(f"Series keys: {list(series_info.keys())}")

                # Check for observations
                if 'observations' in series_info:
                    obs = series_info['observations']
                    self.logger.info(
                        f"Found 'observations' key with {len(obs) if isinstance(obs, list) else type(obs)} items")
                    if isinstance(obs, list) and obs:
                        self.logger.info(f"Sample observation: {obs[0]}")

                # Check for period/value arrays
                if 'period' in series_info:
                    periods = series_info['period']
                    self.logger.info(
                        f"Found 'period' array with {len(periods) if isinstance(periods, list) else type(periods)} items")
                    if isinstance(periods, list) and periods:
                        self.logger.info(f"Sample periods: {periods[:3]}")

                if 'value' in series_info:
                    values = series_info['value']
                    self.logger.info(
                        f"Found 'value' array with {len(values) if isinstance(values, list) else type(values)} items")
                    if isinstance(values, list) and values:
                        self.logger.info(f"Sample values: {values[:3]}")

                # Show some raw data structure
                self.logger.info("Raw series structure (first 1000 chars):")
                self.logger.info(json.dumps(series_info, indent=2)[:1000] + "...")

                break  # Stop at first successful response
            else:
                self.logger.warning(f"No data returned for test {i + 1}")

        self.logger.info("=== DEBUG COMPLETE ===")

    def export_all_metadata_to_csv(self):
        """Export all metadata tables to CSV for analysis"""
        self.logger.info("Exporting all metadata to CSV files...")

        with sqlite3.connect(self.db_path) as conn:
            # Export main metadata tables
            tables_to_export = [
                'providers', 'datasets', 'series_metadata',
                'data_freshness', 'series_dimensions', 'processing_log'
            ]

            for table in tables_to_export:
                try:
                    df = pd.read_sql_query(f"SELECT * FROM {table}", conn)
                    output_path = f"{self.output_dir}/{table}_metadata.csv"
                    df.to_csv(output_path, index=False)
                    self.logger.info(f"Exported {table} ({len(df)} rows) to {output_path}")
                except Exception as e:
                    self.logger.error(f"Failed to export {table}: {e}")

        self.logger.info("Metadata export completed!")


# Production usage examples
if __name__ == "__main__":
    # Initialize production scraper with optimized settings
    scraper = ProductionDBNomicsScraper(
        max_workers=10,  # Increased for faster processing
        delay=0.05,  # Reduced delay for speed
        output_dir="db_nomics_production_data"
    )

    print("🚀 Production DB Nomics Scraper")
    print("=" * 50)

    # Test single series save to verify the fix
    print("\n🧪 Testing single series save...")
    test_success = scraper.test_single_series_save(
        "ACOSS",
        "ACOSS_STAT_QUARTERLY_PAYROLL_EMPLOYMENT",
        "payroll_not_seasonally_adjusted.ASSOC.levels.Q"
    )

    if test_success:
        print("✅ Single series test PASSED - Data is being saved correctly!")
    else:
        print("❌ Single series test FAILED - There's still an issue")
        exit(1)

    # Debug: Test one series to understand the data structure
    print("\n🔍 Debugging single series to understand data structure...")
    scraper.debug_single_series(
        "ACOSS",
        "ACOSS_STAT_QUARTERLY_PAYROLL_EMPLOYMENT",
        "payroll_not_seasonally_adjusted.CA.levels.Q"
    )

    # Example 1: Target specific providers (like ACOSS)
    target_providers = ["ACOSS"]  # Start with just ACOSS for debugging

    print(f"📊 Starting targeted scraping for providers: {target_providers}")
    scraper.scrape_complete_data(
        target_providers=target_providers,
        max_datasets_per_provider=1,  # Just one dataset for debugging
        max_providers=1  # Just one provider
    )

    # Example 2: Easy data extraction
    print("\n📈 Testing data extraction...")
    acoss_data = scraper.get_series_data_easy(
        "ACOSS",
        "ACOSS_STAT_QUARTERLY_PAYROLL_EMPLOYMENT",
        "payroll_not_seasonally_adjusted.CA.levels.Q"
    )

    if not acoss_data.empty:
        print(f"✅ Retrieved {len(acoss_data)} ACOSS records")
        print(acoss_data.head())

        # Export to CSV
        csv_path = scraper.export_series_to_csv(
            "ACOSS",
            "ACOSS_STAT_QUARTERLY_PAYROLL_EMPLOYMENT",
            "payroll_not_seasonally_adjusted.CA.levels.Q"
        )
        print(f"📁 Exported to: {csv_path}")
    else:
        print("❌ No ACOSS data found")

    # Example 3: Get comprehensive statistics
    print("\n📊 Database Statistics:")
    stats = scraper.get_database_statistics()
    for key, value in stats.items():
        if isinstance(value, dict):
            print(f"{key}:")
            for k, v in value.items():
                print(f"  {k}: {v}")
        else:
            print(f"{key}: {value}")

    # Example 4: Check data freshness
    print("\n🕒 Data Freshness Report:")
    freshness = scraper.check_data_freshness_report(24)
    print(f"Fresh series (24h): {len(freshness[freshness['freshness_status'] == 'FRESH'])}")
    print(f"Stale series (24h): {len(freshness[freshness['freshness_status'] == 'STALE'])}")

    # Example 5: Get all series catalog
    print("\n📋 Series Catalog:")
    catalog = scraper.get_all_series_catalog()
    print(f"Total series in catalog: {len(catalog)}")
    if not catalog.empty:
        print(catalog[['series_id', 'record_count', 'last_scraped_at']].head())

    # Example 6: Update stale data
    print("\n🔄 Updating stale series...")
    update_results = scraper.update_stale_series(hours_threshold=24, max_updates=5)
    print(f"Update results: {update_results}")

    # Example 7: Export metadata for analysis
    scraper.export_all_metadata_to_csv()

    # Example 8: Create dashboard data
    dashboard_data = scraper.create_production_dashboard_data()

    # Save dashboard data as JSON
    with open(f"{scraper.output_dir}/dashboard_data.json", 'w') as f:
        json.dump(dashboard_data, f, indent=2, default=str)

    print(f"\n💾 Dashboard data saved to {scraper.output_dir}/dashboard_data.json")
    print("🎉 Production scraping demo completed!")