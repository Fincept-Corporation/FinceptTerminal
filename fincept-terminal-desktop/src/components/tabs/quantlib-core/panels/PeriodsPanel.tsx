import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { periodsApi } from '../quantlibCoreApi';

export default function PeriodsPanel() {
  const today = new Date().toISOString().split('T')[0];
  const plus6m = new Date(Date.now() + 182 * 86400000).toISOString().split('T')[0];

  // Fixed Coupon Period
  const fcp = useEndpoint();
  const [fcpNot, setFcpNot] = useState('10000000');
  const [fcpRate, setFcpRate] = useState('0.05');
  const [fcpStart, setFcpStart] = useState(today);
  const [fcpEnd, setFcpEnd] = useState(plus6m);
  const [fcpDc, setFcpDc] = useState('ACT/360');

  // Float Coupon Period
  const flp = useEndpoint();
  const [flpNot, setFlpNot] = useState('10000000');
  const [flpSpread, setFlpSpread] = useState('0');
  const [flpStart, setFlpStart] = useState(today);
  const [flpEnd, setFlpEnd] = useState(plus6m);
  const [flpDc, setFlpDc] = useState('ACT/360');
  const [flpFixing, setFlpFixing] = useState('0.05');

  // Day Count Fraction
  const dcfr = useEndpoint();
  const [dcfStart, setDcfStart] = useState(today);
  const [dcfEnd, setDcfEnd] = useState(plus6m);
  const [dcfConv, setDcfConv] = useState('ACT/360');

  return (
    <>
      <EndpointCard title="Fixed Coupon Period" description="Single fixed coupon calculation">
        <Row>
          <Field label="Notional"><Input value={fcpNot} onChange={setFcpNot} type="number" /></Field>
          <Field label="Rate"><Input value={fcpRate} onChange={setFcpRate} type="number" /></Field>
          <Field label="Start"><Input value={fcpStart} onChange={setFcpStart} type="date" /></Field>
          <Field label="End"><Input value={fcpEnd} onChange={setFcpEnd} type="date" /></Field>
          <Field label="Day Count"><Select value={fcpDc} onChange={setFcpDc} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
          <RunButton loading={fcp.loading} onClick={() => fcp.run(() =>
            periodsApi.fixedCoupon(Number(fcpNot), Number(fcpRate), fcpStart, fcpEnd, fcpDc)
          )} />
        </Row>
        <ResultPanel data={fcp.result} error={fcp.error} />
      </EndpointCard>

      <EndpointCard title="Float Coupon Period" description="Single floating coupon calculation">
        <Row>
          <Field label="Notional"><Input value={flpNot} onChange={setFlpNot} type="number" /></Field>
          <Field label="Spread"><Input value={flpSpread} onChange={setFlpSpread} type="number" /></Field>
          <Field label="Start"><Input value={flpStart} onChange={setFlpStart} type="date" /></Field>
          <Field label="End"><Input value={flpEnd} onChange={setFlpEnd} type="date" /></Field>
          <Field label="Day Count"><Select value={flpDc} onChange={setFlpDc} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
          <Field label="Fixing Rate"><Input value={flpFixing} onChange={setFlpFixing} type="number" /></Field>
          <RunButton loading={flp.loading} onClick={() => flp.run(() =>
            periodsApi.floatCoupon(Number(flpNot), flpStart, flpEnd, Number(flpSpread), flpDc, Number(flpFixing))
          )} />
        </Row>
        <ResultPanel data={flp.result} error={flp.error} />
      </EndpointCard>

      <EndpointCard title="Day Count Fraction" description="Year fraction between two dates">
        <Row>
          <Field label="Start"><Input value={dcfStart} onChange={setDcfStart} type="date" /></Field>
          <Field label="End"><Input value={dcfEnd} onChange={setDcfEnd} type="date" /></Field>
          <Field label="Convention"><Select value={dcfConv} onChange={setDcfConv} options={['ACT/360', 'ACT/365', '30/360', 'ACT/ACT']} /></Field>
          <RunButton loading={dcfr.loading} onClick={() => dcfr.run(() =>
            periodsApi.dayCountFraction(dcfStart, dcfEnd, dcfConv)
          )} />
        </Row>
        <ResultPanel data={dcfr.result} error={dcfr.error} />
      </EndpointCard>
    </>
  );
}
