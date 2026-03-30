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

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_retry3_results.txt'
retries = []

# #8: try remaining QuantLib-style names
for stype in ['amortization', 'amortized', 'annuity', 'schedule', 'notional',
              'linearly_declining', 'step', 'balloon', 'interest_only', 'bond']:
    retries.append((f'8_{stype}', 'POST', '/quantlib/core/types/notional-schedule',
                    {'notional': 1000000, 'periods': 4, 'schedule_type': stype}))

# #47: float leg - the error says "no fixing rate, fixed rate, or curve provided"
# so these field names are explicitly mentioned in the error - the server reads them but
# doesn't find them. Try different nesting or plural forms.
retries.append(('47f', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'curve': [0.04, 0.042, 0.043, 0.044, 0.045, 0.046, 0.047, 0.048]}))
retries.append(('47g', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'curve': {'rates': [0.04, 0.042, 0.043, 0.044], 'tenors': ['3M','6M','9M','1Y']}}))
retries.append(('47h', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'flat_rate': 0.04}))
retries.append(('47i', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'index_rate': 0.04}))
retries.append(('47j', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'libor': 0.04}))
retries.append(('47k', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'flat_forward': 0.04}))

# #50/#51 - server-side 500s, try minimal validation to see if required fields
# are different than expected. Send invalid types to force 422 validation errors.
retries.append(('50_probe', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 'X', 'rate': 'X', 'start_date': 'X', 'end_date': 'X',
                 'day_count_fraction': 'X'}))
retries.append(('51_probe', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 'X', 'start_date': 'X', 'end_date': 'X',
                 'libor_rate': 'X', 'spread': 'X', 'day_count_fraction': 'X'}))

with open(OUT, 'w', encoding='utf-8') as f:
    for label, method, path, body in retries:
        status, resp = post(path, body)
        ok = 'OK' if status == 200 else 'FAIL'
        f.write(f'--- #{label} {method} {path}\n')
        f.write(f'  Body: {json.dumps(body)}\n')
        f.write(f'  Status: {status} {ok}\n')
        if status == 200:
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
