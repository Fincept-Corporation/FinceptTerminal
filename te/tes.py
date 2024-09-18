import psycopg2
import requests
import csv
import json

# PostgreSQL connection setup
conn = psycopg2.connect(
    dbname="data",
    user="postgres",
    password="12Tilak34##",
    host="localhost",
    port="5432"
)
cur = conn.cursor()

# Reserved SQL keywords that need to be escaped
reserved_keywords = ['desc', 'source', 'limit', 'offset', 'from', 'select', 'where', 'group', 'order']


# Helper function to determine PostgreSQL data type based on Python types
def determine_type(value):
    if isinstance(value, int):
        if -32768 <= value <= 32767:
            return 'SMALLINT'
        elif -2147483648 <= value <= 2147483647:
            return 'INTEGER'
        else:
            return 'BIGINT'
    elif isinstance(value, float):
        return 'FLOAT'
    elif isinstance(value, bool):
        return 'BOOLEAN'
    elif isinstance(value, str):
        return 'TEXT'
    elif isinstance(value, list):
        if all(isinstance(item, (int, float, str, bool)) for item in value):
            return 'TEXT[]'
        return 'JSONB'
    elif isinstance(value, dict):
        return 'JSONB'
    else:
        return 'TEXT'


# Function to safely escape reserved SQL keywords
def escape_column_name(column_name):
    if column_name.lower() in reserved_keywords:
        return f'"{column_name}"'
    return column_name


# Function to quote table names if they contain special characters
def escape_table_name(table_name):
    if "-" in table_name:
        return f'"{table_name}"'
    return table_name


# Function to create a table based on JSON structure
def create_table_if_not_exists(table_name, json_data):
    table_name = escape_table_name(table_name)
    columns = ["id SERIAL PRIMARY KEY"]
    for key, value in json_data.items():
        column_name = escape_column_name(key)
        data_type = determine_type(value)
        columns.append(f"{column_name} {data_type}")

    create_query = f"CREATE TABLE IF NOT EXISTS {table_name} ({', '.join(columns)});"
    print(f"Creating table if not exists: {table_name}\nQuery: {create_query}")
    cur.execute(create_query)
    conn.commit()


# Function to update table structure based on new JSON fields
def update_table_structure(table_name, json_data):
    table_name = escape_table_name(table_name)

    # Fetching existing columns properly
    existing_columns_query = f"SELECT column_name FROM information_schema.columns WHERE table_schema = 'public' AND table_name = '{table_name.strip('\"')}';"
    cur.execute(existing_columns_query)
    existing_columns = [row[0].lower() for row in cur.fetchall()]
    print(f"Existing columns in {table_name}: {existing_columns}")

    columns_to_add = []
    for key, value in json_data.items():
        column_name = escape_column_name(key).strip('"').lower()
        data_type = determine_type(value)

        if column_name not in existing_columns:
            columns_to_add.append(f"ADD COLUMN {escape_column_name(key)} {data_type}")
        else:
            print(f"Column {column_name} already exists in {table_name}. Skipping addition.")

    if columns_to_add:
        alter_query = f"ALTER TABLE {table_name} {', '.join(columns_to_add)};"
        print(f"Altering table {table_name} with query: {alter_query}")
        cur.execute(alter_query)
        conn.commit()
    else:
        print(f"No columns to add for table {table_name}")


# Function to insert data into the table
def insert_data(table_name, json_data):
    table_name = escape_table_name(table_name)
    columns = [escape_column_name(col) for col in json_data.keys()]
    values = []

    for col in json_data.keys():
        value = json_data[col]
        if isinstance(value, list) and determine_type(value) in ['TEXT[]', 'INTEGER[]', 'FLOAT[]']:
            values.append(convert_to_postgres_array(value))
        elif isinstance(value, (dict, list)):
            values.append(json.dumps(value))
        else:
            values.append(value)

    placeholders = ', '.join(['%s'] * len(values))
    insert_query = f"INSERT INTO {table_name} ({', '.join(columns)}) VALUES ({placeholders})"
    print(f"Inserting data into {table_name}\nQuery: {insert_query}\nValues: {values}")
    cur.execute(insert_query, values)
    conn.commit()


# Function to convert lists to PostgreSQL arrays
def convert_to_postgres_array(value):
    if isinstance(value, list):
        return '{' + ','.join(f'"{str(v)}"' if isinstance(v, str) else str(v) for v in value) + '}'
    return value


# Function to link the resource_info and resource_record tables
def create_resource_tables(resource_id, metadata, records):
    info_table_name = f'resource_info_{resource_id}'
    create_table_if_not_exists(info_table_name, metadata)
    update_table_structure(info_table_name, metadata)
    insert_data(info_table_name, metadata)

    record_table_name = f'resource_record_{resource_id}'
    if records:
        for record in records:
            create_table_if_not_exists(record_table_name, record)
            update_table_structure(record_table_name, record)
            insert_data(record_table_name, record)


# Function to fetch data with pagination
def fetch_and_store_resource_data(api_url, resource_id):
    offset = 0
    limit = 1000
    while True:
        paginated_url = f"{api_url}&offset={offset}&limit={limit}"
        response = requests.get(paginated_url)
        data = response.json()

        # Store metadata and records for this resource
        metadata = {key: data[key] for key in data if key != 'records'}
        records = data.get('records', [])
        print(f"Fetched metadata for {resource_id}: {metadata.keys()}")
        print(f"Fetched {len(records)} records for {resource_id}")

        create_resource_tables(resource_id, metadata, records)

        if len(records) < limit:
            break
        offset += limit


# Main function to process all resource IDs from a CSV file
def process_resources_from_csv(csv_file):
    with open(csv_file, mode='r') as file:
        csv_reader = csv.reader(file)
        for row in csv_reader:
            resource_id = row[0]
            api_url = f'https://api.data.gov.in/resource/{resource_id}?api-key=579b464db66ec23bdd0000014ce2ee75345140b047c3eae390ac4b1c&format=json'
            print(f"Processing resource ID: {resource_id}")
            fetch_and_store_resource_data(api_url, resource_id)


# Example: Provide the path to the CSV file with resource IDs
csv_file_path = 'resource.csv'
process_resources_from_csv(csv_file_path)

# Close the cursor and connection
cur.close()
conn.close()
