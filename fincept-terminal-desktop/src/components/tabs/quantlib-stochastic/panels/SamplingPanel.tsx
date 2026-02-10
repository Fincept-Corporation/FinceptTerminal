import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { samplingApi } from '../quantlibStochasticApi';

export default function SamplingPanel() {
  // Sobol
  const sob = useEndpoint();
  const [sobDim, setSobDim] = useState('3');
  const [sobN, setSobN] = useState('100');
  const [sobSkip, setSobSkip] = useState('0');

  // Antithetic
  const ant = useEndpoint();
  const [antN, setAntN] = useState('1000');

  // Correlated Normals
  const cn = useEndpoint();
  const [cnRho, setCnRho] = useState('0.5');
  const [cnN, setCnN] = useState('1000');

  // Multivariate Normal
  const mvn = useEndpoint();
  const [mvnMean, setMvnMean] = useState('[0, 0]');
  const [mvnCov, setMvnCov] = useState('[[1, 0.5], [0.5, 1]]');

  // Jump Sampler
  const jmp = useEndpoint();
  const [jmpN, setJmpN] = useState('1000');
  const [jmpInt, setJmpInt] = useState('1');
  const [jmpMean, setJmpMean] = useState('0');
  const [jmpStd, setJmpStd] = useState('0.1');
  const [jmpDist, setJmpDist] = useState('normal');

  // Distribution Sample
  const dst = useEndpoint();
  const [dstN, setDstN] = useState('1000');
  const [dstDist, setDstDist] = useState('exponential');
  const [dstParams, setDstParams] = useState('{"scale": 1}');

  return (
    <>
      <EndpointCard title="Sobol Sequence" description="Quasi-random low-discrepancy sequence">
        <Row>
          <Field label="Dimensions"><Input value={sobDim} onChange={setSobDim} type="number" /></Field>
          <Field label="N Samples"><Input value={sobN} onChange={setSobN} type="number" /></Field>
          <Field label="Skip"><Input value={sobSkip} onChange={setSobSkip} type="number" /></Field>
          <RunButton loading={sob.loading} onClick={() => sob.run(() => samplingApi.sobol(Number(sobDim), Number(sobN), Number(sobSkip)))} />
        </Row>
        <ResultPanel data={sob.result} error={sob.error} />
      </EndpointCard>

      <EndpointCard title="Antithetic Samples" description="Variance reduction via antithetic variates">
        <Row>
          <Field label="N Samples"><Input value={antN} onChange={setAntN} type="number" /></Field>
          <RunButton loading={ant.loading} onClick={() => ant.run(() => samplingApi.antithetic(Number(antN)))} />
        </Row>
        <ResultPanel data={ant.result} error={ant.error} />
      </EndpointCard>

      <EndpointCard title="Correlated Normals" description="Paired correlated normal samples">
        <Row>
          <Field label="Rho"><Input value={cnRho} onChange={setCnRho} type="number" /></Field>
          <Field label="N Samples"><Input value={cnN} onChange={setCnN} type="number" /></Field>
          <RunButton loading={cn.loading} onClick={() => cn.run(() => samplingApi.correlatedNormals(Number(cnRho), Number(cnN)))} />
        </Row>
        <ResultPanel data={cn.result} error={cn.error} />
      </EndpointCard>

      <EndpointCard title="Multivariate Normal" description="Multivariate normal sampling">
        <Row>
          <Field label="Mean (JSON)" width="200px"><Input value={mvnMean} onChange={setMvnMean} /></Field>
          <Field label="Cov (JSON)" width="300px"><Input value={mvnCov} onChange={setMvnCov} /></Field>
          <RunButton loading={mvn.loading} onClick={() => mvn.run(() => samplingApi.multivariateNormal(JSON.parse(mvnMean), JSON.parse(mvnCov)))} />
        </Row>
        <ResultPanel data={mvn.result} error={mvn.error} />
      </EndpointCard>

      <EndpointCard title="Jump Sampler" description="Jump-arrival + jump-size samples">
        <Row>
          <Field label="N"><Input value={jmpN} onChange={setJmpN} type="number" /></Field>
          <Field label="Intensity"><Input value={jmpInt} onChange={setJmpInt} type="number" /></Field>
          <Field label="Jump Mean"><Input value={jmpMean} onChange={setJmpMean} type="number" /></Field>
          <Field label="Jump Std"><Input value={jmpStd} onChange={setJmpStd} type="number" /></Field>
          <Field label="Distribution"><Select value={jmpDist} onChange={setJmpDist} options={['normal', 'exponential', 'uniform']} /></Field>
          <RunButton loading={jmp.loading} onClick={() => jmp.run(() => samplingApi.jump(Number(jmpN), Number(jmpInt), Number(jmpMean), Number(jmpStd), jmpDist))} />
        </Row>
        <ResultPanel data={jmp.result} error={jmp.error} />
      </EndpointCard>

      <EndpointCard title="Distribution Sample" description="General distribution sampling">
        <Row>
          <Field label="N"><Input value={dstN} onChange={setDstN} type="number" /></Field>
          <Field label="Distribution"><Select value={dstDist} onChange={setDstDist} options={['exponential', 'normal', 'uniform', 'poisson', 'gamma', 'beta']} /></Field>
          <Field label="Params (JSON)" width="200px"><Input value={dstParams} onChange={setDstParams} /></Field>
          <RunButton loading={dst.loading} onClick={() => dst.run(() => samplingApi.distribution(Number(dstN), dstDist, JSON.parse(dstParams)))} />
        </Row>
        <ResultPanel data={dst.result} error={dst.error} />
      </EndpointCard>
    </>
  );
}
