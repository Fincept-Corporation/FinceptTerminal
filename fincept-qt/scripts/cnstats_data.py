#!/usr/bin/env python3
"""
China National Statistics API Wrapper
A COMPLETE wrapper for accessing China's National Bureau of Statistics data.
Provides access to ALL Chinese economic indicators, price indices, industrial data, and more.

This wrapper covers ALL functionality available in the cnstats library:
- Main stats function for querying statistical data
- Hierarchical indicator tree browsing
- Region code listing (provincial and city)
- Complete database code support
- Full API parameter handling

Usage Examples:
    # Get macro economic data (exact cnstats behavior)
    python cnstats_data.py stats A0D01 202201

    # Get provincial data with automatic dbcode detection
    python cnstats_data.py stats A010101 202201 110000

    # Get city data with automatic dbcode detection
    python cnstats_data.py stats A010101 202201 370200

    # List hierarchical indicator tree (exact cnstats behavior)
    python cnstats_data.py --tree --dbcode hgyd

    # List province region codes (exact cnstats behavior)
    python cnstats_data.py --list-regcode

    # List city region codes (exact cnstats behavior)
    python cnstats_data.py --list-regcode --dbcode csnd

Database Codes (COMPLETE):
    - hgyd: 宏观月度数据 (Macro monthly data - default)
    - hgjd: 宏观季度数据 (Macro quarterly data)
    - hgnd: 宏观年度数据 (Macro annual data)
    - fsyd: 分省月度数据 (Provincial monthly data)
    - fsjd: 分省季度数据 (Provincial quarterly data)
    - fsnd: 分省年度数据 (Provincial annual data)
    - csyd: 城市月度数据 (City monthly data)
    - csjd: 城市季度数据 (City quarterly data)
    - csnd: 城市年度数据 (City annual data)

Data Source: China National Bureau of Statistics (http://www.stats.gov.cn/)
Library: cnstats (https://github.com/songjian/cnstats)
"""

import sys
import json
import os
import requests
import time
from datetime import datetime
from typing import Dict, List, Any, Optional, Union

# --- 1. CONFIGURATION ---

# API Configuration
BASE_URL = "https://data.stats.gov.cn/easyquery.htm"
API_HEADERS = {
    'Accept-Encoding': 'gzip, deflate, br',
    'Connection': 'keep-alive',
    'Referer': 'https://data.stats.gov.cn/easyquery.htm?cn=A01',
    'Host': 'data.stats.gov.cn',
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.51 Safari/537.36 Edg/99.0.1150.36',
    'X-Requested-With': 'XMLHttpRequest',
    'Cookie': '_trs_uv=l0krufmy_6_30qm; JSESSIONID=JkGLaObMfWG3_P3_bNKa59cUydvE_nJDUpJOsskem4S-E-wgJeA7!-2135294552; u=1'
}

# Request configuration
TIMEOUT = 30
MAX_RETRIES = 3

# Database codes mapping
DATABASE_CODES = {
    'hgyd': {'name': '宏观月度数据', 'type': 'macro', 'period': 'monthly'},
    'hgjd': {'name': '宏观季度数据', 'type': 'macro', 'period': 'quarterly'},
    'hgnd': {'name': '宏观年度数据', 'type': 'macro', 'period': 'annual'},
    'fsyd': {'name': '分省月度数据', 'type': 'provincial', 'period': 'monthly'},
    'fsjd': {'name': '分省季度数据', 'type': 'provincial', 'period': 'quarterly'},
    'fsnd': {'name': '分省年度数据', 'type': 'provincial', 'period': 'annual'},
    'csyd': {'name': '城市月度数据', 'type': 'city', 'period': 'monthly'},
    'csjd': {'name': '城市季度数据', 'type': 'city', 'period': 'quarterly'},
    'csnd': {'name': '城市年度数据', 'type': 'city', 'period': 'annual'}
}

# Common indicator codes
COMMON_INDICATORS = {
    'A0D01': '货币供应量',
    'A0101': '价格指数',
    'A010101': '居民消费价格指数',
    'A0201': '工业增加值增长速度',
    'A0301': '能源主要产品产量',
    'A0401': '固定资产投资概况',
    'A0501': '服务业生产指数',
    'A0601': '房地产开发投资情况',
    'A0701': '社会消费品零售总额',
    'A0801': '进出口总额'
}

# --- 2. PRIVATE HELPER FUNCTIONS ---

def _generate_timestamp() -> str:
    """Generate random timestamp for API requests"""
    return str(int(round(time.time() * 1000)))

def easyquery(m: str = 'QueryData', dbcode: str = 'hgyd', rowcode: str = 'zb',
               colcode: str = 'sj', wds: List[Dict] = None, dfwds: List[Dict] = None,
               id: str = None, wdcode: str = None) -> Dict[str, Any]:
    """
    EXACT replica of cnstats easyquery function
    Centralized request handler for China Statistics API

    Args:
        m: API method name (exact parameter name from cnstats)
        dbcode: Database code
        rowcode: Row code
        colcode: Column code
        wds: WDS parameters
        dfwds: DFWDS parameters
        id: ID parameter
        wdcode: WD code parameter

    Returns:
        Raw API response (exact format from cnstats)
    """
    if wds is None:
        wds = []
    if dfwds is None:
        dfwds = []

    try:
        # Exact URL and parameters from cnstats
        url = 'https://data.stats.gov.cn/easyquery.htm'
        obj = {
            'm': m,
            'dbcode': dbcode,
            'rowcode': rowcode,
            'colcode': colcode,
            'wds': json.dumps(wds),
            'dfwds': json.dumps(dfwds),
            'k1': _generate_timestamp(),
            'h': '1',
        }

        if id:
            obj['id'] = id
        if wdcode:
            obj['wdcode'] = wdcode

        # Exact request handling from cnstats
        requests.packages.urllib3.disable_warnings()
        response = requests.post(url, data=obj, headers=API_HEADERS, verify=False, timeout=TIMEOUT)
        response.raise_for_status()

        return response.json()

    except Exception as e:
        return {"returncode": 500, "returndata": {}, "error": str(e)}

# --- 3. EXACT CNSTATS CORE FUNCTIONS ---

def stats(zbcode: str, datestr: str, regcode: str = None, dbcode: str = 'hgyd') -> List[str]:
    """
    EXACT replica of cnstats stats function
    Main function for retrieving statistical data

    Args:
        zbcode: Statistical indicator code
        datestr: Date string (YYYYMM format, supports comma-separated multiple dates)
        regcode: Optional region code for provincial/city data
        dbcode: Database code (default: 'hgyd')

    Returns:
        List of strings in exact cnstats format (for CLI compatibility)
        Each element is space-separated values: [name, code, date/region, value]
    """
    try:
        wds = []
        dfwds = []

        # Build exact cnstats query parameters
        if zbcode:
            dfwds.append({"wdcode": "zb", "valuecode": zbcode})

        if datestr:
            dfwds.append({"wdcode": "sj", "valuecode": datestr})

        if regcode:
            wds.append({"wdcode": "reg", "valuecode": regcode})

        # Use exact cnstats easyquery call
        ret = easyquery(dbcode=dbcode, dfwds=dfwds)

        if ret['returncode'] == 200:
            # Exact cnstats data processing
            data_dict = {}
            for n in ret['returndata']['wdnodes']:
                if n['wdcode'] == 'zb':
                    data_dict['zb'] = {}
                    for i in n['nodes']:
                        data_dict['zb'][i['code']] = i['cname']
                if n['wdcode'] == 'sj':
                    data_dict['sj'] = {}
                    for i in n['nodes']:
                        data_dict['sj'][i['code']] = i['cname']
                if n['wdcode'] == 'reg':
                    data_dict['reg'] = {}
                    for i in n['nodes']:
                        data_dict['reg'][i['code']] = i['cname']

            result = []
            for n in ret['returndata']['datanodes']:
                if n['data']['hasdata'] == True:
                    if len(data_dict) == 2:
                        # Format without region: name code date value
                        result.append([
                            data_dict['zb'][n['wds'][0]['valuecode']],
                            n['wds'][0]['valuecode'],
                            n['wds'][1]['valuecode'],
                            n['data']['strdata']
                        ])
                    if len(data_dict) == 3:
                        # Format with region: name code date region value
                        result.append([
                            data_dict['zb'][n['wds'][0]['valuecode']],
                            n['wds'][0]['valuecode'],
                            n['wds'][1]['valuecode'],
                            n['wds'][2]['valuecode'],
                            n['data']['strdata']
                        ])

            return result
        else:
            return []

    except Exception as e:
        return []

def get_tree(id: str = 'zb', dbcode: str = 'hgyd') -> None:
    """
    EXACT replica of cnstats get_tree function
    Prints hierarchical indicator tree to stdout (exact cnstats behavior)

    Args:
        id: Starting node ID (default: 'zb')
        dbcode: Database code (default: 'hgyd')

    Returns:
        None (prints to stdout like original cnstats)
    """
    try:
        for n in easyquery(m='getTree', dbcode=dbcode, wdcode='zb', id=id):
            try:
                print(n['id'], n['name'])
            except UnicodeEncodeError:
                print(n['id'], n['name'].encode('ascii', 'ignore').decode('ascii'))
            if n['isParent']:
                get_tree(n['id'], dbcode)
    except Exception as e:
        print(f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}")

def get_reg(dbcode: str = 'fsyd') -> List[Dict[str, str]]:
    """
    EXACT replica of cnstats get_reg function
    Returns region codes (provincial or city based on dbcode)

    Args:
        dbcode: Database code (default: 'fsyd' for provinces)

    Returns:
        List of region dictionaries with 'code' and 'name' keys
    """
    try:
        wds = [{"wdcode": "zb", "valuecode": "A01010101"}]
        ret = easyquery(dbcode=dbcode, rowcode='reg', wds=wds)

        if ret['returncode'] == 200:
            return ret['returndata']['wdnodes'][1]['nodes']
        else:
            return []
    except Exception as e:
        return []

# --- 4. JSON WRAPPER FUNCTIONS ---

def stats_json(zbcode: str, datestr: str, regcode: str = None,
               dbcode: str = 'hgyd') -> Dict[str, Any]:
    """
    JSON wrapper for stats function
    Returns exact cnstats data in JSON format for Rust integration

    Args:
        zbcode: Statistical indicator code
        datestr: Date string
        regcode: Optional region code
        dbcode: Database code

    Returns:
        Formatted JSON response
    """
    try:
        raw_result = stats(zbcode, datestr, regcode, dbcode)

        # Convert raw cnstats format to structured JSON
        formatted_data = []
        for row in raw_result:
            if len(row) == 4:
                # Format without region: name code date value
                formatted_data.append({
                    'name': row[0],
                    'code': row[1],
                    'date': row[2],
                    'value': row[3],
                    'database': dbcode,
                    'database_name': DATABASE_CODES.get(dbcode, {}).get('name', dbcode).encode('ascii', 'ignore').decode('ascii') if DATABASE_CODES.get(dbcode, {}).get('name', dbcode) else dbcode
                })
            elif len(row) == 5:
                # Format with region: name code date region value
                formatted_data.append({
                    'name': row[0],
                    'code': row[1],
                    'date': row[2],
                    'region': row[3],
                    'value': row[4],
                    'database': dbcode,
                    'database_name': DATABASE_CODES.get(dbcode, {}).get('name', dbcode).encode('ascii', 'ignore').decode('ascii') if DATABASE_CODES.get(dbcode, {}).get('name', dbcode) else dbcode
                })

        return {
            "data": formatted_data,
            "metadata": {
                "source": "China National Bureau of Statistics",
                "indicator_code": zbcode,
                "date_requested": datestr,
                "region_requested": regcode,
                "database": dbcode,
                "database_name": DATABASE_CODES.get(dbcode, {}).get('name', dbcode),
                "last_updated": datetime.now().isoformat(),
                "count": len(formatted_data)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "indicator_code": zbcode,
                "date_requested": datestr,
                "region_requested": regcode,
                "database": dbcode
            },
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

def tree_json(dbcode: str = 'hgyd', id: str = 'zb') -> Dict[str, Any]:
    """
    JSON wrapper for get_tree function
    Returns hierarchical indicator tree in JSON format

    Args:
        dbcode: Database code
        id: Starting node ID

    Returns:
        Formatted JSON response
    """
    try:
        tree_nodes = []

        def _collect_nodes(current_id: str, level: int = 0):
            try:
                for n in easyquery(m='getTree', dbcode=dbcode, wdcode='zb', id=current_id):
                    node_data = {
                        'id': n.get('id'),
                        'name': n.get('name'),
                        'is_parent': n.get('isParent', False),
                        'level': level,
                        'database': dbcode,
                        'database_name': DATABASE_CODES.get(dbcode, {}).get('name', dbcode).encode('ascii', 'ignore').decode('ascii') if DATABASE_CODES.get(dbcode, {}).get('name', dbcode) else dbcode
                    }
                    tree_nodes.append(node_data)

                    if n.get('isParent'):
                        _collect_nodes(n['id'], level + 1)
            except:
                pass

        _collect_nodes(id)

        return {
            "data": tree_nodes,
            "metadata": {
                "source": "China National Bureau of Statistics",
                "database": dbcode,
                "database_name": DATABASE_CODES.get(dbcode, {}).get('name', dbcode),
                "root_id": id,
                "last_updated": datetime.now().isoformat(),
                "count": len(tree_nodes)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "database": dbcode,
                "root_id": id
            },
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

def regions_json(dbcode: str = 'fsyd') -> Dict[str, Any]:
    """
    JSON wrapper for get_reg function
    Returns region codes in JSON format

    Args:
        dbcode: Database code

    Returns:
        Formatted JSON response
    """
    try:
        raw_regions = get_reg(dbcode)

        # Enhance with metadata
        formatted_regions = []
        for region in raw_regions:
            region_data = {
                'code': region.get('code'),
                'name': region.get('cname', '').encode('ascii', 'ignore').decode('ascii') if region.get('cname') else '',
                'database': dbcode,
                'database_name': DATABASE_CODES.get(dbcode, {}).get('name', dbcode).encode('ascii', 'ignore').decode('ascii') if DATABASE_CODES.get(dbcode, {}).get('name', dbcode) else dbcode
            }

            # Add region type based on database
            if dbcode.startswith('cs'):
                region_data['type'] = 'city'
            elif dbcode.startswith('fs'):
                region_data['type'] = 'province'
            else:
                region_data['type'] = 'unknown'

            formatted_regions.append(region_data)

        return {
            "data": formatted_regions,
            "metadata": {
                "source": "China National Bureau of Statistics",
                "database": dbcode,
                "database_name": DATABASE_CODES.get(dbcode, {}).get('name', dbcode),
                "region_type": "city" if dbcode.startswith('cs') else "province",
                "last_updated": datetime.now().isoformat(),
                "count": len(formatted_regions)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "database": dbcode
            },
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

# --- 6. UTILITY FUNCTIONS ---

def get_supported_databases() -> Dict[str, Any]:
    """
    Get list of supported database codes and their information

    Returns:
        Formatted response containing database information
    """
    try:
        databases = []
        for code, info in DATABASE_CODES.items():
            database_info = {
                'code': code,
                'name': info.get('name', code),
                'type': info.get('type', 'unknown'),
                'period': info.get('period', 'unknown')
            }
            databases.append(database_info)

        return {
            "data": databases,
            "metadata": {
                "source": "China National Bureau of Statistics API Wrapper",
                "last_updated": datetime.now().isoformat(),
                "count": len(databases)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

def get_common_indicators() -> Dict[str, Any]:
    """
    Get list of common statistical indicators

    Returns:
        Formatted response containing common indicators
    """
    try:
        indicators = []
        for code, name in COMMON_INDICATORS.items():
            indicator_info = {
                'code': code,
                'name': name,
                'category': 'common'
            }
            indicators.append(indicator_info)

        return {
            "data": indicators,
            "metadata": {
                "source": "China National Bureau of Statistics API Wrapper",
                "last_updated": datetime.now().isoformat(),
                "count": len(indicators),
                "note": "Most frequently used indicators"
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

def test_api_connection() -> Dict[str, Any]:
    """
    Test connection to the China Statistics API

    Returns:
        Formatted response containing connection test results
    """
    try:
        # Test with a simple query
        result = get_stats('A010101', '202201', None, 'hgyd')

        if not result['error']:
            return {
                "data": {
                    "status": "connected",
                    "source": result['metadata']['source'],
                    "test_indicator": "A010101",
                    "test_date": "202201",
                    "records_found": result['metadata']['count']
                },
                "metadata": {
                    "source": "China National Bureau of Statistics",
                    "action": "test_api_connection",
                    "last_updated": datetime.now().isoformat(),
                    "test_time": datetime.now().isoformat()
                },
                "error": None
            }
        else:
            return {
                "data": {
                    "status": "failed",
                    "error": result['error']
                },
                "metadata": {
                    "source": "China National Bureau of Statistics",
                    "action": "test_api_connection",
                    "last_updated": datetime.now().isoformat(),
                    "test_time": datetime.now().isoformat()
                },
                "error": result['error']
            }

    except Exception as e:
        return {
            "data": {
                "status": "failed",
                "error": str(e)
            },
            "metadata": {
                "source": "China National Bureau of Statistics",
                "action": "test_api_connection",
                "last_updated": datetime.now().isoformat(),
                "test_time": datetime.now().isoformat()
            },
            "error": f"Error: {str(e).encode('ascii', 'ignore').decode('ascii')}"
        }

# --- 8. EXACT CNSTATS CLI INTERFACE WITH JSON SUPPORT ---

def print_usage():
    """Print usage instructions matching exact cnstats behavior."""
    print("China National Statistics API Wrapper (EXACT cnstats + JSON support)")
    print("=" * 70)
    print()
    print("ORIGINAL CNSTATS USAGE (exact behavior):")
    print("  # Get macro economic data")
    print(f"  python {sys.argv[0]} A0D01 202201")
    print()
    print("  # Get provincial data (auto dbcode detection)")
    print(f"  python {sys.argv[0]} A010101 202201 110000")
    print()
    print("  # Get city data (auto dbcode detection)")
    print(f"  python {sys.argv[0]} A010101 202201 370200")
    print()
    print("  # List hierarchical indicator tree")
    print(f"  python {sys.argv[0]} --tree --dbcode hgyd")
    print()
    print("  # List province region codes")
    print(f"  python {sys.argv[0]} --list-regcode")
    print()
    print("  # List city region codes")
    print(f"  python {sys.argv[0]} --list-regcode --dbcode csnd")
    print()
    print("JSON OUTPUT OPTIONS (for Rust integration):")
    print("  # Get data in JSON format")
    print(f"  python {sys.argv[0]} --json --action stats --zbcode A0D01 --date 202201")
    print()
    print("  # Get indicator tree in JSON")
    print(f"  python {sys.argv[0]} --json --action tree --dbcode hgyd")
    print()
    print("  # Get regions in JSON")
    print(f"  python {sys.argv[0]} --json --action regions --dbcode fsyd")
    print()
    print("Database Codes (COMPLETE):")
    for code, info in DATABASE_CODES.items():
        try:
            print(f"  {code}: {info['name']}")
        except UnicodeEncodeError:
            print(f"  {code}: {info.get('name', code).encode('ascii', 'ignore').decode('ascii')}")
    print()
    print("Common Indicators:")
    for code, name in COMMON_INDICATORS.items():
        try:
            print(f"  {code}: {name}")
        except UnicodeEncodeError:
            print(f"  {code}: {name.encode('ascii', 'ignore').decode('ascii')}")
    print()

def main():
    """Main CLI function supporting both original cnstats behavior and JSON output."""

    # Check for help
    if '--help' in sys.argv or '-h' in sys.argv:
        print_usage()
        sys.exit(0)

    # Check if JSON output mode
    json_mode = '--json' in sys.argv

    # Remove --json from args for processing
    clean_args = [arg for arg in sys.argv if arg != '--json']

    if len(clean_args) < 2:
        print_usage()
        sys.exit(1)

    # EXACT CNSTATS ARGPARSE LOGIC
    import argparse

    parser = argparse.ArgumentParser(
        prog='cn-stats',
        description='China National Bureau of Statistics data Python package (Enhanced with JSON support)'
    )
    parser.add_argument('--tree', action='store_const', const='zb',
                       help='List indicator codes')
    parser.add_argument('--list-regcode', action='store_const', const='list_regcode',
                       help='List region codes, default lists provincial regions, add --dbcode csnd for major cities')
    parser.add_argument('--dbcode', nargs='?', const='hgyd', default='hgyd',
                       help='Database code, default: hgyd(macro monthly), options: hgjd(macro quarterly), hgnd(macro annual), fsyd(provincial monthly), fsjd(provincial quarterly), fsnd(provincial annual), csyd(city monthly), csjd(city quarterly), csnd(city annual)')
    parser.add_argument('--regcode', nargs='?', help='Region code, e.g.: 110000 is Beijing')
    parser.add_argument('--json', action='store_true', help='Output JSON format (for Rust integration)')
    parser.add_argument('--action', choices=['stats', 'tree', 'regions'],
                       help='JSON mode action: stats(statistical data), tree(indicator tree), regions(region codes)')
    parser.add_argument('--zbcode', help='Indicator code (JSON mode)')
    parser.add_argument('--date', help='Query date (JSON mode)')
    parser.add_argument('zbcode_pos', help='Indicator code (positional parameter)', nargs='?')
    parser.add_argument('date_pos', help='Query date (positional parameter)', nargs='?')
    parser.add_argument('regcode_pos', help='Region code (positional parameter)', nargs='?')

    # Parse the cleaned arguments
    args = parser.parse_args(clean_args[1:])

    try:
        if json_mode:
            # JSON MODE - for Rust integration
            if not args.action:
                print(json.dumps({"error": "--action is required in JSON mode"}))
                sys.exit(1)

            if args.action == 'stats':
                if not args.zbcode or not args.date:
                    print(json.dumps({"error": "--zbcode and --date are required for stats action"}))
                    sys.exit(1)

                # Auto-detect dbcode for regions
                dbcode = args.dbcode
                if args.regcode and dbcode == 'hgyd':
                    if args.regcode[-4:] == '0000':
                        dbcode = 'fsyd'
                    else:
                        dbcode = 'csyd'

                result = stats_json(args.zbcode, args.date, args.regcode, dbcode)
                print(json.dumps(result, indent=2))

            elif args.action == 'tree':
                result = tree_json(args.dbcode)
                print(json.dumps(result, indent=2))

            elif args.action == 'regions':
                result = regions_json(args.dbcode)
                print(json.dumps(result, indent=2))

            else:
                print(json.dumps({"error": f"Unknown action: {args.action}"}))
                sys.exit(1)

        else:
            # ORIGINAL CNSTATS MODE
            if args.tree == 'zb':
                # Exact cnstats tree behavior - prints to stdout
                get_tree(args.tree, args.dbcode)

            elif args.list_regcode == 'list_regcode':
                # Exact cnstats region listing behavior
                if args.dbcode[:2] == 'cs':
                    for n in get_reg('csnd'):
                        try:
                            print(n['code'], n['name'])
                        except UnicodeEncodeError:
                            print(n['code'], n['name'].encode('ascii', 'ignore').decode('ascii'))
                else:
                    for n in get_reg():
                        try:
                            print(n['code'], n['name'])
                        except UnicodeEncodeError:
                            print(n['code'], n['name'].encode('ascii', 'ignore').decode('ascii'))

            else:
                # Exact cnstats stats behavior with auto dbcode detection
                zbcode = args.zbcode_pos
                date = args.date_pos
                regcode = args.regcode or args.regcode_pos
                dbcode = args.dbcode

                # Auto-detect dbcode logic (exact from cnstats)
                if regcode:
                    if regcode[-4:] == '0000':
                        dbcode = 'fsyd'
                    else:
                        dbcode = 'csyd'

                # Get data and print in exact cnstats format
                r = stats(zbcode, date, regcode, dbcode)
                for row in r:
                    try:
                        print(' '.join(row))
                    except UnicodeEncodeError:
                        # Handle encoding issues by converting to ASCII
                        safe_row = [item.encode('ascii', 'ignore').decode('ascii') if isinstance(item, str) else str(item) for item in row]
                        print(' '.join(safe_row))

    except KeyboardInterrupt:
        sys.exit(0)
    except Exception as e:
        if json_mode:
            error_result = {
                "data": [],
                "metadata": {},
                "error": f"Error: {str(e)}"
            }
            print(json.dumps(error_result, indent=2))
        else:
            print(f"Error: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()