import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { dupireApi, sviApi } from '../quantlibModelsApi';

export default function VolModelsPanel() {
  // Dupire
  const dup = useEndpoint();
  const [dupSpot, setDupSpot] = useState('100');
  const [dupR, setDupR] = useState('0');
  const [dupQ, setDupQ] = useState('0');
  const [dupSigma, setDupSigma] = useState('0.2');
  const [dupStrike, setDupStrike] = useState('100');
  const [dupT, setDupT] = useState('1');
  const [dupType, setDupType] = useState('call');

  // SVI Calibrate
  const svi = useEndpoint();
  const [sviStrikes, setSviStrikes] = useState('80, 90, 95, 100, 105, 110, 120');
  const [sviVols, setSviVols] = useState('0.25, 0.22, 0.21, 0.20, 0.21, 0.22, 0.25');
  const [sviFwd, setSviFwd] = useState('100');
  const [sviT, setSviT] = useState('1');

  return (
    <>
      <EndpointCard title="Dupire Local Vol Price" description="Option pricing under Dupire local volatility">
        <Row>
          <Field label="Spot"><Input value={dupSpot} onChange={setDupSpot} type="number" /></Field>
          <Field label="r"><Input value={dupR} onChange={setDupR} type="number" /></Field>
          <Field label="q"><Input value={dupQ} onChange={setDupQ} type="number" /></Field>
          <Field label="Sigma"><Input value={dupSigma} onChange={setDupSigma} type="number" /></Field>
          <Field label="Strike"><Input value={dupStrike} onChange={setDupStrike} type="number" /></Field>
          <Field label="T"><Input value={dupT} onChange={setDupT} type="number" /></Field>
          <Field label="Type"><Select value={dupType} onChange={setDupType} options={['call', 'put']} /></Field>
          <RunButton loading={dup.loading} onClick={() => dup.run(() =>
            dupireApi.price(Number(dupSpot), Number(dupR), Number(dupQ), Number(dupSigma), Number(dupStrike), Number(dupT), dupType)
          )} />
        </Row>
        <ResultPanel data={dup.result} error={dup.error} />
      </EndpointCard>

      <EndpointCard title="SVI Calibrate" description="Stochastic Volatility Inspired parameterization">
        <Row>
          <Field label="Strikes" width="300px"><Input value={sviStrikes} onChange={setSviStrikes} /></Field>
          <Field label="Market Vols" width="300px"><Input value={sviVols} onChange={setSviVols} /></Field>
          <Field label="Forward"><Input value={sviFwd} onChange={setSviFwd} type="number" /></Field>
          <Field label="T"><Input value={sviT} onChange={setSviT} type="number" /></Field>
          <RunButton loading={svi.loading} onClick={() => svi.run(() =>
            sviApi.calibrate(sviStrikes.split(',').map(Number), sviVols.split(',').map(Number), Number(sviFwd), Number(sviT))
          )} />
        </Row>
        <ResultPanel data={svi.result} error={svi.error} />
      </EndpointCard>
    </>
  );
}
