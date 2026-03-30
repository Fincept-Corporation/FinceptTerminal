import urllib.request
import json

BASE = 'https://api.fincept.in'
HEADERS = {
    'X-API-Key': 'fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo',
    'Accept': 'application/json',
    'Content-Type': 'application/json',
    'User-Agent': 'FinceptTerminal/4.0.0'
}

def post(path, body):
    data = json.dumps(body).encode()
    req = urllib.request.Request(BASE + path, data=data, headers=HEADERS, method='POST')
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())
    except Exception as ex:
        return 0, {'detail': str(ex)}

def get(path):
    h = {k: v for k, v in HEADERS.items() if k != 'Content-Type'}
    req = urllib.request.Request(BASE + path, headers=h, method='GET')
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())
    except Exception as ex:
        return 0, {'detail': str(ex)}

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_final2_results.txt'
retries = []

# Try OpenAPI schema to see notional-schedule model
retries.append(('openapi_schema', 'GET', '/openapi.json', None))
retries.append(('docs_redoc', 'GET', '/redoc', None))

# #47: fixings must be a dict - try date->rate mapping
retries.append(('47P', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixings': {'2024-01-01': 0.04, '2024-04-01': 0.041,
                             '2024-07-01': 0.042, '2024-10-01': 0.043,
                             '2025-01-01': 0.044, '2025-04-01': 0.045,
                             '2025-07-01': 0.046, '2025-10-01': 0.047}}))
retries.append(('47Q', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixings': {'rate': 0.04}}))

with open(OUT, 'w', encoding='utf-8') as f:
    for label, method, path, body in retries:
        if method == 'GET':
            status, resp = get(path)
        else:
            status, resp = post(path, body)
        ok = 'OK' if status == 200 else 'FAIL'
        f.write(f'--- #{label} {method} {path}\n')
        if body:
            f.write(f'  Body: {json.dumps(body)}\n')
        f.write(f'  Status: {status} {ok}\n')
        if status == 200:
            if label == 'openapi_schema':
                # Just show the notional-schedule and float leg parts
                text = json.dumps(resp)
                # Find notional-schedule
                idx = text.find('notional-schedule')
                f.write(f'  notional-schedule excerpt: {text[max(0,idx-50):idx+500]}\n')
                idx2 = text.find('float-coupon')
                f.write(f'  float-coupon excerpt: {text[max(0,idx2-50):idx2+500]}\n')
            else:
                data = resp.get('data', resp)
                if isinstance(data, dict):
                    f.write(f'  data keys: {list(data.keys())}\n')
                    f.write(f'  data: {json.dumps(data)[:600]}\n')
                elif isinstance(data, list):
                    f.write(f'  data list len={len(data)}: {json.dumps(data)[:600]}\n')
                else:
                    f.write(f'  data: {str(data)[:400]}\n')
        else:
            detail = resp.get('detail', resp)
            f.write(f'  detail: {json.dumps(detail)[:700]}\n')
        f.write('\n')
