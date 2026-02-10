import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { localVolApi } from '../quantlibVolatilityApi';

export default function LocalVolPanel() {
  // Constant Local Vol
  const con = useEndpoint();
  const [conVol, setConVol] = useState('0.2');
  const [conSpot, setConSpot] = useState('100');
  const [conTime, setConTime] = useState('1');

  // Implied to Local
  const itl = useEndpoint();
  const [itlSpot, setItlSpot] = useState('100');
  const [itlRate, setItlRate] = useState('0.05');
  const [itlStrike, setItlStrike] = useState('100');
  const [itlExpiry, setItlExpiry] = useState('1');
  const [itlIv, setItlIv] = useState('0.2');
  const [itlDk, setItlDk] = useState('0.01');
  const [itlDt, setItlDt] = useState('0.01');

  return (
    <>
      <EndpointCard title="Constant Local Vol" description="Local volatility from constant diffusion">
        <Row>
          <Field label="Volatility"><Input value={conVol} onChange={setConVol} type="number" /></Field>
          <Field label="Spot"><Input value={conSpot} onChange={setConSpot} type="number" /></Field>
          <Field label="Time"><Input value={conTime} onChange={setConTime} type="number" /></Field>
          <RunButton loading={con.loading} onClick={() => con.run(() =>
            localVolApi.constant(Number(conVol), Number(conSpot), Number(conTime))
          )} />
        </Row>
        <ResultPanel data={con.result} error={con.error} />
      </EndpointCard>

      <EndpointCard title="Implied to Local Vol" description="Dupire's formula: implied vol to local vol">
        <Row>
          <Field label="Spot"><Input value={itlSpot} onChange={setItlSpot} type="number" /></Field>
          <Field label="Rate"><Input value={itlRate} onChange={setItlRate} type="number" /></Field>
          <Field label="Strike"><Input value={itlStrike} onChange={setItlStrike} type="number" /></Field>
          <Field label="Expiry"><Input value={itlExpiry} onChange={setItlExpiry} type="number" /></Field>
          <Field label="Implied Vol"><Input value={itlIv} onChange={setItlIv} type="number" /></Field>
          <Field label="dk"><Input value={itlDk} onChange={setItlDk} type="number" /></Field>
          <Field label="dt"><Input value={itlDt} onChange={setItlDt} type="number" /></Field>
          <RunButton loading={itl.loading} onClick={() => itl.run(() =>
            localVolApi.impliedToLocal(Number(itlSpot), Number(itlRate), Number(itlStrike), Number(itlExpiry), Number(itlIv), Number(itlDk), Number(itlDt))
          )} />
        </Row>
        <ResultPanel data={itl.result} error={itl.error} />
      </EndpointCard>
    </>
  );
}
