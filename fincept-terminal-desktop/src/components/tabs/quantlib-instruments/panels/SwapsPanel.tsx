import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { irsApi, fraApi, oisApi } from '../quantlibInstrumentsApi';

const sampleTenors = '0.5, 1, 2, 3, 5, 7, 10';
const sampleRates = '0.04, 0.042, 0.044, 0.045, 0.048, 0.049, 0.05';

export default function SwapsPanel() {
  // IRS Value
  const iv = useEndpoint();
  const [ivEff, setIvEff] = useState('2024-03-15');
  const [ivMat, setIvMat] = useState('2029-03-15');
  const [ivNot, setIvNot] = useState('10000000');
  const [ivRate, setIvRate] = useState('0.04');
  const [ivRef, setIvRef] = useState('2024-01-15');
  const [ivType, setIvType] = useState('payer');

  // IRS Par Rate
  const ip = useEndpoint();
  const [ipEff, setIpEff] = useState('2024-03-15');
  const [ipMat, setIpMat] = useState('2029-03-15');
  const [ipNot, setIpNot] = useState('10000000');
  const [ipRate, setIpRate] = useState('0.04');
  const [ipRef, setIpRef] = useState('2024-01-15');

  // IRS DV01
  const idv = useEndpoint();
  const [idvEff, setIdvEff] = useState('2024-03-15');
  const [idvMat, setIdvMat] = useState('2029-03-15');
  const [idvNot, setIdvNot] = useState('10000000');
  const [idvRate, setIdvRate] = useState('0.04');
  const [idvRef, setIdvRef] = useState('2024-01-15');

  // FRA Value
  const fv = useEndpoint();
  const [fvTrade, setFvTrade] = useState('2024-01-15');
  const [fvStart, setFvStart] = useState('3');
  const [fvEnd, setFvEnd] = useState('6');
  const [fvNot, setFvNot] = useState('10000000');
  const [fvRate, setFvRate] = useState('0.045');
  const [fvRef, setFvRef] = useState('2024-01-15');

  // FRA Break-Even
  const fb = useEndpoint();
  const [fbTrade, setFbTrade] = useState('2024-01-15');
  const [fbStart, setFbStart] = useState('3');
  const [fbEnd, setFbEnd] = useState('6');
  const [fbNot, setFbNot] = useState('10000000');
  const [fbRate, setFbRate] = useState('0.045');
  const [fbRef, setFbRef] = useState('2024-01-15');

  // OIS Value
  const ov = useEndpoint();
  const [ovRef, setOvRef] = useState('2024-01-15');
  const [ovEff, setOvEff] = useState('2024-01-17');
  const [ovMat, setOvMat] = useState('2025-01-17');
  const [ovNot, setOvNot] = useState('10000000');
  const [ovRate, setOvRate] = useState('0.04');

  // OIS Build Curve
  const oc = useEndpoint();
  const [ocRef, setOcRef] = useState('2024-01-15');
  const [ocRates, setOcRates] = useState('0.04, 0.041, 0.042, 0.043, 0.044');

  return (
    <>
      <EndpointCard title="IRS Value" description="Value an interest rate swap">
        <Row>
          <Field label="Effective"><Input value={ivEff} onChange={setIvEff} /></Field>
          <Field label="Maturity"><Input value={ivMat} onChange={setIvMat} /></Field>
          <Field label="Notional"><Input value={ivNot} onChange={setIvNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={ivRate} onChange={setIvRate} type="number" /></Field>
          <Field label="Ref Date"><Input value={ivRef} onChange={setIvRef} /></Field>
          <Field label="Type"><Select value={ivType} onChange={setIvType} options={['payer', 'receiver']} /></Field>
          <RunButton loading={iv.loading} onClick={() => iv.run(() => irsApi.value(ivEff, ivMat, Number(ivNot), Number(ivRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), ivRef, 2, 'ACT/365', ivType))} />
        </Row>
        <ResultPanel data={iv.result} error={iv.error} />
      </EndpointCard>

      <EndpointCard title="IRS Par Rate" description="Compute par swap rate">
        <Row>
          <Field label="Effective"><Input value={ipEff} onChange={setIpEff} /></Field>
          <Field label="Maturity"><Input value={ipMat} onChange={setIpMat} /></Field>
          <Field label="Notional"><Input value={ipNot} onChange={setIpNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={ipRate} onChange={setIpRate} type="number" /></Field>
          <Field label="Ref Date"><Input value={ipRef} onChange={setIpRef} /></Field>
          <RunButton loading={ip.loading} onClick={() => ip.run(() => irsApi.parRate(ipEff, ipMat, Number(ipNot), Number(ipRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), ipRef))} />
        </Row>
        <ResultPanel data={ip.result} error={ip.error} />
      </EndpointCard>

      <EndpointCard title="IRS DV01" description="Dollar value of 1bp for IRS">
        <Row>
          <Field label="Effective"><Input value={idvEff} onChange={setIdvEff} /></Field>
          <Field label="Maturity"><Input value={idvMat} onChange={setIdvMat} /></Field>
          <Field label="Notional"><Input value={idvNot} onChange={setIdvNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={idvRate} onChange={setIdvRate} type="number" /></Field>
          <Field label="Ref Date"><Input value={idvRef} onChange={setIdvRef} /></Field>
          <RunButton loading={idv.loading} onClick={() => idv.run(() => irsApi.dv01(idvEff, idvMat, Number(idvNot), Number(idvRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), idvRef))} />
        </Row>
        <ResultPanel data={idv.result} error={idv.error} />
      </EndpointCard>

      <EndpointCard title="FRA Value" description="Value a Forward Rate Agreement">
        <Row>
          <Field label="Trade Date"><Input value={fvTrade} onChange={setFvTrade} /></Field>
          <Field label="Start (months)"><Input value={fvStart} onChange={setFvStart} type="number" /></Field>
          <Field label="End (months)"><Input value={fvEnd} onChange={setFvEnd} type="number" /></Field>
          <Field label="Notional"><Input value={fvNot} onChange={setFvNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={fvRate} onChange={setFvRate} type="number" /></Field>
          <Field label="Ref Date"><Input value={fvRef} onChange={setFvRef} /></Field>
          <RunButton loading={fv.loading} onClick={() => fv.run(() => fraApi.value(fvTrade, Number(fvStart), Number(fvEnd), Number(fvNot), Number(fvRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), fvRef))} />
        </Row>
        <ResultPanel data={fv.result} error={fv.error} />
      </EndpointCard>

      <EndpointCard title="FRA Break-Even" description="Break-even FRA rate">
        <Row>
          <Field label="Trade Date"><Input value={fbTrade} onChange={setFbTrade} /></Field>
          <Field label="Start (months)"><Input value={fbStart} onChange={setFbStart} type="number" /></Field>
          <Field label="End (months)"><Input value={fbEnd} onChange={setFbEnd} type="number" /></Field>
          <Field label="Notional"><Input value={fbNot} onChange={setFbNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={fbRate} onChange={setFbRate} type="number" /></Field>
          <Field label="Ref Date"><Input value={fbRef} onChange={setFbRef} /></Field>
          <RunButton loading={fb.loading} onClick={() => fb.run(() => fraApi.breakEven(fbTrade, Number(fbStart), Number(fbEnd), Number(fbNot), Number(fbRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number), fbRef))} />
        </Row>
        <ResultPanel data={fb.result} error={fb.error} />
      </EndpointCard>

      <EndpointCard title="OIS Value" description="Value an Overnight Index Swap">
        <Row>
          <Field label="Ref Date"><Input value={ovRef} onChange={setOvRef} /></Field>
          <Field label="Effective"><Input value={ovEff} onChange={setOvEff} /></Field>
          <Field label="Maturity"><Input value={ovMat} onChange={setOvMat} /></Field>
          <Field label="Notional"><Input value={ovNot} onChange={setOvNot} type="number" /></Field>
          <Field label="Fixed Rate"><Input value={ovRate} onChange={setOvRate} type="number" /></Field>
          <RunButton loading={ov.loading} onClick={() => ov.run(() => oisApi.value(ovRef, ovEff, ovMat, Number(ovNot), Number(ovRate), sampleTenors.split(',').map(Number), sampleRates.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ov.result} error={ov.error} />
      </EndpointCard>

      <EndpointCard title="OIS Build Curve" description="Build OIS discount curve">
        <Row>
          <Field label="Ref Date"><Input value={ocRef} onChange={setOcRef} /></Field>
          <Field label="OIS Rates" width="300px"><Input value={ocRates} onChange={setOcRates} /></Field>
          <RunButton loading={oc.loading} onClick={() => oc.run(() => oisApi.buildCurve(ocRef, ocRates.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={oc.result} error={oc.error} />
      </EndpointCard>
    </>
  );
}
