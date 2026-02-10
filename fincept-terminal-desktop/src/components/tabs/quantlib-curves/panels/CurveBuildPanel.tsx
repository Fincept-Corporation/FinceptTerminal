import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { curveApi, interpolationApi, smoothApi } from '../quantlibCurvesApi';

const sampleTenors = '0.25, 0.5, 1, 2, 3, 5, 7, 10, 20, 30';
const sampleValues = '0.04, 0.042, 0.045, 0.048, 0.05, 0.052, 0.053, 0.054, 0.055, 0.056';

export default function CurveBuildPanel() {
  // Build Curve
  const build = useEndpoint();
  const [bRef, setBRef] = useState('2025-01-15');
  const [bTenors, setBTenors] = useState(sampleTenors);
  const [bValues, setBValues] = useState(sampleValues);
  const [bType, setBType] = useState('zero');
  const [bInterp, setBInterp] = useState('linear');

  // Zero Rate
  const zr = useEndpoint();
  const [zrRef, setZrRef] = useState('2025-01-15');
  const [zrTenors, setZrTenors] = useState(sampleTenors);
  const [zrValues, setZrValues] = useState(sampleValues);
  const [zrQuery, setZrQuery] = useState('4');

  // Discount Factor
  const df = useEndpoint();
  const [dfRef, setDfRef] = useState('2025-01-15');
  const [dfTenors, setDfTenors] = useState(sampleTenors);
  const [dfValues, setDfValues] = useState(sampleValues);
  const [dfQuery, setDfQuery] = useState('5');

  // Forward Rate
  const fr = useEndpoint();
  const [frRef, setFrRef] = useState('2025-01-15');
  const [frTenors, setFrTenors] = useState(sampleTenors);
  const [frValues, setFrValues] = useState(sampleValues);
  const [frStart, setFrStart] = useState('2');
  const [frEnd, setFrEnd] = useState('5');

  // Instantaneous Forward
  const inf = useEndpoint();
  const [infRef, setInfRef] = useState('2025-01-15');
  const [infTenors, setInfTenors] = useState(sampleTenors);
  const [infValues, setInfValues] = useState(sampleValues);
  const [infQuery, setInfQuery] = useState('3');

  // Curve Points
  const cp = useEndpoint();
  const [cpRef, setCpRef] = useState('2025-01-15');
  const [cpTenors, setCpTenors] = useState(sampleTenors);
  const [cpValues, setCpValues] = useState(sampleValues);
  const [cpQuery, setCpQuery] = useState('1, 2, 3, 5, 10, 15, 20, 30');

  // Interpolate
  const interp = useEndpoint();
  const [iX, setIX] = useState('1, 2, 3, 5, 10');
  const [iY, setIY] = useState('0.04, 0.045, 0.048, 0.052, 0.054');
  const [iQX, setIQX] = useState('1.5, 4, 7');
  const [iMethod, setIMethod] = useState('linear');

  // Interpolate Derivative
  const deriv = useEndpoint();
  const [dX, setDX] = useState('1, 2, 3, 5, 10');
  const [dY, setDY] = useState('0.04, 0.045, 0.048, 0.052, 0.054');
  const [dQX, setDQX] = useState('3');

  // Monotonicity Check
  const mono = useEndpoint();
  const [mX, setMX] = useState('1, 2, 3, 5, 10');
  const [mY, setMY] = useState('0.04, 0.045, 0.048, 0.052, 0.054');
  const [mQX, setMQX] = useState('1.5, 4, 7');

  // Constrained Fit
  const cfit = useEndpoint();
  const [cfTenors, setCfTenors] = useState(sampleTenors);
  const [cfRates, setCfRates] = useState(sampleValues);

  // Smoothness Penalty
  const smooth = useEndpoint();
  const [smX, setSmX] = useState('1, 2, 3, 5, 10');
  const [smY, setSmY] = useState('0.04, 0.045, 0.048, 0.052, 0.054');
  const [smQX, setSmQX] = useState('1.5, 4, 7');

  return (
    <>
      <EndpointCard title="Build Curve" description="Construct yield curve from tenors and rates">
        <Row>
          <Field label="Ref Date"><Input value={bRef} onChange={setBRef} /></Field>
          <Field label="Tenors (comma-sep)" width="250px"><Input value={bTenors} onChange={setBTenors} /></Field>
          <Field label="Values (comma-sep)" width="250px"><Input value={bValues} onChange={setBValues} /></Field>
          <Field label="Type"><Select value={bType} onChange={setBType} options={['zero', 'discount', 'forward']} /></Field>
          <Field label="Interpolation"><Select value={bInterp} onChange={setBInterp} options={['linear', 'log_linear', 'cubic', 'monotone_cubic']} /></Field>
          <RunButton loading={build.loading} onClick={() => build.run(() => curveApi.build(bRef, bTenors.split(',').map(Number), bValues.split(',').map(Number), bType, bInterp))} />
        </Row>
        <ResultPanel data={build.result} error={build.error} />
      </EndpointCard>

      <EndpointCard title="Zero Rate" description="Query zero rate at a tenor">
        <Row>
          <Field label="Ref Date"><Input value={zrRef} onChange={setZrRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={zrTenors} onChange={setZrTenors} /></Field>
          <Field label="Values" width="250px"><Input value={zrValues} onChange={setZrValues} /></Field>
          <Field label="Query Tenor"><Input value={zrQuery} onChange={setZrQuery} type="number" /></Field>
          <RunButton loading={zr.loading} onClick={() => zr.run(() => curveApi.zeroRate(zrRef, zrTenors.split(',').map(Number), zrValues.split(',').map(Number), Number(zrQuery)))} />
        </Row>
        <ResultPanel data={zr.result} error={zr.error} />
      </EndpointCard>

      <EndpointCard title="Discount Factor" description="Query discount factor at a tenor">
        <Row>
          <Field label="Ref Date"><Input value={dfRef} onChange={setDfRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={dfTenors} onChange={setDfTenors} /></Field>
          <Field label="Values" width="250px"><Input value={dfValues} onChange={setDfValues} /></Field>
          <Field label="Query Tenor"><Input value={dfQuery} onChange={setDfQuery} type="number" /></Field>
          <RunButton loading={df.loading} onClick={() => df.run(() => curveApi.discountFactor(dfRef, dfTenors.split(',').map(Number), dfValues.split(',').map(Number), Number(dfQuery)))} />
        </Row>
        <ResultPanel data={df.result} error={df.error} />
      </EndpointCard>

      <EndpointCard title="Forward Rate" description="Forward rate between two tenors">
        <Row>
          <Field label="Ref Date"><Input value={frRef} onChange={setFrRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={frTenors} onChange={setFrTenors} /></Field>
          <Field label="Values" width="250px"><Input value={frValues} onChange={setFrValues} /></Field>
          <Field label="Start Tenor"><Input value={frStart} onChange={setFrStart} type="number" /></Field>
          <Field label="End Tenor"><Input value={frEnd} onChange={setFrEnd} type="number" /></Field>
          <RunButton loading={fr.loading} onClick={() => fr.run(() => curveApi.forwardRate(frRef, frTenors.split(',').map(Number), frValues.split(',').map(Number), Number(frStart), Number(frEnd)))} />
        </Row>
        <ResultPanel data={fr.result} error={fr.error} />
      </EndpointCard>

      <EndpointCard title="Instantaneous Forward" description="Instantaneous forward rate at a tenor">
        <Row>
          <Field label="Ref Date"><Input value={infRef} onChange={setInfRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={infTenors} onChange={setInfTenors} /></Field>
          <Field label="Values" width="250px"><Input value={infValues} onChange={setInfValues} /></Field>
          <Field label="Query Tenor"><Input value={infQuery} onChange={setInfQuery} type="number" /></Field>
          <RunButton loading={inf.loading} onClick={() => inf.run(() => curveApi.instantaneousForward(infRef, infTenors.split(',').map(Number), infValues.split(',').map(Number), Number(infQuery)))} />
        </Row>
        <ResultPanel data={inf.result} error={inf.error} />
      </EndpointCard>

      <EndpointCard title="Curve Points" description="Evaluate curve at multiple tenors">
        <Row>
          <Field label="Ref Date"><Input value={cpRef} onChange={setCpRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={cpTenors} onChange={setCpTenors} /></Field>
          <Field label="Values" width="250px"><Input value={cpValues} onChange={setCpValues} /></Field>
          <Field label="Query Tenors" width="250px"><Input value={cpQuery} onChange={setCpQuery} /></Field>
          <RunButton loading={cp.loading} onClick={() => cp.run(() => curveApi.curvePoints(cpRef, cpTenors.split(',').map(Number), cpValues.split(',').map(Number), cpQuery.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={cp.result} error={cp.error} />
      </EndpointCard>

      <EndpointCard title="Interpolate" description="Interpolate values at query points">
        <Row>
          <Field label="X (comma-sep)" width="200px"><Input value={iX} onChange={setIX} /></Field>
          <Field label="Y (comma-sep)" width="200px"><Input value={iY} onChange={setIY} /></Field>
          <Field label="Query X" width="150px"><Input value={iQX} onChange={setIQX} /></Field>
          <Field label="Method"><Select value={iMethod} onChange={setIMethod} options={['linear', 'log_linear', 'cubic', 'monotone_cubic']} /></Field>
          <RunButton loading={interp.loading} onClick={() => interp.run(() => interpolationApi.interpolate(iX.split(',').map(Number), iY.split(',').map(Number), iQX.split(',').map(Number), iMethod))} />
        </Row>
        <ResultPanel data={interp.result} error={interp.error} />
      </EndpointCard>

      <EndpointCard title="Interpolate Derivative" description="Derivative of interpolated curve at a point">
        <Row>
          <Field label="X (comma-sep)" width="200px"><Input value={dX} onChange={setDX} /></Field>
          <Field label="Y (comma-sep)" width="200px"><Input value={dY} onChange={setDY} /></Field>
          <Field label="Query X"><Input value={dQX} onChange={setDQX} type="number" /></Field>
          <RunButton loading={deriv.loading} onClick={() => deriv.run(() => interpolationApi.derivative(dX.split(',').map(Number), dY.split(',').map(Number), Number(dQX)))} />
        </Row>
        <ResultPanel data={deriv.result} error={deriv.error} />
      </EndpointCard>

      <EndpointCard title="Monotonicity Check" description="Check monotonicity of interpolated curve">
        <Row>
          <Field label="X" width="200px"><Input value={mX} onChange={setMX} /></Field>
          <Field label="Y" width="200px"><Input value={mY} onChange={setMY} /></Field>
          <Field label="Query X" width="150px"><Input value={mQX} onChange={setMQX} /></Field>
          <RunButton loading={mono.loading} onClick={() => mono.run(() => interpolationApi.monotonicityCheck(mX.split(',').map(Number), mY.split(',').map(Number), mQX.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={mono.result} error={mono.error} />
      </EndpointCard>

      <EndpointCard title="Constrained Fit" description="Fit curve with monotonicity / positivity constraints">
        <Row>
          <Field label="Tenors (comma-sep)" width="250px"><Input value={cfTenors} onChange={setCfTenors} /></Field>
          <Field label="Rates (comma-sep)" width="250px"><Input value={cfRates} onChange={setCfRates} /></Field>
          <RunButton loading={cfit.loading} onClick={() => cfit.run(() => smoothApi.constrainedFit(cfTenors.split(',').map(Number), cfRates.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={cfit.result} error={cfit.error} />
      </EndpointCard>

      <EndpointCard title="Smoothness Penalty" description="Interpolation with smoothness penalty">
        <Row>
          <Field label="X" width="200px"><Input value={smX} onChange={setSmX} /></Field>
          <Field label="Y" width="200px"><Input value={smY} onChange={setSmY} /></Field>
          <Field label="Query X" width="150px"><Input value={smQX} onChange={setSmQX} /></Field>
          <RunButton loading={smooth.loading} onClick={() => smooth.run(() => smoothApi.smoothnessPenalty(smX.split(',').map(Number), smY.split(',').map(Number), smQX.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={smooth.result} error={smooth.error} />
      </EndpointCard>
    </>
  );
}
