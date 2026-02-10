import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { factorApi } from '../quantlibMlApi';

export default function FactorPanel() {
  // Statistical Factor
  const sf = useEndpoint();
  const [sfReturns, setSfReturns] = useState('[[0.01,-0.02,0.03],[0.02,0.01,-0.01],[-0.01,0.03,0.02],[0.005,-0.01,0.015]]');
  const [sfFactors, setSfFactors] = useState('2');

  // Cross-Sectional Factor
  const csf = useEndpoint();
  const [csfReturns, setCsfReturns] = useState('[[0.01,-0.02,0.03],[0.02,0.01,-0.01],[-0.01,0.03,0.02]]');
  const [csfChars, setCsfChars] = useState('[[1,2],[3,4],[5,6]]');
  const [csfPeriods, setCsfPeriods] = useState('3');

  // Covariance Estimate
  const cov = useEndpoint();
  const [covReturns, setCovReturns] = useState('[[0.01,-0.02,0.03],[0.02,0.01,-0.01],[-0.01,0.03,0.02],[0.005,-0.01,0.015]]');
  const [covMethod, setCovMethod] = useState('sample');

  return (
    <>
      <EndpointCard title="Statistical Factor Model" description="PCA-based factor decomposition">
        <Row>
          <Field label="Returns (JSON 2D)" width="400px"><Input value={sfReturns} onChange={setSfReturns} /></Field>
          <Field label="N Factors"><Input value={sfFactors} onChange={setSfFactors} type="number" /></Field>
          <RunButton loading={sf.loading} onClick={() => sf.run(() => factorApi.statistical(JSON.parse(sfReturns), Number(sfFactors)))} />
        </Row>
        <ResultPanel data={sf.result} error={sf.error} />
      </EndpointCard>

      <EndpointCard title="Cross-Sectional Factor" description="Fama-MacBeth style cross-sectional regression">
        <Row>
          <Field label="Returns (JSON 2D)" width="300px"><Input value={csfReturns} onChange={setCsfReturns} /></Field>
          <Field label="Characteristics (JSON 2D)" width="200px"><Input value={csfChars} onChange={setCsfChars} /></Field>
          <Field label="N Periods"><Input value={csfPeriods} onChange={setCsfPeriods} type="number" /></Field>
          <RunButton loading={csf.loading} onClick={() => csf.run(() => factorApi.crossSectional(JSON.parse(csfReturns), JSON.parse(csfChars), Number(csfPeriods)))} />
        </Row>
        <ResultPanel data={csf.result} error={csf.error} />
      </EndpointCard>

      <EndpointCard title="Covariance Estimate" description="Sample / Ledoit-Wolf / Shrunk covariance">
        <Row>
          <Field label="Returns (JSON 2D)" width="400px"><Input value={covReturns} onChange={setCovReturns} /></Field>
          <Field label="Method"><Select value={covMethod} onChange={setCovMethod} options={['sample', 'ledoit_wolf', 'shrunk', 'oas']} /></Field>
          <RunButton loading={cov.loading} onClick={() => cov.run(() => factorApi.covarianceEstimate(JSON.parse(covReturns), covMethod))} />
        </Row>
        <ResultPanel data={cov.result} error={cov.error} />
      </EndpointCard>
    </>
  );
}
