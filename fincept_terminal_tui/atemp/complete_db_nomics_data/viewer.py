import dearpygui.dearpygui as dpg
import sqlite3
import os
from tkinter import filedialog
import json


class DatabaseViewer:
    def __init__(self):
        self.db_path = None
        self.connection = None
        self.tables = []

    def browse_db_file(self):
        # Hide DearPyGui window temporarily to show file dialog
        dpg.hide_viewport()

        file_path = filedialog.askopenfilename(
            title="Select Database File",
            filetypes=[
                ("SQLite Database", "*.db"),
                ("SQLite Database", "*.sqlite"),
                ("SQLite Database", "*.sqlite3"),
                ("All Files", "*.*")
            ]
        )

        # Show DearPyGui window again
        dpg.show_viewport()

        if file_path:
            self.load_database(file_path)

    def load_database(self, db_path):
        try:
            if self.connection:
                self.connection.close()

            self.db_path = db_path
            self.connection = sqlite3.connect(db_path)

            # Update UI
            dpg.set_value("db_path_text", f"Database: {os.path.basename(db_path)}")
            dpg.set_value("status_text", "✅ Database loaded successfully!")

            # Load tables
            self.load_tables()

        except Exception as e:
            dpg.set_value("status_text", f"❌ Error: {str(e)}")

    def load_tables(self):
        if not self.connection:
            return

        try:
            cursor = self.connection.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
            self.tables = [row[0] for row in cursor.fetchall()]

            # Clear and populate table list
            dpg.delete_item("table_list", children_only=True)

            if self.tables:
                for table in self.tables:
                    dpg.add_selectable(label=table, parent="table_list",
                                       callback=lambda s, a, u: self.on_table_select(u),
                                       user_data=table)
                dpg.set_value("status_text", f"✅ Found {len(self.tables)} tables")
            else:
                dpg.add_text("No tables found", parent="table_list", color=(200, 200, 100))

        except Exception as e:
            dpg.set_value("status_text", f"❌ Error loading tables: {str(e)}")

    def on_table_select(self, table_name):
        self.show_table_info(table_name)
        self.show_table_data(table_name)

    def show_table_info(self, table_name):
        if not self.connection:
            return

        try:
            cursor = self.connection.cursor()

            # Get table schema
            cursor.execute(f"PRAGMA table_info({table_name});")
            columns = cursor.fetchall()

            # Get row count
            cursor.execute(f"SELECT COUNT(*) FROM {table_name};")
            row_count = cursor.fetchone()[0]

            # Clear and populate table info
            dpg.delete_item("table_info", children_only=True)

            dpg.add_text(f"📊 Table: {table_name}", parent="table_info", color=(100, 255, 100))
            dpg.add_text(f"📈 Rows: {row_count:,}", parent="table_info", color=(100, 200, 255))
            dpg.add_text(f"📋 Columns: {len(columns)}", parent="table_info", color=(100, 200, 255))
            dpg.add_separator(parent="table_info")

            dpg.add_text("🗂️ Column Structure:", parent="table_info", color=(255, 200, 100))

            for col in columns:
                col_id, col_name, col_type, not_null, default_val, primary_key = col

                # Format column info
                pk_indicator = " 🔑" if primary_key else ""
                null_indicator = " (NOT NULL)" if not_null else ""
                default_indicator = f" DEFAULT: {default_val}" if default_val else ""

                col_text = f"  • {col_name} ({col_type}){pk_indicator}{null_indicator}{default_indicator}"
                dpg.add_text(col_text, parent="table_info", color=(200, 200, 200))

        except Exception as e:
            dpg.delete_item("table_info", children_only=True)
            dpg.add_text(f"❌ Error: {str(e)}", parent="table_info", color=(255, 100, 100))

    def show_table_data(self, table_name, limit=100):
        if not self.connection:
            return

        try:
            cursor = self.connection.cursor()
            cursor.execute(f"SELECT * FROM {table_name} LIMIT {limit};")
            rows = cursor.fetchall()

            # Get column names
            cursor.execute(f"PRAGMA table_info({table_name});")
            columns = [col[1] for col in cursor.fetchall()]

            # Clear table data view
            dpg.delete_item("table_data", children_only=True)

            if rows:
                dpg.add_text(f"📋 Data Preview (First {len(rows)} rows):",
                             parent="table_data", color=(255, 200, 100))
                dpg.add_separator(parent="table_data")

                # Create table headers
                header_text = " | ".join([f"{col:15}" for col in columns])
                dpg.add_text(header_text, parent="table_data", color=(100, 255, 255))
                dpg.add_text("-" * len(header_text), parent="table_data", color=(100, 100, 100))

                # Show data rows
                for i, row in enumerate(rows):
                    if i >= 50:  # Limit display for performance
                        dpg.add_text(f"... and {len(rows) - 50} more rows",
                                     parent="table_data", color=(200, 200, 100))
                        break

                    row_text = " | ".join([f"{str(cell)[:15]:15}" for cell in row])
                    dpg.add_text(row_text, parent="table_data", color=(200, 200, 200))
            else:
                dpg.add_text("📭 No data found in table", parent="table_data", color=(200, 200, 100))

        except Exception as e:
            dpg.delete_item("table_data", children_only=True)
            dpg.add_text(f"❌ Error: {str(e)}", parent="table_data", color=(255, 100, 100))

    def export_table_data(self):
        if not self.connection or not hasattr(self, 'current_table'):
            dpg.set_value("status_text", "❌ No table selected")
            return

        try:
            cursor = self.connection.cursor()
            cursor.execute(f"SELECT * FROM {self.current_table};")
            rows = cursor.fetchall()

            # Get column names
            cursor.execute(f"PRAGMA table_info({self.current_table});")
            columns = [col[1] for col in cursor.fetchall()]

            # Convert to JSON
            data = []
            for row in rows:
                row_dict = dict(zip(columns, row))
                data.append(row_dict)

            # Save to file
            filename = f"{self.current_table}_export.json"
            with open(filename, 'w') as f:
                json.dump(data, f, indent=2, default=str)

            dpg.set_value("status_text", f"✅ Exported {len(data)} rows to {filename}")

        except Exception as e:
            dpg.set_value("status_text", f"❌ Export error: {str(e)}")

    def execute_custom_query(self):
        if not self.connection:
            dpg.set_value("status_text", "❌ No database loaded")
            return

        query = dpg.get_value("custom_query")
        if not query.strip():
            return

        try:
            cursor = self.connection.cursor()
            cursor.execute(query)

            if query.strip().upper().startswith('SELECT'):
                results = cursor.fetchall()

                # Clear and show results
                dpg.delete_item("query_results", children_only=True)

                if results:
                    dpg.add_text(f"📊 Query Results ({len(results)} rows):",
                                 parent="query_results", color=(100, 255, 100))

                    # Show column names if available
                    if cursor.description:
                        columns = [desc[0] for desc in cursor.description]
                        header_text = " | ".join([f"{col:15}" for col in columns])
                        dpg.add_text(header_text, parent="query_results", color=(100, 255, 255))
                        dpg.add_text("-" * len(header_text), parent="query_results", color=(100, 100, 100))

                    # Show results
                    for i, row in enumerate(results[:50]):  # Limit to 50 rows
                        row_text = " | ".join([f"{str(cell)[:15]:15}" for cell in row])
                        dpg.add_text(row_text, parent="query_results", color=(200, 200, 200))

                    if len(results) > 50:
                        dpg.add_text(f"... and {len(results) - 50} more rows",
                                     parent="query_results", color=(200, 200, 100))
                else:
                    dpg.add_text("📭 No results", parent="query_results", color=(200, 200, 100))
            else:
                self.connection.commit()
                dpg.add_text("✅ Query executed successfully", parent="query_results", color=(100, 255, 100))

            dpg.set_value("status_text", "✅ Query executed")

        except Exception as e:
            dpg.delete_item("query_results", children_only=True)
            dpg.add_text(f"❌ Error: {str(e)}", parent="query_results", color=(255, 100, 100))
            dpg.set_value("status_text", f"❌ Query error: {str(e)}")


# Initialize the database viewer
db_viewer = DatabaseViewer()

# Create DearPyGui interface
dpg.create_context()

with dpg.window(label="🗃️ Database Viewer", width=1200, height=800):
    dpg.add_text("🗃️ SQLite Database Inspector & Viewer", color=(100, 255, 100))
    dpg.add_separator()

    # File selection section
    with dpg.group(horizontal=True):
        dpg.add_button(label="📁 Browse Database File", callback=db_viewer.browse_db_file)
        dpg.add_text("No database loaded", tag="db_path_text", color=(200, 200, 100))

    dpg.add_text("Ready to load database...", tag="status_text", color=(100, 200, 255))
    dpg.add_separator()

    # Main content area
    with dpg.group(horizontal=True):
        # Left panel - Tables list
        with dpg.group():
            dpg.add_text("📋 Tables:", color=(255, 200, 100))
            with dpg.child_window(tag="table_list", width=200, height=300, border=True):
                dpg.add_text("Load a database to see tables", color=(150, 150, 150))

        dpg.add_spacer(width=20)

        # Middle panel - Table info
        with dpg.group():
            dpg.add_text("ℹ️ Table Information:", color=(255, 200, 100))
            with dpg.child_window(tag="table_info", width=350, height=300, border=True):
                dpg.add_text("Select a table to see details", color=(150, 150, 150))

        dpg.add_spacer(width=20)

        # Right panel - Table data
        with dpg.group():
            dpg.add_text("📊 Table Data:", color=(255, 200, 100))
            with dpg.child_window(tag="table_data", width=400, height=300, border=True):
                dpg.add_text("Select a table to preview data", color=(150, 150, 150))

    dpg.add_separator()

    # Custom query section
    dpg.add_text("🔍 Custom SQL Query:", color=(255, 200, 100))
    dpg.add_input_text(tag="custom_query", multiline=True, height=60, width=800,
                       hint="Enter SQL query (e.g., SELECT * FROM table_name LIMIT 10)")

    with dpg.group(horizontal=True):
        dpg.add_button(label="▶️ Execute Query", callback=db_viewer.execute_custom_query)
        dpg.add_button(label="💾 Export Current Table", callback=db_viewer.export_table_data)

    dpg.add_separator()

    dpg.add_text("📊 Query Results:", color=(255, 200, 100))
    with dpg.child_window(tag="query_results", height=200, border=True):
        dpg.add_text("Execute a query to see results here", color=(150, 150, 150))

# Sample databases info
with dpg.window(label="📖 Help & Examples", width=400, height=300, pos=(1220, 50)):
    dpg.add_text("🔧 Supported Database Types:", color=(100, 255, 100))
    dpg.add_text("• SQLite (.db, .sqlite, .sqlite3)", color=(200, 200, 200))
    dpg.add_separator()

    dpg.add_text("📝 Sample Queries:", color=(100, 255, 100))
    dpg.add_text("• SELECT * FROM table_name;", color=(200, 200, 200))
    dpg.add_text("• SELECT COUNT(*) FROM table_name;", color=(200, 200, 200))
    dpg.add_text("• PRAGMA table_info(table_name);", color=(200, 200, 200))
    dpg.add_text("• SELECT name FROM sqlite_master;", color=(200, 200, 200))
    dpg.add_separator()

    dpg.add_text("💡 Features:", color=(100, 255, 100))
    dpg.add_text("• Browse and select DB files", color=(200, 200, 200))
    dpg.add_text("• View table structure", color=(200, 200, 200))
    dpg.add_text("• Preview table data", color=(200, 200, 200))
    dpg.add_text("• Execute custom queries", color=(200, 200, 200))
    dpg.add_text("• Export data to JSON", color=(200, 200, 200))

dpg.create_viewport(title="🗃️ Database Viewer - SQLite Inspector", width=1220, height=820)
dpg.setup_dearpygui()
dpg.show_viewport()
dpg.start_dearpygui()
dpg.destroy_context()

# Clean up database connection
if db_viewer.connection:
    db_viewer.connection.close()