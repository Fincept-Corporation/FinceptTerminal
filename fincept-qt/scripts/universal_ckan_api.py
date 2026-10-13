#!/usr/bin/env python3
"""
Universal CKAN Data Portals API Wrapper

One client for several national CKAN open-data portals. Used by the Gov Data screen's
"CKAN Portals" provider (portal selector) and callable from the command line.

Positional CLI (the convention the app uses — JSON on stdout, non-zero exit on error):
    python universal_ckan_api.py portals
    python universal_ckan_api.py publishers <portal> [limit]
    python universal_ckan_api.py datasets   <portal> <organization> [rows]
    python universal_ckan_api.py search     <portal> <query ...> [rows]
    python universal_ckan_api.py resources  <portal> <dataset_id>
    python universal_ckan_api.py dataset    <portal> <dataset_id>
    python universal_ckan_api.py test       <portal>

Flag CLI (kept for backward compatibility):
    python universal_ckan_api.py --country ie --action search_datasets --query "climate" [--limit 20]
    actions: list_organizations, get_organization_details, list_datasets, search_datasets,
             get_dataset_details, get_datasets_by_organization, get_dataset_resources,
             get_resource_details, test_connection, supported_countries

Portals (all keyless, verified 2026-10): Argentina, Australia, Chile, Ireland, Italy, Latvia,
Mexico, Netherlands, Slovenia, United Kingdom, Uruguay.
Removed: United States (catalog.data.gov retired its CKAN API — every action answers 404) and
Brazil (dados.gov.br now requires an API token — HTTP 401).

API documentation: https://docs.ckan.org/en/2.9/api/
"""

import json
import sys
import time
from datetime import datetime
from typing import Any, Dict, List, Optional

import requests

# --- 1. CONFIGURATION ---

CKAN_PORTALS: Dict[str, Dict[str, str]] = {
    'ar': {'name': 'Argentina (datos.gob.ar)', 'base_url': 'https://datos.gob.ar/api/3/action',
           'country_code': 'AR'},
    'au': {'name': 'Australia (data.gov.au)', 'base_url': 'https://data.gov.au/data/api/3/action',
           'country_code': 'AU'},
    'cl': {'name': 'Chile (datos.gob.cl)', 'base_url': 'https://datos.gob.cl/api/3/action',
           'country_code': 'CL'},
    'ie': {'name': 'Ireland (data.gov.ie)', 'base_url': 'https://data.gov.ie/api/3/action',
           'country_code': 'IE'},
    'it': {'name': 'Italy (dati.gov.it)', 'base_url': 'https://www.dati.gov.it/opendata/api/3/action',
           'country_code': 'IT'},
    'lv': {'name': 'Latvia (data.gov.lv)', 'base_url': 'https://data.gov.lv/dati/api/3/action',
           'country_code': 'LV'},
    'mx': {'name': 'Mexico (datos.gob.mx)', 'base_url': 'https://www.datos.gob.mx/api/3/action',
           'country_code': 'MX'},
    'nl': {'name': 'Netherlands (data.overheid.nl)', 'base_url': 'https://data.overheid.nl/data/api/3/action',
           'country_code': 'NL'},
    'si': {'name': 'Slovenia (podatki.gov.si)', 'base_url': 'https://podatki.gov.si/api/3/action',
           'country_code': 'SI'},
    'uk': {'name': 'United Kingdom (data.gov.uk)', 'base_url': 'https://data.gov.uk/api/3/action',
           'country_code': 'UK'},
    'uy': {'name': 'Uruguay (catalogodatos.gub.uy)', 'base_url': 'https://catalogodatos.gub.uy/api/3/action',
           'country_code': 'UY'},
}

DEFAULT_PORTAL = 'ie'

TIMEOUT = 30
MAX_ATTEMPTS = 2          # one retry on timeouts / connection errors / 5xx / non-JSON bodies
NOTES_MAX_CHARS = 400     # dataset descriptions are trimmed in list views (full text via `dataset`)

HEADERS = {
    'User-Agent': 'Fincept-Terminal/1.0',
    'Accept': 'application/json',
}


def _log(msg: str) -> None:
    print(msg, file=sys.stderr)


# --- 2. REQUEST HELPERS ---

def _get_portal_config(country_code: str) -> Dict[str, str]:
    code = (country_code or '').strip().lower()
    if code not in CKAN_PORTALS:
        raise ValueError(f"Unsupported portal '{country_code}'. Supported: {', '.join(sorted(CKAN_PORTALS))}")
    return CKAN_PORTALS[code]


def _metadata(portal: Dict[str, str], action: str, **extra: Any) -> Dict[str, Any]:
    meta = {
        "source": portal['name'],
        "country": portal['country_code'],
        "action": action,
        "last_updated": datetime.now().isoformat(),
    }
    meta.update(extra)
    return meta


def _make_request(country_code: str, action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """GET <base>/<action>?params. Returns {"success", "data", "metadata", "error"}; `data` is CKAN's
    `result` unchanged (callers reshape it)."""
    try:
        portal = _get_portal_config(country_code)
    except ValueError as e:
        return {"success": False, "data": [], "metadata": {}, "error": str(e)}

    url = f"{portal['base_url']}/{action}"
    last_error = "unknown error"
    for attempt in range(MAX_ATTEMPTS):
        if attempt:
            time.sleep(1.5)
        try:
            response = requests.get(url, params=params or {}, headers=HEADERS, timeout=TIMEOUT)
        except requests.exceptions.Timeout:
            last_error = f"Request timed out after {TIMEOUT} s"
            continue
        except requests.exceptions.ConnectionError:
            last_error = f"Could not connect to {portal['name']}"
            continue
        except requests.exceptions.RequestException as e:
            last_error = f"Request failed: {e}"
            break

        if response.status_code >= 500:
            last_error = f"HTTP {response.status_code} from {portal['name']}"
            continue
        try:
            body = response.json()
        except ValueError:
            last_error = f"HTTP {response.status_code}: the portal did not answer with JSON"
            if response.status_code == 200:
                continue  # transient gateway pages are served with 200 on some portals
            break
        if response.status_code >= 400 or not isinstance(body, dict) or not body.get('success', False):
            err = body.get('error') if isinstance(body, dict) else None
            msg = err.get('message') if isinstance(err, dict) else err
            last_error = f"HTTP {response.status_code}: {msg or 'CKAN API error'}"
            break
        return {"success": True, "data": body.get('result'),
                "metadata": _metadata(portal, action, url=url), "error": None}

    return {"success": False, "data": [], "metadata": _metadata(portal, action, url=url), "error": last_error}


def _fail(message: str, metadata: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    return {"success": False, "data": [], "metadata": metadata or {}, "error": message}


def _tag_names(dataset: Dict[str, Any]) -> List[str]:
    names = []
    for t in dataset.get('tags') or []:
        if isinstance(t, dict):
            n = t.get('display_name') or t.get('name')
        else:
            n = str(t)
        if n:
            names.append(n)
    return names


def _slim_dataset(ds: Dict[str, Any]) -> Dict[str, Any]:
    notes = ds.get('notes') or ''
    if len(notes) > NOTES_MAX_CHARS:
        notes = notes[:NOTES_MAX_CHARS].rstrip() + '...'
    org = ds.get('organization') or {}
    resources = ds.get('resources') or []
    formats = []
    for r in resources:
        f = (r.get('format') or '').upper()
        if f and f not in formats:
            formats.append(f)
    return {
        "id": ds.get('id', ''),
        "name": ds.get('name', ''),
        "title": ds.get('title') or ds.get('name', ''),
        "notes": notes,
        "organization": org.get('title') or org.get('name', '') if isinstance(org, dict) else '',
        "num_resources": ds.get('num_resources', len(resources)),
        "resource_formats": formats,
        "metadata_created": ds.get('metadata_created', ''),
        "metadata_modified": ds.get('metadata_modified', ''),
        "license": ds.get('license_title') or ds.get('license_id', ''),
        "tags": _tag_names(ds),
    }


def _slim_resource(res: Dict[str, Any], dataset: Dict[str, Any]) -> Dict[str, Any]:
    size = res.get('size')
    try:
        size = int(size) if size not in (None, '') else None
    except (TypeError, ValueError):
        size = None
    return {
        "id": res.get('id', ''),
        "name": res.get('name') or res.get('description') or res.get('id', ''),
        "description": res.get('description') or '',
        "format": (res.get('format') or '').upper(),
        "size": size,
        "url": res.get('url', ''),
        "last_modified": res.get('last_modified') or res.get('metadata_modified') or res.get('created', ''),
        "dataset_name": dataset.get('name', ''),
        "dataset_title": dataset.get('title', ''),
    }


# --- 3. COMMANDS ---

def get_supported_countries() -> Dict[str, Any]:
    """The portal list (code, name, country) — feeds the panel's portal selector."""
    portals = [{
        "code": code,
        "name": cfg['name'],
        "country_code": cfg['country_code'],
        "base_url": cfg['base_url'],
        "default": code == DEFAULT_PORTAL,
    } for code, cfg in sorted(CKAN_PORTALS.items(), key=lambda kv: kv[1]['name'])]
    return {"success": True, "data": portals,
            "metadata": {"source": "Universal CKAN API Wrapper", "count": len(portals),
                         "default": DEFAULT_PORTAL, "last_updated": datetime.now().isoformat()},
            "error": None}


def list_organizations(country_code: str, limit: Optional[int] = 300) -> Dict[str, Any]:
    """Publishers with their dataset counts, largest first (one package_search facet query). Falls
    back to organization_list (names only) for portals whose search facets are unavailable."""
    limit = max(1, min(int(limit or 300), 1000))
    result = _make_request(country_code, 'package_search',
                           {'rows': 0, 'facet.field': '["organization"]', 'facet.limit': limit})
    orgs: List[Dict[str, Any]] = []
    if result['success'] and isinstance(result['data'], dict):
        items = (((result['data'].get('search_facets') or {}).get('organization') or {}).get('items') or [])
        for it in items:
            name = it.get('name', '')
            if not name:
                continue
            orgs.append({"id": name, "name": name, "display_name": it.get('display_name') or name,
                         "dataset_count": int(it.get('count') or 0)})
        orgs.sort(key=lambda o: (-o['dataset_count'], o['display_name'].lower()))

    if not orgs:
        fallback = _make_request(country_code, 'organization_list', {'limit': limit})
        if not fallback['success']:
            return fallback
        for name in fallback['data'] or []:
            if isinstance(name, str) and name:
                orgs.append({"id": name, "name": name, "display_name": name.replace('-', ' ').title(),
                             "dataset_count": -1})
        result = fallback

    meta = dict(result['metadata'])
    meta.update({"action": "list_organizations", "count": len(orgs), "total_count": len(orgs)})
    return {"success": True, "data": orgs, "metadata": meta, "error": None}


def search_datasets(country_code: str, query: str = "", rows: int = 50,
                    organization: Optional[str] = None, tags: Optional[List[str]] = None) -> Dict[str, Any]:
    """package_search; results trimmed to the fields a list view needs."""
    params: Dict[str, Any] = {'rows': max(1, min(int(rows or 50), 1000))}
    if query:
        params['q'] = query
    filters = []
    if organization:
        filters.append(f'organization:"{organization}"')
    if tags:
        filters.append('(' + ' OR '.join(f'tags:"{t}"' for t in tags) + ')')
    if filters:
        params['fq'] = ' AND '.join(filters)
    if not query:
        params['sort'] = 'metadata_modified desc'

    result = _make_request(country_code, 'package_search', params)
    if not result['success']:
        return result
    payload = result['data'] if isinstance(result['data'], dict) else {}
    datasets = [_slim_dataset(d) for d in payload.get('results') or [] if isinstance(d, dict)]
    meta = dict(result['metadata'])
    meta.update({"action": "search_datasets", "query": query, "organization": organization,
                 "count": len(datasets), "total_count": int(payload.get('count') or len(datasets))})
    return {"success": True, "data": datasets, "metadata": meta, "error": None}


def get_datasets_by_organization(country_code: str, organization_id: str,
                                 limit: Optional[int] = 100) -> Dict[str, Any]:
    """Datasets of one publisher, newest first — a single filtered package_search (the old version
    fetched every package separately)."""
    if not organization_id:
        return _fail("An organization id is required")
    result = search_datasets(country_code, "", limit or 100, organization=organization_id)
    if result['success']:
        result['metadata']['action'] = 'get_datasets_by_organization'
    return result


def list_datasets(country_code: str, limit: Optional[int] = 50) -> Dict[str, Any]:
    """Most recently modified datasets on the portal."""
    result = search_datasets(country_code, "", limit or 50)
    if result['success']:
        result['metadata']['action'] = 'list_datasets'
    return result


def get_dataset_details(country_code: str, dataset_id: str) -> Dict[str, Any]:
    if not dataset_id:
        return _fail("A dataset id is required")
    result = _make_request(country_code, 'package_show', {'id': dataset_id})
    if not result['success']:
        return result
    ds = result['data'] if isinstance(result['data'], dict) else {}
    if not ds:
        return _fail(f"Dataset '{dataset_id}' not found", result['metadata'])
    details = _slim_dataset(ds)
    details['notes'] = ds.get('notes') or ''
    details['resources'] = [_slim_resource(r, ds) for r in ds.get('resources') or [] if isinstance(r, dict)]
    meta = dict(result['metadata'])
    meta.update({"action": "get_dataset_details", "count": 1})
    return {"success": True, "data": details, "metadata": meta, "error": None}


def get_dataset_resources(country_code: str, dataset_id: str) -> Dict[str, Any]:
    if not dataset_id:
        return _fail("A dataset id is required")
    result = _make_request(country_code, 'package_show', {'id': dataset_id})
    if not result['success']:
        return result
    ds = result['data'] if isinstance(result['data'], dict) else {}
    resources = [_slim_resource(r, ds) for r in ds.get('resources') or [] if isinstance(r, dict)]
    meta = dict(result['metadata'])
    meta.update({"action": "get_dataset_resources", "dataset_id": dataset_id,
                 "dataset_name": ds.get('name', ''), "count": len(resources), "total_count": len(resources)})
    return {"success": True, "data": resources, "metadata": meta, "error": None}


def get_organization_details(country_code: str, organization_id: str) -> Dict[str, Any]:
    if not organization_id:
        return _fail("An organization id is required")
    result = _make_request(country_code, 'organization_show', {'id': organization_id})
    if not result['success']:
        return result
    org = result['data'] if isinstance(result['data'], dict) else {}
    return {"success": True,
            "data": {"id": org.get('id', ''), "name": org.get('name', ''),
                     "display_name": org.get('display_name') or org.get('title') or org.get('name', ''),
                     "description": org.get('description') or '',
                     "dataset_count": org.get('package_count', 0),
                     "image_url": org.get('image_url') or '', "created": org.get('created', '')},
            "metadata": result['metadata'], "error": None}


def get_resource_details(country_code: str, resource_id: str) -> Dict[str, Any]:
    if not resource_id:
        return _fail("A resource id is required")
    return _make_request(country_code, 'resource_show', {'id': resource_id})


def test_portal_connection(country_code: str) -> Dict[str, Any]:
    result = _make_request(country_code, 'package_search', {'rows': 0})
    if not result['success']:
        return result
    count = result['data'].get('count', 0) if isinstance(result['data'], dict) else 0
    return {"success": True,
            "data": {"status": "connected", "portal": result['metadata'].get('source'), "datasets": count},
            "metadata": result['metadata'], "error": None}


# --- 4. CLI ---

def _int_arg(value: str, default: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


POSITIONAL_COMMANDS = ('portals', 'supported-countries', 'publishers', 'organizations', 'datasets', 'search',
                       'resources', 'dataset', 'dataset-details', 'recent', 'recent-datasets', 'test')


def run_positional(argv: List[str]) -> Dict[str, Any]:
    command = argv[0].strip().lower().replace('_', '-')
    args = argv[1:]
    if command not in POSITIONAL_COMMANDS:
        return _fail(f"Unknown command '{argv[0]}'. Commands: portals, publishers, datasets, search, "
                     "resources, dataset, recent, test")
    if command in ('portals', 'supported-countries'):
        return get_supported_countries()
    if not args:
        return _fail(f"Usage: {command} <portal> [args...] — portals: {', '.join(sorted(CKAN_PORTALS))}")
    portal = args[0]
    rest = args[1:]
    if command in ('publishers', 'organizations'):
        return list_organizations(portal, _int_arg(rest[0], 300) if rest else 300)
    if command == 'datasets':
        if not rest:
            return _fail("Usage: datasets <portal> <organization> [rows]")
        return get_datasets_by_organization(portal, rest[0], _int_arg(rest[1], 100) if len(rest) > 1 else 100)
    if command == 'search':
        if not rest:
            return _fail("Usage: search <portal> <query> [rows]")
        rows = 50
        if len(rest) > 1 and rest[-1].isdigit():
            rows = int(rest[-1])
            rest = rest[:-1]
        return search_datasets(portal, " ".join(rest), rows)
    if command == 'resources':
        if not rest:
            return _fail("Usage: resources <portal> <dataset_id>")
        return get_dataset_resources(portal, rest[0])
    if command in ('dataset', 'dataset-details'):
        if not rest:
            return _fail("Usage: dataset <portal> <dataset_id>")
        return get_dataset_details(portal, rest[0])
    if command in ('recent', 'recent-datasets'):
        return list_datasets(portal, _int_arg(rest[0], 50) if rest else 50)
    if command == 'test':
        return test_portal_connection(portal)
    return _fail(f"Unknown command '{argv[0]}'. Commands: portals, publishers, datasets, search, "
                 "resources, dataset, recent, test")


def run_flags(argv: List[str]) -> Dict[str, Any]:
    opts: Dict[str, str] = {}
    i = 0
    while i < len(argv):
        if argv[i].startswith('--') and i + 1 < len(argv):
            opts[argv[i][2:]] = argv[i + 1]
            i += 2
        else:
            i += 1
    action = opts.get('action', '')
    country = opts.get('country', '')
    if action == 'supported_countries':
        return get_supported_countries()
    if not country or not action:
        return _fail("--country and --action are required")
    limit = _int_arg(opts.get('limit', ''), 50)
    item_id = opts.get('id', '')
    org = opts.get('org', '')
    if action == 'list_organizations':
        return list_organizations(country, limit)
    if action == 'get_organization_details':
        return get_organization_details(country, org)
    if action == 'list_datasets':
        return list_datasets(country, limit)
    if action == 'search_datasets':
        return search_datasets(country, opts.get('query', ''), limit, org or None)
    if action == 'get_dataset_details':
        return get_dataset_details(country, item_id)
    if action == 'get_datasets_by_organization':
        return get_datasets_by_organization(country, org, limit)
    if action == 'get_dataset_resources':
        return get_dataset_resources(country, item_id)
    if action == 'get_resource_details':
        return get_resource_details(country, item_id)
    if action == 'test_connection':
        return test_portal_connection(country)
    return _fail(f"Unknown action '{action}'")


def main() -> None:
    argv = sys.argv[1:]
    if not argv or argv[0] in ('-h', '--help'):
        result = _fail("Usage: universal_ckan_api.py <command> [args...]  (commands: portals, publishers, "
                       "datasets, search, resources, dataset, recent, test)")
    else:
        try:
            result = run_flags(argv) if argv[0].startswith('--') else run_positional(argv)
        except Exception as e:  # never let a traceback replace the JSON contract
            _log(f"universal_ckan_api: {type(e).__name__}: {e}")
            result = _fail(f"{type(e).__name__}: {e}")
    print(json.dumps(result, ensure_ascii=True, default=str))
    sys.exit(0 if result.get('success') else 1)


if __name__ == "__main__":
    main()
