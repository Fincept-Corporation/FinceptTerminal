# -*- coding: utf-8 -*-
# database_tab.py

import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import psycopg2
import psycopg2.extras
import threading
import traceback
import datetime
import csv

# Import new logger module
from fincept_terminal.Utils.Logging.logger import (
    info, debug, warning, error, operation, monitor_performance
)


class DatabaseTab(BaseTab):
    """Bloomberg Terminal style PostgreSQL Database Management Tab with performance optimizations"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg color scheme - Pre-computed for performance
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # Database connection variables
        self.connection = None
        self.cursor = None
        self.current_db = None
        self.current_table = None
        self.current_page = 1
        self.items_per_page = 100
        self.total_rows = 0
        self.total_pages = 0
        self.table_columns = []
        self.selected_columns = []
        self.table_data = []
        self.is_loading = False

        # Connection settings
        self.connection_settings = {
            'host': 'localhost',
            'port': '5432',
            'user': 'postgres',
            'password': '',
            'database': 'postgres'
        }

        # Performance optimization caches
        self._column_cache = {}
        self._table_info_cache = {}
        self._last_query_time = None

        debug("DatabaseTab initialized", context={'default_db': self.connection_settings['database']})

    def get_label(self):
        return "Database"

    @monitor_performance
    def create_content(self):
        """Create Bloomberg-style database terminal layout"""
        with operation("create_database_content"):
            try:
                # Top bar with Bloomberg branding
                with dpg.group(horizontal=True):
                    dpg.add_text("FINCEPT", color=self.BLOOMBERG_ORANGE)
                    dpg.add_text("DATABASE TERMINAL", color=self.BLOOMBERG_WHITE)
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_input_text(label="", default_value="SQL Query", width=300)
                    dpg.add_button(label="EXECUTE", width=80, callback=self.execute_query)
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                    dpg.add_text(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), tag="db_time_display")

                dpg.add_separator()

                # Function keys for database operations
                with dpg.group(horizontal=True):
                    function_keys = [
                        ("F1:CONNECT", self.connect_to_postgres),
                        ("F2:TABLES", self.refresh_tables),
                        ("F3:QUERY", self.load_table_data),
                        ("F4:EXPORT", self.export_to_csv),
                        ("F5:SCHEMA", self.show_table_schema),
                        ("F6:STATS", self.show_table_stats)
                    ]

                    for label, callback in function_keys:
                        dpg.add_button(label=label, width=120, height=25, callback=callback)

                dpg.add_separator()

                # Main database layout
                with dpg.group(horizontal=True):
                    # Left panel - Connection & Navigation
                    self.create_left_db_panel()

                    # Center panel - Data Viewer
                    self.create_center_db_panel()

                    # Right panel - Controls & Info
                    self.create_right_db_panel()

                # Bottom status bar
                dpg.add_separator()
                self.create_db_status_bar()

                info("Database content created successfully")

            except Exception as e:
                error("Error creating database content", context={'error': str(e)}, exc_info=True)
                # Fallback content
                dpg.add_text("DATABASE TERMINAL", color=self.BLOOMBERG_ORANGE)
                dpg.add_text("Error loading database interface. Please try again.")

    def create_left_db_panel(self):
        """Create left database connection and navigation panel"""
        with dpg.child_window(width=400, height=650, border=True):
            dpg.add_text("DATABASE CONNECTION", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Connection settings
            dpg.add_text("Connection Settings", color=self.BLOOMBERG_YELLOW)

            # Pre-define connection inputs for better performance
            connection_inputs = [
                ("Host", "db_host", self.connection_settings['host']),
                ("Port", "db_port", self.connection_settings['port']),
                ("Username", "db_user", self.connection_settings['user']),
                ("Password", "db_password", self.connection_settings['password'])
            ]

            for label, tag, default_value in connection_inputs:
                is_password = label == "Password"
                dpg.add_input_text(label=label, default_value=default_value,
                                   tag=tag, width=200, password=is_password)

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                connection_buttons = [
                    ("Connect", self.connect_to_postgres, 90),
                    ("Disconnect", self.disconnect_from_postgres, 90),
                    ("Test", self.test_connection, 90)
                ]

                for label, callback, width in connection_buttons:
                    dpg.add_button(label=label, callback=callback, width=width)

            dpg.add_separator()

            # Connection status
            dpg.add_text("Connection Status", color=self.BLOOMBERG_YELLOW)

            status_items = [
                ("connection_status_text", "Status: Disconnected", self.BLOOMBERG_RED),
                ("server_info", "Server: Not connected", None),
                ("current_db_info", "Database: None", None),
                ("tables_count", "Total Tables: 0", None)
            ]

            for tag, text, color in status_items:
                dpg.add_text(text, tag=tag, color=color or self.BLOOMBERG_WHITE)

            dpg.add_spacer(height=10)
            with dpg.child_window(height=80, tag="connection_log", border=True):
                dpg.add_text("Ready to connect...", tag="log_text")

            dpg.add_separator()

            # Database and table selection
            dpg.add_text("Select Database", color=self.BLOOMBERG_YELLOW)
            dpg.add_combo([], label="Database", tag="database_combo",
                          callback=self.on_database_selected, width=-1)
            dpg.add_spacer(height=10)
            dpg.add_button(label="Refresh DBs", callback=self.refresh_databases, width=-1)

            dpg.add_spacer(height=10)
            dpg.add_text("Select Table", color=self.BLOOMBERG_YELLOW)
            dpg.add_combo([], label="Table", tag="table_combo",
                          callback=self.on_table_selected, width=-1)
            dpg.add_spacer(height=10)
            dpg.add_button(label="Refresh Tables", callback=self.refresh_tables, width=-1)

    def create_center_db_panel(self):
        """Create center data viewer panel"""
        with dpg.child_window(width=700, height=650, border=True):
            dpg.add_text("DATA VIEWER", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Table information
            dpg.add_text("Table Information", color=self.BLOOMBERG_YELLOW)

            table_info_items = [
                ("selected_table_name", "Table: None"),
                ("table_columns_count", "Columns: 0"),
                ("table_rows_count", "Estimated Rows: 0")
            ]

            for tag, text in table_info_items:
                dpg.add_text(text, tag=tag)

            dpg.add_spacer(height=10)

            info_buttons = [
                ("Table Schema", self.show_table_schema),
                ("Quick Stats", self.show_table_stats)
            ]

            for label, callback in info_buttons:
                dpg.add_button(label=label, callback=callback, width=-1)

            dpg.add_separator()

            # Data status
            dpg.add_text("Select a table and load data to view", tag="data_status", color=self.BLOOMBERG_GRAY)

            # Data table container with horizontal scroll
            with dpg.child_window(height=450, border=True, tag="data_table_container", horizontal_scrollbar=True):
                dpg.add_text("No data loaded", color=self.BLOOMBERG_GRAY)

    def create_right_db_panel(self):
        """Create right controls and info panel"""
        with dpg.child_window(width=500, height=650, border=True):
            dpg.add_text("DATA CONTROLS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Column selection
            dpg.add_text("Select Columns to View", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                dpg.add_button(label="Select All", callback=self.select_all_columns, width=90)
                dpg.add_button(label="Clear All", callback=self.clear_all_columns, width=90)

            dpg.add_spacer(height=5)
            with dpg.child_window(height=150, tag="columns_checkboxes", border=True):
                dpg.add_text("Select a table first...")

            dpg.add_separator()

            # Query controls
            dpg.add_text("Query Controls", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Rows per page:")
            dpg.add_combo([25, 50, 100, 200, 500], default_value=100,
                          tag="rows_per_page", callback=self.on_rows_per_page_changed, width=-1)

            dpg.add_spacer(height=10)
            dpg.add_text("WHERE Clause:")
            dpg.add_input_text(hint="id > 100", tag="where_clause", width=-1)

            dpg.add_spacer(height=10)
            dpg.add_text("ORDER BY:")
            dpg.add_input_text(hint="id DESC", tag="order_by", width=-1)

            dpg.add_spacer(height=10)
            control_buttons = [
                ("Load Data", self.load_table_data),
                ("Export CSV", self.export_to_csv)
            ]

            for label, callback in control_buttons:
                dpg.add_button(label=label, callback=callback, width=-1)

            dpg.add_separator()

            # Pagination controls
            dpg.add_text("Pagination", color=self.BLOOMBERG_YELLOW)

            pagination_info = [
                ("page_info", "Page: 0 of 0"),
                ("total_rows_info", "Total Rows: 0")
            ]

            for tag, text in pagination_info:
                dpg.add_text(text, tag=tag)

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                pagination_buttons = [
                    ("First", self.first_page, 60),
                    ("Prev", self.previous_page, 60),
                    ("Next", self.next_page, 60),
                    ("Last", self.last_page, 60)
                ]

                for label, callback, width in pagination_buttons:
                    dpg.add_button(label=label, callback=callback, width=width)

            dpg.add_spacer(height=10)
            dpg.add_text("Go to page:")
            with dpg.group(horizontal=True):
                dpg.add_input_int(tag="goto_page", width=100, default_value=1)
                dpg.add_button(label="Go", callback=self.goto_page, width=50)

            dpg.add_spacer(height=10)
            dpg.add_progress_bar(tag="loading_progress", width=-1, show=False)

    def create_db_status_bar(self):
        """Create database status bar"""
        with dpg.group(horizontal=True):
            status_items = [
                ("DATABASE STATUS:", "DISCONNECTED", self.BLOOMBERG_GRAY, self.BLOOMBERG_RED),
                ("CONNECTION TYPE:", "POSTGRESQL", self.BLOOMBERG_GRAY, self.BLOOMBERG_ORANGE),
                ("LAST QUERY:", "NONE", self.BLOOMBERG_GRAY, self.BLOOMBERG_WHITE)
            ]

            for i, (label, value, label_color, value_color) in enumerate(status_items):
                if i > 0:
                    dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
                dpg.add_text(label, color=label_color)

                tag = None
                if "STATUS" in label:
                    tag = "db_status"
                elif "QUERY" in label:
                    tag = "last_query_time"

                dpg.add_text(value, color=value_color, tag=tag)

    # Database connection methods with performance optimizations
    @monitor_performance
    def connect_to_postgres(self):
        """Connect to PostgreSQL database"""

        def connect_thread():
            with operation("connect_to_postgres"):
                try:
                    self.update_log("Connecting to PostgreSQL...")
                    dpg.configure_item("loading_progress", show=True)

                    # Get connection settings from UI
                    connection_params = {
                        'host': dpg.get_value("db_host"),
                        'port': dpg.get_value("db_port"),
                        'user': dpg.get_value("db_user"),
                        'password': dpg.get_value("db_password"),
                        'database': 'postgres'  # Connect to default postgres db first
                    }

                    debug("Attempting database connection", context=connection_params)

                    # Create connection
                    self.connection = psycopg2.connect(**connection_params)
                    self.cursor = self.connection.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

                    # Update UI on main thread
                    dpg.set_value("connection_status_text", "Status: Connected")
                    dpg.configure_item("connection_status_text", color=self.BLOOMBERG_GREEN)
                    dpg.set_value("server_info", f"Server: {connection_params['host']}:{connection_params['port']}")
                    dpg.set_value("db_status", "CONNECTED")
                    dpg.configure_item("db_status", color=self.BLOOMBERG_GREEN)

                    self.update_log("Connected successfully!")
                    info("PostgreSQL connection established",
                         context={'host': connection_params['host'], 'port': connection_params['port']})

                    self.refresh_databases()

                except Exception as e:
                    error_msg = f"Connection failed: {str(e)}"
                    self.update_log(error_msg)
                    dpg.set_value("connection_status_text", "Status: Connection Failed")
                    dpg.configure_item("connection_status_text", color=self.BLOOMBERG_RED)
                    error("PostgreSQL connection failed", context={'error': str(e)}, exc_info=True)
                finally:
                    dpg.configure_item("loading_progress", show=False)

        threading.Thread(target=connect_thread, daemon=True).start()

    def disconnect_from_postgres(self):
        """Disconnect from PostgreSQL"""
        with operation("disconnect_from_postgres"):
            try:
                if self.cursor:
                    self.cursor.close()
                if self.connection:
                    self.connection.close()

                # Clear state
                self.connection = None
                self.cursor = None
                self.current_db = None
                self.current_table = None

                # Clear caches for performance
                self._column_cache.clear()
                self._table_info_cache.clear()

                # Update UI
                ui_updates = [
                    ("connection_status_text", "Status: Disconnected", self.BLOOMBERG_RED),
                    ("server_info", "Server: Not connected", None),
                    ("current_db_info", "Database: None", None),
                    ("tables_count", "Total Tables: 0", None),
                    ("db_status", "DISCONNECTED", self.BLOOMBERG_RED)
                ]

                for tag, text, color in ui_updates:
                    dpg.set_value(tag, text)
                    if color:
                        dpg.configure_item(tag, color=color)

                # Clear combos
                dpg.configure_item("database_combo", items=[])
                dpg.configure_item("table_combo", items=[])

                self.update_log("Disconnected from database")
                info("PostgreSQL disconnected")

            except Exception as e:
                error_msg = f"Error during disconnect: {str(e)}"
                self.update_log(error_msg)
                error("Error during PostgreSQL disconnect", context={'error': str(e)}, exc_info=True)

    def test_connection(self):
        """Test database connection"""

        def test_thread():
            with operation("test_connection"):
                try:
                    self.update_log("Testing connection...")

                    connection_params = {
                        'host': dpg.get_value("db_host"),
                        'port': dpg.get_value("db_port"),
                        'user': dpg.get_value("db_user"),
                        'password': dpg.get_value("db_password"),
                        'database': 'postgres'
                    }

                    test_conn = psycopg2.connect(**connection_params)
                    test_conn.close()

                    self.update_log("Connection test successful!")
                    info("Connection test successful", context=connection_params)

                except Exception as e:
                    error_msg = f"Connection test failed: {str(e)}"
                    self.update_log(error_msg)
                    error("Connection test failed", context={'error': str(e)}, exc_info=True)

        threading.Thread(target=test_thread, daemon=True).start()

    @monitor_performance
    def refresh_databases(self):
        """Refresh the list of available databases"""
        if not self.connection:
            self.update_log("Not connected to database")
            return

        with operation("refresh_databases"):
            try:
                self.cursor.execute("""
                    SELECT datname FROM pg_database 
                    WHERE datistemplate = false 
                    ORDER BY datname
                """)

                databases = [row['datname'] for row in self.cursor.fetchall()]
                dpg.configure_item("database_combo", items=databases)

                self.update_log(f"Found {len(databases)} databases")
                debug("Databases refreshed", context={'count': len(databases), 'databases': databases})

            except Exception as e:
                error_msg = f"Error fetching databases: {str(e)}"
                self.update_log(error_msg)
                error("Error fetching databases", context={'error': str(e)}, exc_info=True)

    @monitor_performance
    def on_database_selected(self, sender, app_data):
        """Handle database selection"""
        if not app_data:
            return

        with operation("database_selection", context={'database': app_data}):
            try:
                # Reconnect to selected database
                if self.connection:
                    self.connection.close()

                connection_params = {
                    'host': dpg.get_value("db_host"),
                    'port': dpg.get_value("db_port"),
                    'user': dpg.get_value("db_user"),
                    'password': dpg.get_value("db_password"),
                    'database': app_data
                }

                self.connection = psycopg2.connect(**connection_params)
                self.cursor = self.connection.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
                self.current_db = app_data

                dpg.set_value("current_db_info", f"Database: {app_data}")
                self.update_log(f"Connected to database: {app_data}")
                info("Database selected", context={'database': app_data})

                self.refresh_tables()

            except Exception as e:
                error_msg = f"Error connecting to database {app_data}: {str(e)}"
                self.update_log(error_msg)
                error("Error connecting to selected database",
                      context={'database': app_data, 'error': str(e)}, exc_info=True)

    @monitor_performance
    def refresh_tables(self):
        """Refresh the list of tables in current database"""
        if not self.connection or not self.current_db:
            return

        with operation("refresh_tables"):
            try:
                self.cursor.execute("""
                    SELECT table_name 
                    FROM information_schema.tables 
                    WHERE table_schema = 'public' 
                    ORDER BY table_name
                """)

                tables = [row['table_name'] for row in self.cursor.fetchall()]
                dpg.configure_item("table_combo", items=tables)
                dpg.set_value("tables_count", f"Total Tables: {len(tables)}")

                self.update_log(f"Found {len(tables)} tables")
                debug("Tables refreshed",
                      context={'database': self.current_db, 'count': len(tables)})

            except Exception as e:
                error_msg = f"Error fetching tables: {str(e)}"
                self.update_log(error_msg)
                error("Error fetching tables", context={'error': str(e)}, exc_info=True)

    def on_table_selected(self, sender, app_data):
        """Handle table selection"""
        if not app_data:
            return

        debug("Table selected", context={'table': app_data})
        self.current_table = app_data
        dpg.set_value("selected_table_name", f"Table: {app_data}")

        self.load_table_info()
        self.load_table_columns()

    @monitor_performance
    def load_table_info(self):
        """Load information about the selected table with caching"""
        if not self.current_table:
            return

        # Check cache first
        cache_key = f"{self.current_db}_{self.current_table}_info"
        if cache_key in self._table_info_cache:
            cached_info = self._table_info_cache[cache_key]
            dpg.set_value("table_rows_count", f"Estimated Rows: {cached_info:,}")
            debug("Table info loaded from cache", context={'table': self.current_table})
            return

        with operation("load_table_info", context={'table': self.current_table}):
            try:
                # Get row count estimate
                self.cursor.execute("""
                    SELECT reltuples::BIGINT as estimate 
                    FROM pg_class 
                    WHERE relname = %s
                """, (self.current_table,))

                result = self.cursor.fetchone()
                row_estimate = result['estimate'] if result else 0

                # Cache the result
                self._table_info_cache[cache_key] = row_estimate

                dpg.set_value("table_rows_count", f"Estimated Rows: {row_estimate:,}")
                debug("Table info loaded",
                      context={'table': self.current_table, 'estimated_rows': row_estimate})

            except Exception as e:
                error_msg = f"Error getting table info: {str(e)}"
                self.update_log(error_msg)
                error("Error loading table info",
                      context={'table': self.current_table, 'error': str(e)}, exc_info=True)

    @monitor_performance
    def load_table_columns(self):
        """Load columns for the selected table with caching"""
        if not self.current_table:
            return

        # Check cache first
        cache_key = f"{self.current_db}_{self.current_table}_columns"
        if cache_key in self._column_cache:
            columns = self._column_cache[cache_key]
            self._create_column_checkboxes(columns)
            debug("Table columns loaded from cache", context={'table': self.current_table})
            return

        with operation("load_table_columns", context={'table': self.current_table}):
            try:
                self.cursor.execute("""
                    SELECT column_name, data_type, is_nullable, column_default
                    FROM information_schema.columns 
                    WHERE table_name = %s 
                    ORDER BY ordinal_position
                """, (self.current_table,))

                columns = self.cursor.fetchall()
                self.table_columns = [col['column_name'] for col in columns]

                # Cache the result
                self._column_cache[cache_key] = columns

                dpg.set_value("table_columns_count", f"Columns: {len(self.table_columns)}")

                self._create_column_checkboxes(columns)

                # Select all by default
                self.selected_columns = self.table_columns.copy()

                debug("Table columns loaded",
                      context={'table': self.current_table, 'column_count': len(self.table_columns)})

            except Exception as e:
                error_msg = f"Error loading columns: {str(e)}"
                self.update_log(error_msg)
                error("Error loading table columns",
                      context={'table': self.current_table, 'error': str(e)}, exc_info=True)

    def _create_column_checkboxes(self, columns):
        """Create column checkboxes efficiently"""
        # Clear existing checkboxes
        dpg.delete_item("columns_checkboxes", children_only=True)

        with dpg.group(parent="columns_checkboxes"):
            for col in columns:
                col_name = col['column_name']
                col_type = col['data_type']
                dpg.add_checkbox(label=f"{col_name} ({col_type})",
                                 tag=f"col_check_{col_name}",
                                 default_value=True)

    def select_all_columns(self):
        """Select all columns"""
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}"):
                dpg.set_value(f"col_check_{col}", True)
        debug("All columns selected", context={'table': self.current_table})

    def clear_all_columns(self):
        """Clear all column selections"""
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}"):
                dpg.set_value(f"col_check_{col}", False)
        debug("All columns cleared", context={'table': self.current_table})

    def get_selected_columns(self):
        """Get list of currently selected columns"""
        selected = []
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}") and dpg.get_value(f"col_check_{col}"):
                selected.append(col)
        return selected

    @monitor_performance
    def load_table_data(self):
        """Load table data with pagination and performance optimizations"""
        if not self.current_table:
            self.update_log("No table selected")
            return

        def load_data_thread():
            with operation("load_table_data",
                           context={'table': self.current_table, 'page': self.current_page}):
                try:
                    self.is_loading = True
                    dpg.configure_item("loading_progress", show=True)
                    dpg.set_value("data_status", "Loading data...")

                    # Get selected columns
                    selected_cols = self.get_selected_columns()
                    if not selected_cols:
                        self.update_log("No columns selected")
                        return

                    self.selected_columns = selected_cols

                    # Safely quote column names and table name
                    quoted_columns = [f'"{col}"' for col in selected_cols]
                    columns_str = ", ".join(quoted_columns)
                    quoted_table = f'"{self.current_table}"'

                    # Build base query with proper quoting
                    base_query = f"SELECT {columns_str} FROM {quoted_table}"
                    count_query = f"SELECT COUNT(*) as total FROM {quoted_table}"

                    # Add WHERE clause if specified
                    where_clause = dpg.get_value("where_clause").strip()
                    if where_clause:
                        # Validate WHERE clause doesn't contain dangerous keywords
                        dangerous_keywords = ['DROP', 'DELETE', 'INSERT', 'UPDATE', 'TRUNCATE', 'ALTER']
                        where_upper = where_clause.upper()
                        for keyword in dangerous_keywords:
                            if keyword in where_upper:
                                error_msg = f"Dangerous keyword '{keyword}' not allowed in WHERE clause"
                                self.update_log(error_msg)
                                warning("Dangerous SQL keyword blocked",
                                        context={'keyword': keyword, 'where_clause': where_clause})
                                return

                        base_query += f" WHERE {where_clause}"
                        count_query += f" WHERE {where_clause}"

                    # Add ORDER BY if specified
                    order_by = dpg.get_value("order_by").strip()
                    if order_by:
                        base_query += f" ORDER BY {order_by}"

                    # Get total count first
                    self.cursor.execute(count_query)
                    self.total_rows = self.cursor.fetchone()['total']

                    # Calculate pagination
                    self.items_per_page = int(dpg.get_value("rows_per_page"))
                    if self.total_rows > 0:
                        self.total_pages = max(1, (self.total_rows + self.items_per_page - 1) // self.items_per_page)
                    else:
                        self.total_pages = 1

                    # Ensure current page is valid
                    if self.current_page > self.total_pages:
                        self.current_page = self.total_pages
                    elif self.current_page < 1:
                        self.current_page = 1

                    # Add LIMIT and OFFSET
                    offset = (self.current_page - 1) * self.items_per_page
                    data_query = f"{base_query} LIMIT %s OFFSET %s"

                    # Execute query with parameters
                    query_start_time = datetime.datetime.now()
                    self.cursor.execute(data_query, (self.items_per_page, offset))
                    self.table_data = self.cursor.fetchall()
                    query_duration = (datetime.datetime.now() - query_start_time).total_seconds()

                    # Update UI on main thread
                    self.update_data_table()
                    self.update_pagination_info()

                    dpg.set_value("data_status", f"Loaded {len(self.table_data)} rows")
                    dpg.set_value("last_query_time", datetime.datetime.now().strftime('%H:%M:%S'))
                    self.update_log(f"Loaded {len(self.table_data)} rows from {self.current_table}")

                    info("Table data loaded successfully",
                         context={
                             'table': self.current_table,
                             'rows_loaded': len(self.table_data),
                             'total_rows': self.total_rows,
                             'page': self.current_page,
                             'query_duration_ms': f"{query_duration * 1000:.1f}"
                         })

                except Exception as e:
                    error_msg = f"Error loading data: {str(e)}"
                    self.update_log(error_msg)
                    dpg.set_value("data_status", "Error loading data")
                    error("Error loading table data",
                          context={'table': self.current_table, 'error': str(e)}, exc_info=True)
                finally:
                    self.is_loading = False
                    dpg.configure_item("loading_progress", show=False)

        threading.Thread(target=load_data_thread, daemon=True).start()

    @monitor_performance
    def update_data_table(self):
        """Update the data table display with performance optimizations"""
        with operation("update_data_table"):
            # Clear existing table
            dpg.delete_item("data_table_container", children_only=True)

            if not self.table_data:
                dpg.add_text("No data to display", parent="data_table_container")
                return

            # Create new table with horizontal and vertical scrollbars
            with dpg.table(header_row=True, borders_innerH=True, borders_outerH=True,
                           borders_innerV=True, borders_outerV=True,
                           parent="data_table_container", scrollY=True, scrollX=True, height=400):

                # Add columns with fixed widths for better horizontal scrolling
                for col in self.selected_columns:
                    dpg.add_table_column(label=col, width_fixed=True, init_width_or_weight=150)

                # Add data rows with optimized value processing
                for row in self.table_data:
                    with dpg.table_row():
                        for col in self.selected_columns:
                            value = row.get(col, '')
                            display_value = self._format_cell_value(value)
                            dpg.add_text(display_value)

            debug("Data table updated",
                  context={'rows_displayed': len(self.table_data), 'columns': len(self.selected_columns)})

    def _format_cell_value(self, value):
        """Efficiently format cell values for display"""
        if value is None:
            return 'NULL'

        try:
            str_value = str(value)
            # Limit display length for very long values
            if len(str_value) > 100:
                return str_value[:97] + "..."
            return str_value
        except:
            return '<unconvertible>'

    def update_pagination_info(self):
        """Update pagination information"""
        dpg.set_value("page_info", f"Page: {self.current_page} of {self.total_pages}")
        dpg.set_value("total_rows_info", f"Total Rows: {self.total_rows:,}")
        dpg.set_value("goto_page", self.current_page)

    # Pagination methods with performance tracking
    def on_rows_per_page_changed(self, sender, app_data):
        """Handle rows per page change"""
        self.items_per_page = int(app_data)
        self.current_page = 1
        if self.current_table:
            debug("Rows per page changed", context={'new_value': self.items_per_page})
            self.load_table_data()

    def first_page(self):
        """Go to first page"""
        if self.current_page > 1:
            self.current_page = 1
            debug("Navigation: first page")
            self.load_table_data()

    def previous_page(self):
        """Go to previous page"""
        if self.current_page > 1:
            self.current_page -= 1
            debug("Navigation: previous page", context={'current_page': self.current_page})
            self.load_table_data()

    def next_page(self):
        """Go to next page"""
        if self.current_page < self.total_pages:
            self.current_page += 1
            debug("Navigation: next page", context={'current_page': self.current_page})
            self.load_table_data()

    def last_page(self):
        """Go to last page"""
        if self.current_page < self.total_pages:
            self.current_page = self.total_pages
            debug("Navigation: last page", context={'current_page': self.current_page})
            self.load_table_data()

    def goto_page(self):
        """Go to specific page"""
        try:
            page = dpg.get_value("goto_page")
            if 1 <= page <= self.total_pages:
                self.current_page = page
                debug("Navigation: goto page", context={'target_page': page})
                self.load_table_data()
            else:
                error_msg = f"Invalid page number. Must be between 1 and {self.total_pages}"
                self.update_log(error_msg)
                warning("Invalid page number",
                        context={'requested_page': page, 'max_pages': self.total_pages})
        except:
            self.update_log("Invalid page number")
            warning("Invalid page number format")

    # Utility methods with performance optimizations
    @monitor_performance
    def show_table_schema(self):
        """Show detailed table schema"""
        if not self.current_table:
            return

        with operation("show_table_schema", context={'table': self.current_table}):
            try:
                self.cursor.execute("""
                    SELECT column_name, data_type, character_maximum_length, 
                           is_nullable, column_default, numeric_precision, numeric_scale
                    FROM information_schema.columns 
                    WHERE table_name = %s 
                    ORDER BY ordinal_position
                """, (self.current_table,))

                schema_info = self.cursor.fetchall()
                schema_text = f"Schema for table '{self.current_table}':\n\n"

                for col in schema_info:
                    schema_text += f"• {col['column_name']} ({col['data_type']})"
                    if col['character_maximum_length']:
                        schema_text += f"({col['character_maximum_length']})"
                    if col['is_nullable'] == 'NO':
                        schema_text += " NOT NULL"
                    if col['column_default']:
                        schema_text += f" DEFAULT {col['column_default']}"
                    schema_text += "\n"

                self.update_log(schema_text)
                info("Table schema displayed",
                     context={'table': self.current_table, 'columns': len(schema_info)})

            except Exception as e:
                error_msg = f"Error getting schema: {str(e)}"
                self.update_log(error_msg)
                error("Error retrieving table schema",
                      context={'table': self.current_table, 'error': str(e)}, exc_info=True)

    @monitor_performance
    def show_table_stats(self):
        """Show basic table statistics"""
        if not self.current_table:
            return

        with operation("show_table_stats", context={'table': self.current_table}):
            try:
                # Get exact row count
                self.cursor.execute(f'SELECT COUNT(*) as exact_count FROM "{self.current_table}"')
                exact_count = self.cursor.fetchone()['exact_count']

                # Get table size
                self.cursor.execute("""
                    SELECT pg_size_pretty(pg_total_relation_size(%s)) as size
                """, (self.current_table,))
                table_size = self.cursor.fetchone()['size']

                stats_text = f"Statistics for '{self.current_table}':\n"
                stats_text += f"• Exact row count: {exact_count:,}\n"
                stats_text += f"• Table size: {table_size}\n"
                stats_text += f"• Columns: {len(self.table_columns)}"

                self.update_log(stats_text)
                info("Table statistics displayed",
                     context={
                         'table': self.current_table,
                         'exact_rows': exact_count,
                         'size': table_size,
                         'columns': len(self.table_columns)
                     })

            except Exception as e:
                error_msg = f"Error getting stats: {str(e)}"
                self.update_log(error_msg)
                error("Error retrieving table statistics",
                      context={'table': self.current_table, 'error': str(e)}, exc_info=True)

    @monitor_performance
    def export_to_csv(self):
        """Export current data view to CSV"""
        if not self.table_data:
            self.update_log("No data to export")
            return

        with operation("export_to_csv",
                       context={'table': self.current_table, 'rows': len(self.table_data)}):
            try:
                filename = f"{self.current_table}_{datetime.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

                with open(filename, 'w', newline='', encoding='utf-8') as csvfile:
                    writer = csv.DictWriter(csvfile, fieldnames=self.selected_columns)
                    writer.writeheader()

                    for row in self.table_data:
                        # Convert row to dict with only selected columns
                        row_dict = {}
                        for col in self.selected_columns:
                            value = row.get(col, '')
                            # Handle various data types safely
                            if value is None:
                                row_dict[col] = ''
                            else:
                                try:
                                    row_dict[col] = str(value)
                                except:
                                    row_dict[col] = '<unconvertible>'
                        writer.writerow(row_dict)

                self.update_log(f"Exported {len(self.table_data)} rows to {filename}")
                info("Data exported to CSV",
                     context={
                         'filename': filename,
                         'rows_exported': len(self.table_data),
                         'columns_exported': len(self.selected_columns)
                     })

            except Exception as e:
                error_msg = f"Export failed: {str(e)}"
                self.update_log(error_msg)
                error("CSV export failed", context={'error': str(e)}, exc_info=True)

    def execute_query(self):
        """Execute custom SQL query"""
        self.update_log("Custom query execution - feature available in Pro version")
        info("Custom query execution attempted")

    def update_log(self, message):
        """Update the connection log efficiently"""
        try:
            if dpg.does_item_exist("log_text"):
                current_log = dpg.get_value("log_text")
                new_log = f"{message}\n{current_log}"
                # Keep only last 10 lines for performance
                lines = new_log.split('\n')[:10]
                dpg.set_value("log_text", '\n'.join(lines))
        except:
            pass

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing - Bloomberg terminal has fixed layout for stability"""
        debug("Component resize requested - using fixed Bloomberg layout")

    @monitor_performance
    def cleanup(self):
        """Clean up database connections and caches"""
        with operation("database_tab_cleanup"):
            try:
                # Close database connections
                if self.cursor:
                    self.cursor.close()
                if self.connection:
                    self.connection.close()

                # Clear performance caches
                self._column_cache.clear()
                self._table_info_cache.clear()

                # Clear data structures
                self.table_data.clear()
                self.table_columns.clear()
                self.selected_columns.clear()

                info("Database tab cleaned up successfully",
                     context={'cache_items_cleared': len(self._column_cache) + len(self._table_info_cache)})

            except Exception as e:
                error("Error during database cleanup", context={'error': str(e)}, exc_info=True)