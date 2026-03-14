"""
Spain Open Data (datos.gob.es) Data Fetcher
Access to Spanish government datasets through hierarchical catalogue → dataset → resources structure
Returns JSON output for Rust integration
"""

import sys
import json
import os
import requests
from typing import Dict, Any, List, Optional
from datetime import datetime
import urllib.parse

# Configuration
BASE_URL = "https://datos.gob.es/apidata/"
TIMEOUT = 30

# Spanish to English translation dictionary for common terms
SPANISH_TRANSLATIONS = {
    # Government terms
    "ayuntamiento": "town hall/city council",
    "municipio": "municipality",
    "concejo": "council",
    "diputación": "provincial council",
    "gobierno": "government",
    "ministerio": "ministry",
    "secretaría": "secretariat",
    "departamento": "department",
    "consejería": "regional ministry",
    "junta": "board/regional government",
    "comunidad autónoma": "autonomous community",
    "provincia": "province",
    "estado": "state",

    # Geographic terms
    "callejero": "street directory/address book",
    "calles": "streets",
    "vías": "roads/ways",
    "urbanismo": "urban planning",
    "infraestructuras": "infrastructures",
    "territorio": "territory",
    "cartografía": "cartography/maps",
    "geográfico": "geographic",
    "postal": "postal",

    # Data/common terms
    "datos": "data",
    "catálogo": "catalogue",
    "conjunto de datos": "dataset",
    "recurso": "resource",
    "distribución": "distribution",
    "formato": "format",
    "licencia": "license",
    "publicación": "publication",
    "actualización": "update",
    "creación": "creation",

    # Economic/Financial terms
    "presupuesto": "budget",
    "ingresos": "income/revenue",
    "gastos": "expenses",
    "contratos": "contracts",
    "facturación": "billing/invoicing",
    "económico": "economic",
    "finanzas": "finances",
    "fiscal": "fiscal",
    "tributos": "taxes",

    # Social terms
    "educación": "education",
    "sanidad": "health/healthcare",
    "servicios sociales": "social services",
    "empleo": "employment",
    "población": "population",
    "demografía": "demographics",
    "cultura": "culture",
    "deporte": "sports",

    # Environmental terms
    "medio ambiente": "environment",
    "residuos": "waste",
    "calidad del aire": "air quality",
    "agua": "water",
    "energía": "energy",
    "clima": "climate",
    "contaminación": "pollution",

    # Transport terms
    "transporte": "transport",
    "tráfico": "traffic",
    "carreteras": "roads",
    "autobuses": "buses",
    "aparcamiento": "parking",
    "movilidad": "mobility",

    # Administrative terms
    "administración": "administration",
    "público": "public",
    "oficial": "official",
    "registro": "registry",
    "normativa": "regulations",
    "procedimientos": "procedures",
    "tramites": "procedures/formalities",

    # Common words
    "nuevo": "new",
    "histórico": "historical",
    "anual": "annual",
    "mensual": "monthly",
    "trimestral": "quarterly",
    "estadísticas": "statistics",
    "indicadores": "indicators",
    "informe": "report",
    "listado": "listing",
    "relación": "relation/list",

    # Common dataset name patterns
    "padrón": "census/registry",
    "censo": "census",
    "directorio": "directory",
    "guía": "guide",
    "catálogo de bienes": "catalog of goods",
    "inventario": "inventory",
    "expedientes": "records/files",
    "acuerdos": "agreements",
    "actas": "minutes/records",
    "convenios": "agreements/conventions"
}

def translate_spanish_text(text: str) -> Dict[str, Any]:
    """Translate Spanish text to English with context"""
    if not text or not isinstance(text, str):
        return {"original": text, "translated": text, "found_translations": []}

    original_text = text
    lower_text = text.lower()
    found_translations = []

    # Replace known Spanish terms with English translations
    translated_text = original_text
    for spanish, english in SPANISH_TRANSLATIONS.items():
        if spanish in lower_text:
            translated_text = translated_text.replace(spanish, english)
            found_translations.append({"spanish": spanish, "english": english})

    return {
        "original": original_text,
        "translated": translated_text,
        "found_translations": found_translations,
        "translation_confidence": "high" if found_translations else "none"
    }

def enhance_with_translation(data_item: Dict[str, Any]) -> Dict[str, Any]:
    """Add English translations to data items"""
    if not isinstance(data_item, dict):
        return data_item

    enhanced_item = data_item.copy()

    # Translate title if present (could be string or list)
    if "title" in data_item:
        title = data_item["title"]
        if isinstance(title, str):
            enhanced_item["title_translation"] = translate_spanish_text(title)
        elif isinstance(title, list) and title:
            # Handle multilingual titles - use the Spanish one for translation
            spanish_title = ""
            for item in title:
                if isinstance(item, dict) and item.get("_lang") == "es":
                    spanish_title = item.get("_value", "")
                    break
            if spanish_title:
                enhanced_item["title_translation"] = translate_spanish_text(spanish_title)

    # Translate description if present (could be string or list)
    if "description" in data_item:
        description = data_item["description"]
        if isinstance(description, str):
            enhanced_item["description_translation"] = translate_spanish_text(description)
        elif isinstance(description, list) and description:
            # Handle multilingual descriptions - use the Spanish one for translation
            spanish_desc = ""
            for item in description:
                if isinstance(item, dict) and item.get("_lang") == "es":
                    spanish_desc = item.get("_value", "")
                    break
            if spanish_desc:
                enhanced_item["description_translation"] = translate_spanish_text(spanish_desc)

    return enhanced_item

def _make_request(endpoint: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Centralized request handler for all API calls"""
    try:
        url = f"{BASE_URL}{endpoint}"
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Ensure JSON format is requested
        if params is None:
            params = {}
        params['_format'] = 'json'

        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # Check for API-specific errors
        if not data.get('result'):
            return {"error": "No results found", "raw_response": data}

        # Add translations to items
        result_data = data.get('result', {})
        if 'items' in result_data and isinstance(result_data['items'], list):
            enhanced_items = [enhance_with_translation(item) for item in result_data['items']]
            result_data['items'] = enhanced_items

        return {
            "data": result_data,
            "metadata": {
                "source": "Spain Open Data (datos.gob.es)",
                "endpoint": endpoint,
                "timestamp": datetime.now().isoformat(),
                "total_items": len(result_data.get('items', [])),
                "translation_enabled": True
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP Error {e.response.status_code}: {e.response.text}"}
    except requests.exceptions.Timeout:
        return {"error": "Request timeout"}
    except requests.exceptions.ConnectionError:
        return {"error": "Connection error"}
    except json.JSONDecodeError:
        return {"error": "Invalid JSON response"}
    except Exception as e:
        return {"error": f"Unexpected error: {str(e)}"}

def _extract_id_from_uri(uri: str) -> str:
    """Extract the ID part from a URI"""
    if not uri:
        return ""
    return uri.split('/')[-1]

def _clean_publisher_id(publisher_id: str) -> str:
    """Clean publisher ID to ensure proper format"""
    if not publisher_id:
        return ""

    # Remove base URL if present
    if publisher_id.startswith("http://datos.gob.es/recurso/sector-publico/org/"):
        return publisher_id.replace("http://datos.gob.es/recurso/sector-publico/org/", "/sector-publico/org/")
    elif not publisher_id.startswith("/"):
        return f"/sector-publico/org/{publisher_id}"

    return publisher_id

# Level 1: Catalogue Operations
def list_catalogues() -> Dict[str, Any]:
    """List all available catalogues (publishers)"""
    result = _make_request("catalog/publisher")

    if result.get("error") is None and result.get("data", {}).get("items"):
        # The API returns a list of URI strings, convert to objects with extracted IDs
        items = result["data"]["items"]
        enhanced_items = []

        for item in items:
            if isinstance(item, str) and item.startswith("http"):
                # Extract ID from URI and create object structure
                extracted_id = _extract_id_from_uri(item)
                enhanced_item = {
                    "uri": item,
                    "extracted_id": extracted_id
                }
                enhanced_items.append(enhanced_item)
            else:
                # If it's already an object, just ensure it has extracted_id
                if isinstance(item, dict):
                    if "uri" in item and "extracted_id" not in item:
                        item["extracted_id"] = _extract_id_from_uri(item["uri"])
                    enhanced_items.append(item)

        result["data"]["items"] = enhanced_items
        result["metadata"]["catalogue_count"] = len(enhanced_items)
        result["metadata"]["description"] = "List of Spanish government data publishers"

    return result

def get_catalogue_details(publisher_id: str) -> Dict[str, Any]:
    """Get details for a specific catalogue (publisher)"""
    if not publisher_id:
        return {"error": "Publisher ID is required"}

    # For now, we'll return the catalogue from the list as the API doesn't have a specific endpoint for single catalogue details
    catalogues_result = list_catalogues()

    if catalogues_result.get("error"):
        return catalogues_result

    clean_id = _clean_publisher_id(publisher_id)

    # Find the specific catalogue in the list
    for catalogue in catalogues_result.get("data", {}).get("items", []):
        if catalogue.get("id") == clean_id or catalogue.get("extracted_id") == publisher_id:
            return {
                "data": catalogue,
                "metadata": {
                    "source": "Spain Open Data (datos.gob.es)",
                    "catalogue_id": publisher_id,
                    "timestamp": datetime.now().isoformat()
                },
                "error": None
            }

    return {"error": f"Catalogue with ID '{publisher_id}' not found"}

# Level 2: Dataset Operations
def list_datasets(publisher_id: Optional[str] = None, page: int = 1, page_size: int = 20) -> Dict[str, Any]:
    """List datasets, optionally filtered by publisher"""
    params = {
        "_page": page,
        "_pageSize": page_size
    }

    if publisher_id:
        params["publisher"] = _clean_publisher_id(publisher_id)

    result = _make_request("catalog/dataset", params)

    if result.get("error") is None and result.get("data", {}).get("items"):
        # Enhance dataset data with extracted IDs
        for item in result["data"]["items"]:
            if "uri" in item:
                item["extracted_id"] = _extract_id_from_uri(item["uri"])

        result["metadata"]["dataset_count"] = len(result["data"]["items"])
        result["metadata"]["page"] = page
        result["metadata"]["page_size"] = page_size
        result["metadata"]["publisher_filter"] = publisher_id
        result["metadata"]["description"] = "List of datasets" + (f" from publisher: {publisher_id}" if publisher_id else "")

    return result

def get_dataset_details(dataset_id: str) -> Dict[str, Any]:
    """Get details for a specific dataset including its resources/distributions"""
    if not dataset_id:
        return {"error": "Dataset ID is required"}

    result = _make_request(f"catalog/dataset/{urllib.parse.quote(dataset_id)}")

    if result.get("error") is None and result.get("data", {}).get("items"):
        # Count distributions (data files)
        dataset = result["data"]["items"][0]  # API returns single item in array
        distributions = dataset.get("distribution", [])

        # Add distribution count and formats summary
        format_counts = {}
        for dist in distributions:
            # Handle different format structures
            format_field = dist.get("format", "")
            if isinstance(format_field, str):
                # Extract format from URI or use direct value
                if format_field.startswith("http"):
                    format_label = format_field.split("/")[-1].upper()
                else:
                    format_label = format_field
            elif isinstance(format_field, dict):
                format_label = format_field.get("label", "Unknown")
            else:
                format_label = "Unknown"

            format_counts[format_label] = format_counts.get(format_label, 0) + 1

        result["metadata"]["distribution_count"] = len(distributions)
        result["metadata"]["format_summary"] = format_counts
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["description"] = f"Dataset details with {len(distributions)} data files"

    return result

# Level 3: Series/Resources Operations
def list_dataset_resources(dataset_id: str) -> Dict[str, Any]:
    """List all resources/data files available for a specific dataset"""
    dataset_result = get_dataset_details(dataset_id)

    if dataset_result.get("error"):
        return dataset_result

    if not dataset_result.get("data", {}).get("items"):
        return {"error": "No dataset found"}

    dataset = dataset_result["data"]["items"][0]
    distributions = dataset.get("distribution", [])

    # Enhance distribution data with translations
    for i, dist in enumerate(distributions):
        dist["resource_index"] = i
        if "accessURL" in dist:
            dist["download_available"] = True
        else:
            dist["download_available"] = False

        # Add translations for resource titles
        if "title" in dist:
            dist["title_translation"] = translate_spanish_text(dist["title"])

    return {
        "data": distributions,
        "metadata": {
            "source": "Spain Open Data (datos.gob.es)",
            "dataset_id": dataset_id,
            "dataset_title": dataset.get("title", "Unknown Dataset"),
            "resource_count": len(distributions),
            "timestamp": datetime.now().isoformat(),
            "description": "List of data files available for download"
        },
        "error": None
    }

def get_resource_download_info(dataset_id: str, resource_index: Optional[int] = None, resource_title: Optional[str] = None) -> Dict[str, Any]:
    """Get download information for a specific resource in a dataset"""
    resources_result = list_dataset_resources(dataset_id)

    if resources_result.get("error"):
        return resources_result

    resources = resources_result.get("data", [])

    if not resources:
        return {"error": "No resources found for this dataset"}

    target_resource = None

    # Find resource by index or title
    if resource_index is not None:
        if 0 <= resource_index < len(resources):
            target_resource = resources[resource_index]
        else:
            return {"error": f"Resource index {resource_index} out of range (0-{len(resources)-1})"}
    elif resource_title:
        for resource in resources:
            title = resource.get("title", "")
            # Handle both string and list titles
            if isinstance(title, str):
                title_str = title
            elif isinstance(title, list) and title:
                # Use the English title if available, otherwise first one
                title_str = ""
                for item in title:
                    if isinstance(item, dict) and item.get("_lang") == "en":
                        title_str = item.get("_value", "")
                        break
                if not title_str:
                    title_str = title[0].get("_value", "") if title[0] else ""
            else:
                title_str = str(title)

            if title_str.lower() == resource_title.lower():
                target_resource = resource
                break
        if not target_resource:
            return {"error": f"No resource found with title: {resource_title}"}
    else:
        # Return first resource if no specific one requested
        target_resource = resources[0]

    # Extract file format from different possible structures
    format_field = target_resource.get("format", "")
    if isinstance(format_field, str):
        if format_field.startswith("http"):
            file_format = format_field.split("/")[-1].upper()
        else:
            file_format = format_field
    elif isinstance(format_field, dict):
        file_format = format_field.get("label", "Unknown")
    else:
        file_format = "Unknown"

    # Extract resource title for display
    title = target_resource.get("title", "Unknown Resource")
    if isinstance(title, list) and title:
        # Use English title if available
        title_str = ""
        for item in title:
            if isinstance(item, dict) and item.get("_lang") == "en":
                title_str = item.get("_value", "")
                break
        if not title_str:
            title_str = title[0].get("_value", "") if title[0] else "Unknown Resource"
    elif isinstance(title, str):
        title_str = title
    else:
        title_str = str(title)

    # Add download information
    download_info = {
        "resource_info": target_resource,
        "download_url": target_resource.get("accessURL"),
        "file_format": file_format,
        "download_available": bool(target_resource.get("accessURL")),
        "metadata": {
            "source": "Spain Open Data (datos.gob.es)",
            "dataset_id": dataset_id,
            "resource_title": title_str,
            "timestamp": datetime.now().isoformat(),
            "description": "Download information for dataset resource"
        }
    }

    return {
        "data": download_info,
        "metadata": {
            "source": "Spain Open Data (datos.gob.es)",
            "endpoint": "resource_download_info",
            "timestamp": datetime.now().isoformat(),
            "description": "Download information for dataset resource"
        },
        "error": None
    }

def search_datasets(query: str, publisher_id: Optional[str] = None, page: int = 1, page_size: int = 20) -> Dict[str, Any]:
    """Search datasets by query string, optionally filtered by publisher"""
    if not query:
        return {"error": "Search query is required"}

    # The API doesn't have a direct search endpoint, so we'll filter the datasets
    datasets_result = list_datasets(publisher_id, page, page_size)

    if datasets_result.get("error"):
        return datasets_result

    datasets = datasets_result.get("data", {}).get("items", [])

    # Filter datasets by query in title or description (including translations)
    query_lower = query.lower()
    filtered_datasets = []

    for dataset in datasets:
        title = dataset.get("title", "").lower()
        description = dataset.get("description", "").lower()

        # Also search in translated content
        title_trans = dataset.get("title_translation", {}).get("translated", "").lower()
        desc_trans = dataset.get("description_translation", {}).get("translated", "").lower()

        if (query_lower in title or query_lower in description or
            query_lower in title_trans or query_lower in desc_trans):
            filtered_datasets.append(dataset)

    return {
        "data": {
            "items": filtered_datasets,
            "total_found": len(filtered_datasets),
            "search_query": query,
            "publisher_filter": publisher_id
        },
        "metadata": {
            "source": "Spain Open Data (datos.gob.es)",
            "search_timestamp": datetime.now().isoformat(),
            "results_count": len(filtered_datasets),
            "description": f"Search results for '{query}'" + (f" from publisher: {publisher_id}" if publisher_id else "")
        },
        "error": None
    }

def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: spain_data.py <command> [args]",
            "available_commands": {
                "catalogues": "List all catalogues (publishers)",
                "catalogue-details <publisher_id>": "Get details for a specific catalogue",
                "datasets [publisher_id] [page] [page_size]": "List datasets, optionally filtered by publisher",
                "dataset-details <dataset_id>": "Get details for a specific dataset",
                "resources <dataset_id>": "List all resources for a dataset",
                "resource-info <dataset_id> [index|title]": "Get download info for a specific resource",
                "search <query> [publisher_id]": "Search datasets by query"
            },
            "examples": [
                "spain_data.py catalogues",
                "spain_data.py catalogue-details /sector-publico/org/L01280040",
                "spain_data.py datasets /sector-publico/org/L01280040",
                "spain_data.py dataset-details callejero-alcorcon",
                "spain_data.py resources callejero-alcorcon",
                "spain_data.py resource-info callejero-alcorcon 0",
                "spain_data.py search \"callejero\" /sector-publico/org/L01280040"
            ]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "catalogues":
            result = list_catalogues()

        elif command == "catalogue-details":
            if len(sys.argv) < 3:
                result = {"error": "Publisher ID is required: catalogue-details <publisher_id>"}
            else:
                publisher_id = sys.argv[2]
                result = get_catalogue_details(publisher_id)

        elif command == "datasets":
            publisher_id = sys.argv[2] if len(sys.argv) > 2 else None
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 else 20
            result = list_datasets(publisher_id, page, page_size)

        elif command == "dataset-details":
            if len(sys.argv) < 3:
                result = {"error": "Dataset ID is required: dataset-details <dataset_id>"}
            else:
                dataset_id = sys.argv[2]
                result = get_dataset_details(dataset_id)

        elif command == "resources":
            if len(sys.argv) < 3:
                result = {"error": "Dataset ID is required: resources <dataset_id>"}
            else:
                dataset_id = sys.argv[2]
                result = list_dataset_resources(dataset_id)

        elif command == "resource-info":
            if len(sys.argv) < 3:
                result = {"error": "Dataset ID is required: resource-info <dataset_id> [index|title]"}
            else:
                dataset_id = sys.argv[2]
                resource_param = sys.argv[3] if len(sys.argv) > 3 else None

                resource_index = None
                resource_title = None

                if resource_param:
                    try:
                        resource_index = int(resource_param)
                    except ValueError:
                        resource_title = resource_param

                result = get_resource_download_info(dataset_id, resource_index, resource_title)

        elif command == "search":
            if len(sys.argv) < 3:
                result = {"error": "Search query is required: search <query> [publisher_id]"}
            else:
                query = sys.argv[2]
                publisher_id = sys.argv[3] if len(sys.argv) > 3 else None
                result = search_datasets(query, publisher_id)

        else:
            result = {"error": f"Unknown command: {command}"}

    except ValueError as e:
        result = {"error": f"Invalid argument: {str(e)}"}
    except IndexError:
        result = {"error": "Missing required arguments"}
    except Exception as e:
        result = {"error": f"Error: {str(e)}"}

    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()