
import os
import psycopg2
from psycopg2 import pool, sql
from fastapi import FastAPI, HTTPException, Request, Depends
from fastapi.responses import JSONResponse
from concurrent.futures import ThreadPoolExecutor, ProcessPoolExecutor
from threading import Lock
from pydantic import BaseModel
from typing import List, Optional
import google.generativeai as genai
import requests
from fastapi.middleware.gzip import GZipMiddleware
import httpx
import logging

app = FastAPI()

API_KEY = "AIzaSyDqwj91u7AjqWk7KcjgUX73VTX5PawNZxY"
genai.configure(api_key=API_KEY)

# Define Perplexica API endpoint
PERPLEXICA_API_URL = "http://localhost:3001/api/search"



# Input model for Perplexica API
class PerplexicaRequest(BaseModel):
    query: str  # The user's question
    focus_mode: str = "webSearch"  # Optional: default is "webSearch"

@app.post("/perplexica/search")
async def query_perplexica(request: PerplexicaRequest):
    """
    Endpoint to interact with the Perplexica API.
    Users provide a query, and this endpoint forwards it to Perplexica and returns the response.
    """
    # Define the payload for Perplexica
    payload = {
        "chatModel": {
            "provider": "ollama",
            "model": "llama3.2:latest"
        },
        "embeddingModel": {
            "provider": "local",
            "model": "xenova-gte-small"
        },
        "focusMode": request.focus_mode,
        "query": request.query,
        "optimizationMode": "speed"
    }

    try:
        # Make the POST request to the Perplexica API
        response = requests.post(PERPLEXICA_API_URL, json=payload)
        response.raise_for_status()  # Raise HTTPError for bad responses (4xx, 5xx)
        perplexica_response = response.json()

        # Extract the response message and sources
        return {
            "message": perplexica_response.get("message", "No message found"),
            "sources": perplexica_response.get("sources", [])
        }

    except requests.exceptions.RequestException as e:
        raise HTTPException(status_code=500, detail=f"Error querying Perplexica API: {e}")
    except json.JSONDecodeError:
        raise HTTPException(status_code=500, detail="Failed to parse the Perplexica API response.")

# Database connection details
DB_HOST = os.getenv('DB_HOST', 'localhost')
DB_PORT = os.getenv('DB_PORT', '5432')
DB_USER = os.getenv('DB_USER', 'postgres')
DB_PASSWORD = os.getenv('DB_PASSWORD', '12Rjkrs34##')
DB_NAME = os.getenv('DB_NAME', 'postgres')

# Initialize connection pool
db_pool = psycopg2.pool.SimpleConnectionPool(
    minconn=1,
    maxconn=20,  # You can adjust this number based on your needs and system capacity
    user=DB_USER,
    password=DB_PASSWORD,
    host=DB_HOST,
    port=DB_PORT,
    database=DB_NAME
)

# ThreadPoolExecutor for I/O-bound tasks
thread_pool = ThreadPoolExecutor(max_workers=10)
# ProcessPoolExecutor for CPU-bound tasks
process_pool = ProcessPoolExecutor(max_workers=4)

# Global counter and lock to track the number of requests
request_counter = 0
counter_lock = Lock()


def connect_db(dbname=None):
    if dbname:
        # Connect to the specific database
        conn = psycopg2.connect(
            dbname=dbname,
            user=DB_USER,
            password=DB_PASSWORD,
            host=DB_HOST,
            port=DB_PORT
        )
    else:
        # Get a connection from the pool to the default database
        conn = db_pool.getconn()
    return conn


def close_db(conn):
    db_pool.putconn(conn)


# Function to execute a database query in a separate thread
def execute_db_query(query, params=None, dbname=None):
    conn = connect_db(dbname=dbname)
    try:
        cur = conn.cursor()
        cur.execute(query, params)
        result = cur.fetchall()
        cur.close()
    finally:
        if dbname:
            conn.close()
        else:
            close_db(conn)
    return result


# Middleware to increment request counter
@app.middleware("http")
async def increment_request_counter(request: Request, call_next):
    global request_counter
    with counter_lock:
        request_counter += 1
    response = await call_next(request)
    return response


@app.get("/databases")
async def get_databases():
    query = "SELECT datname FROM pg_database WHERE datistemplate = false;"
    future = thread_pool.submit(execute_db_query, query)
    databases = future.result()
    return [db[0] for db in databases]

# Global variables to control the API status
API_STATUS = "UP"  # Options: "UP", "DOWN", "WORKING"
status_lock = Lock()

@app.get("/status")
async def get_status():
    """
    Endpoint to check the current status of the API.
    Returns one of the following statuses:
    - UP: API is running normally.
    - DOWN: API is inactive.
    - WORKING: API is in maintenance mode.
    """
    global API_STATUS
    with status_lock:
        return {"status": API_STATUS}

@app.post("/status/maintenance")
async def set_maintenance():
    """
    Endpoint to set the API status to maintenance (WORKING).
    """
    global API_STATUS
    with status_lock:
        API_STATUS = "WORKING"
    return {"status": API_STATUS, "message": "API set to maintenance mode."}

@app.post("/status/activate")
async def activate_api():
    """
    Endpoint to set the API status to active (UP).
    """
    global API_STATUS
    with status_lock:
        API_STATUS = "UP"
    return {"status": API_STATUS, "message": "API activated."}

@app.post("/status/deactivate")
async def deactivate_api():
    """
    Endpoint to set the API status to inactive (DOWN).
    """
    global API_STATUS
    with status_lock:
        API_STATUS = "DOWN"
    return {"status": API_STATUS, "message": "API deactivated."}


@app.get("/{dbname}/tables")
async def get_tables(dbname: str):
    query = "SELECT table_name FROM information_schema.tables WHERE table_schema='public';"
    future = thread_pool.submit(execute_db_query, query, dbname=dbname)
    tables = future.result()
    return [table[0] for table in tables]


@app.get("/{dbname}/{table}/data")
async def get_table_data(dbname: str, table: str, limit: Optional[int] = None):
    # Query to get the column names
    columns_query = sql.SQL(
        "SELECT column_name FROM information_schema.columns WHERE table_name = {table_name} AND table_schema='public';").format(
        table_name=sql.Literal(table)
    )
    columns_future = thread_pool.submit(execute_db_query, columns_query, dbname=dbname)
    columns = columns_future.result()

    # Determine the correct case for the "Date" column
    date_column = next((col[0] for col in columns if col[0].lower() == 'date'), None)

    # Build the query based on the optional limit
    if date_column:
        if limit:
            # If a limit is provided, include it in the query
            data_query = sql.SQL('SELECT * FROM {table} ORDER BY {date_column} DESC LIMIT %s;').format(
                table=sql.Identifier(table),
                date_column=sql.Identifier(date_column)
            )
            params = (limit,)
        else:
            # No limit provided, fetch all rows
            data_query = sql.SQL('SELECT * FROM {table} ORDER BY {date_column} DESC;').format(
                table=sql.Identifier(table),
                date_column=sql.Identifier(date_column)
            )
            params = None
    else:
        if limit:
            # If a limit is provided, include it in the query
            data_query = sql.SQL('SELECT * FROM {table} LIMIT %s;').format(
                table=sql.Identifier(table)
            )
            params = (limit,)
        else:
            # No limit provided, fetch all rows
            data_query = sql.SQL('SELECT * FROM {table};').format(
                table=sql.Identifier(table)
            )
            params = None

    try:
        # Submit the query to the thread pool and wait for the result
        data_future = thread_pool.submit(execute_db_query, data_query, params, dbname=dbname)
        rows = data_future.result()

        # Get column names after executing the data query
        column_names = [desc[0] for desc in columns]

        # Return the data as a list of dictionaries
        return [dict(zip(column_names, row)) for row in rows]

    except psycopg2.Error as e:
        raise HTTPException(status_code=500, detail=f"Database query failed: {e}")

    except IndexError as e:
        raise HTTPException(status_code=500, detail=f"Index Error: {e}")


@app.get("/{dbname}/{table}/{column}/data")
async def get_column_data(dbname: str, table: str, column: str, unique: bool = False):
    # Query to check if the column exists in the specified table
    columns_query = sql.SQL(
        "SELECT column_name FROM information_schema.columns WHERE table_name = {table_name} AND table_schema='public';").format(
        table_name=sql.Literal(table)
    )
    columns_future = thread_pool.submit(execute_db_query, columns_query, dbname=dbname)
    columns = columns_future.result()

    # Check if the requested column exists
    if column not in [col[0] for col in columns]:
        raise HTTPException(status_code=400, detail=f"Column '{column}' does not exist in table '{table}'")

    # SQL query to fetch the data for the specific column
    if unique:
        # Query to fetch distinct values from the column
        data_query = sql.SQL('SELECT DISTINCT {column} FROM {table};').format(
            column=sql.Identifier(column),
            table=sql.Identifier(table)
        )
    else:
        # Query to fetch all values from the column
        data_query = sql.SQL('SELECT {column} FROM {table};').format(
            column=sql.Identifier(column),
            table=sql.Identifier(table)
        )

    try:
        # Submit the query to the thread pool and wait for the result
        data_future = thread_pool.submit(execute_db_query, data_query, None, dbname=dbname)
        rows = data_future.result()

        # Return the data as a list of values
        return {column: [row[0] for row in rows]}

    except psycopg2.Error as e:
        raise HTTPException(status_code=500, detail=f"Database query failed: {e}")


@app.get("/{dbname}/{table}/{column}/filter")
async def get_rows_by_column_value(dbname: str, table: str, column: str, value: str):
    # Query to check if the column exists in the specified table
    columns_query = sql.SQL(
        "SELECT column_name FROM information_schema.columns WHERE table_name = {table_name} AND table_schema='public';").format(
        table_name=sql.Literal(table)
    )
    columns_future = thread_pool.submit(execute_db_query, columns_query, dbname=dbname)
    columns = columns_future.result()

    # Check if the requested column exists
    if column not in [col[0] for col in columns]:
        raise HTTPException(status_code=400, detail=f"Column '{column}' does not exist in table '{table}'")

    # Query to fetch all rows where the column has the specified value
    data_query = sql.SQL('SELECT * FROM {table} WHERE {column} = %s;').format(
        column=sql.Identifier(column),
        table=sql.Identifier(table)
    )

    try:
        # Submit the query to the thread pool and wait for the result
        data_future = thread_pool.submit(execute_db_query, data_query, (value,), dbname=dbname)
        rows = data_future.result()

        # Return the data as a list of dictionaries
        column_names = [desc[0] for desc in columns]
        return [dict(zip(column_names, row)) for row in rows]

    except psycopg2.Error as e:
        raise HTTPException(status_code=500, detail=f"Database query failed: {e}")


@app.get("/{dbname}/{table}/countries")
async def get_distinct_countries(dbname: str, table: str, limit: int = 500, offset: int = 0):
    # Query to check if the 'country' column exists in the specified table
    columns_query = sql.SQL(
        "SELECT column_name FROM information_schema.columns WHERE table_name = {table_name} AND table_schema='public';").format(
        table_name=sql.Literal(table)
    )
    columns_future = thread_pool.submit(execute_db_query, columns_query, dbname=dbname)
    columns = columns_future.result()

    # Check if the 'country' column exists
    if 'country' not in [col[0] for col in columns]:
        raise HTTPException(status_code=400, detail=f"Column 'country' does not exist in table '{table}'")

    # Query to fetch distinct country values
    query_str = 'SELECT DISTINCT country FROM {table} WHERE country IS NOT NULL LIMIT %s OFFSET %s;'

    data_query = sql.SQL(query_str).format(
        table=sql.Identifier(table)
    )

    # Set query parameters
    params = (limit, offset)

    try:
        # Submit the query to the thread pool and wait for the result
        data_future = thread_pool.submit(execute_db_query, data_query, params, dbname=dbname)
        rows = data_future.result()

        # Return the distinct country values
        return {"countries": [row[0] for row in rows]}

    except psycopg2.Error as e:
        raise HTTPException(status_code=500, detail=f"Database query failed: {e}")


@app.get("/{dbname}/{table}/sectors_and_industries_and_stocks")
async def get_sectors_industries_stocks(dbname: str, table: str, filter_column: str, filter_value: str,
                                        sector: Optional[str] = None, industry: Optional[str] = None, limit: int = 500,
                                        offset: int = 0):
    # Query to check if the 'filter_column', 'sector', 'industry', 'symbol', and 'market_cap' columns exist in the specified table
    columns_query = sql.SQL(
        "SELECT column_name FROM information_schema.columns WHERE table_name = {table_name} AND table_schema='public';").format(
        table_name=sql.Literal(table)
    )
    columns_future = thread_pool.submit(execute_db_query, columns_query, dbname=dbname)
    columns = columns_future.result()

    # Ensure necessary columns exist: country, sector, industry, symbol, and market_cap
    if filter_column not in [col[0] for col in columns] or 'sector' not in [col[0] for col in
                                                                            columns] or 'industry' not in [col[0] for
                                                                                                           col in
                                                                                                           columns] or 'symbol' not in [
        col[0] for col in columns] or 'market_cap' not in [col[0] for col in columns]:
        raise HTTPException(status_code=400,
                            detail=f"'{filter_column}', 'sector', 'industry', 'symbol', or 'market_cap' column does not exist in table '{table}'")

    # Step 1: Return distinct sectors for the given country if no sector is provided
    if not sector and not industry:
        query_str = 'SELECT DISTINCT sector FROM {table} WHERE {filter_column} = %s AND sector IS NOT NULL LIMIT %s OFFSET %s;'
        data_query = sql.SQL(query_str).format(
            table=sql.Identifier(table),
            filter_column=sql.Identifier(filter_column)
        )
        params = (filter_value, limit, offset)

        try:
            data_future = thread_pool.submit(execute_db_query, data_query, params, dbname=dbname)
            rows = data_future.result()
            return {"sectors": [row[0] for row in rows]}
        except psycopg2.Error as e:
            raise HTTPException(status_code=500, detail=f"Database query failed: {e}")

    # Step 2: Return distinct industries for the given sector if sector is provided but no industry is provided
    elif sector and not industry:
        query_str = 'SELECT DISTINCT industry FROM {table} WHERE {filter_column} = %s AND sector = %s AND industry IS NOT NULL LIMIT %s OFFSET %s;'
        data_query = sql.SQL(query_str).format(
            table=sql.Identifier(table),
            filter_column=sql.Identifier(filter_column)
        )
        params = (filter_value, sector, limit, offset)

        try:
            data_future = thread_pool.submit(execute_db_query, data_query, params, dbname=dbname)
            rows = data_future.result()
            return {"industries": [row[0] for row in rows]}
        except psycopg2.Error as e:
            raise HTTPException(status_code=500, detail=f"Database query failed: {e}")

    # Step 3: Return stocks (with market_cap) for the given industry if both sector and industry are provided
    elif sector and industry:
        query_str = 'SELECT symbol, name, market_cap FROM {table} WHERE {filter_column} = %s AND sector = %s AND industry = %s AND symbol IS NOT NULL LIMIT %s OFFSET %s;'
        data_query = sql.SQL(query_str).format(
            table=sql.Identifier(table),
            filter_column=sql.Identifier(filter_column)
        )
        params = (filter_value, sector, industry, limit, offset)

        try:
            data_future = thread_pool.submit(execute_db_query, data_query, params, dbname=dbname)
            rows = data_future.result()

            # Return stocks with market_cap as a list of dictionaries
            return [{"symbol": row[0], "name": row[1], "market_cap": row[2]} for row in rows]
        except psycopg2.Error as e:
            raise HTTPException(status_code=500, detail=f"Database query failed: {e}")

    else:
        raise HTTPException(status_code=400, detail="Invalid combination of parameters")


# Define the input model for user input
class UserInputModel(BaseModel):
    user_input: str


# Function to call Gemini API using google.generativeai
def call_gemini_api(input_text: str):
    try:
        # Create the Gemini model object
        model = genai.GenerativeModel('models/gemini-1.5-flash')

        # Define a prompt for generating a response using the Gemini API
        prompt = (
            "Analyze the text and provide a summary with key insights:\n\n"
        )

        # Generate a response using the model
        response = model.generate_content(f"{prompt}\n\nText: {input_text}")

        # Extract the relevant information from the response
        extracted_info = response.text

        return extracted_info
    except Exception as e:
        # Handle any errors that occur during the Gemini API call
        raise HTTPException(status_code=500, detail=f"Error querying Gemini API: {str(e)}")


# New endpoint to receive user input, process it using Gemini API, and return the result
@app.post("/process-gemini/")
async def process_gemini_input(user_input: UserInputModel):
    input_text = user_input.user_input

    # Submit the task to call the Gemini API to the thread pool
    gemini_future = thread_pool.submit(call_gemini_api, input_text)

    # Wait for the result from the thread pool
    gemini_response = gemini_future.result()

    # Return the response from the Gemini API
    return {"gemini_response": gemini_response}


class DataModel(BaseModel):
    data: List[int]


@app.post("/cpu-bound-task")
async def handle_cpu_bound_task(data: DataModel):
    # Submit CPU-bound tasks to the process pool
    futures = [process_pool.submit(some_cpu_bound_task, d) for d in data.data]

    # Gather results
    results = [future.result() for future in futures]

    return results


# Endpoint to get the total number of requests made
@app.get("/request-count")
async def get_request_count():
    global request_counter
    with counter_lock:
        return {"total_requests": request_counter}


if __name__ == "__main__":
    import uvicorn

    # Use `workers` in uvicorn to handle multiple requests in parallel
    uvicorn.run(app, host="0.0.0.0", port=5000, workers=8)

