import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { legsApi } from '../quantlibCoreApi';

export default function LegsPanel() {
  const today = new Date().toISOString().split('T')[0];
  const plus5y = new Date(Date.now() + 5 * 365.25 * 86400000).toISOString().split('T')[0];

  // Fixed Leg
  const fixed = useEndpoint();
  const [flNot, setFlNot] = useState('10000000');
  const [flRate, setFlRate] = useState('0.05');
  const [flStart, setFlStart] = useState(today);
  const [flEnd, setFlEnd] = useState(plus5y);
  const [flFreq, setFlFreq] = useState('Semi-Annual');
  const [flDc, setFlDc] = useState('ACT/360');

  // Float Leg
  const flt = useEndpoint();
  const [ftNot, setFtNot] = useState('10000000');
  const [ftSpread, setFtSpread] = useState('0');
  const [ftStart, setFtStart] = useState(today);
  const [ftEnd, setFtEnd] = useState(plus5y);
  const [ftFreq, setFtFreq] = useState('Quarterly');
  const [ftDc, setFtDc] = useState('ACT/360');
  const [ftIdx, setFtIdx] = useState('SOFR');

  // Zero Coupon Leg
  const zc = useEndpoint();
  const [zcNot, setZcNot] = useState('10000000');
  const [zcRate, setZcRate] = useState('0.04');
  const [zcStart, setZcStart] = useState(today);
  const [zcEnd, setZcEnd] = useState(plus5y);
  const [zcDc, setZcDc] = useState('ACT/360');

  return (
    <>
      <EndpointCard title="Fixed Leg" description="Generate fixed-rate cashflow schedule">
        <Row>
          <Field label="Notional"><Input value={flNot} onChange={setFlNot} type="number" /></Field>
          <Field label="Rate"><Input value={flRate} onChange={setFlRate} type="number" /></Field>
          <Field label="Start"><Input value={flStart} onChange={setFlStart} type="date" /></Field>
          <Field label="End"><Input value={flEnd} onChange={setFlEnd} type="date" /></Field>
          <Field label="Frequency"><Select value={flFreq} onChange={setFlFreq} options={['Annual', 'Semi-Annual', 'Quarterly', 'Monthly']} /></Field>
          <Field label="Day Count"><Select value={flDc} onChange={setFlDc} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
        </Row>
        <Row>
          <RunButton loading={fixed.loading} onClick={() => fixed.run(() =>
            legsApi.fixed(Number(flNot), Number(flRate), flStart, flEnd, flFreq, flDc)
          )} />
        </Row>
        <ResultPanel data={fixed.result} error={fixed.error} />
      </EndpointCard>

      <EndpointCard title="Float Leg" description="Generate floating-rate cashflow schedule">
        <Row>
          <Field label="Notional"><Input value={ftNot} onChange={setFtNot} type="number" /></Field>
          <Field label="Spread"><Input value={ftSpread} onChange={setFtSpread} type="number" /></Field>
          <Field label="Start"><Input value={ftStart} onChange={setFtStart} type="date" /></Field>
          <Field label="End"><Input value={ftEnd} onChange={setFtEnd} type="date" /></Field>
          <Field label="Frequency"><Select value={ftFreq} onChange={setFtFreq} options={['Annual', 'Semi-Annual', 'Quarterly', 'Monthly']} /></Field>
          <Field label="Day Count"><Select value={ftDc} onChange={setFtDc} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
          <Field label="Index"><Input value={ftIdx} onChange={setFtIdx} /></Field>
        </Row>
        <Row>
          <RunButton loading={flt.loading} onClick={() => flt.run(() =>
            legsApi.float(Number(ftNot), ftStart, ftEnd, Number(ftSpread), ftFreq, ftDc, ftIdx)
          )} />
        </Row>
        <ResultPanel data={flt.result} error={flt.error} />
      </EndpointCard>

      <EndpointCard title="Zero Coupon Leg" description="Single cashflow at maturity">
        <Row>
          <Field label="Notional"><Input value={zcNot} onChange={setZcNot} type="number" /></Field>
          <Field label="Rate"><Input value={zcRate} onChange={setZcRate} type="number" /></Field>
          <Field label="Start"><Input value={zcStart} onChange={setZcStart} type="date" /></Field>
          <Field label="End"><Input value={zcEnd} onChange={setZcEnd} type="date" /></Field>
          <Field label="Day Count"><Select value={zcDc} onChange={setZcDc} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
          <RunButton loading={zc.loading} onClick={() => zc.run(() =>
            legsApi.zeroCoupon(Number(zcNot), Number(zcRate), zcStart, zcEnd, zcDc)
          )} />
        </Row>
        <ResultPanel data={zc.result} error={zc.error} />
      </EndpointCard>
    </>
  );
}
