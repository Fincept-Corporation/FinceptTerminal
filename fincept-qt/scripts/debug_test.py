import urllib.request
import json

out_path = r'C:\windowsdisk\finceptTerminal\fincept-qt\scripts\debug_out.txt'

with open(out_path, 'w') as f:
    f.write('starting\n')
    try:
        BASE = 'https://api.fincept.in'
        HEADERS = {
            'X-API-Key': 'fk_user_vU20qwUxKtPmg0fWpriNBhcAnBVGgOtJxsKiiwfD9Qo',
            'Accept': 'application/json',
            'Content-Type': 'application/json',
            'User-Agent': 'FinceptTerminal/4.0.0'
        }
        body = {'amount': 100, 'from_currency': 'USD', 'to_currency': 'EUR', 'rate': 0.92}
        data = json.dumps(body).encode()
        req = urllib.request.Request(BASE + '/quantlib/core/types/money/convert', data=data, headers=HEADERS, method='POST')
        with urllib.request.urlopen(req, timeout=20) as r:
            resp = json.loads(r.read())
            f.write(f'200 OK\n{json.dumps(resp)[:500]}\n')
    except urllib.error.HTTPError as e:
        resp = json.loads(e.read())
        f.write(f'{e.code} FAIL\n{json.dumps(resp)[:500]}\n')
    except Exception as ex:
        f.write(f'EXCEPTION: {ex}\n')
    f.write('done\n')
