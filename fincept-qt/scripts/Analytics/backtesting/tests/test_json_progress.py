"""Output hygiene (A0.3, R1, R2) and the progress-line protocol (A0.8)."""
import datetime as dt
import json
import time

import numpy as np
import pandas as pd

from base import progress as P
from base.base_provider import (classify_error, convert_keys_to_camel, error_envelope, json_response,
                                normalize_envelope, periods_per_year)


def test_numpy_pandas_values_serialise_strictly():
    payload = {'success': True, 'data': {
        'i': np.int64(3), 'f': np.float32(1.5), 'n': np.float64('nan'), 'inf': float('inf'), 'b': np.bool_(True),
        'arr': np.array([1.0, np.nan]), 'ts': pd.Timestamp('2024-01-02'), 'tsi': pd.Timestamp('2024-01-02 09:30'),
        'nat': pd.NaT, 'd': dt.date(2024, 1, 3), 's': pd.Series([1, 2]), 'set': {1},
        'parameters': {'fast_period': 10, 'slow_x': np.int32(2)}, 'priceData': {'RELIANCE_NS': {'t': []}},
        'snake_field': 1, pd.Timestamp('2024-05-01'): 'key'}}
    text = json_response(payload)
    assert '\n' not in text and 'NaN' not in text and 'Infinity' not in text
    d = json.loads(text)['data']
    assert d['i'] == 3 and d['f'] == 1.5 and d['n'] is None and d['inf'] is None and d['b'] is True
    assert d['arr'] == [1.0, None] and d['ts'] == '2024-01-02' and d['tsi'] == '2024-01-02 09:30:00'
    assert d['nat'] is None and d['d'] == '2024-01-03' and d['s'] == [1, 2] and d['set'] == [1]
    assert d['parameters'] == {'fast_period': 10, 'slow_x': 2}        # data-map keys survive (B7)
    assert 'RELIANCE_NS' in d['priceData'] and d['snakeField'] == 1 and d['2024-05-01'] == 'key'


def test_camel_case_is_idempotent_and_protects_maps():
    x = {'best_parameters': {'fast_period': 1}, 'all_results': [{'parameters': {'a_b': 1}, 'objective_value': 2}]}
    once = convert_keys_to_camel(x)
    assert once == convert_keys_to_camel(once)
    assert once['bestParameters'] == {'fast_period': 1} and once['allResults'][0]['parameters'] == {'a_b': 1}


def test_error_envelope_shape():
    e = error_envelope('Market data for X could not be loaded.')
    assert e['errorCode'] == 'DATA' and e['details'] == {'traceback': None, 'logs': [], 'symbol': None}
    tb = 'Optimise failed: Traceback (most recent call last):\n  File "x.py"\nValueError: bad range'
    out = json.loads(json_response({'success': False, 'error': tb}))
    assert out['error'] == 'Optimise failed' and 'Traceback' in out['details']['traceback']
    nested = normalize_envelope({'success': True, 'data': {'success': False, 'error': 'No parameter ranges supplied'}})
    assert nested['success'] is False and nested['errorCode'] == 'INPUT'
    assert classify_error('No module named zipline', ModuleNotFoundError()) == 'MISSING_PACKAGE'
    assert classify_error('Cancelled by user') == 'CANCELLED'


def test_progress_line_format_and_parse():
    line = P.format_line('optimize', 240, 500, 'Optimising 240 / 500', best=1.21, eta_sec=14.04, workers=4)
    assert line.startswith('@@BT_PROGRESS {') and '\n' not in line
    obj = P.parse_line(line)
    assert obj == {'phase': 'optimize', 'done': 240, 'total': 500, 'message': 'Optimising 240 / 500', 'best': 1.21,
                   'etaSec': 14.0, 'workers': 4}
    assert P.parse_line('[log] hello') is None and P.parse_line('@@BT_PROGRESS {broken') is None
    assert P.parse_line(P.format_line('data', 1, None, best=float('nan')))['best'] is None


def test_progress_rate_limit(capsys):
    assert P.emit('sweep', 1, 10, force=True)
    assert not P.emit('sweep', 2, 10)                    # within 0.25 s of the previous line
    assert P.emit('sweep', 3, 10, force=True)
    time.sleep(0.3)
    assert P.emit('sweep', 4, 10)
    err = capsys.readouterr().err
    assert len([x for x in err.splitlines() if x.startswith('@@BT_PROGRESS')]) == 3


def test_progress_eta():
    p = P.Progress('monte_carlo', total=100, emit_start=False)
    p.t0 -= 10.0
    p.done = 25
    assert 25 < p.eta() < 35


def test_cancel_token_is_cheap_and_detects(tmp_path):
    f = tmp_path / 'c.flag'
    tok = P.CancelToken({'cancelFile': str(f)}, min_interval=0.0)
    assert not tok.cancelled()
    f.write_text('x')
    assert tok.cancelled()
    assert P.CancelToken({}).cancelled() is False


def test_periods_per_year_shim_matches_metrics():
    assert periods_per_year(pd.bdate_range('2024-01-01', periods=10)) == 252
    assert periods_per_year(pd.to_datetime(['2024-01-06', '2024-01-07'])) == 365
