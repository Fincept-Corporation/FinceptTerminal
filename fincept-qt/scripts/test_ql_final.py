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

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_final_results.txt'
retries = []

# #8: still need valid schedule_type besides "constant". Try more options.
for stype in ['principal', 'par', 'face', 'outstanding', 'remaining',
              'proportional', 'staggered', 'even', 'regular', 'level']:
    retries.append((f'8_{stype}', 'POST', '/quantlib/core/types/notional-schedule',
                    {'notional': 1000000, 'periods': 4, 'schedule_type': stype}))

# #47: float leg - the error message itself says "fixing rate, fixed rate, or curve"
# These are internal variable names. The issue may be a field name that does not pass
# through correctly from JSON. Try "fixings" as array, or "discount_curve"
retries.append(('47L', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixings': [0.04, 0.041, 0.042, 0.043, 0.044, 0.045, 0.046, 0.047]}))
retries.append(('47M', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'discount_curve': [0.04, 0.041, 0.042, 0.043]}))
retries.append(('47N', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'zero_rates': [0.04, 0.041, 0.042, 0.043]}))
retries.append(('47O', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'forward_rates': [0.04, 0.041, 0.042, 0.043]}))

# #50: fixed-coupon: 500 with correct types. This is a real server bug.
# Try omitting optional fields to isolate what crashes it.
retries.append(('50_min', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count_fraction': 0.5}))

# #51: float-coupon requires: notional, spread, start_date, end_date.
# libor_rate is not validated (no 422) but causes 500. Try without it.
retries.append(('51_nolib', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01'}))
retries.append(('51_nolib2', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'spread': 0.0,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01'}))

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
