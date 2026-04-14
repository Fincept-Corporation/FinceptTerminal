import urllib.request
import json

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

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_retry2_results.txt'

retries = []

# #8: Try all plausible schedule types
for stype in ['fixed', 'declining', 'reducing', 'flat', 'equal', 'uniform',
              'constant', 'accreting', 'straight-line', 'straight_line',
              'linear_amortization', 'scheduled', 'custom']:
    retries.append((f'8_{stype}', 'POST', '/quantlib/core/types/notional-schedule',
                    {'notional': 1000000, 'periods': 4, 'schedule_type': stype}))

# #47: float leg needs fixing_rate
retries.append(('47c', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixing_rate': 0.04}))
retries.append(('47d', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixed_rate': 0.04}))
retries.append(('47e', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'rate': 0.04}))

# #50: periods/fixed-coupon - server needs rate + day_count_fraction
# but 500 happens when both provided. Try omitting day_count_fraction and using day_count string
retries.append(('50e', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count': 'ACT/365'}))
retries.append(('50f', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count': 'Actual/365'}))
retries.append(('50g', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count': '30/360'}))
retries.append(('50h', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count_fraction': 181.0/365.0}))

# #51: periods/float-coupon - all attempts return 500
# Try minimalist body first to find required fields via 422
retries.append(('51d', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000}))
retries.append(('51e', 'POST', '/quantlib/core/periods/float-coupon',
                {}))
retries.append(('51f', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'libor_rate': 0.04, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01',
                 'day_count': 'ACT/360'}))
retries.append(('51g', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'rate': 0.04, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01',
                 'day_count': 'ACT/360'}))
retries.append(('51h', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'fixing_rate': 0.04, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01',
                 'day_count_fraction': 0.25}))

with open(OUT, 'w', encoding='utf-8') as f:
    for label, method, path, body in retries:
        if method == 'POST':
            status, resp = post(path, body)
        else:
            status, resp = get(path)
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
