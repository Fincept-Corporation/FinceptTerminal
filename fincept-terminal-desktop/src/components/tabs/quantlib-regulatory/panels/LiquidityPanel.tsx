import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { liquidityApi } from '../quantlibRegulatoryApi';

export default function LiquidityPanel() {
  // LCR
  const lcr = useEndpoint();
  const [lcrHqla1, setLcrHqla1] = useState('500000');
  const [lcrHqla2a, setLcrHqla2a] = useState('100000');
  const [lcrHqla2b, setLcrHqla2b] = useState('50000');
  const [lcrRetStable, setLcrRetStable] = useState('200000');
  const [lcrRetLess, setLcrRetLess] = useState('100000');
  const [lcrUnsecured, setLcrUnsecured] = useState('150000');
  const [lcrSecured, setLcrSecured] = useState('50000');
  const [lcrOther, setLcrOther] = useState('30000');
  const [lcrInflows, setLcrInflows] = useState('80000');

  // NSFR
  const nsfr = useEndpoint();
  const [nsfrCap, setNsfrCap] = useState('300000');
  const [nsfrStable, setNsfrStable] = useState('200000');
  const [nsfrLessStable, setNsfrLessStable] = useState('100000');
  const [nsfrWholesale, setNsfrWholesale] = useState('150000');
  const [nsfrCash, setNsfrCash] = useState('50000');
  const [nsfrSec, setNsfrSec] = useState('100000');
  const [nsfrLoansRet, setNsfrLoansRet] = useState('200000');
  const [nsfrLoansCorp, setNsfrLoansCorp] = useState('300000');
  const [nsfrOther, setNsfrOther] = useState('50000');

  return (
    <>
      <EndpointCard title="LCR" description="Liquidity Coverage Ratio">
        <Row>
          <Field label="HQLA Level 1"><Input value={lcrHqla1} onChange={setLcrHqla1} type="number" /></Field>
          <Field label="HQLA Level 2A"><Input value={lcrHqla2a} onChange={setLcrHqla2a} type="number" /></Field>
          <Field label="HQLA Level 2B"><Input value={lcrHqla2b} onChange={setLcrHqla2b} type="number" /></Field>
          <Field label="Retail Stable"><Input value={lcrRetStable} onChange={setLcrRetStable} type="number" /></Field>
          <Field label="Retail Less Stable"><Input value={lcrRetLess} onChange={setLcrRetLess} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Unsecured Wholesale"><Input value={lcrUnsecured} onChange={setLcrUnsecured} type="number" /></Field>
          <Field label="Secured Funding"><Input value={lcrSecured} onChange={setLcrSecured} type="number" /></Field>
          <Field label="Other Outflows"><Input value={lcrOther} onChange={setLcrOther} type="number" /></Field>
          <Field label="Inflows"><Input value={lcrInflows} onChange={setLcrInflows} type="number" /></Field>
          <RunButton loading={lcr.loading} onClick={() => lcr.run(() =>
            liquidityApi.lcr(
              Number(lcrHqla1), Number(lcrHqla2a), Number(lcrHqla2b),
              Number(lcrRetStable), Number(lcrRetLess),
              Number(lcrUnsecured), Number(lcrSecured), Number(lcrOther), Number(lcrInflows)
            )
          )} />
        </Row>
        <ResultPanel data={lcr.result} error={lcr.error} />
      </EndpointCard>

      <EndpointCard title="NSFR" description="Net Stable Funding Ratio">
        <Row>
          <Field label="Capital"><Input value={nsfrCap} onChange={setNsfrCap} type="number" /></Field>
          <Field label="Stable Deposits"><Input value={nsfrStable} onChange={setNsfrStable} type="number" /></Field>
          <Field label="Less Stable Dep."><Input value={nsfrLessStable} onChange={setNsfrLessStable} type="number" /></Field>
          <Field label="Wholesale Funding"><Input value={nsfrWholesale} onChange={setNsfrWholesale} type="number" /></Field>
          <Field label="Cash & Reserves"><Input value={nsfrCash} onChange={setNsfrCash} type="number" /></Field>
        </Row>
        <Row>
          <Field label="Securities L1"><Input value={nsfrSec} onChange={setNsfrSec} type="number" /></Field>
          <Field label="Loans (Retail)"><Input value={nsfrLoansRet} onChange={setNsfrLoansRet} type="number" /></Field>
          <Field label="Loans (Corporate)"><Input value={nsfrLoansCorp} onChange={setNsfrLoansCorp} type="number" /></Field>
          <Field label="Other Assets"><Input value={nsfrOther} onChange={setNsfrOther} type="number" /></Field>
          <RunButton loading={nsfr.loading} onClick={() => nsfr.run(() =>
            liquidityApi.nsfr(
              Number(nsfrCap), Number(nsfrStable), Number(nsfrLessStable),
              Number(nsfrWholesale), Number(nsfrCash), Number(nsfrSec),
              Number(nsfrLoansRet), Number(nsfrLoansCorp), Number(nsfrOther)
            )
          )} />
        </Row>
        <ResultPanel data={nsfr.result} error={nsfr.error} />
      </EndpointCard>
    </>
  );
}
