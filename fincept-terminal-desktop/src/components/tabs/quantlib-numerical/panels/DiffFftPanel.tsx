import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { diffApi, fftApi, integrationApi } from '../quantlibNumericalApi';

export default function DiffFftPanel() {
  // Derivative
  const der = useEndpoint();
  const [derFunc, setDerFunc] = useState('sin');
  const [derX, setDerX] = useState('1.0');
  const [derH, setDerH] = useState('1e-7');
  const [derMethod, setDerMethod] = useState('central');

  // Gradient
  const grad = useEndpoint();
  const [gradFunc, setGradFunc] = useState('rosenbrock');
  const [gradX, setGradX] = useState('1.0, 1.0');
  const [gradH, setGradH] = useState('1e-7');

  // Hessian
  const hess = useEndpoint();
  const [hessFunc, setHessFunc] = useState('rosenbrock');
  const [hessX, setHessX] = useState('1.0, 1.0');

  // FFT Forward
  const fwd = useEndpoint();
  const [fwdData, setFwdData] = useState('1, 0, -1, 0, 1, 0, -1, 0');

  // FFT Inverse
  const inv = useEndpoint();
  const [invData, setInvData] = useState('4, 0, 0, 0, -4, 0, 0, 0');

  // FFT Convolve
  const conv = useEndpoint();
  const [convA, setConvA] = useState('1, 2, 3');
  const [convB, setConvB] = useState('0, 1, 0.5');

  // Quadrature
  const quad = useEndpoint();
  const [quadFunc, setQuadFunc] = useState('sin');
  const [quadA, setQuadA] = useState('0');
  const [quadB, setQuadB] = useState('3.14159');
  const [quadMethod, setQuadMethod] = useState('simpson');
  const [quadN, setQuadN] = useState('100');

  // MC Integration
  const mc = useEndpoint();
  const [mcFunc, setMcFunc] = useState('sphere_volume');
  const [mcBounds, setMcBounds] = useState('[[-1,1],[-1,1],[-1,1]]');
  const [mcN, setMcN] = useState('10000');

  // Stratified
  const strat = useEndpoint();
  const [stratFunc, setStratFunc] = useState('sin');
  const [stratA, setStratA] = useState('0');
  const [stratB, setStratB] = useState('3.14159');
  const [stratStrata, setStratStrata] = useState('10');
  const [stratSPS, setStratSPS] = useState('100');

  return (
    <>
      <EndpointCard title="Derivative" description="Finite difference derivative">
        <Row>
          <Field label="Function"><Input value={derFunc} onChange={setDerFunc} /></Field>
          <Field label="x"><Input value={derX} onChange={setDerX} type="number" /></Field>
          <Field label="h"><Input value={derH} onChange={setDerH} type="number" /></Field>
          <Field label="Method"><Select value={derMethod} onChange={setDerMethod} options={['central', 'forward', 'backward']} /></Field>
          <RunButton loading={der.loading} onClick={() => der.run(() => diffApi.derivative(derFunc, Number(derX), Number(derH), derMethod))} />
        </Row>
        <ResultPanel data={der.result} error={der.error} />
      </EndpointCard>

      <EndpointCard title="Gradient" description="Finite difference gradient">
        <Row>
          <Field label="Function"><Input value={gradFunc} onChange={setGradFunc} /></Field>
          <Field label="x (comma-sep)" width="200px"><Input value={gradX} onChange={setGradX} /></Field>
          <Field label="h"><Input value={gradH} onChange={setGradH} type="number" /></Field>
          <RunButton loading={grad.loading} onClick={() => grad.run(() => diffApi.gradient(gradFunc, gradX.split(',').map(Number), Number(gradH)))} />
        </Row>
        <ResultPanel data={grad.result} error={grad.error} />
      </EndpointCard>

      <EndpointCard title="Hessian" description="Finite difference Hessian matrix">
        <Row>
          <Field label="Function"><Input value={hessFunc} onChange={setHessFunc} /></Field>
          <Field label="x (comma-sep)" width="200px"><Input value={hessX} onChange={setHessX} /></Field>
          <RunButton loading={hess.loading} onClick={() => hess.run(() => diffApi.hessian(hessFunc, hessX.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={hess.result} error={hess.error} />
      </EndpointCard>

      <EndpointCard title="FFT Forward" description="Fast Fourier Transform">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={fwdData} onChange={setFwdData} /></Field>
          <RunButton loading={fwd.loading} onClick={() => fwd.run(() => fftApi.forward(fwdData.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={fwd.result} error={fwd.error} />
      </EndpointCard>

      <EndpointCard title="FFT Inverse" description="Inverse Fast Fourier Transform">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={invData} onChange={setInvData} /></Field>
          <RunButton loading={inv.loading} onClick={() => inv.run(() => fftApi.inverse(invData.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={inv.result} error={inv.error} />
      </EndpointCard>

      <EndpointCard title="FFT Convolve" description="Convolution via FFT">
        <Row>
          <Field label="Array A" width="200px"><Input value={convA} onChange={setConvA} /></Field>
          <Field label="Array B" width="200px"><Input value={convB} onChange={setConvB} /></Field>
          <RunButton loading={conv.loading} onClick={() => conv.run(() => fftApi.convolve(convA.split(',').map(Number), convB.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={conv.result} error={conv.error} />
      </EndpointCard>

      <EndpointCard title="Quadrature" description="Numerical integration via quadrature">
        <Row>
          <Field label="Function"><Input value={quadFunc} onChange={setQuadFunc} /></Field>
          <Field label="a"><Input value={quadA} onChange={setQuadA} type="number" /></Field>
          <Field label="b"><Input value={quadB} onChange={setQuadB} type="number" /></Field>
          <Field label="Method"><Select value={quadMethod} onChange={setQuadMethod} options={['simpson', 'trapezoidal', 'gauss']} /></Field>
          <Field label="n"><Input value={quadN} onChange={setQuadN} type="number" /></Field>
          <RunButton loading={quad.loading} onClick={() => quad.run(() => integrationApi.quadrature(quadFunc, Number(quadA), Number(quadB), quadMethod, Number(quadN)))} />
        </Row>
        <ResultPanel data={quad.result} error={quad.error} />
      </EndpointCard>

      <EndpointCard title="Monte Carlo Integration" description="MC integration over bounds">
        <Row>
          <Field label="Function"><Input value={mcFunc} onChange={setMcFunc} /></Field>
          <Field label="Bounds (JSON)" width="250px"><Input value={mcBounds} onChange={setMcBounds} /></Field>
          <Field label="N Samples"><Input value={mcN} onChange={setMcN} type="number" /></Field>
          <RunButton loading={mc.loading} onClick={() => mc.run(() => integrationApi.monteCarlo(mcFunc, JSON.parse(mcBounds), Number(mcN)))} />
        </Row>
        <ResultPanel data={mc.result} error={mc.error} />
      </EndpointCard>

      <EndpointCard title="Stratified Integration" description="Stratified sampling integration">
        <Row>
          <Field label="Function"><Input value={stratFunc} onChange={setStratFunc} /></Field>
          <Field label="a"><Input value={stratA} onChange={setStratA} type="number" /></Field>
          <Field label="b"><Input value={stratB} onChange={setStratB} type="number" /></Field>
          <Field label="Strata"><Input value={stratStrata} onChange={setStratStrata} type="number" /></Field>
          <Field label="Samples/Stratum"><Input value={stratSPS} onChange={setStratSPS} type="number" /></Field>
          <RunButton loading={strat.loading} onClick={() => strat.run(() => integrationApi.stratified(stratFunc, Number(stratA), Number(stratB), Number(stratStrata), Number(stratSPS)))} />
        </Row>
        <ResultPanel data={strat.result} error={strat.error} />
      </EndpointCard>
    </>
  );
}
