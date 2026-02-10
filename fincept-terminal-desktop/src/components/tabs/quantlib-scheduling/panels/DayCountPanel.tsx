import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { dayCountApi, scheduleApi } from '../quantlibSchedulingApi';

export default function DayCountPanel() {
  // Year Fraction
  const yf = useEndpoint();
  const [yfStart, setYfStart] = useState('2025-01-01');
  const [yfEnd, setYfEnd] = useState('2025-07-01');
  const [yfConv, setYfConv] = useState('ACT/360');

  // Day Count
  const dc = useEndpoint();
  const [dcStart, setDcStart] = useState('2025-01-01');
  const [dcEnd, setDcEnd] = useState('2025-07-01');
  const [dcConv, setDcConv] = useState('ACT/360');

  // Batch Year Fraction
  const byf = useEndpoint();
  const [byfPairs, setByfPairs] = useState('[["2025-01-01","2025-07-01"],["2025-03-15","2025-09-15"]]');
  const [byfConv, setByfConv] = useState('ACT/360');

  // List Conventions
  const lc = useEndpoint();

  // Generate Schedule
  const gs = useEndpoint();
  const [gsEff, setGsEff] = useState('2025-01-15');
  const [gsTerm, setGsTerm] = useState('2027-01-15');
  const [gsFreq, setGsFreq] = useState('Semi-Annual');
  const [gsCal, setGsCal] = useState('Weekend');
  const [gsAdj, setGsAdj] = useState('Modified Following');
  const [gsStub, setGsStub] = useState('Short Front');

  return (
    <>
      <EndpointCard title="Year Fraction" description="Compute year fraction between two dates">
        <Row>
          <Field label="Start Date"><Input value={yfStart} onChange={setYfStart} /></Field>
          <Field label="End Date"><Input value={yfEnd} onChange={setYfEnd} /></Field>
          <Field label="Convention"><Select value={yfConv} onChange={setYfConv} options={['ACT/360', 'ACT/365', 'ACT/ACT', '30/360', '30E/360']} /></Field>
          <RunButton loading={yf.loading} onClick={() => yf.run(() => dayCountApi.yearFraction(yfStart, yfEnd, yfConv))} />
        </Row>
        <ResultPanel data={yf.result} error={yf.error} />
      </EndpointCard>

      <EndpointCard title="Day Count" description="Count days between two dates">
        <Row>
          <Field label="Start Date"><Input value={dcStart} onChange={setDcStart} /></Field>
          <Field label="End Date"><Input value={dcEnd} onChange={setDcEnd} /></Field>
          <Field label="Convention"><Select value={dcConv} onChange={setDcConv} options={['ACT/360', 'ACT/365', 'ACT/ACT', '30/360', '30E/360']} /></Field>
          <RunButton loading={dc.loading} onClick={() => dc.run(() => dayCountApi.dayCount(dcStart, dcEnd, dcConv))} />
        </Row>
        <ResultPanel data={dc.result} error={dc.error} />
      </EndpointCard>

      <EndpointCard title="Batch Year Fraction" description="Year fractions for multiple date pairs">
        <Row>
          <Field label="Date Pairs (JSON 2D)" width="450px"><Input value={byfPairs} onChange={setByfPairs} /></Field>
          <Field label="Convention"><Select value={byfConv} onChange={setByfConv} options={['ACT/360', 'ACT/365', 'ACT/ACT', '30/360', '30E/360']} /></Field>
          <RunButton loading={byf.loading} onClick={() => byf.run(() => dayCountApi.batchYearFraction(JSON.parse(byfPairs), byfConv))} />
        </Row>
        <ResultPanel data={byf.result} error={byf.error} />
      </EndpointCard>

      <EndpointCard title="List Conventions" description="Available day count conventions">
        <Row>
          <RunButton loading={lc.loading} onClick={() => lc.run(() => dayCountApi.conventions())} />
        </Row>
        <ResultPanel data={lc.result} error={lc.error} />
      </EndpointCard>

      <EndpointCard title="Generate Schedule" description="Generate coupon/payment schedule">
        <Row>
          <Field label="Effective Date"><Input value={gsEff} onChange={setGsEff} /></Field>
          <Field label="Termination Date"><Input value={gsTerm} onChange={setGsTerm} /></Field>
          <Field label="Frequency"><Select value={gsFreq} onChange={setGsFreq} options={['Annual', 'Semi-Annual', 'Quarterly', 'Monthly', 'Weekly']} /></Field>
          <Field label="Calendar"><Input value={gsCal} onChange={setGsCal} /></Field>
          <Field label="Adjustment"><Select value={gsAdj} onChange={setGsAdj} options={['Modified Following', 'Following', 'Preceding', 'Unadjusted']} /></Field>
          <Field label="Stub"><Select value={gsStub} onChange={setGsStub} options={['Short Front', 'Long Front', 'Short Back', 'Long Back']} /></Field>
          <RunButton loading={gs.loading} onClick={() => gs.run(() => scheduleApi.generate(gsEff, gsTerm, gsFreq, gsCal, gsAdj, gsStub))} />
        </Row>
        <ResultPanel data={gs.result} error={gs.error} />
      </EndpointCard>
    </>
  );
}
