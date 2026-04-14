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

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_corrected_results.txt'
retries = []

# #8: From docs description: "constant, linear, mortgage, bullet"
retries.append(('8_linear', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'linear'}))
retries.append(('8_mortgage', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'mortgage', 'rate': 0.05}))
retries.append(('8_bullet', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'bullet'}))
# Maybe server uses different case
retries.append(('8_LINEAR', 'POST', '/quantlib/core/types/notional-schedule',
                {'notional': 1000000, 'periods': 4, 'schedule_type': 'LINEAR'}))

# #47: FloatLegRequest has fixings as dict[date_str -> float]
# With correct date keys, the fixings dict should work.
# The error when fixings={'rate': 0.04} was "Cannot parse date from string: rate"
# meaning it IS reading the dict keys as dates. Send proper date string keys.
retries.append(('47_fixings_ok', 'POST', '/quantlib/core/legs/float',
                {'notional': 1000000, 'spread': 0.01, 'frequency': '3M',
                 'start_date': '2024-01-01', 'end_date': '2026-01-01',
                 'fixings': {
                     '2024-01-01': 0.04,
                     '2024-04-01': 0.041,
                     '2024-07-01': 0.042,
                     '2024-10-01': 0.043,
                     '2025-01-01': 0.044,
                     '2025-04-01': 0.045,
                     '2025-07-01': 0.046,
                     '2025-10-01': 0.047
                 }}))

# #50: FixedPeriodRequest uses day_count string NOT day_count_fraction float
# The schema shows: notional, rate, start_date, end_date, day_count (default ACT/360)
retries.append(('50_daycount', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01', 'day_count': 'ACT/360'}))
retries.append(('50_minimal', 'POST', '/quantlib/core/periods/fixed-coupon',
                {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
                 'end_date': '2024-07-01'}))

# #51: FloatPeriodRequest schema: notional, spread, start_date, end_date, day_count, fixing_rate
# fixing_rate is optional (anyOf null)
retries.append(('51_fixing_rate', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'spread': 0.01, 'start_date': '2024-01-01',
                 'end_date': '2024-04-01', 'fixing_rate': 0.04}))
retries.append(('51_fixing_rate_dc', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'spread': 0.01, 'start_date': '2024-01-01',
                 'end_date': '2024-04-01', 'fixing_rate': 0.04, 'day_count': 'ACT/360'}))
retries.append(('51_no_fixing', 'POST', '/quantlib/core/periods/float-coupon',
                {'notional': 1000000, 'spread': 0.01, 'start_date': '2024-01-01',
                 'end_date': '2024-04-01', 'day_count': 'ACT/360'}))

# Also re-test #45 to confirm zero-rate-convert behavior:
# The schema name ZeroRateConvertRequest; direction, value, t - it returns discount_factor
# both directions return discount_factor - this is the API behavior.
retries.append(('45_check', 'POST', '/quantlib/core/ops/zero-rate-convert',
                {'direction': 'continuous_to_annual', 'value': 0.05, 't': 1.0}))

with open(OUT, 'w', encoding='utf-8') as f:
    for label, method, path, body in retries:
        status, resp = post(path, body)
        ok = 'OK' if status == 200 else 'FAIL'
        f.write(f'--- #{label} POST {path}\n')
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
