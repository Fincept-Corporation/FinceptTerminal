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

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_retry_results.txt'

retries = []

# #6: 405 - tried GET, try POST with no body, then check if endpoint exists differently
retries.append(('6a', 'POST', '/quantlib/core/types/spread/from-bps', {'bps': 50}))
retries.append(('6b', 'GET',  '/quantlib/core/types/spread/from-bps', None))  # no query param
# Try with query param as POST body style
retries.append(('6c', 'POST', '/quantlib/core/types/spread/from-bps?bps=50', {}))

# #8: "Unknown schedule type: linear" - try 'amortizing', 'bullet', 'step_up', 'step_down', 'constant'
retries.append(('8a', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'amortizing'}))
retries.append(('8b', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'bullet'}))
retries.append(('8c', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'step_down'}))

# #16: gradient - x must be list
retries.append(('16a', 'POST', '/quantlib/core/autodiff/gradient',
                {'func_name': 'sin', 'x': [1.0]}))
retries.append(('16b', 'POST', '/quantlib/core/autodiff/gradient',
                {'func_name': 'sin', 'x': [1.0, 2.0]}))

# #17: taylor-expand - needs x0 field (not x)
retries.append(('17a', 'POST', '/quantlib/core/autodiff/taylor-expand',
                {'func_name': 'sin', 'x0': 1.0, 'order': 3}))
retries.append(('17b', 'POST', '/quantlib/core/autodiff/taylor-expand',
                {'func_name': 'sin', 'x0': 1.0, 'x': 1.5, 'order': 3}))

# #33: math/two-arg - available: maximum, minimum, power (not pow)
retries.append(('33a', 'POST', '/quantlib/core/math/two-arg',
                {'func_name': 'power', 'x': 2.0, 'y': 10.0}))
retries.append(('33b', 'POST', '/quantlib/core/math/two-arg',
                {'func_name': 'maximum', 'x': 2.0, 'y': 10.0}))

# #45: zero-rate-convert returned discount_factor but we wanted rate conversion
# data was {"discount_factor": 0.951...} - note the direction field may control output
retries.append(('45b', 'POST', '/quantlib/core/ops/zero-rate-convert',
                {'direction': 'annual_to_continuous', 'value': 0.05, 't': 1.0}))

# #46: legs/fixed - "Cannot parse tenor: SEMIANNUAL" - try lowercase or standard codes
retries.append(('46a', 'POST', '/quantlib/core/legs/fixed',
                {'notional': 1000000, 'rate': 0.05, 'frequency': 'semi-annual',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))
retries.append(('46b', 'POST', '/quantlib/core/legs/fixed',
                {'notional': 1000000, 'rate': 0.05, 'frequency': '6M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))
retries.append(('46c', 'POST', '/quantlib/core/legs/fixed',
                {'notional': 1000000, 'rate': 0.05, 'frequency': 'annual',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))
retries.append(('46d', 'POST', '/quantlib/core/legs/fixed',
                {'notional': 1000000, 'rate': 0.05, 'frequency': '1Y',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))

# #47: legs/float - same tenor parsing issue
retries.append(('47a', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))
retries.append(('47b', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': 'quarterly',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01'}))

# #50: periods/fixed-coupon - 500 error, try different field names
retries.append(('50a', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'dcf': 0.5}))
retries.append(('50b', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'coupon_rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count_fraction': 0.5}))
retries.append(('50c', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01'}))
retries.append(('50d', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'face_value': 1000000, 'coupon_rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count_fraction': 0.5}))

# #51: periods/float-coupon - 500 error
retries.append(('51a', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'rate': 0.05, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01'}))
retries.append(('51b', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'index_rate': 0.04, 'spread': 0.01,
                 'start_date': '2024-01-01', 'end_date': '2024-04-01'}))
retries.append(('51c', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'libor_rate': 0.04, 'spread': 0.01,
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
                f.write(f'  data: {json.dumps(data)[:500]}\n')
            elif isinstance(data, list):
                f.write(f'  data list len={len(data)}: {json.dumps(data)[:500]}\n')
            else:
                f.write(f'  data: {str(data)[:400]}\n')
        else:
            detail = resp.get('detail', resp)
            f.write(f'  detail: {json.dumps(detail)[:600]}\n')
        f.write('\n')
