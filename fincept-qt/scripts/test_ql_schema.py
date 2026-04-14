import urllib.request
import json

BASE = 'https://api.fincept.in'
HEADERS = {
    'X-API-Key': 'fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo',
    'Accept': 'application/json',
    'User-Agent': 'FinceptTerminal/4.0.1'
}

def get(path):
    h = {k: v for k, v in HEADERS.items()}
    req = urllib.request.Request(BASE + path, headers=h, method='GET')
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read())
    except Exception as ex:
        return 0, {'detail': str(ex)}

status, resp = get('/openapi.json')
OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_schema_results.txt'

with open(OUT, 'w', encoding='utf-8') as f:
    if status != 200:
        f.write(f'Failed: {resp}\n')
    else:
        schemas = resp.get('components', {}).get('schemas', {})
        # Print schemas relevant to our failing endpoints
        targets = [
            'NotionalScheduleRequest',
            'FloatLegRequest',
            'FloatPeriodRequest',
            'FixedCouponPeriodRequest',
            'FixedPeriodRequest',
        ]
        for name in targets:
            if name in schemas:
                f.write(f'=== {name} ===\n')
                f.write(json.dumps(schemas[name], indent=2) + '\n\n')
            else:
                f.write(f'=== {name}: NOT FOUND ===\n')
                # Try to find partial matches
                matches = [k for k in schemas if name.lower()[:6] in k.lower()]
                f.write(f'  Similar: {matches}\n\n')

        # Also dump all schema names
        f.write('=== ALL SCHEMA NAMES ===\n')
        for k in sorted(schemas.keys()):
            f.write(f'  {k}\n')
