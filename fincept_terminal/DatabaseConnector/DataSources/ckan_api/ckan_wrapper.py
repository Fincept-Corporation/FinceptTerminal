import dearpygui.dearpygui as dpg
import requests
import json
import threading

# Global variables
datasets = []
current_data = {}
current_page = 0
records_per_page = 10
selected_dataset = None


def fetch_collections():
    """Fetch available collections and their datasets"""
    global datasets
    dpg.set_value("status_text", "Loading all datasets... This may take a moment")

    def fetch_thread():
        global datasets
        datasets_list = []

        # Method 1: Try to get comprehensive dataset list via collections (expanded range)
        print("DEBUG: Scanning collections 1-100...")
        for collection_id in range(1, 101):  # Increased range to 100
            try:
                url = f"https://api-production.data.gov.sg/v2/public/api/collections/{collection_id}/metadata"
                response = requests.get(url, timeout=10)

                if response.status_code == 200:
                    data = response.json()
                    collection_meta = data.get('data', {}).get('collectionMetadata', {})
                    collection_name = collection_meta.get('name', f'Collection {collection_id}')
                    child_datasets = collection_meta.get('childDatasets', [])

                    print(f"DEBUG: Collection {collection_id} ({collection_name}): {len(child_datasets)} datasets")

                    # Get ALL datasets from this collection, not just first 5
                    for idx, dataset_id in enumerate(child_datasets):
                        datasets_list.append({
                            'display_name': f"[C{collection_id}] {collection_name} - Dataset {idx + 1}",
                            'dataset_id': dataset_id,
                            'collection_id': collection_id
                        })
                else:
                    print(f"DEBUG: Collection {collection_id}: {response.status_code}")
            except Exception as e:
                print(f"DEBUG: Collection {collection_id}: Error - {e}")
                continue

            # Update progress
            if collection_id % 10 == 0:
                dpg.set_value("status_text",
                              f"Scanned {collection_id} collections... Found {len(datasets_list)} datasets")

        # Method 2: Try to discover more datasets using direct API exploration
        print("DEBUG: Attempting direct dataset discovery...")
        try:
            # Try the newer API endpoints that might list all datasets
            api_endpoints = [
                "https://api-production.data.gov.sg/v2/public/api/datasets",
                "https://api.data.gov.sg/v1/public/api/datasets",
                "https://data.gov.sg/api/action/package_list"
            ]

            for endpoint in api_endpoints:
                try:
                    print(f"DEBUG: Trying endpoint: {endpoint}")
                    response = requests.get(endpoint, timeout=15)
                    if response.status_code == 200:
                        data = response.json()
                        print(
                            f"DEBUG: Endpoint response keys: {list(data.keys()) if isinstance(data, dict) else 'Not a dict'}")

                        # Handle different response formats
                        if 'result' in data and isinstance(data['result'], list):
                            # CKAN format
                            for dataset_name in data['result'][:100]:  # Limit to prevent overload
                                datasets_list.append({
                                    'display_name': f"[DIRECT] {dataset_name}",
                                    'dataset_id': dataset_name,
                                    'collection_id': 'direct'
                                })
                        elif 'data' in data:
                            # V2 API format
                            dataset_data = data['data']
                            if isinstance(dataset_data, list):
                                for dataset in dataset_data[:100]:
                                    if isinstance(dataset, dict) and 'id' in dataset:
                                        datasets_list.append({
                                            'display_name': f"[DIRECT] {dataset.get('name', dataset['id'])}",
                                            'dataset_id': dataset['id'],
                                            'collection_id': 'direct'
                                        })
                except Exception as e:
                    print(f"DEBUG: Endpoint {endpoint} failed: {e}")
                    continue
        except Exception as e:
            print(f"DEBUG: Direct discovery failed: {e}")

        # Method 3: Try systematic dataset ID discovery
        print("DEBUG: Attempting systematic dataset ID discovery...")
        # Some common patterns found in Singapore's dataset IDs
        id_patterns = [
            "d_{}",  # Standard pattern we've seen
            "dataset_{}",
            "sg_{}",
        ]

        # Try some known working IDs and variations
        known_working_samples = ["3f960c10fed6145404ca7b821f263b87", "af2042c77ffaf0db5d75561ce9ef5688"]

        for sample_id in known_working_samples:
            # Try variations of known working IDs
            for i in range(50):  # Try 50 variations
                try:
                    # Modify last few characters
                    test_id = sample_id[:-2] + f"{i:02d}"
                    test_url = f"https://data.gov.sg/api/action/datastore_search?resource_id=d_{test_id}&limit=1"

                    response = requests.get(test_url, timeout=5)
                    if response.status_code == 200:
                        data = response.json()
                        if data.get('success'):
                            datasets_list.append({
                                'display_name': f"[DISCOVERED] Dataset d_{test_id}",
                                'dataset_id': f"d_{test_id}",
                                'collection_id': 'discovered'
                            })
                            print(f"DEBUG: Found working dataset: d_{test_id}")
                except:
                    continue

        print(f"DEBUG: Total datasets found: {len(datasets_list)}")
        datasets = datasets_list
        dataset_names = [d['display_name'] for d in datasets]
        dpg.configure_item("dataset_combo", items=dataset_names)
        dpg.set_value("dataset_combo", "Select a dataset...")
        dpg.set_value("status_text",
                      f"Loaded {len(datasets)} datasets from {max([d['collection_id'] for d in datasets if str(d['collection_id']).isdigit()] + [0])} collections")

    threading.Thread(target=fetch_thread, daemon=True).start()


def on_dataset_selected(sender, app_data, user_data):
    """Callback when dataset is selected"""
    global selected_dataset, current_page

    print(f"DEBUG: Dataset selected - '{app_data}'")

    if not app_data or app_data == "Select a dataset...":
        return

    # Find dataset by display name
    selected_dataset = None
    for dataset in datasets:
        if dataset['display_name'] == app_data:
            selected_dataset = dataset
            break

    if selected_dataset:
        dpg.set_value("selected_info", f"Selected: {selected_dataset['display_name']}")
        dpg.set_value("status_text", f"Loading data for: {selected_dataset['dataset_id']}")
        print(f"DEBUG: Selected dataset ID: {selected_dataset['dataset_id']}")
        current_page = 1
        fetch_dataset_data(offset=0)
    else:
        dpg.set_value("status_text", f"Dataset '{app_data}' not found")


def fetch_dataset_data(offset=0):
    """Fetch data for selected dataset with pagination"""
    global current_data, records_per_page

    if not selected_dataset:
        dpg.set_value("status_text", "Please select a dataset first")
        return

    dpg.set_value("status_text", "Fetching data...")

    def fetch_thread():
        global current_data
        try:
            dataset_id = selected_dataset['dataset_id']
            print(f"DEBUG: Fetching data for dataset: {dataset_id}")

            url = f"https://data.gov.sg/api/action/datastore_search?resource_id={dataset_id}"
            url += f"&limit={records_per_page}&offset={offset}"
            print(f"DEBUG: API URL: {url}")

            response = requests.get(url, timeout=15)
            print(f"DEBUG: Response status: {response.status_code}")

            if response.status_code == 200:
                data = response.json()
                print(f"DEBUG: Response success: {data.get('success')}")

                if data.get('success'):
                    result = data.get('result', {})
                    records = result.get('records', [])
                    total = result.get('total', 0)

                    print(f"DEBUG: Found {len(records)} records out of {total} total")

                    current_data = {
                        'records': records,
                        'total': total,
                        'offset': offset,
                        'fields': result.get('fields', [])
                    }

                    update_data_display()
                    update_pagination_info()
                    dpg.set_value("status_text", f"Showing {len(records)} of {total} records")
                else:
                    error_msg = data.get('error', {})
                    print(f"DEBUG: API Error: {error_msg}")
                    dpg.set_value("status_text", f"API Error: {error_msg.get('message', 'Unknown error')}")
            else:
                print(f"DEBUG: HTTP Error: {response.status_code}")
                dpg.set_value("status_text", f"HTTP Error: {response.status_code}")
        except Exception as e:
            print(f"DEBUG: Exception: {str(e)}")
            dpg.set_value("status_text", f"Error: {str(e)}")

    threading.Thread(target=fetch_thread, daemon=True).start()


def update_data_display():
    """Update the data table display"""
    global current_data
    records = current_data.get('records', [])
    fields = current_data.get('fields', [])

    print(f"DEBUG: Updating display with {len(records)} records")

    # Clear existing table
    if dpg.does_item_exist("data_table"):
        dpg.delete_item("data_table")

    if not records:
        dpg.add_text("No data available", tag="data_table", parent="data_container")
        return

    # Create new table
    with dpg.table(header_row=True, tag="data_table", parent="data_container"):
        # If no fields info, use keys from first record
        if not fields and records:
            field_names = list(records[0].keys())
        else:
            field_names = [f.get('id', f'Field_{i}') for i, f in enumerate(fields)]

        print(f"DEBUG: Field names: {field_names[:8]}")

        # Add columns
        for field_name in field_names[:8]:  # Limit to 8 columns
            dpg.add_table_column(label=str(field_name)[:15])

        # Add data rows
        for record in records:
            with dpg.table_row():
                for field_name in field_names[:8]:
                    value = str(record.get(field_name, ''))[:50]
                    dpg.add_text(value)


def update_pagination_info():
    """Update pagination controls"""
    global current_data, current_page, records_per_page
    total = current_data.get('total', 0)
    offset = current_data.get('offset', 0)
    current_page = offset // records_per_page + 1
    total_pages = (total + records_per_page - 1) // records_per_page

    dpg.set_value("page_info", f"Page {current_page} of {total_pages}")

    # Enable/disable navigation buttons
    dpg.configure_item("prev_btn", enabled=current_page > 1)
    dpg.configure_item("next_btn", enabled=current_page < total_pages)


def on_prev_page():
    """Go to previous page"""
    global current_page, records_per_page
    if current_page > 1:
        offset = (current_page - 2) * records_per_page
        fetch_dataset_data(offset)


def on_next_page():
    """Go to next page"""
    global current_page, records_per_page, current_data
    total = current_data.get('total', 0)
    max_pages = (total + records_per_page - 1) // records_per_page
    if current_page < max_pages:
        offset = current_page * records_per_page
        fetch_dataset_data(offset)


def on_records_per_page_change(sender, app_data):
    """Update records per page"""
    global records_per_page
    records_per_page = max(1, app_data)


# Create GUI
dpg.create_context()

with dpg.window(label="Singapore Data Portal Explorer", width=1000, height=700, tag="main_window"):
    dpg.add_text("Singapore Data.gov.sg Explorer", color=[100, 200, 255])
    dpg.add_separator()

    # Controls
    with dpg.group(horizontal=True):
        dpg.add_button(label="Load Datasets", callback=lambda: fetch_collections())
        dpg.add_combo(label="Select Dataset", tag="dataset_combo", width=400,
                      callback=on_dataset_selected, default_value="Load datasets first...")
        dpg.add_input_int(label="Records/Page", default_value=10, width=100,
                          callback=on_records_per_page_change)

    dpg.add_text("", tag="selected_info", color=[150, 255, 150])
    dpg.add_separator()

    # Pagination controls
    with dpg.group(horizontal=True):
        dpg.add_button(label="◀ Previous", tag="prev_btn", callback=on_prev_page, enabled=False)
        dpg.add_text("Page 0 of 0", tag="page_info")
        dpg.add_button(label="Next ▶", tag="next_btn", callback=on_next_page, enabled=False)

    dpg.add_separator()

    # Data display area
    dpg.add_text("Data will appear here after selecting a dataset")
    with dpg.child_window(height=400, tag="data_container"):
        pass

    dpg.add_separator()
    dpg.add_text("Ready", tag="status_text", color=[255, 255, 100])

dpg.create_viewport(title="Singapore Data Explorer", width=1020, height=720)
dpg.setup_dearpygui()
dpg.set_primary_window("main_window", True)
dpg.show_viewport()
dpg.start_dearpygui()
dpg.destroy_context()