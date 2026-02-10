import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { mathApi } from '../quantlibCoreApi';

export default function MathPanel() {
  const evalM = useEndpoint();
  const [mFunc, setMFunc] = useState('sin');
  const [mX, setMX] = useState('1.5708');

  const twoArg = useEndpoint();
  const [taFunc, setTaFunc] = useState('pow');
  const [taX, setTaX] = useState('2');
  const [taY, setTaY] = useState('10');

  return (
    <>
      <EndpointCard title="Math Eval (single arg)" description="sin, cos, tan, exp, log, sqrt, etc.">
        <Row>
          <Field label="Function"><Select value={mFunc} onChange={setMFunc} options={['sin', 'cos', 'tan', 'exp', 'log', 'sqrt', 'abs', 'ceil', 'floor']} /></Field>
          <Field label="x"><Input value={mX} onChange={setMX} type="number" /></Field>
          <RunButton loading={evalM.loading} onClick={() => evalM.run(() => mathApi.eval(mFunc, Number(mX)))} />
        </Row>
        <ResultPanel data={evalM.result} error={evalM.error} />
      </EndpointCard>

      <EndpointCard title="Math Two-Arg" description="pow, atan2, max, min, copysign, etc.">
        <Row>
          <Field label="Function"><Select value={taFunc} onChange={setTaFunc} options={['pow', 'atan2', 'max', 'min', 'copysign', 'fmod', 'hypot']} /></Field>
          <Field label="x"><Input value={taX} onChange={setTaX} type="number" /></Field>
          <Field label="y"><Input value={taY} onChange={setTaY} type="number" /></Field>
          <RunButton loading={twoArg.loading} onClick={() => twoArg.run(() => mathApi.twoArg(taFunc, Number(taX), Number(taY)))} />
        </Row>
        <ResultPanel data={twoArg.result} error={twoArg.error} />
      </EndpointCard>
    </>
  );
}
