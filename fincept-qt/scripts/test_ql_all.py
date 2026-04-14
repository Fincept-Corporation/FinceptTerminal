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
    (26, 'POST', '/quantlib/core/distributions/gamma/cdf',
         {'x': 2.0, 'alpha': 2.0, 'beta': 1.0}),
    (27, 'POST', '/quantlib/core/distributions/gamma/pdf',
         {'x': 2.0, 'alpha': 2.0, 'beta': 1.0}),
    (32, 'POST', '/quantlib/core/math/eval',
         {'func_name': 'sqrt', 'x': 2.0}),
    (33, 'POST', '/quantlib/core/math/two-arg',
         {'func_name': 'pow', 'x': 2.0, 'y': 10.0}),
    (34, 'POST', '/quantlib/core/ops/black-scholes',
         {'spot': 100, 'strike': 105, 'rate': 0.05, 'volatility': 0.2, 'time': 1.0, 'option_type': 'call'}),
    (35, 'POST', '/quantlib/core/ops/black76',
         {'forward': 100, 'strike': 105, 'discount_factor': 0.95, 'volatility': 0.2, 'time': 1.0, 'option_type': 'call'}),
    (36, 'POST', '/quantlib/core/ops/forward-rate',
         {'df1': 0.95, 'df2': 0.90, 't1': 1.0, 't2': 2.0}),
    (37, 'POST', '/quantlib/core/ops/discount-cashflows',
         {'cashflows': [100, 100, 1100], 'times': [1, 2, 3], 'discount_factors': [0.95, 0.90, 0.86]}),
    (38, 'POST', '/quantlib/core/ops/interpolate',
         {'x_data': [1, 2, 3, 4], 'y_data': [1, 4, 9, 16], 'x': 2.5, 'method': 'linear'}),
    (39, 'POST', '/quantlib/core/ops/statistics',
         {'values': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]}),
    (40, 'POST', '/quantlib/core/ops/var',
         {'returns': [-0.02, 0.01, -0.015, 0.03, -0.01, 0.02], 'confidence': 0.95, 'method': 'historical'}),
    (41, 'POST', '/quantlib/core/ops/percentile',
         {'values': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 'p': 0.9}),
    (42, 'POST', '/quantlib/core/ops/covariance-matrix',
         {'returns': [[0.01, 0.02], [0.03, -0.01], [0.02, 0.01], [-0.01, 0.03]]}),
    (44, 'POST', '/quantlib/core/ops/gbm-paths',
         {'spot': 100, 'drift': 0.05, 'volatility': 0.2, 'time': 1.0, 'n_steps': 10, 'n_paths': 3}),
    (45, 'POST', '/quantlib/core/ops/zero-rate-convert',
         {'direction': 'continuous_to_annual', 'value': 0.05, 't': 1.0}),
    (46, 'POST', '/quantlib/core/legs/fixed',
         {'notional': 1000000, 'rate': 0.05, 'frequency': 'semiannual',
          'start_date': '2024-01-01', 'end_date': '2026-01-01'}),
    (47, 'POST', '/quantlib/core/legs/float',
         {'notional': 1000000, 'spread': 0.01, 'frequency': 'quarterly',
          'start_date': '2024-01-01', 'end_date': '2026-01-01'}),
    (48, 'POST', '/quantlib/core/legs/zero-coupon',
         {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01', 'end_date': '2029-01-01'}),
    (50, 'POST', '/quantlib/core/periods/fixed-coupon',
         {'notional': 1000000, 'rate': 0.05, 'start_date': '2024-01-01',
          'end_date': '2024-07-01', 'day_count_fraction': 0.5}),
    (51, 'POST', '/quantlib/core/periods/float-coupon',
         {'notional': 1000000, 'libor_rate': 0.04, 'spread': 0.01,
          'start_date': '2024-01-01', 'end_date': '2024-04-01'}),
]

OUT = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\ql_results.txt'
with open(OUT, 'w', encoding='utf-8') as f:
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
                f.write(f'  data sample: {json.dumps(data)[:600]}\n')
            elif isinstance(data, list):
                f.write(f'  data list len={len(data)} sample: {json.dumps(data)[:600]}\n')
            else:
                f.write(f'  data: {str(data)[:400]}\n')
        else:
            detail = resp.get('detail', resp)
            f.write(f'  detail: {json.dumps(detail)[:800]}\n')
        f.write('\n')
