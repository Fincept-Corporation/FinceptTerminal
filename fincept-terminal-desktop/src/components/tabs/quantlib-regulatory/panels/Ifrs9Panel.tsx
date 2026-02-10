import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { ifrs9Api } from '../quantlibRegulatoryApi';

export default function Ifrs9Panel() {
  // Stage Assessment
  const stage = useEndpoint();
  const [stDpd, setStDpd] = useState('30');
  const [stDown, setStDown] = useState('2');
  const [stPdRatio, setStPdRatio] = useState('2.5');
  const [stImpaired, setStImpaired] = useState('false');

  // ECL 12-Month
  const ecl12 = useEndpoint();
  const [e12Pd, setE12Pd] = useState('0.02');
  const [e12Lgd, setE12Lgd] = useState('0.45');
  const [e12Ead, setE12Ead] = useState('1000000');
  const [e12Dr, setE12Dr] = useState('0');

  // ECL Lifetime
  const eclLt = useEndpoint();
  const [eltPdCurve, setEltPdCurve] = useState('0.02, 0.025, 0.03, 0.035, 0.04');
  const [eltLgd, setEltLgd] = useState('0.45');
  const [eltEadCurve, setEltEadCurve] = useState('1000000, 900000, 800000, 700000, 600000');
  const [eltDr, setEltDr] = useState('0.03, 0.03, 0.03, 0.03, 0.03');

  // SICR
  const sicr = useEndpoint();
  const [sicrOrig, setSicrOrig] = useState('0.01');
  const [sicrCurr, setSicrCurr] = useState('0.03');
  const [sicrAbs, setSicrAbs] = useState('0.005');
  const [sicrRel, setSicrRel] = useState('2');

  return (
    <>
      <EndpointCard title="Stage Assessment" description="IFRS 9 staging (1/2/3) based on credit deterioration">
        <Row>
          <Field label="Days Past Due"><Input value={stDpd} onChange={setStDpd} type="number" /></Field>
          <Field label="Rating Downgrade"><Input value={stDown} onChange={setStDown} type="number" /></Field>
          <Field label="PD Increase Ratio"><Input value={stPdRatio} onChange={setStPdRatio} type="number" /></Field>
          <Field label="Credit Impaired">
            <select
              value={stImpaired}
              onChange={e => setStImpaired(e.target.value)}
              style={{
                backgroundColor: '#000000', border: '1px solid #2A2A2A', color: '#FFFFFF',
                padding: '8px 10px', borderRadius: '2px', fontSize: '10px', width: '100%',
                fontFamily: '"IBM Plex Mono", Consolas, monospace', outline: 'none',
              }}
            >
              <option value="false">No</option>
              <option value="true">Yes</option>
            </select>
          </Field>
          <RunButton loading={stage.loading} onClick={() => stage.run(() =>
            ifrs9Api.stageAssessment(Number(stDpd), Number(stDown), Number(stPdRatio), stImpaired === 'true')
          )} />
        </Row>
        <ResultPanel data={stage.result} error={stage.error} />
      </EndpointCard>

      <EndpointCard title="ECL 12-Month" description="12-month expected credit loss">
        <Row>
          <Field label="PD (12m)"><Input value={e12Pd} onChange={setE12Pd} type="number" /></Field>
          <Field label="LGD"><Input value={e12Lgd} onChange={setE12Lgd} type="number" /></Field>
          <Field label="EAD"><Input value={e12Ead} onChange={setE12Ead} type="number" /></Field>
          <Field label="Discount Rate"><Input value={e12Dr} onChange={setE12Dr} type="number" /></Field>
          <RunButton loading={ecl12.loading} onClick={() => ecl12.run(() =>
            ifrs9Api.ecl12m(Number(e12Pd), Number(e12Lgd), Number(e12Ead), Number(e12Dr))
          )} />
        </Row>
        <ResultPanel data={ecl12.result} error={ecl12.error} />
      </EndpointCard>

      <EndpointCard title="ECL Lifetime" description="Lifetime expected credit loss from PD curve">
        <Row>
          <Field label="PD Curve" width="200px"><Input value={eltPdCurve} onChange={setEltPdCurve} /></Field>
          <Field label="LGD"><Input value={eltLgd} onChange={setEltLgd} type="number" /></Field>
          <Field label="EAD Curve" width="200px"><Input value={eltEadCurve} onChange={setEltEadCurve} /></Field>
          <Field label="Discount Rates" width="200px"><Input value={eltDr} onChange={setEltDr} /></Field>
          <RunButton loading={eclLt.loading} onClick={() => eclLt.run(() =>
            ifrs9Api.eclLifetime(
              eltPdCurve.split(',').map(Number), Number(eltLgd),
              eltEadCurve.split(',').map(Number), eltDr.split(',').map(Number)
            )
          )} />
        </Row>
        <ResultPanel data={eclLt.result} error={eclLt.error} />
      </EndpointCard>

      <EndpointCard title="SICR" description="Significant Increase in Credit Risk detection">
        <Row>
          <Field label="Origination PD"><Input value={sicrOrig} onChange={setSicrOrig} type="number" /></Field>
          <Field label="Current PD"><Input value={sicrCurr} onChange={setSicrCurr} type="number" /></Field>
          <Field label="Abs Threshold"><Input value={sicrAbs} onChange={setSicrAbs} type="number" /></Field>
          <Field label="Rel Threshold"><Input value={sicrRel} onChange={setSicrRel} type="number" /></Field>
          <RunButton loading={sicr.loading} onClick={() => sicr.run(() =>
            ifrs9Api.sicr(Number(sicrOrig), Number(sicrCurr), Number(sicrAbs), Number(sicrRel))
          )} />
        </Row>
        <ResultPanel data={sicr.result} error={sicr.error} />
      </EndpointCard>
    </>
  );
}
