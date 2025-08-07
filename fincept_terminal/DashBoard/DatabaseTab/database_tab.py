# -*- coding: utf-8 -*-
"""
Database Tab module for Fincept Terminal
Updated to use centralized logging system
"""


import dearpygui.dearpygui as dpg
from fincept_terminal.Utils.base_tab import BaseTab
import psycopg2
import psycopg2.extras
import threading
import traceback
import datetime
import csv

from fincept_terminal.Utils.Logging.logger import logger, log_operation

class DatabaseTab(BaseTab):
    """Bloomberg Terminal style PostgreSQL Database Management Tab with exact original functionality"""

    def __init__(self, main_app=None):
        super().__init__(main_app)
        self.main_app = main_app

        # Bloomberg color scheme
        self.BLOOMBERG_ORANGE = [255, 165, 0]
        self.BLOOMBERG_WHITE = [255, 255, 255]
        self.BLOOMBERG_RED = [255, 0, 0]
        self.BLOOMBERG_GREEN = [0, 200, 0]
        self.BLOOMBERG_YELLOW = [255, 255, 0]
        self.BLOOMBERG_GRAY = [120, 120, 120]
        self.BLOOMBERG_BLUE = [100, 150, 250]
        self.BLOOMBERG_BLACK = [0, 0, 0]

        # Database connection variables - EXACT same as original
        self.connection = None
        self.cursor = None
        self.current_db = None
        self.current_table = None
        self.current_page = 1
        self.items_per_page = 100  # Ensure this is an integer
        self.total_rows = 0
        self.total_pages = 0
        self.table_columns = []
        self.selected_columns = []
        self.table_data = []
        self.is_loading = False

        # Connection settings - EXACT same as original
        self.connection_settings = {
            'host': 'localhost',
            'port': '5432',
            'user': 'postgres',
            'password': '',
            'database': 'postgres'
        }

    def get_label(self):
        return "Database"

    def create_content(self):
        """Create Bloomberg-style database terminal layout with exact original functionality"""
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
                dpg.add_button(label="F1:CONNECT", width=120, height=25, callback=self.connect_to_postgres)
                dpg.add_button(label="F2:TABLES", width=120, height=25, callback=self.refresh_tables)
                dpg.add_button(label="F3:QUERY", width=120, height=25, callback=self.load_table_data)
                dpg.add_button(label="F4:EXPORT", width=120, height=25, callback=self.export_to_csv)
                dpg.add_button(label="F5:SCHEMA", width=120, height=25, callback=self.show_table_schema)
                dpg.add_button(label="F6:STATS", width=120, height=25, callback=self.show_table_stats)

            dpg.add_separator()

            # Main database layout - EXACT same structure as original
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

        except Exception as e:
            logger.error(f"Error creating database content: {e}", module="Database_Tab", context={'e': e})
            # Fallback content
            dpg.add_text("DATABASE TERMINAL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text("Error loading database interface. Please try again.")

    def create_left_db_panel(self):
        """Create left database connection and navigation panel - EXACT same as original"""
        with dpg.child_window(width=400, height=650, border=True):
            dpg.add_text("DATABASE CONNECTION", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Connection settings - EXACT same as original
            dpg.add_text("Connection Settings", color=self.BLOOMBERG_YELLOW)
            dpg.add_input_text(label="Host", default_value=self.connection_settings['host'],
                               tag="db_host", width=200)
            dpg.add_input_text(label="Port", default_value=self.connection_settings['port'],
                               tag="db_port", width=200)
            dpg.add_input_text(label="Username", default_value=self.connection_settings['user'],
                               tag="db_user", width=200)
            dpg.add_input_text(label="Password", default_value=self.connection_settings['password'],
                               tag="db_password", password=True, width=200)

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_button(label="Connect", callback=self.connect_to_postgres, width=90)
                dpg.add_button(label="Disconnect", callback=self.disconnect_from_postgres, width=90)
                dpg.add_button(label="Test", callback=self.test_connection, width=90)

            dpg.add_separator()

            # Connection status - EXACT same as original
            dpg.add_text("Connection Status", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Status: Disconnected", tag="connection_status_text", color=self.BLOOMBERG_RED)
            dpg.add_text("Server: Not connected", tag="server_info")
            dpg.add_text("Database: None", tag="current_db_info")
            dpg.add_text("Total Tables: 0", tag="tables_count")

            dpg.add_spacer(height=10)
            with dpg.child_window(height=80, tag="connection_log", border=True):
                dpg.add_text("Ready to connect...", tag="log_text")

            dpg.add_separator()

            # Database and table selection - EXACT same as original
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
        """Create center data viewer panel - EXACT same as original"""
        with dpg.child_window(width=700, height=650, border=True):
            dpg.add_text("DATA VIEWER", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Table information - EXACT same as original
            dpg.add_text("Table Information", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Table: None", tag="selected_table_name")
            dpg.add_text("Columns: 0", tag="table_columns_count")
            dpg.add_text("Estimated Rows: 0", tag="table_rows_count")
            dpg.add_spacer(height=10)
            dpg.add_button(label="Table Schema", callback=self.show_table_schema, width=-1)
            dpg.add_button(label="Quick Stats", callback=self.show_table_stats, width=-1)

            dpg.add_separator()

            # Data status
            dpg.add_text("Select a table and load data to view", tag="data_status", color=self.BLOOMBERG_GRAY)

            # Data table container - EXACT same as original with horizontal scroll
            with dpg.child_window(height=450, border=True, tag="data_table_container", horizontal_scrollbar=True):
                dpg.add_text("No data loaded", color=self.BLOOMBERG_GRAY)

    def create_right_db_panel(self):
        """Create right controls and info panel - EXACT same as original"""
        with dpg.child_window(width=500, height=650, border=True):
            dpg.add_text("DATA CONTROLS", color=self.BLOOMBERG_ORANGE)
            dpg.add_separator()

            # Column selection - EXACT same as original
            dpg.add_text("Select Columns to View", color=self.BLOOMBERG_YELLOW)
            with dpg.group(horizontal=True):
                dpg.add_button(label="Select All", callback=self.select_all_columns, width=90)
                dpg.add_button(label="Clear All", callback=self.clear_all_columns, width=90)

            dpg.add_spacer(height=5)
            with dpg.child_window(height=150, tag="columns_checkboxes", border=True):
                dpg.add_text("Select a table first...")

            dpg.add_separator()

            # Query controls - EXACT same as original
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
            dpg.add_button(label="Load Data", callback=self.load_table_data, width=-1)
            dpg.add_button(label="Export CSV", callback=self.export_to_csv, width=-1)

            dpg.add_separator()

            # Pagination controls - EXACT same as original
            dpg.add_text("Pagination", color=self.BLOOMBERG_YELLOW)
            dpg.add_text("Page: 0 of 0", tag="page_info")
            dpg.add_text("Total Rows: 0", tag="total_rows_info")

            dpg.add_spacer(height=10)
            with dpg.group(horizontal=True):
                dpg.add_button(label="First", callback=self.first_page, width=60)
                dpg.add_button(label="Prev", callback=self.previous_page, width=60)
                dpg.add_button(label="Next", callback=self.next_page, width=60)
                dpg.add_button(label="Last", callback=self.last_page, width=60)

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
            dpg.add_text("DATABASE STATUS:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("DISCONNECTED", color=self.BLOOMBERG_RED, tag="db_status")
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("CONNECTION TYPE:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("POSTGRESQL", color=self.BLOOMBERG_ORANGE)
            dpg.add_text(" | ", color=self.BLOOMBERG_GRAY)
            dpg.add_text("LAST QUERY:", color=self.BLOOMBERG_GRAY)
            dpg.add_text("NONE", color=self.BLOOMBERG_WHITE, tag="last_query_time")

    # Database connection methods - EXACT same as original
    def connect_to_postgres(self):
        """Connect to PostgreSQL database - EXACT same as original"""

        def connect_thread():
            try:
                self.update_log("Connecting to PostgreSQL...")
                dpg.configure_item("loading_progress", show=True)

                # Get connection settings from UI
                host = dpg.get_value("db_host")
                port = dpg.get_value("db_port")
                user = dpg.get_value("db_user")
                password = dpg.get_value("db_password")

                # Create connection
                self.connection = psycopg2.connect(
                    host=host,
                    port=port,
                    user=user,
                    password=password,
                    database='postgres'  # Connect to default postgres db first
                )

                self.cursor = self.connection.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

                # Update UI on main thread
                dpg.set_value("connection_status_text", "Status: Connected")
                dpg.configure_item("connection_status_text", color=self.BLOOMBERG_GREEN)
                dpg.set_value("server_info", f"Server: {host}:{port}")
                dpg.set_value("db_status", "CONNECTED")
                dpg.configure_item("db_status", color=self.BLOOMBERG_GREEN)

                self.update_log("Connected successfully!")
                self.refresh_databases()

            except Exception as e:
                error_msg = f"Connection failed: {str(e)}"
                self.update_log(error_msg)
                dpg.set_value("connection_status_text", "Status: Connection Failed")
                dpg.configure_item("connection_status_text", color=self.BLOOMBERG_RED)
            finally:
                dpg.configure_item("loading_progress", show=False)

        threading.Thread(target=connect_thread, daemon=True).start()

    def disconnect_from_postgres(self):
        """Disconnect from PostgreSQL - EXACT same as original"""
        try:
            if self.cursor:
                self.cursor.close()
            if self.connection:
                self.connection.close()

            self.connection = None
            self.cursor = None
            self.current_db = None
            self.current_table = None

            dpg.set_value("connection_status_text", "Status: Disconnected")
            dpg.configure_item("connection_status_text", color=self.BLOOMBERG_RED)
            dpg.set_value("server_info", "Server: Not connected")
            dpg.set_value("current_db_info", "Database: None")
            dpg.set_value("tables_count", "Total Tables: 0")
            dpg.set_value("db_status", "DISCONNECTED")
            dpg.configure_item("db_status", color=self.BLOOMBERG_RED)

            # Clear combos
            dpg.configure_item("database_combo", items=[])
            dpg.configure_item("table_combo", items=[])

            self.update_log("Disconnected from database")

        except Exception as e:
            self.update_log(f"Error during disconnect: {str(e)}")

    def test_connection(self):
        """Test database connection - EXACT same as original"""

        def test_thread():
            try:
                self.update_log("Testing connection...")

                host = dpg.get_value("db_host")
                port = dpg.get_value("db_port")
                user = dpg.get_value("db_user")
                password = dpg.get_value("db_password")

                test_conn = psycopg2.connect(
                    host=host,
                    port=port,
                    user=user,
                    password=password,
                    database='postgres'
                )
                test_conn.close()

                self.update_log("Connection test successful!")

            except Exception as e:
                self.update_log(f"Connection test failed: {str(e)}")

        threading.Thread(target=test_thread, daemon=True).start()

    def refresh_databases(self):
        """Refresh the list of available databases - EXACT same as original"""
        if not self.connection:
            self.update_log("Not connected to database")
            return

        try:
            self.cursor.execute("""
                SELECT datname FROM pg_database 
                WHERE datistemplate = false 
                ORDER BY datname
            """)

            databases = [row['datname'] for row in self.cursor.fetchall()]
            dpg.configure_item("database_combo", items=databases)

            self.update_log(f"Found {len(databases)} databases")

        except Exception as e:
            self.update_log(f"Error fetching databases: {str(e)}")

    def on_database_selected(self, sender, app_data):
        """Handle database selection - EXACT same as original"""
        if not app_data:
            return

        try:
            # Reconnect to selected database
            if self.connection:
                self.connection.close()

            host = dpg.get_value("db_host")
            port = dpg.get_value("db_port")
            user = dpg.get_value("db_user")
            password = dpg.get_value("db_password")

            self.connection = psycopg2.connect(
                host=host,
                port=port,
                user=user,
                password=password,
                database=app_data
            )

            self.cursor = self.connection.cursor(cursor_factory=psycopg2.extras.RealDictCursor)
            self.current_db = app_data

            dpg.set_value("current_db_info", f"Database: {app_data}")
            self.update_log(f"Connected to database: {app_data}")

            self.refresh_tables()

        except Exception as e:
            self.update_log(f"Error connecting to database {app_data}: {str(e)}")

    def refresh_tables(self):
        """Refresh the list of tables in current database - EXACT same as original"""
        if not self.connection or not self.current_db:
            return

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

        except Exception as e:
            self.update_log(f"Error fetching tables: {str(e)}")

    def on_table_selected(self, sender, app_data):
        """Handle table selection - EXACT same as original"""
        if not app_data:
            return

        self.current_table = app_data
        dpg.set_value("selected_table_name", f"Table: {app_data}")

        self.load_table_info()
        self.load_table_columns()

    def load_table_info(self):
        """Load information about the selected table - EXACT same as original"""
        if not self.current_table:
            return

        try:
            # Get row count estimate
            self.cursor.execute(f"""
                SELECT reltuples::BIGINT as estimate 
                FROM pg_class 
                WHERE relname = %s
            """, (self.current_table,))
            result = self.cursor.fetchone()
            row_estimate = result['estimate'] if result else 0

            dpg.set_value("table_rows_count", f"Estimated Rows: {row_estimate:,}")

        except Exception as e:
            self.update_log(f"Error getting table info: {str(e)}")

    def load_table_columns(self):
        """Load columns for the selected table - EXACT same as original"""
        if not self.current_table:
            return

        try:
            self.cursor.execute("""
                SELECT column_name, data_type, is_nullable, column_default
                FROM information_schema.columns 
                WHERE table_name = %s 
                ORDER BY ordinal_position
            """, (self.current_table,))

            columns = self.cursor.fetchall()
            self.table_columns = [col['column_name'] for col in columns]

            dpg.set_value("table_columns_count", f"Columns: {len(self.table_columns)}")

            # Create column checkboxes
            dpg.delete_item("columns_checkboxes", children_only=True)

            with dpg.group(parent="columns_checkboxes"):
                for col in columns:
                    col_name = col['column_name']
                    col_type = col['data_type']
                    dpg.add_checkbox(label=f"{col_name} ({col_type})",
                                     tag=f"col_check_{col_name}",
                                     default_value=True)

            # Select all by default
            self.selected_columns = self.table_columns.copy()

        except Exception as e:
            self.update_log(f"Error loading columns: {str(e)}")

    def select_all_columns(self):
        """Select all columns - EXACT same as original"""
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}"):
                dpg.set_value(f"col_check_{col}", True)

    def clear_all_columns(self):
        """Clear all column selections - EXACT same as original"""
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}"):
                dpg.set_value(f"col_check_{col}", False)

    def get_selected_columns(self):
        """Get list of currently selected columns - EXACT same as original"""
        selected = []
        for col in self.table_columns:
            if dpg.does_item_exist(f"col_check_{col}") and dpg.get_value(f"col_check_{col}"):
                selected.append(col)
        return selected

    def load_table_data(self):
        """Load table data with pagination - EXACT same as original with FIXED VERSION"""
        if not self.current_table:
            self.update_log("No table selected")
            return

        def load_data_thread():
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
                            self.update_log(f"Dangerous keyword '{keyword}' not allowed in WHERE clause")
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

                # Calculate pagination - FIX: Convert to int
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
                self.cursor.execute(data_query, (self.items_per_page, offset))
                self.table_data = self.cursor.fetchall()

                # Update UI on main thread
                self.update_data_table()
                self.update_pagination_info()

                dpg.set_value("data_status", f"Loaded {len(self.table_data)} rows")
                dpg.set_value("last_query_time", datetime.datetime.now().strftime('%H:%M:%S'))
                self.update_log(f"Loaded {len(self.table_data)} rows from {self.current_table}")

            except Exception as e:
                error_msg = f"Error loading data: {str(e)}"
                logger.error("Debug - Full error: {traceback.format_exc()}", module="Database_Tab")  # Debug info
                self.update_log(error_msg)
                dpg.set_value("data_status", "Error loading data")
            finally:
                self.is_loading = False
                dpg.configure_item("loading_progress", show=False)

        threading.Thread(target=load_data_thread, daemon=True).start()

    def update_data_table(self):
        """Update the data table display - EXACT same as original"""
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

            # Add data rows
            for row in self.table_data:
                with dpg.table_row():
                    for col in self.selected_columns:
                        value = row.get(col, '')
                        # Convert to string and handle None values
                        if value is None:
                            display_value = 'NULL'
                        else:
                            try:
                                display_value = str(value)
                            except:
                                display_value = '<unconvertible>'

                        # Limit display length for very long values
                        if len(display_value) > 100:
                            display_value = display_value[:97] + "..."
                        dpg.add_text(display_value)

    def update_pagination_info(self):
        """Update pagination information - EXACT same as original"""
        dpg.set_value("page_info", f"Page: {self.current_page} of {self.total_pages}")
        dpg.set_value("total_rows_info", f"Total Rows: {self.total_rows:,}")
        dpg.set_value("goto_page", self.current_page)

    # Pagination methods - EXACT same as original
    def on_rows_per_page_changed(self, sender, app_data):
        """Handle rows per page change - EXACT same as original"""
        self.items_per_page = int(app_data)  # Convert to int
        self.current_page = 1
        if self.current_table:
            self.load_table_data()

    def first_page(self):
        """Go to first page - EXACT same as original"""
        if self.current_page > 1:
            self.current_page = 1
            self.load_table_data()

    def previous_page(self):
        """Go to previous page - EXACT same as original"""
        if self.current_page > 1:
            self.current_page -= 1
            self.load_table_data()

    def next_page(self):
        """Go to next page - EXACT same as original"""
        if self.current_page < self.total_pages:
            self.current_page += 1
            self.load_table_data()

    def last_page(self):
        """Go to last page - EXACT same as original"""
        if self.current_page < self.total_pages:
            self.current_page = self.total_pages
            self.load_table_data()

    def goto_page(self):
        """Go to specific page - EXACT same as original"""
        try:
            page = dpg.get_value("goto_page")
            if 1 <= page <= self.total_pages:
                self.current_page = page
                self.load_table_data()
            else:
                self.update_log(f"Invalid page number. Must be between 1 and {self.total_pages}")
        except:
            self.update_log("Invalid page number")

    # Utility methods - EXACT same as original
    def show_table_schema(self):
        """Show detailed table schema - EXACT same as original"""
        if not self.current_table:
            return

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

        except Exception as e:
            self.update_log(f"Error getting schema: {str(e)}")

    def show_table_stats(self):
        """Show basic table statistics - EXACT same as original"""
        if not self.current_table:
            return

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

        except Exception as e:
            self.update_log(f"Error getting stats: {str(e)}")

    def export_to_csv(self):
        """Export current data view to CSV - EXACT same as original"""
        if not self.table_data:
            self.update_log("No data to export")
            return

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

        except Exception as e:
            self.update_log(f"Export failed: {str(e)}")

    def execute_query(self):
        """Execute custom SQL query"""
        self.update_log("Custom query execution - feature available in Pro version")

    def update_log(self, message):
        """Update the connection log - EXACT same as original"""
        try:
            if dpg.does_item_exist("log_text"):
                current_log = dpg.get_value("log_text")
                new_log = f"{message}\n{current_log}"
                # Keep only last 10 lines
                lines = new_log.split('\n')[:10]
                dpg.set_value("log_text", '\n'.join(lines))
        except:
            pass

    def resize_components(self, left_width, center_width, right_width, top_height, bottom_height, cell_height):
        """Handle component resizing"""
        # Bloomberg terminal has fixed layout for stability
        pass

    def cleanup(self):
        """Clean up database connections - EXACT same as original"""
        try:
            if self.cursor:
                self.cursor.close()
            if self.connection:
                self.connection.close()
            logger.info("Database connections cleaned up", module="Database_Tab")
        except:
            pass