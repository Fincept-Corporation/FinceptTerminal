import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { rateApi, impliedVolApi } from '../quantlibSolverApi';

const compTypes = ['continuous', 'simple', 'annual', 'semi_annual'];

export default function RatesPanel() {
  // Discount Factor
  const df = useEndpoint();
  const [dfRate, setDfRate] = useState('0.05');
  const [dfT, setDfT] = useState('1');
  const [dfComp, setDfComp] = useState('continuous');

  // Zero Rate
  const zr = useEndpoint();
  const [zrDF, setZrDF] = useState('0.95');
  const [zrT, setZrT] = useState('1');
  const [zrComp, setZrComp] = useState('continuous');

  // Forward Rate
  const fr = useEndpoint();
  const [frDF1, setFrDF1] = useState('0.97');
  const [frDF2, setFrDF2] = useState('0.94');
  const [frT1, setFrT1] = useState('1');
  const [frT2, setFrT2] = useState('2');
  const [frComp, setFrComp] = useState('simple');

  // Par Rate
  const pr = useEndpoint();
  const [prDFs, setPrDFs] = useState('0.97, 0.94, 0.91, 0.88, 0.85');
  const [prTimes, setPrTimes] = useState('1, 2, 3, 4, 5');
  const [prFreq, setPrFreq] = useState('2');

  // Implied Vol (BS)
  const ivBS = useEndpoint();
  const [ivBSPrice, setIvBSPrice] = useState('10');
  const [ivBSSpot, setIvBSSpot] = useState('100');
  const [ivBSK, setIvBSK] = useState('100');
  const [ivBST, setIvBST] = useState('1');
  const [ivBSR, setIvBSR] = useState('0.05');
  const [ivBSType, setIvBSType] = useState('call');
  const [ivBSDiv, setIvBSDiv] = useState('0');

  // Implied Vol (Black76)
  const ivB76 = useEndpoint();
  const [ivB76Price, setIvB76Price] = useState('5');
  const [ivB76Fwd, setIvB76Fwd] = useState('100');
  const [ivB76K, setIvB76K] = useState('100');
  const [ivB76T, setIvB76T] = useState('1');
  const [ivB76R, setIvB76R] = useState('0.05');
  const [ivB76Type, setIvB76Type] = useState('call');

  return (
    <>
      <EndpointCard title="Discount Factor" description="Compute discount factor from rate">
        <Row>
          <Field label="Rate"><Input value={dfRate} onChange={setDfRate} type="number" /></Field>
          <Field label="Time"><Input value={dfT} onChange={setDfT} type="number" /></Field>
          <Field label="Compounding"><Select value={dfComp} onChange={setDfComp} options={compTypes} /></Field>
          <RunButton loading={df.loading} onClick={() => df.run(() => rateApi.discountFactor(Number(dfRate), Number(dfT), dfComp))} />
        </Row>
        <ResultPanel data={df.result} error={df.error} />
      </EndpointCard>

      <EndpointCard title="Zero Rate" description="Implied zero rate from discount factor">
        <Row>
          <Field label="Discount Factor"><Input value={zrDF} onChange={setZrDF} type="number" /></Field>
          <Field label="Time"><Input value={zrT} onChange={setZrT} type="number" /></Field>
          <Field label="Compounding"><Select value={zrComp} onChange={setZrComp} options={compTypes} /></Field>
          <RunButton loading={zr.loading} onClick={() => zr.run(() => rateApi.zeroRate(Number(zrDF), Number(zrT), zrComp))} />
        </Row>
        <ResultPanel data={zr.result} error={zr.error} />
      </EndpointCard>

      <EndpointCard title="Forward Rate" description="Forward rate between two maturities">
        <Row>
          <Field label="DF1"><Input value={frDF1} onChange={setFrDF1} type="number" /></Field>
          <Field label="DF2"><Input value={frDF2} onChange={setFrDF2} type="number" /></Field>
          <Field label="T1"><Input value={frT1} onChange={setFrT1} type="number" /></Field>
          <Field label="T2"><Input value={frT2} onChange={setFrT2} type="number" /></Field>
          <Field label="Compounding"><Select value={frComp} onChange={setFrComp} options={compTypes} /></Field>
          <RunButton loading={fr.loading} onClick={() => fr.run(() => rateApi.forwardRate(Number(frDF1), Number(frDF2), Number(frT1), Number(frT2), frComp))} />
        </Row>
        <ResultPanel data={fr.result} error={fr.error} />
      </EndpointCard>

      <EndpointCard title="Par Rate" description="Par swap rate from discount factors">
        <Row>
          <Field label="Discount Factors" width="300px"><Input value={prDFs} onChange={setPrDFs} /></Field>
          <Field label="Times" width="200px"><Input value={prTimes} onChange={setPrTimes} /></Field>
          <Field label="Frequency"><Input value={prFreq} onChange={setPrFreq} type="number" /></Field>
          <RunButton loading={pr.loading} onClick={() => pr.run(() => rateApi.parRate(prDFs.split(',').map(Number), prTimes.split(',').map(Number), Number(prFreq)))} />
        </Row>
        <ResultPanel data={pr.result} error={pr.error} />
      </EndpointCard>

      <EndpointCard title="Implied Vol (Black-Scholes)" description="BS implied volatility from option price">
        <Row>
          <Field label="Price"><Input value={ivBSPrice} onChange={setIvBSPrice} type="number" /></Field>
          <Field label="Spot"><Input value={ivBSSpot} onChange={setIvBSSpot} type="number" /></Field>
          <Field label="Strike"><Input value={ivBSK} onChange={setIvBSK} type="number" /></Field>
          <Field label="Time"><Input value={ivBST} onChange={setIvBST} type="number" /></Field>
          <Field label="Rate"><Input value={ivBSR} onChange={setIvBSR} type="number" /></Field>
          <Field label="Type"><Select value={ivBSType} onChange={setIvBSType} options={['call', 'put']} /></Field>
          <Field label="Div Yield"><Input value={ivBSDiv} onChange={setIvBSDiv} type="number" /></Field>
          <RunButton loading={ivBS.loading} onClick={() => ivBS.run(() => impliedVolApi.bs(Number(ivBSPrice), Number(ivBSSpot), Number(ivBSK), Number(ivBST), Number(ivBSR), ivBSType, Number(ivBSDiv)))} />
        </Row>
        <ResultPanel data={ivBS.result} error={ivBS.error} />
      </EndpointCard>

      <EndpointCard title="Implied Vol (Black76)" description="Black76 implied volatility from option price">
        <Row>
          <Field label="Price"><Input value={ivB76Price} onChange={setIvB76Price} type="number" /></Field>
          <Field label="Forward"><Input value={ivB76Fwd} onChange={setIvB76Fwd} type="number" /></Field>
          <Field label="Strike"><Input value={ivB76K} onChange={setIvB76K} type="number" /></Field>
          <Field label="Time"><Input value={ivB76T} onChange={setIvB76T} type="number" /></Field>
          <Field label="Rate"><Input value={ivB76R} onChange={setIvB76R} type="number" /></Field>
          <Field label="Type"><Select value={ivB76Type} onChange={setIvB76Type} options={['call', 'put']} /></Field>
          <RunButton loading={ivB76.loading} onClick={() => ivB76.run(() => impliedVolApi.black76(Number(ivB76Price), Number(ivB76Fwd), Number(ivB76K), Number(ivB76T), Number(ivB76R), ivB76Type))} />
        </Row>
        <ResultPanel data={ivB76.result} error={ivB76.error} />
      </EndpointCard>
    </>
  );
}
