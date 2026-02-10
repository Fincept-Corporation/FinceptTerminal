import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { fixedBondApi, zeroBondApi, inflationBondApi } from '../quantlibInstrumentsApi';

export default function BondsPanel() {
  // Fixed Bond Cashflows
  const cf = useEndpoint();
  const [cfIssue, setCfIssue] = useState('2024-01-15');
  const [cfMat, setCfMat] = useState('2034-01-15');
  const [cfCoupon, setCfCoupon] = useState('0.05');
  const [cfFace, setCfFace] = useState('100');
  const [cfFreq, setCfFreq] = useState('2');

  // Fixed Bond Price
  const bp = useEndpoint();
  const [bpIssue, setBpIssue] = useState('2024-01-15');
  const [bpMat, setBpMat] = useState('2034-01-15');
  const [bpCoupon, setBpCoupon] = useState('0.05');
  const [bpYield, setBpYield] = useState('0.045');

  // Fixed Bond Yield
  const by = useEndpoint();
  const [byIssue, setByIssue] = useState('2024-01-15');
  const [byMat, setByMat] = useState('2034-01-15');
  const [byCoupon, setByCoupon] = useState('0.05');
  const [byPrice, setByPrice] = useState('103.5');

  // Fixed Bond Analytics
  const ba = useEndpoint();
  const [baIssue, setBaIssue] = useState('2024-01-15');
  const [baMat, setBaMat] = useState('2034-01-15');
  const [baCoupon, setBaCoupon] = useState('0.05');
  const [baYield, setBaYield] = useState('0.045');

  // Zero Coupon Price
  const zc = useEndpoint();
  const [zcIssue, setZcIssue] = useState('2024-01-15');
  const [zcMat, setZcMat] = useState('2025-01-15');
  const [zcFace, setZcFace] = useState('100');

  // Inflation-Linked
  const il = useEndpoint();
  const [ilIssue, setIlIssue] = useState('2024-01-15');
  const [ilMat, setIlMat] = useState('2034-01-15');
  const [ilCoupon, setIlCoupon] = useState('0.02');
  const [ilBaseCpi, setIlBaseCpi] = useState('260');
  const [ilCurCpi, setIlCurCpi] = useState('285');

  return (
    <>
      <EndpointCard title="Fixed Bond Cashflows" description="Generate bond cashflow schedule">
        <Row>
          <Field label="Issue Date"><Input value={cfIssue} onChange={setCfIssue} /></Field>
          <Field label="Maturity"><Input value={cfMat} onChange={setCfMat} /></Field>
          <Field label="Coupon Rate"><Input value={cfCoupon} onChange={setCfCoupon} type="number" /></Field>
          <Field label="Face"><Input value={cfFace} onChange={setCfFace} type="number" /></Field>
          <Field label="Frequency"><Input value={cfFreq} onChange={setCfFreq} type="number" /></Field>
          <RunButton loading={cf.loading} onClick={() => cf.run(() => fixedBondApi.cashflows(cfIssue, cfMat, Number(cfCoupon), Number(cfFace), Number(cfFreq)))} />
        </Row>
        <ResultPanel data={cf.result} error={cf.error} />
      </EndpointCard>

      <EndpointCard title="Fixed Bond Price" description="Price a fixed coupon bond from yield">
        <Row>
          <Field label="Issue Date"><Input value={bpIssue} onChange={setBpIssue} /></Field>
          <Field label="Maturity"><Input value={bpMat} onChange={setBpMat} /></Field>
          <Field label="Coupon Rate"><Input value={bpCoupon} onChange={setBpCoupon} type="number" /></Field>
          <Field label="Yield"><Input value={bpYield} onChange={setBpYield} type="number" /></Field>
          <RunButton loading={bp.loading} onClick={() => bp.run(() => fixedBondApi.price(bpIssue, bpMat, Number(bpCoupon), Number(bpYield)))} />
        </Row>
        <ResultPanel data={bp.result} error={bp.error} />
      </EndpointCard>

      <EndpointCard title="Fixed Bond Yield" description="Solve for YTM from clean price">
        <Row>
          <Field label="Issue Date"><Input value={byIssue} onChange={setByIssue} /></Field>
          <Field label="Maturity"><Input value={byMat} onChange={setByMat} /></Field>
          <Field label="Coupon Rate"><Input value={byCoupon} onChange={setByCoupon} type="number" /></Field>
          <Field label="Clean Price"><Input value={byPrice} onChange={setByPrice} type="number" /></Field>
          <RunButton loading={by.loading} onClick={() => by.run(() => fixedBondApi.yieldCalc(byIssue, byMat, Number(byCoupon), Number(byPrice)))} />
        </Row>
        <ResultPanel data={by.result} error={by.error} />
      </EndpointCard>

      <EndpointCard title="Fixed Bond Analytics" description="Full analytics: duration, convexity, DV01">
        <Row>
          <Field label="Issue Date"><Input value={baIssue} onChange={setBaIssue} /></Field>
          <Field label="Maturity"><Input value={baMat} onChange={setBaMat} /></Field>
          <Field label="Coupon Rate"><Input value={baCoupon} onChange={setBaCoupon} type="number" /></Field>
          <Field label="Yield"><Input value={baYield} onChange={setBaYield} type="number" /></Field>
          <RunButton loading={ba.loading} onClick={() => ba.run(() => fixedBondApi.analytics(baIssue, baMat, Number(baCoupon), Number(baYield)))} />
        </Row>
        <ResultPanel data={ba.result} error={ba.error} />
      </EndpointCard>

      <EndpointCard title="Zero Coupon Bond" description="Price a zero coupon bond">
        <Row>
          <Field label="Issue Date"><Input value={zcIssue} onChange={setZcIssue} /></Field>
          <Field label="Maturity"><Input value={zcMat} onChange={setZcMat} /></Field>
          <Field label="Face Value"><Input value={zcFace} onChange={setZcFace} type="number" /></Field>
          <RunButton loading={zc.loading} onClick={() => zc.run(() => zeroBondApi.price(zcIssue, zcMat, Number(zcFace)))} />
        </Row>
        <ResultPanel data={zc.result} error={zc.error} />
      </EndpointCard>

      <EndpointCard title="Inflation-Linked Bond" description="CPI-adjusted bond valuation">
        <Row>
          <Field label="Issue Date"><Input value={ilIssue} onChange={setIlIssue} /></Field>
          <Field label="Maturity"><Input value={ilMat} onChange={setIlMat} /></Field>
          <Field label="Coupon Rate"><Input value={ilCoupon} onChange={setIlCoupon} type="number" /></Field>
          <Field label="Base CPI"><Input value={ilBaseCpi} onChange={setIlBaseCpi} type="number" /></Field>
          <Field label="Current CPI"><Input value={ilCurCpi} onChange={setIlCurCpi} type="number" /></Field>
          <RunButton loading={il.loading} onClick={() => il.run(() => inflationBondApi.calc(ilIssue, ilMat, Number(ilCoupon), Number(ilBaseCpi), Number(ilCurCpi)))} />
        </Row>
        <ResultPanel data={il.result} error={il.error} />
      </EndpointCard>
    </>
  );
}
