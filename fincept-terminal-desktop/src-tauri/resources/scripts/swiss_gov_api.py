#!/usr/bin/env python3
"""
Swiss Government API Wrapper (opendata.swiss)
Fetches Swiss government data using CKAN platform structure:
1. Catalogue - Lists all data publishers (organizations)
2. Datasets - Lists datasets from a specific publisher or search across all
3. Resources - Gets actual data files (CSV/XLS/JSON) for a dataset

Features:
- Automatic translation of non-English content to English
- Modular endpoint design - one endpoint failure doesn't affect others
- Comprehensive error handling
- Hierarchical data access pattern

Usage: python swiss_gov_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any
from datetime import datetime
import urllib.parse
import re

# --- 1. CONFIGURATION ---
BASE_URL = "https://opendata.swiss/api/3/action"
API_KEY = os.environ.get('SWISS_GOV_API_KEY')
TIMEOUT = 30

# Language detection and translation patterns
GERMAN_PATTERNS = [
    r'\b(das|der|die|und|in|den|von|zu|dem|des|im|ein|eine|mit|für|auf|ist|sind|haben|wurde)\b',
    r'\b(Statistik|Daten|Organisation|Bundesamt|Schweiz|Kanton|Gemeinde)\b'
]

FRENCH_PATTERNS = [
    r'\b(le|la|les|de|du|des|et|en|pour|dans|une|avec|par|sur|est|sont|ont|été)\b',
    r'\b(statistique|données|organisation|office|fédéral|suisse|canton|commune)\b'
]

ITALIAN_PATTERNS = [
    r'\b(il|lo|la|le|di|del|della|dei|degli|e|in|un|uno|con|per|su|è|sono|hanno)\b',
    r'\b(statistica|dati|organizzazione|ufficio|federale|svizzera|cantone|comune)\b'
]

def _detect_language(text: str) -> str:
    """
    Detect the language of the given text

    Args:
        text: Text to analyze

    Returns:
        Detected language: 'de', 'fr', 'it', or 'en'
    """
    if not text or not isinstance(text, str):
        return 'en'

    text_lower = text.lower()

    # Count matches for each language pattern
    de_matches = sum(len(re.findall(pattern, text_lower)) for pattern in GERMAN_PATTERNS)
    fr_matches = sum(len(re.findall(pattern, text_lower)) for pattern in FRENCH_PATTERNS)
    it_matches = sum(len(re.findall(pattern, text_lower)) for pattern in ITALIAN_PATTERNS)

    # Determine language based on pattern matches
    if de_matches > fr_matches and de_matches > it_matches:
        return 'de'
    elif fr_matches > de_matches and fr_matches > it_matches:
        return 'fr'
    elif it_matches > de_matches and it_matches > fr_matches:
        return 'it'
    else:
        return 'en'

def _translate_text(text: str, source_lang: Optional[str] = None) -> str:
    """
    Simple translation dictionary for common Swiss government terms
    In a production environment, this would use a proper translation API

    Args:
        text: Text to translate
        source_lang: Source language (auto-detected if not provided)

    Returns:
        Translated text in English
    """
    if not text:
        return text

    if source_lang is None:
        source_lang = _detect_language(text)

    if source_lang == 'en':
        return text

    # Simple translation dictionaries
    translations = {
        'de': {
            'Bundesamt': 'Federal Office',
            'Statistik': 'Statistics',
            'Daten': 'Data',
            'Organisation': 'Organization',
            'Schweiz': 'Switzerland',
            'Kanton': 'Canton',
            'Gemeinde': 'Municipality',
            'Jahr': 'Year',
            'Monat': 'Month',
            'Tag': 'Day',
            'Verkehr': 'Transport',
            'Gesundheit': 'Health',
            'Bildung': 'Education',
            'Umwelt': 'Environment',
            'Wirtschaft': 'Economy',
            'Bevölkerung': 'Population',
            'Tourismus': 'Tourism',
            'Energie': 'Energy',
            'Verwaltung': 'Administration',
            'Justiz': 'Justice',
            'Polizei': 'Police',
            'Armee': 'Army',
            'Sozial': 'Social',
            'Kultur': 'Culture',
            'Sport': 'Sport',
            'Medien': 'Media',
            'Wissenschaft': 'Science',
            'Technologie': 'Technology',
            'Innovation': 'Innovation',
            'Digital': 'Digital',
            'Nachhaltigkeit': 'Sustainability',
            'Klima': 'Climate',
            'Wetter': 'Weather',
            'Geologie': 'Geology',
            'Karte': 'Map',
            'Lage': 'Location',
            'Koordinaten': 'Coordinates',
            'Höhe': 'Altitude',
            'Fläche': 'Area',
            'Einwohner': 'Inhabitants',
            'Haushalte': 'Households',
            'Arbeit': 'Work',
            'Arbeitslosigkeit': 'Unemployment',
            'Einkommen': 'Income',
            'Steuern': 'Taxes',
            'Wahlen': 'Elections',
            'Abstimmungen': 'Voting',
            'Gesetz': 'Law',
            'Verordnung': 'Ordinance',
            'Bericht': 'Report',
            'Studie': 'Study',
            'Analyse': 'Analysis',
            'Prognose': 'Forecast',
            'Plan': 'Plan',
            'Programm': 'Program',
            'Projekt': 'Project',
            'Initiative': 'Initiative',
            'Massnahme': 'Measure',
            'Entwicklung': 'Development',
            'Trend': 'Trend',
            'Indikator': 'Indicator',
            'Index': 'Index',
            'Statistische': 'Statistical',
            'Erhebung': 'Survey',
            'Zensus': 'Census',
            'Register': 'Register',
            'Verzeichnis': 'Directory',
            'Dokument': 'Document',
            'Publikation': 'Publication',
            'Artikel': 'Article',
            'Pressemitteilung': 'Press Release',
            'Information': 'Information',
            'Hilfe': 'Help',
            'Kontakt': 'Contact',
            'Impressum': 'Imprint',
            'Datenschutz': 'Data Protection',
            'Nutzungsbedingungen': 'Terms of Use'
        },
        'fr': {
            'Office fédéral': 'Federal Office',
            'Statistique': 'Statistics',
            'Données': 'Data',
            'Organisation': 'Organization',
            'Suisse': 'Switzerland',
            'Canton': 'Canton',
            'Commune': 'Municipality',
            'Année': 'Year',
            'Mois': 'Month',
            'Jour': 'Day',
            'Transport': 'Transport',
            'Santé': 'Health',
            'Éducation': 'Education',
            'Environnement': 'Environment',
            'Économie': 'Economy',
            'Population': 'Population',
            'Tourisme': 'Tourism',
            'Énergie': 'Energy',
            'Administration': 'Administration',
            'Justice': 'Justice',
            'Police': 'Police',
            'Armée': 'Army',
            'Social': 'Social',
            'Culture': 'Culture',
            'Sport': 'Sport',
            'Médias': 'Media',
            'Science': 'Science',
            'Technologie': 'Technology',
            'Innovation': 'Innovation',
            'Numérique': 'Digital',
            'Durabilité': 'Sustainability',
            'Climat': 'Climate',
            'Météo': 'Weather',
            'Géologie': 'Geology',
            'Carte': 'Map',
            'Lieu': 'Location',
            'Coordonnées': 'Coordinates',
            'Altitude': 'Altitude',
            'Surface': 'Area',
            'Habitants': 'Inhabitants',
            'Ménages': 'Households',
            'Travail': 'Work',
            'Chômage': 'Unemployment',
            'Revenu': 'Income',
            'Impôts': 'Taxes',
            'Élections': 'Elections',
            'Votation': 'Voting',
            'Loi': 'Law',
            'Ordonnance': 'Ordinance',
            'Rapport': 'Report',
            'Étude': 'Study',
            'Analyse': 'Analysis',
            'Prévision': 'Forecast',
            'Plan': 'Plan',
            'Programme': 'Program',
            'Projet': 'Project',
            'Initiative': 'Initiative',
            'Mesure': 'Measure',
            'Développement': 'Development',
            'Tendance': 'Trend',
            'Indicateur': 'Indicator',
            'Index': 'Index',
            'Enquête': 'Survey',
            'Recensement': 'Census',
            'Registre': 'Register',
            'Répertoire': 'Directory',
            'Document': 'Document',
            'Publication': 'Publication',
            'Article': 'Article',
            'Communiqué': 'Press Release',
            'Information': 'Information',
            'Aide': 'Help',
            'Contact': 'Contact',
            'Mentions légales': 'Legal Notice',
            'Protection des données': 'Data Protection',
            "Conditions d'utilisation": 'Terms of Use'
        },
        'it': {
            'Ufficio federale': 'Federal Office',
            'Statistica': 'Statistics',
            'Dati': 'Data',
            'Organizzazione': 'Organization',
            'Svizzera': 'Switzerland',
            'Cantone': 'Canton',
            'Comune': 'Municipality',
            'Anno': 'Year',
            'Mese': 'Month',
            'Giorno': 'Day',
            'Trasporto': 'Transport',
            'Salute': 'Health',
            'Educazione': 'Education',
            'Ambiente': 'Environment',
            'Economia': 'Economy',
            'Popolazione': 'Population',
            'Turismo': 'Tourism',
            'Energia': 'Energy',
            'Amministrazione': 'Administration',
            'Giustizia': 'Justice',
            'Polizia': 'Police',
            'Esercito': 'Army',
            'Sociale': 'Social',
            'Cultura': 'Culture',
            'Sport': 'Sport',
            'Media': 'Media',
            'Scienza': 'Science',
            'Tecnologia': 'Technology',
            'Innovazione': 'Innovation',
            'Digitale': 'Digital',
            'Sostenibilità': 'Sustainability',
            'Clima': 'Climate',
            'Tempo': 'Weather',
            'Geologia': 'Geology',
            'Mappa': 'Map',
            'Luogo': 'Location',
            'Coordinate': 'Coordinates',
            'Altitudine': 'Altitude',
            'Superficie': 'Area',
            'Abitanti': 'Inhabitants',
            'Famiglie': 'Households',
            'Lavoro': 'Work',
            'Disoccupazione': 'Unemployment',
            'Reddito': 'Income',
            'Imposte': 'Taxes',
            'Elezioni': 'Elections',
            'Votazione': 'Voting',
            'Legge': 'Law',
            'Ordinanza': 'Ordinance',
            'Rapporto': 'Report',
            'Studio': 'Study',
            'Analisi': 'Analysis',
            'Previsione': 'Forecast',
            'Piano': 'Plan',
            'Programma': 'Program',
            'Progetto': 'Project',
            'Iniziativa': 'Initiative',
            'Misura': 'Measure',
            'Sviluppo': 'Development',
            'Tendenza': 'Trend',
            'Indicatore': 'Indicator',
            'Indice': 'Index',
            'Indagine': 'Survey',
            'Censimento': 'Census',
            'Registro': 'Register',
            'Elenco': 'Directory',
            'Documento': 'Document',
            'Pubblicazione': 'Publication',
            'Articolo': 'Article',
            'Comunicato': 'Press Release',
            'Informazione': 'Information',
            'Aiuto': 'Help',
            'Contatto': 'Contact',
            'Note legali': 'Legal Notice',
            'Protezione dei dati': 'Data Protection',
            'Condizioni di utilizzo': 'Terms of Use'
        }
    }

    if source_lang in translations:
        translated_text = text
        for original, english in translations[source_lang].items():
            translated_text = re.sub(r'\b' + re.escape(original) + r'\b', english, translated_text, flags=re.IGNORECASE)
        return translated_text

    return text

def _make_request(action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for opendata.swiss CKAN API

    Args:
        action: CKAN action name (e.g., 'organization_list', 'package_search')
        params: Query parameters for the request

    Returns:
        JSON response data or error information
    """
    try:
        url = f"{BASE_URL}/{action}"

        # Default parameters
        if params is None:
            params = {}

        # Setup headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Add API key to header if available
        if API_KEY:
            headers['Authorization'] = API_KEY

        # Make request with timeout
        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # Check for CKAN API errors
        if not data.get('success', False):
            error_msg = data.get('error', {}).get('message', 'Unknown CKAN API error')
            return {
                "data": [],
                "metadata": {"source": "opendata.swiss", "last_updated": datetime.now().isoformat()},
                "error": f"CKAN API Error: {error_msg}"
            }

        return {
            "data": data.get('result', []),
            "metadata": {
                "source": "opendata.swiss",
                "last_updated": datetime.now().isoformat(),
                "action": action
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"HTTP Error: {e.response.status_code} - {e.response.text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Request timeout. The opendata.swiss API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error. Unable to connect to opendata.swiss API."
        }
    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Network or request error: {str(e)}"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {},
            "error": "Invalid JSON response from opendata.swiss API"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"An unexpected error occurred: {str(e)}"
        }

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== CATALOGUE (ORGANIZATIONS/PUBLISHERS) ======
def get_publishers() -> Dict[str, Any]:
    """
    Get list of all data publishers (organizations) in opendata.swiss

    Returns:
        JSON response with publisher list
    """
    try:
        result = _make_request("organization_list")

        if result["error"]:
            return result

        # Enhance publisher data with basic info
        enhanced_data = []
        publishers = result.get("data", [])

        for publisher_id in publishers:
            # Translate publisher ID to readable name
            display_name = publisher_id.replace("-", " ").title()
            translated_name = _translate_text(display_name)

            enhanced_publisher = {
                "id": publisher_id,
                "name": translated_name,
                "original_name": display_name,
                "display_name": translated_name
            }
            enhanced_data.append(enhanced_publisher)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching publishers: {str(e)}"
        }

def get_publisher_details(publisher_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific publisher

    Args:
        publisher_id: The unique ID of the publisher

    Returns:
        JSON response with publisher details
    """
    try:
        params = {'id': publisher_id}
        result = _make_request("organization_show", params)

        if result["error"]:
            return result

        # Enhance publisher data
        publisher_data = result.get("data", {})

        # Detect and translate content
        title = publisher_data.get("title", "")
        description = publisher_data.get("description", "")

        title_lang = _detect_language(title)
        desc_lang = _detect_language(description)

        enhanced_publisher = {
            "id": publisher_data.get("id"),
            "name": publisher_data.get("name"),
            "title": _translate_text(title, title_lang),
            "original_title": title,
            "description": _translate_text(description, desc_lang),
            "original_description": description,
            "image_url": publisher_data.get("image_display_url"),
            "created": publisher_data.get("created"),
            "num_datasets": publisher_data.get("package_count", 0),
            "users": publisher_data.get("users", []),
            "language_detected": {
                "title": title_lang,
                "description": desc_lang
            }
        }

        result["data"] = enhanced_publisher
        result["metadata"]["publisher_id"] = publisher_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching publisher details: {str(e)}"
        }

# ====== DATASETS ======
def get_datasets_by_publisher(publisher_id: str, rows: int = 100) -> Dict[str, Any]:
    """
    Get all datasets published by a specific publisher

    Args:
        publisher_id: The unique ID of the publisher
        rows: Number of datasets to return (default: 100)

    Returns:
        JSON response with dataset list
    """
    try:
        # Search for datasets by owner_org
        query = f"owner_org:{publisher_id}"
        params = {'q': query, 'rows': rows}

        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            # Detect and translate content
            title = dataset.get("title", "")
            notes = dataset.get("notes", "")

            title_lang = _detect_language(title)
            notes_lang = _detect_language(notes)

            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": _translate_text(title, title_lang),
                "original_title": title,
                "notes": _translate_text(notes, notes_lang),
                "original_notes": notes,
                "publisher_id": publisher_id,
                "metadata_created": dataset.get("metadata_created"),
                "metadata_modified": dataset.get("metadata_modified"),
                "state": dataset.get("state"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])],
                "language_detected": {
                    "title": title_lang,
                    "notes": notes_lang
                }
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["publisher_id"] = publisher_id
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching datasets: {str(e)}"
        }

def search_datasets(query: str, rows: int = 50, fq: Optional[str] = None,
                   sort: Optional[str] = None) -> Dict[str, Any]:
    """
    Search for datasets across all publishers

    Args:
        query: Search query string
        rows: Number of results to return (default: 50)
        fq: Filter query (e.g., 'tags:hospitals' or 'organization:specific-org')
        sort: Sort order (e.g., 'title_string_en asc' or 'metadata_modified desc')

    Returns:
        JSON response with search results
    """
    try:
        params = {'q': query, 'rows': rows}

        if fq:
            params["fq"] = fq
        if sort:
            params["sort"] = sort

        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            # Detect and translate content
            title = dataset.get("title", "")
            notes = dataset.get("notes", "")

            title_lang = _detect_language(title)
            notes_lang = _detect_language(notes)

            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": _translate_text(title, title_lang),
                "original_title": title,
                "notes": _translate_text(notes, notes_lang),
                "original_notes": notes,
                "organization": dataset.get("organization", {}).get("name") if dataset.get("organization") else None,
                "metadata_created": dataset.get("metadata_created"),
                "metadata_modified": dataset.get("metadata_modified"),
                "state": dataset.get("state"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])],
                "language_detected": {
                    "title": title_lang,
                    "notes": notes_lang
                }
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["query"] = query
        result["metadata"]["filter"] = fq
        result["metadata"]["sort"] = sort
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching datasets: {str(e)}"
        }

def get_dataset_details(dataset_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset

    Args:
        dataset_id: The unique ID or name of the dataset

    Returns:
        JSON response with dataset details
    """
    try:
        params = {'id': dataset_id}
        result = _make_request("package_show", params)

        if result["error"]:
            return result

        # Enhance dataset data
        dataset_data = result.get("data", {})

        # Detect and translate content
        title = dataset_data.get("title", "")
        notes = dataset_data.get("notes", "")

        title_lang = _detect_language(title)
        notes_lang = _detect_language(notes)

        enhanced_dataset = {
            "id": dataset_data.get("id"),
            "name": dataset_data.get("name"),
            "title": _translate_text(title, title_lang),
            "original_title": title,
            "notes": _translate_text(notes, notes_lang),
            "original_notes": notes,
            "url": dataset_data.get("url"),
            "author": dataset_data.get("author"),
            "author_email": dataset_data.get("author_email"),
            "maintainer": dataset_data.get("maintainer"),
            "maintainer_email": dataset_data.get("maintainer_email"),
            "license_id": dataset_data.get("license_id"),
            "license_title": dataset_data.get("license_title"),
            "organization": dataset_data.get("organization", {}).get("name") if dataset_data.get("organization") else None,
            "metadata_created": dataset_data.get("metadata_created"),
            "metadata_modified": dataset_data.get("metadata_modified"),
            "state": dataset_data.get("state"),
            "version": dataset_data.get("version"),
            "tags": [tag.get("display_name") for tag in dataset_data.get("tags", [])],
            "language_detected": {
                "title": title_lang,
                "notes": notes_lang
            }
        }

        result["data"] = enhanced_dataset
        result["metadata"]["dataset_id"] = dataset_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching dataset details: {str(e)}"
        }

# ====== RESOURCES (DATA FILES) ======
def get_dataset_resources(dataset_id: str) -> Dict[str, Any]:
    """
    Get all data files (resources) for a specific dataset

    Args:
        dataset_id: The unique ID or name of the dataset

    Returns:
        JSON response with resource list
    """
    try:
        params = {'id': dataset_id}
        result = _make_request("package_show", params)

        if result["error"]:
            return result

        # Extract resources from dataset
        dataset_data = result.get("data", {})
        resources = dataset_data.get("resources", [])

        # Enhance resource data
        enhanced_data = []
        for resource in resources:
            # Detect and translate content
            name = resource.get("name", "")
            description = resource.get("description", "")

            name_lang = _detect_language(name)
            desc_lang = _detect_language(description)

            enhanced_resource = {
                "id": resource.get("id"),
                "name": _translate_text(name, name_lang),
                "original_name": name,
                "description": _translate_text(description, desc_lang),
                "original_description": description,
                "format": resource.get("format", ""),
                "url": resource.get("url", ""),
                "size": resource.get("size"),
                "mimetype": resource.get("mimetype"),
                "mimetype_inner": resource.get("mimetype_inner"),
                "created": resource.get("created"),
                "last_modified": resource.get("last_modified"),
                "resource_type": resource.get("resource_type"),
                "package_id": dataset_id,
                "position": resource.get("position"),
                "cache_last_updated": resource.get("cache_last_updated"),
                "webstore_last_updated": resource.get("webstore_last_updated"),
                "language_detected": {
                    "name": name_lang,
                    "description": desc_lang
                }
            }
            enhanced_data.append(enhanced_resource)

        result["data"] = enhanced_data
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["dataset_name"] = dataset_data.get("name")
        result["metadata"]["resource_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching dataset resources: {str(e)}"
        }

def get_resource_info(resource_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific resource

    Args:
        resource_id: The unique ID of the resource

    Returns:
        JSON response with resource details
    """
    try:
        params = {'id': resource_id}
        result = _make_request("resource_show", params)

        if result["error"]:
            return result

        # Enhance resource data
        resource_data = result.get("data", {})

        # Detect and translate content
        name = resource_data.get("name", "")
        description = resource_data.get("description", "")

        name_lang = _detect_language(name)
        desc_lang = _detect_language(description)

        enhanced_resource = {
            "id": resource_data.get("id"),
            "name": _translate_text(name, name_lang),
            "original_name": name,
            "description": _translate_text(description, desc_lang),
            "original_description": description,
            "format": resource_data.get("format", ""),
            "url": resource_data.get("url", ""),
            "size": resource_data.get("size"),
            "mimetype": resource_data.get("mimetype"),
            "mimetype_inner": resource_data.get("mimetype_inner"),
            "created": resource_data.get("created"),
            "last_modified": resource_data.get("last_modified"),
            "resource_type": resource_data.get("resource_type"),
            "package_id": resource_data.get("package_id"),
            "position": resource_data.get("position"),
            "cache_last_updated": resource_data.get("cache_last_updated"),
            "webstore_last_updated": resource_data.get("webstore_last_updated"),
            "language_detected": {
                "name": name_lang,
                "description": desc_lang
            }
        }

        result["data"] = enhanced_resource
        result["metadata"]["resource_id"] = resource_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching resource info: {str(e)}"
        }

def download_resource_preview(resource_url: str, max_lines: int = 10) -> Dict[str, Any]:
    """
    Download a preview of a resource (first few lines of CSV/TSV)

    Args:
        resource_url: Direct URL to the resource file
        max_lines: Maximum number of lines to preview (default: 10)

    Returns:
        JSON response with preview data
    """
    try:
        headers = {
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Make request to download file
        response = requests.get(resource_url, headers=headers, timeout=TIMEOUT, stream=True)
        response.raise_for_status()

        # Check if it's a text-based file
        content_type = response.headers.get('content-type', '').lower()
        if not ('csv' in content_type or 'text' in content_type or 'excel' in content_type or 'zip' in content_type):
            return {
                "data": [],
                "metadata": {"url": resource_url, "content_type": content_type},
                "error": f"Preview not available for file type: {content_type}"
            }

        # Read first few lines
        lines = []
        line_count = 0

        for line in response.iter_lines(decode_unicode=True):
            if line_count >= max_lines:
                break
            if line.strip():  # Skip empty lines
                lines.append(line)
                line_count += 1

        # Try to parse as CSV if it looks like CSV
        preview_data = {
            "raw_lines": lines,
            "line_count": len(lines),
            "url": resource_url,
            "content_type": content_type
        }

        # Basic CSV parsing for preview
        if lines and ',' in lines[0]:
            try:
                import csv
                from io import StringIO

                csv_reader = csv.reader(StringIO('\n'.join(lines)))
                csv_data = list(csv_reader)

                # Translate headers if needed
                headers = csv_data[0] if csv_data else []
                translated_headers = [_translate_text(header, _detect_language(header)) for header in headers]

                preview_data["csv_preview"] = {
                    "headers": headers,
                    "translated_headers": translated_headers,
                    "rows": csv_data[1:] if len(csv_data) > 1 else [],
                    "total_columns": len(csv_data[0]) if csv_data else 0
                }
            except:
                pass  # Keep raw lines if CSV parsing fails

        return {
            "data": preview_data,
            "metadata": {
                "url": resource_url,
                "preview_lines": len(lines),
                "content_type": content_type
            },
            "error": None
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": {},
            "metadata": {"url": resource_url},
            "error": f"Failed to download resource: {str(e)}"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {"url": resource_url},
            "error": f"Error processing resource: {str(e)}"
        }

# ====== UTILITY FUNCTIONS ======
def get_popular_publishers(limit: int = 20) -> Dict[str, Any]:
    """
    Get popular publishers based on dataset count

    Args:
        limit: Maximum number of publishers to return

    Returns:
        JSON response with popular publishers
    """
    try:
        # Get all publishers first
        publishers_result = get_publishers()

        if publishers_result["error"]:
            return publishers_result

        publishers = publishers_result.get("data", [])

        # Get dataset count for each publisher (limited for performance)
        popular_publishers = []

        for publisher in publishers[:limit]:
            try:
                datasets_result = get_datasets_by_publisher(publisher["id"], 1)
                if not datasets_result["error"]:
                    search_data = datasets_result.get("metadata", {})
                    total_count = search_data.get("total_count", 0)

                    popular_publishers.append({
                        "id": publisher["id"],
                        "name": publisher["name"],
                        "dataset_count": total_count
                    })
            except:
                continue  # Skip if publisher fails

        # Sort by dataset count
        popular_publishers.sort(key=lambda x: x["dataset_count"], reverse=True)

        return {
            "data": popular_publishers,
            "metadata": {
                "count": len(popular_publishers),
                "limit": limit
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching popular publishers: {str(e)}"
        }

def get_recent_datasets(limit: int = 50) -> Dict[str, Any]:
    """
    Get recently updated datasets

    Args:
        limit: Maximum number of datasets to return

    Returns:
        JSON response with recent datasets
    """
    try:
        params = {'rows': limit, 'sort': 'metadata_modified desc'}
        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            # Detect and translate content
            title = dataset.get("title", "")
            notes = dataset.get("notes", "")

            title_lang = _detect_language(title)
            notes_lang = _detect_language(notes)

            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": _translate_text(title, title_lang),
                "original_title": title,
                "notes": _translate_text(notes, notes_lang),
                "original_notes": notes,
                "organization": dataset.get("organization", {}).get("name") if dataset.get("organization") else None,
                "metadata_modified": dataset.get("metadata_modified"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])],
                "language_detected": {
                    "title": title_lang,
                    "notes": notes_lang
                }
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching recent datasets: {str(e)}"
        }

def test_api_connectivity() -> Dict[str, Any]:
    """
    Test basic connectivity to opendata.swiss API

    Returns:
        Dict with connectivity test results
    """
    results = {}

    # Test Package List API
    try:
        package_list_response = _make_request("package_list", {"limit": 1})
        results["package_list_api"] = {
            "status": "connected" if not package_list_response["error"] else "error",
            "message": package_list_response["error"] or "Successfully connected to package list",
            "response_time_ms": 0  # Could be enhanced with timing
        }
    except Exception as e:
        results["package_list_api"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test Package Search API
    try:
        search_response = search_datasets("statistik", rows=1)
        results["package_search_api"] = {
            "status": "connected" if not search_response["error"] else "error",
            "message": search_response["error"] or "Successfully connected to package search",
            "response_time_ms": 0
        }
    except Exception as e:
        results["package_search_api"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test Translation Function
    try:
        test_text = "Bundesamt für Statistik"
        translated = _translate_text(test_text, "de")
        results["translation_function"] = {
            "status": "working" if translated != test_text else "error",
            "message": f"Translation working: '{test_text}' -> '{translated}'",
            "test_original": test_text,
            "test_translated": translated
        }
    except Exception as e:
        results["translation_function"] = {
            "status": "error",
            "message": f"Translation error: {str(e)}"
        }

    return {
        "data": results,
        "metadata": {
            "test_timestamp": datetime.now().isoformat(),
            "api_key_configured": bool(API_KEY)
        },
        "error": None
    }

# --- 3. CLI INTERFACE ---
def main():
    """CLI interface for Swiss Government API"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python swiss_gov_api.py <command> <args>",
            "available_commands": [
                "publishers",
                "publisher-details <publisher_id>",
                "datasets <publisher_id> [rows]",
                "search <query> [rows] [fq] [sort]",
                "dataset-details <dataset_id>",
                "resources <dataset_id>",
                "resource-info <resource_id>",
                "preview <resource_url>",
                "popular-publishers [limit]",
                "recent-datasets [limit]",
                "test-connectivity"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "publishers":
            result = get_publishers()

        elif command == "publisher-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: publisher-details <publisher_id>"}))
                sys.exit(1)
            publisher_id = sys.argv[2]
            result = get_publisher_details(publisher_id)

        elif command == "datasets":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datasets <publisher_id> [rows]"}))
                sys.exit(1)
            publisher_id = sys.argv[2]
            rows = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_datasets_by_publisher(publisher_id, rows)

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: search <query> [rows] [fq] [sort]"}))
                sys.exit(1)
            query = sys.argv[2]
            rows = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else 50
            fq = sys.argv[4] if len(sys.argv) > 4 and not sys.argv[4].isdigit() else None
            sort = sys.argv[5] if len(sys.argv) > 5 else None
            result = search_datasets(query, rows, fq, sort)

        elif command == "dataset-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: dataset-details <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = get_dataset_details(dataset_id)

        elif command == "resources":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: resources <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = get_dataset_resources(dataset_id)

        elif command == "resource-info":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: resource-info <resource_id>"}))
                sys.exit(1)
            resource_id = sys.argv[2]
            result = get_resource_info(resource_id)

        elif command == "preview":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: preview <resource_url>"}))
                sys.exit(1)
            resource_url = sys.argv[2]
            result = download_resource_preview(resource_url)

        elif command == "popular-publishers":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20
            result = get_popular_publishers(limit)

        elif command == "recent-datasets":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 50
            result = get_recent_datasets(limit)

        elif command == "test-connectivity":
            result = test_api_connectivity()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "publishers",
                    "publisher-details <publisher_id>",
                    "datasets <publisher_id> [rows]",
                    "search <query> [rows] [fq] [sort]",
                    "dataset-details <dataset_id>",
                    "resources <dataset_id>",
                    "resource-info <resource_id>",
                    "preview <resource_url>",
                    "popular-publishers [limit]",
                    "recent-datasets [limit]",
                    "test-connectivity"
                ]
            }

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({
            "error": f"Command execution failed: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }))

if __name__ == "__main__":
    main()