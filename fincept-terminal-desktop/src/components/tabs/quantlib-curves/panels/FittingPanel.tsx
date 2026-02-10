import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { nelsonSiegelApi } from '../quantlibCurvesApi';

const sampleTenors = '0.25, 0.5, 1, 2, 3, 5, 7, 10, 20, 30';
const sampleRates = '0.04, 0.042, 0.045, 0.048, 0.05, 0.052, 0.053, 0.054, 0.055, 0.056';

export default function FittingPanel() {
  // Nelson-Siegel Fit
  const nsf = useEndpoint();
  const [nsfT, setNsfT] = useState(sampleTenors);
  const [nsfR, setNsfR] = useState(sampleRates);

  // Nelson-Siegel Evaluate
  const nse = useEndpoint();
  const [nseB0, setNseB0] = useState('0.056');
  const [nseB1, setNseB1] = useState('-0.016');
  const [nseB2, setNseB2] = useState('-0.005');
  const [nseTau, setNseTau] = useState('1.5');
  const [nseQ, setNseQ] = useState('0.5, 1, 2, 5, 10, 30');

  // NSS Fit
  const nssf = useEndpoint();
  const [nssfT, setNssfT] = useState(sampleTenors);
  const [nssfR, setNssfR] = useState(sampleRates);

  // NSS Evaluate
  const nsse = useEndpoint();
  const [nsseB0, setNsseB0] = useState('0.056');
  const [nsseB1, setNsseB1] = useState('-0.016');
  const [nsseB2, setNsseB2] = useState('-0.005');
  const [nsseB3, setNsseB3] = useState('0.002');
  const [nsseT1, setNsseT1] = useState('1.5');
  const [nsseT2, setNsseT2] = useState('5');
  const [nsseQ, setNsseQ] = useState('0.5, 1, 2, 5, 10, 30');

  return (
    <>
      <EndpointCard title="Nelson-Siegel Fit" description="Fit NS model to market rates">
        <Row>
          <Field label="Tenors (comma-sep)" width="300px"><Input value={nsfT} onChange={setNsfT} /></Field>
          <Field label="Rates (comma-sep)" width="300px"><Input value={nsfR} onChange={setNsfR} /></Field>
          <RunButton loading={nsf.loading} onClick={() => nsf.run(() => nelsonSiegelApi.fit(nsfT.split(',').map(Number), nsfR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nsf.result} error={nsf.error} />
      </EndpointCard>

      <EndpointCard title="Nelson-Siegel Evaluate" description="Evaluate NS curve from parameters">
        <Row>
          <Field label="Beta0"><Input value={nseB0} onChange={setNseB0} type="number" /></Field>
          <Field label="Beta1"><Input value={nseB1} onChange={setNseB1} type="number" /></Field>
          <Field label="Beta2"><Input value={nseB2} onChange={setNseB2} type="number" /></Field>
          <Field label="Tau"><Input value={nseTau} onChange={setNseTau} type="number" /></Field>
          <Field label="Query Tenors" width="200px"><Input value={nseQ} onChange={setNseQ} /></Field>
          <RunButton loading={nse.loading} onClick={() => nse.run(() => nelsonSiegelApi.evaluate(Number(nseB0), Number(nseB1), Number(nseB2), Number(nseTau), nseQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nse.result} error={nse.error} />
      </EndpointCard>

      <EndpointCard title="Nelson-Siegel-Svensson Fit" description="Fit NSS (6-param) model to market rates">
        <Row>
          <Field label="Tenors (comma-sep)" width="300px"><Input value={nssfT} onChange={setNssfT} /></Field>
          <Field label="Rates (comma-sep)" width="300px"><Input value={nssfR} onChange={setNssfR} /></Field>
          <RunButton loading={nssf.loading} onClick={() => nssf.run(() => nelsonSiegelApi.nssFit(nssfT.split(',').map(Number), nssfR.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nssf.result} error={nssf.error} />
      </EndpointCard>

      <EndpointCard title="NSS Evaluate" description="Evaluate NSS curve from 6 parameters">
        <Row>
          <Field label="Beta0"><Input value={nsseB0} onChange={setNsseB0} type="number" /></Field>
          <Field label="Beta1"><Input value={nsseB1} onChange={setNsseB1} type="number" /></Field>
          <Field label="Beta2"><Input value={nsseB2} onChange={setNsseB2} type="number" /></Field>
          <Field label="Beta3"><Input value={nsseB3} onChange={setNsseB3} type="number" /></Field>
          <Field label="Tau1"><Input value={nsseT1} onChange={setNsseT1} type="number" /></Field>
          <Field label="Tau2"><Input value={nsseT2} onChange={setNsseT2} type="number" /></Field>
          <Field label="Query Tenors" width="200px"><Input value={nsseQ} onChange={setNsseQ} /></Field>
          <RunButton loading={nsse.loading} onClick={() => nsse.run(() => nelsonSiegelApi.nssEvaluate(Number(nsseB0), Number(nsseB1), Number(nsseB2), Number(nsseB3), Number(nsseT1), Number(nsseT2), nsseQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nsse.result} error={nsse.error} />
      </EndpointCard>
    </>
  );
}
