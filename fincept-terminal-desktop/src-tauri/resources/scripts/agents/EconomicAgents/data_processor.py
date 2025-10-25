"""
Data processor for handling different economic data formats and sources
"""

import json
import csv
import xml.etree.ElementTree as ET
import pandas as pd
from typing import Dict, List, Any, Union, Optional, Tuple
from datetime import datetime, date
from pathlib import Path
import requests
import sqlite3
from base_agent import EconomicData

class EconomicDataProcessor:
    """Processes economic data from various sources and formats"""

    def __init__(self):
        self.supported_formats = ['json', 'csv', 'xml', 'excel', 'sqlite', 'api']
        self.data_cache = {}

    def load_from_json(self, filepath: str) -> Union[EconomicData, List[EconomicData]]:
        """
        Load economic data from JSON file

        Args:
            filepath (str): Path to JSON file

        Returns:
            Union[EconomicData, List[EconomicData]]: Loaded economic data
        """
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)

            if isinstance(data, dict):
                return self._dict_to_economic_data(data)
            elif isinstance(data, list):
                return [self._dict_to_economic_data(item) for item in data]
            else:
                raise ValueError("Invalid JSON structure")

        except Exception as e:
            raise Exception(f"Error loading JSON file: {e}")

    def load_from_csv(self, filepath: str, date_column: str = "timestamp") -> List[EconomicData]:
        """
        Load economic data from CSV file

        Args:
            filepath (str): Path to CSV file
            date_column (str): Name of the date column

        Returns:
            List[EconomicData]: List of economic data points
        """
        try:
            df = pd.read_csv(filepath)
            data_points = []

            for _, row in df.iterrows():
                try:
                    # Handle date parsing
                    timestamp = self._parse_date(row[date_column]) if date_column in row else datetime.now()

                    data_point = EconomicData(
                        gdp=float(row.get('gdp', 0)),
                        inflation=float(row.get('inflation', 0)),
                        unemployment=float(row.get('unemployment', 0)),
                        interest_rate=float(row.get('interest_rate', 0)),
                        trade_balance=float(row.get('trade_balance', 0)),
                        government_spending=float(row.get('government_spending', 0)),
                        tax_rate=float(row.get('tax_rate', 0)),
                        private_investment=float(row.get('private_investment', 0)),
                        consumer_confidence=float(row.get('consumer_confidence', 0)),
                        timestamp=timestamp
                    )
                    data_points.append(data_point)
                except (ValueError, KeyError) as e:
                    print(f"Skipping invalid row: {e}")
                    continue

            return data_points

        except Exception as e:
            raise Exception(f"Error loading CSV file: {e}")

    def load_from_xml(self, filepath: str) -> List[EconomicData]:
        """
        Load economic data from XML file

        Args:
            filepath (str): Path to XML file

        Returns:
            List[EconomicData]: List of economic data points
        """
        try:
            tree = ET.parse(filepath)
            root = tree.getroot()
            data_points = []

            for data_point in root.findall('data_point'):
                try:
                    data = EconomicData(
                        gdp=float(data_point.find('gdp').text) if data_point.find('gdp') is not None else 0,
                        inflation=float(data_point.find('inflation').text) if data_point.find('inflation') is not None else 0,
                        unemployment=float(data_point.find('unemployment').text) if data_point.find('unemployment') is not None else 0,
                        interest_rate=float(data_point.find('interest_rate').text) if data_point.find('interest_rate') is not None else 0,
                        trade_balance=float(data_point.find('trade_balance').text) if data_point.find('trade_balance') is not None else 0,
                        government_spending=float(data_point.find('government_spending').text) if data_point.find('government_spending') is not None else 0,
                        tax_rate=float(data_point.find('tax_rate').text) if data_point.find('tax_rate') is not None else 0,
                        private_investment=float(data_point.find('private_investment').text) if data_point.find('private_investment') is not None else 0,
                        consumer_confidence=float(data_point.find('consumer_confidence').text) if data_point.find('consumer_confidence') is not None else 0,
                        timestamp=self._parse_date(data_point.find('timestamp').text) if data_point.find('timestamp') is not None else datetime.now()
                    )
                    data_points.append(data)
                except (ValueError, AttributeError) as e:
                    print(f"Skipping invalid XML element: {e}")
                    continue

            return data_points

        except Exception as e:
            raise Exception(f"Error loading XML file: {e}")

    def load_from_excel(self, filepath: str, sheet_name: str = "EconomicData") -> List[EconomicData]:
        """
        Load economic data from Excel file

        Args:
            filepath (str): Path to Excel file
            sheet_name (str): Name of the sheet containing data

        Returns:
            List[EconomicData]: List of economic data points
        """
        try:
            df = pd.read_excel(filepath, sheet_name=sheet_name)
            # Convert DataFrame to CSV format for processing
            csv_data = df.to_csv(index=False)

            # Save temporary CSV and use CSV loader
            import tempfile
            with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as tmp:
                tmp.write(csv_data)
                tmp_path = tmp.name

            try:
                data = self.load_from_csv(tmp_path)
                return data
            finally:
                Path(tmp_path).unlink()  # Clean up temporary file

        except Exception as e:
            raise Exception(f"Error loading Excel file: {e}")

    def load_from_sqlite(self, db_path: str, table_name: str = "economic_data") -> List[EconomicData]:
        """
        Load economic data from SQLite database

        Args:
            db_path (str): Path to SQLite database
            table_name (str): Name of the table containing economic data

        Returns:
            List[EconomicData]: List of economic data points
        """
        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()

            query = f"SELECT * FROM {table_name}"
            cursor.execute(query)
            rows = cursor.fetchall()
            columns = [desc[0] for desc in cursor.description]
            conn.close()

            data_points = []
            for row in rows:
                row_dict = dict(zip(columns, row))
                data_point = EconomicData(
                    gdp=float(row_dict.get('gdp', 0)),
                    inflation=float(row_dict.get('inflation', 0)),
                    unemployment=float(row_dict.get('unemployment', 0)),
                    interest_rate=float(row_dict.get('interest_rate', 0)),
                    trade_balance=float(row_dict.get('trade_balance', 0)),
                    government_spending=float(row_dict.get('government_spending', 0)),
                    tax_rate=float(row_dict.get('tax_rate', 0)),
                    private_investment=float(row_dict.get('private_investment', 0)),
                    consumer_confidence=float(row_dict.get('consumer_confidence', 0)),
                    timestamp=self._parse_date(row_dict.get('timestamp')) if row_dict.get('timestamp') else datetime.now()
                )
                data_points.append(data_point)

            return data_points

        except Exception as e:
            raise Exception(f"Error loading from SQLite: {e}")

    def fetch_from_api(self, api_url: str, api_key: Optional[str] = None, headers: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Fetch economic data from API

        Args:
            api_url (str): API endpoint URL
            api_key (Optional[str]): API authentication key
            headers (Optional[Dict]): Additional headers

        Returns:
            Dict[str, Any]: API response data
        """
        try:
            if headers is None:
                headers = {}

            if api_key:
                headers['Authorization'] = f'Bearer {api_key}'

            response = requests.get(api_url, headers=headers, timeout=30)
            response.raise_for_status()

            return response.json()

        except Exception as e:
            raise Exception(f"Error fetching from API: {e}")

    def save_to_json(self, data: Union[EconomicData, List[EconomicData]], filepath: str) -> bool:
        """
        Save economic data to JSON file

        Args:
            data: Economic data to save
            filepath (str): Output file path

        Returns:
            bool: Success status
        """
        try:
            if isinstance(data, EconomicData):
                json_data = self._economic_data_to_dict(data)
            else:
                json_data = [self._economic_data_to_dict(item) for item in data]

            with open(filepath, 'w') as f:
                json.dump(json_data, f, indent=2, default=str)

            return True

        except Exception as e:
            print(f"Error saving to JSON: {e}")
            return False

    def save_to_csv(self, data: List[EconomicData], filepath: str) -> bool:
        """
        Save economic data to CSV file

        Args:
            data: List of economic data points
            filepath (str): Output file path

        Returns:
            bool: Success status
        """
        try:
            data_dicts = [self._economic_data_to_dict(item) for item in data]

            with open(filepath, 'w', newline='') as csvfile:
                fieldnames = ['timestamp', 'gdp', 'inflation', 'unemployment',
                             'interest_rate', 'trade_balance', 'government_spending',
                             'tax_rate', 'private_investment', 'consumer_confidence']
                writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                writer.writeheader()
                writer.writerows(data_dicts)

            return True

        except Exception as e:
            print(f"Error saving to CSV: {e}")
            return False

    def validate_data_consistency(self, data: List[EconomicData]) -> Tuple[bool, List[str]]:
        """
        Validate consistency of economic data

        Args:
            data: List of economic data points

        Returns:
            Tuple[bool, List[str]]: (is_valid, list of issues)
        """
        issues = []

        if not data:
            issues.append("No data provided")
            return False, issues

        # Check for duplicate timestamps
        timestamps = [dp.timestamp for dp in data]
        if len(timestamps) != len(set(timestamps)):
            issues.append("Duplicate timestamps found")

        # Check data ranges for each point
        for i, dp in enumerate(data):
            from utils import validate_economic_data
            data_issues = validate_economic_data(dp)
            for issue in data_issues:
                issues.append(f"Data point {i}: {issue}")

        # Check for logical inconsistencies
        for i, dp in enumerate(data):
            # Government spending + private investment shouldn't exceed 100% of GDP
            if dp.government_spending + dp.private_investment > 100:
                issues.append(f"Data point {i}: Sum of government spending and private investment exceeds 100%")

            # Tax rate should be reasonable for government spending level
            if dp.government_spending > 30 and dp.tax_rate < 15:
                issues.append(f"Data point {i}: Low tax rate for high government spending")

        return len(issues) == 0, issues

    def interpolate_missing_data(self, data: List[EconomicData], method: str = "linear") -> List[EconomicData]:
        """
        Interpolate missing data points

        Args:
            data: List of economic data points
            method (str): Interpolation method ('linear', 'forward_fill', 'backward_fill')

        Returns:
            List[EconomicData]: Data with interpolated values
        """
        if len(data) < 2:
            return data

        # Sort data by timestamp
        sorted_data = sorted(data, key=lambda x: x.timestamp)

        # Convert to DataFrame for easier interpolation
        df = pd.DataFrame([self._economic_data_to_dict(dp) for dp in sorted_data])

        # Set timestamp as index
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        df.set_index('timestamp', inplace=True)

        # Interpolate missing values
        if method == "linear":
            df.interpolate(method='linear', inplace=True)
        elif method == "forward_fill":
            df.fillna(method='ffill', inplace=True)
        elif method == "backward_fill":
            df.fillna(method='bfill', inplace=True)

        # Convert back to EconomicData objects
        result = []
        for idx, row in df.iterrows():
            data_point = EconomicData(
                gdp=float(row['gdp']),
                inflation=float(row['inflation']),
                unemployment=float(row['unemployment']),
                interest_rate=float(row['interest_rate']),
                trade_balance=float(row['trade_balance']),
                government_spending=float(row['government_spending']),
                tax_rate=float(row['tax_rate']),
                private_investment=float(row['private_investment']),
                consumer_confidence=float(row['consumer_confidence']),
                timestamp=idx.to_pydatetime()
            )
            result.append(data_point)

        return result

    def aggregate_data_by_period(self, data: List[EconomicData], period: str = "monthly") -> List[EconomicData]:
        """
        Aggregate economic data by time period

        Args:
            data: List of economic data points
            period (str): Aggregation period ('daily', 'weekly', 'monthly', 'quarterly', 'yearly')

        Returns:
            List[EconomicData]: Aggregated data
        """
        if not data:
            return []

        # Convert to DataFrame
        df = pd.DataFrame([self._economic_data_to_dict(dp) for dp in data])
        df['timestamp'] = pd.to_datetime(df['timestamp'])
        df.set_index('timestamp', inplace=True)

        # Define aggregation functions
        agg_functions = {
            'gdp': 'mean',
            'inflation': 'mean',
            'unemployment': 'mean',
            'interest_rate': 'mean',
            'trade_balance': 'mean',
            'government_spending': 'mean',
            'tax_rate': 'mean',
            'private_investment': 'mean',
            'consumer_confidence': 'mean'
        }

        # Group by period and aggregate
        if period == "daily":
            resampled = df.resample('D').agg(agg_functions)
        elif period == "weekly":
            resampled = df.resample('W').agg(agg_functions)
        elif period == "monthly":
            resampled = df.resample('M').agg(agg_functions)
        elif period == "quarterly":
            resampled = df.resample('Q').agg(agg_functions)
        elif period == "yearly":
            resampled = df.resample('Y').agg(agg_functions)
        else:
            raise ValueError(f"Unsupported period: {period}")

        # Convert back to EconomicData objects
        result = []
        for idx, row in resampled.iterrows():
            if not pd.isna(row['gdp']):  # Skip empty periods
                data_point = EconomicData(
                    gdp=float(row['gdp']),
                    inflation=float(row['inflation']),
                    unemployment=float(row['unemployment']),
                    interest_rate=float(row['interest_rate']),
                    trade_balance=float(row['trade_balance']),
                    government_spending=float(row['government_spending']),
                    tax_rate=float(row['tax_rate']),
                    private_investment=float(row['private_investment']),
                    consumer_confidence=float(row['consumer_confidence']),
                    timestamp=idx.to_pydatetime()
                )
                result.append(data_point)

        return result

    def _dict_to_economic_data(self, data_dict: Dict) -> EconomicData:
        """Convert dictionary to EconomicData object"""
        return EconomicData(
            gdp=float(data_dict.get('gdp', 0)),
            inflation=float(data_dict.get('inflation', 0)),
            unemployment=float(data_dict.get('unemployment', 0)),
            interest_rate=float(data_dict.get('interest_rate', 0)),
            trade_balance=float(data_dict.get('trade_balance', 0)),
            government_spending=float(data_dict.get('government_spending', 0)),
            tax_rate=float(data_dict.get('tax_rate', 0)),
            private_investment=float(data_dict.get('private_investment', 0)),
            consumer_confidence=float(data_dict.get('consumer_confidence', 0)),
            timestamp=self._parse_date(data_dict.get('timestamp')) if data_dict.get('timestamp') else datetime.now()
        )

    def _economic_data_to_dict(self, data: EconomicData) -> Dict:
        """Convert EconomicData object to dictionary"""
        return {
            'timestamp': data.timestamp.isoformat(),
            'gdp': data.gdp,
            'inflation': data.inflation,
            'unemployment': data.unemployment,
            'interest_rate': data.interest_rate,
            'trade_balance': data.trade_balance,
            'government_spending': data.government_spending,
            'tax_rate': data.tax_rate,
            'private_investment': data.private_investment,
            'consumer_confidence': data.consumer_confidence
        }

    def _parse_date(self, date_value: Any) -> datetime:
        """Parse date from various formats"""
        if isinstance(date_value, datetime):
            return date_value
        elif isinstance(date_value, date):
            return datetime.combine(date_value, datetime.min.time())
        elif isinstance(date_value, str):
            try:
                return datetime.fromisoformat(date_value)
            except ValueError:
                try:
                    return datetime.strptime(date_value, "%Y-%m-%d")
                except ValueError:
                    try:
                        return datetime.strptime(date_value, "%Y-%m-%d %H:%M:%S")
                    except ValueError:
                        return datetime.now()
        else:
            return datetime.now()