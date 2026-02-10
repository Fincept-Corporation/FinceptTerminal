import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { autodiffApi } from '../quantlibCoreApi';

export default function AutoDiffPanel() {
  const dual = useEndpoint();
  const [dFunc, setDFunc] = useState('exp');
  const [dX, setDX] = useState('1.0');
  const [dPow, setDPow] = useState('');

  const grad = useEndpoint();
  const [gFunc, setGFunc] = useState('sum_squares');
  const [gX, setGX] = useState('1.0, 2.0, 3.0');

  const taylor = useEndpoint();
  const [tFunc, setTFunc] = useState('exp');
  const [tX0, setTX0] = useState('0');
  const [tOrder, setTOrder] = useState('4');

  return (
    <>
      <EndpointCard title="Dual Eval (f & f')" description="POST /quantlib/core/autodiff/dual-eval">
        <Row>
          <Field label="Function"><Select value={dFunc} onChange={setDFunc} options={['exp', 'log', 'sin', 'cos', 'sqrt', 'power']} /></Field>
          <Field label="x"><Input value={dX} onChange={setDX} type="number" /></Field>
          <Field label="Power n (opt)"><Input value={dPow} onChange={setDPow} type="number" placeholder="for power" /></Field>
          <RunButton loading={dual.loading} onClick={() => dual.run(() => autodiffApi.dualEval(dFunc, Number(dX), dPow ? Number(dPow) : undefined))} />
        </Row>
        <ResultPanel data={dual.result} error={dual.error} />
      </EndpointCard>

      <EndpointCard title="Gradient" description="POST /quantlib/core/autodiff/gradient">
        <Row>
          <Field label="Function"><Input value={gFunc} onChange={setGFunc} /></Field>
          <Field label="x (comma-sep)" width="200px"><Input value={gX} onChange={setGX} placeholder="1.0, 2.0, 3.0" /></Field>
          <RunButton loading={grad.loading} onClick={() => grad.run(() => autodiffApi.gradient(gFunc, gX.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={grad.result} error={grad.error} />
      </EndpointCard>

      <EndpointCard title="Taylor Expansion" description="POST /quantlib/core/autodiff/taylor-expand">
        <Row>
          <Field label="Function"><Select value={tFunc} onChange={setTFunc} options={['exp', 'log', 'sin', 'cos', 'sqrt']} /></Field>
          <Field label="x₀"><Input value={tX0} onChange={setTX0} type="number" /></Field>
          <Field label="Order"><Input value={tOrder} onChange={setTOrder} type="number" /></Field>
          <RunButton loading={taylor.loading} onClick={() => taylor.run(() => autodiffApi.taylorExpand(tFunc, Number(tX0), Number(tOrder)))} />
        </Row>
        <ResultPanel data={taylor.result} error={taylor.error} />
      </EndpointCard>
    </>
  );
}
