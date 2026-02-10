import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { odeApi, rootsApi, optimizeApi, leastSquaresApi } from '../quantlibNumericalApi';

export default function SolversPanel() {
  // ODE Solve
  const ode = useEndpoint();
  const [odeSystem, setOdeSystem] = useState('lotka_volterra');
  const [odeTSpan, setOdeTSpan] = useState('0, 10');
  const [odeY0, setOdeY0] = useState('10, 5');
  const [odeMethod, setOdeMethod] = useState('rk45');
  const [odeH, setOdeH] = useState('0.01');

  // Root 1D
  const r1 = useEndpoint();
  const [r1Func, setR1Func] = useState('cos');
  const [r1A, setR1A] = useState('0');
  const [r1B, setR1B] = useState('3');
  const [r1Method, setR1Method] = useState('brent');

  // Newton Root
  const nw = useEndpoint();
  const [nwFunc, setNwFunc] = useState('sin');
  const [nwX0, setNwX0] = useState('3');

  // Root ND
  const rn = useEndpoint();
  const [rnSystem, setRnSystem] = useState('simple_system');
  const [rnX0, setRnX0] = useState('1, 1');
  const [rnMethod, setRnMethod] = useState('newton');

  // Minimize
  const min = useEndpoint();
  const [minFunc, setMinFunc] = useState('rosenbrock');
  const [minX0, setMinX0] = useState('0, 0');
  const [minMethod, setMinMethod] = useState('bfgs');
  const [minTol, setMinTol] = useState('1e-8');

  // Nonlinear Least Squares
  const nls = useEndpoint();
  const [nlsModel, setNlsModel] = useState('exponential');
  const [nlsX, setNlsX] = useState('0, 1, 2, 3, 4');
  const [nlsY, setNlsY] = useState('1, 2.7, 7.4, 20.1, 54.6');
  const [nlsX0, setNlsX0] = useState('1, 1');

  return (
    <>
      <EndpointCard title="ODE Solve" description="Solve ordinary differential equations">
        <Row>
          <Field label="System"><Input value={odeSystem} onChange={setOdeSystem} /></Field>
          <Field label="t span" width="150px"><Input value={odeTSpan} onChange={setOdeTSpan} /></Field>
          <Field label="y0" width="150px"><Input value={odeY0} onChange={setOdeY0} /></Field>
          <Field label="Method"><Select value={odeMethod} onChange={setOdeMethod} options={['rk45', 'euler', 'rk4', 'adams']} /></Field>
          <Field label="h"><Input value={odeH} onChange={setOdeH} type="number" /></Field>
          <RunButton loading={ode.loading} onClick={() => ode.run(() => odeApi.solve(odeSystem, odeTSpan.split(',').map(Number), odeY0.split(',').map(Number), odeMethod, Number(odeH)))} />
        </Row>
        <ResultPanel data={ode.result} error={ode.error} />
      </EndpointCard>

      <EndpointCard title="Root Finding 1D" description="Find root of f(x)=0 in [a,b]">
        <Row>
          <Field label="Function"><Input value={r1Func} onChange={setR1Func} /></Field>
          <Field label="a"><Input value={r1A} onChange={setR1A} type="number" /></Field>
          <Field label="b"><Input value={r1B} onChange={setR1B} type="number" /></Field>
          <Field label="Method"><Select value={r1Method} onChange={setR1Method} options={['brent', 'bisection', 'secant', 'ridder']} /></Field>
          <RunButton loading={r1.loading} onClick={() => r1.run(() => rootsApi.find1d(r1Func, Number(r1A), Number(r1B), r1Method))} />
        </Row>
        <ResultPanel data={r1.result} error={r1.error} />
      </EndpointCard>

      <EndpointCard title="Newton Root" description="Newton's method root finding">
        <Row>
          <Field label="Function"><Input value={nwFunc} onChange={setNwFunc} /></Field>
          <Field label="x0"><Input value={nwX0} onChange={setNwX0} type="number" /></Field>
          <RunButton loading={nw.loading} onClick={() => nw.run(() => rootsApi.newton(nwFunc, Number(nwX0)))} />
        </Row>
        <ResultPanel data={nw.result} error={nw.error} />
      </EndpointCard>

      <EndpointCard title="Root Finding ND" description="Multi-dimensional root finding">
        <Row>
          <Field label="System"><Input value={rnSystem} onChange={setRnSystem} /></Field>
          <Field label="x0 (comma-sep)" width="200px"><Input value={rnX0} onChange={setRnX0} /></Field>
          <Field label="Method"><Select value={rnMethod} onChange={setRnMethod} options={['newton', 'broyden']} /></Field>
          <RunButton loading={rn.loading} onClick={() => rn.run(() => rootsApi.findNd(rnSystem, rnX0.split(',').map(Number), rnMethod))} />
        </Row>
        <ResultPanel data={rn.result} error={rn.error} />
      </EndpointCard>

      <EndpointCard title="Minimize" description="Unconstrained function minimization">
        <Row>
          <Field label="Function"><Input value={minFunc} onChange={setMinFunc} /></Field>
          <Field label="x0 (comma-sep)" width="200px"><Input value={minX0} onChange={setMinX0} /></Field>
          <Field label="Method"><Select value={minMethod} onChange={setMinMethod} options={['bfgs', 'nelder_mead', 'conjugate_gradient', 'powell']} /></Field>
          <Field label="Tolerance"><Input value={minTol} onChange={setMinTol} type="number" /></Field>
          <RunButton loading={min.loading} onClick={() => min.run(() => optimizeApi.minimize(minFunc, minX0.split(',').map(Number), minMethod, Number(minTol)))} />
        </Row>
        <ResultPanel data={min.result} error={min.error} />
      </EndpointCard>

      <EndpointCard title="Nonlinear Least Squares" description="Fit a model to data">
        <Row>
          <Field label="Model"><Input value={nlsModel} onChange={setNlsModel} /></Field>
          <Field label="x data" width="200px"><Input value={nlsX} onChange={setNlsX} /></Field>
          <Field label="y data" width="200px"><Input value={nlsY} onChange={setNlsY} /></Field>
          <Field label="x0 (initial)" width="150px"><Input value={nlsX0} onChange={setNlsX0} /></Field>
          <RunButton loading={nls.loading} onClick={() => nls.run(() => leastSquaresApi.fit(nlsModel, nlsX.split(',').map(Number), nlsY.split(',').map(Number), nlsX0.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nls.result} error={nls.error} />
      </EndpointCard>
    </>
  );
}
