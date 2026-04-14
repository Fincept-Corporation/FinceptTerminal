import urllib.request
import json
import os

BASE = 'https://api.fincept.in'
HEADERS = {
    'X-API-Key': 'fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo',
    'Accept': 'application/json',
    'Content-Type': 'application/json',
    'User-Agent': 'FinceptTerminal/4.0.1'
}

def post(path, body):
    data = json.dumps(body).encode()
    req = urllib.request.Request(BASE + path, data=data, headers=HEADERS, method='POST')
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())

def get(path):
    h = {k: v for k, v in HEADERS.items() if k != 'Content-Type'}
    req = urllib.request.Request(BASE + path, headers=h, method='GET')
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())

tests = [
    (4,  'POST', '/quantlib/core/types/money/convert',
         {'amount': 100, 'from_currency': 'USD', 'to_currency': 'EUR', 'rate': 0.92}),
    (5,  'POST', '/quantlib/core/types/rate/convert',
         {'value': 0.05, 'from_type': 'annual', 'to_type': 'continuous'}),
    (6,  'GET',  '/quantlib/core/types/spread/from-bps?bps=50', None),
    (7,  'POST', '/quantlib/core/types/tenor/add-to-date',
         {'start_date': '2024-01-01', 'tenor': '3M'}),
    (8,  'POST', '/quantlib/core/types/notional-schedule',
         {'notional': 1000000, 'periods': 4, 'schedule_type': 'linear'}),
    (10, 'POST', '/quantlib/core/conventions/format-date',
         {'date_str': '2024-01-15', 'format': '%d/%m/%Y'}),
    (11, 'POST', '/quantlib/core/conventions/days-to-years',
         {'value': 365, 'day_count': 'ACT/365'}),
    (12, 'POST', '/quantlib/core/conventions/years-to-days',
         {'value': 1.0, 'day_count': 'ACT/365'}),
    (13, 'POST', '/quantlib/core/conventions/normalize-rate',
         {'value': 0.05, 'compounding': 'annual'}),
    (14, 'POST', '/quantlib/core/conventions/normalize-volatility',
         {'value': 0.2, 'tenor': '1Y'}),
    (15, 'POST', '/quantlib/core/autodiff/dual-eval',
         {'func_name': 'sin', 'x': 1.0}),
    (16, 'POST', '/quantlib/core/autodiff/gradient',
         {'func_name': 'sin', 'x': 1.0}),
    (17, 'POST', '/quantlib/core/autodiff/taylor-expand',
         {'func_name': 'sin', 'x': 1.0, 'order': 3}),
]

out_path = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\test_results_batch1.txt'
with open(out_path, 'w', encoding='utf-8') as f:
    for num, method, path, body in tests:
        if method == 'POST':
            status, resp = post(path, body)
        else:
            status, resp = get(path)
        ok = 'OK' if status == 200 else 'FAIL'
        f.write(f'--- #{num} {method} {path}\n')
        f.write(f'  Status: {status} {ok}\n')
        if status == 200:
            data = resp.get('data', resp)
            if isinstance(data, dict):
                f.write(f'  data keys: {list(data.keys())}\n')
                f.write(f'  data: {json.dumps(data)[:400]}\n')
            elif isinstance(data, list):
                f.write(f'  data (list len={len(data)}): {str(data)[:400]}\n')
            else:
                f.write(f'  data: {str(data)[:400]}\n')
        else:
            f.write(f'  full_resp: {json.dumps(resp)[:800]}\n')
        f.write('\n')
