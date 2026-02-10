import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { transformApi, constructionApi } from '../quantlibCurvesApi';

const sampleTenors = '0.25, 0.5, 1, 2, 3, 5, 7, 10, 20, 30';
const sampleValues = '0.04, 0.042, 0.045, 0.048, 0.05, 0.052, 0.053, 0.054, 0.055, 0.056';

export default function TransformPanel() {
  // Parallel Shift
  const ps = useEndpoint();
  const [psRef, setPsRef] = useState('2025-01-15');
  const [psTenors, setPsTenors] = useState(sampleTenors);
  const [psValues, setPsValues] = useState(sampleValues);
  const [psBps, setPsBps] = useState('25');
  const [psSpace, setPsSpace] = useState('zero_rate');

  // Twist
  const tw = useEndpoint();
  const [twRef, setTwRef] = useState('2025-01-15');
  const [twTenors, setTwTenors] = useState(sampleTenors);
  const [twValues, setTwValues] = useState(sampleValues);
  const [twPivot, setTwPivot] = useState('5');
  const [twShort, setTwShort] = useState('-10');
  const [twLong, setTwLong] = useState('15');

  // Butterfly
  const bf = useEndpoint();
  const [bfRef, setBfRef] = useState('2025-01-15');
  const [bfTenors, setBfTenors] = useState(sampleTenors);
  const [bfValues, setBfValues] = useState(sampleValues);
  const [bfBelly, setBfBelly] = useState('5');
  const [bfWing, setBfWing] = useState('10');
  const [bfBellyShift, setBfBellyShift] = useState('-5');

  // Key Rate Shift
  const kr = useEndpoint();
  const [krRef, setKrRef] = useState('2025-01-15');
  const [krTenors, setKrTenors] = useState(sampleTenors);
  const [krValues, setKrValues] = useState(sampleValues);
  const [krTenor, setKrTenor] = useState('5');
  const [krBps, setKrBps] = useState('25');
  const [krWidth, setKrWidth] = useState('2');

  // Roll Curve
  const roll = useEndpoint();
  const [rlRef, setRlRef] = useState('2025-01-15');
  const [rlTenors, setRlTenors] = useState(sampleTenors);
  const [rlValues, setRlValues] = useState(sampleValues);
  const [rlDays, setRlDays] = useState('30');

  // Scale Curve
  const sc = useEndpoint();
  const [scRef, setScRef] = useState('2025-01-15');
  const [scTenors, setScTenors] = useState(sampleTenors);
  const [scValues, setScValues] = useState(sampleValues);
  const [scFactor, setScFactor] = useState('1.1');

  // Composite Curve
  const comp = useEndpoint();
  const [compRef, setCompRef] = useState('2025-01-15');
  const [compBT, setCompBT] = useState('0.25, 0.5, 1, 2, 3');
  const [compBV, setCompBV] = useState('0.04, 0.042, 0.045, 0.048, 0.05');
  const [compFT, setCompFT] = useState('5, 7, 10, 20, 30');
  const [compFV, setCompFV] = useState('0.052, 0.053, 0.054, 0.055, 0.056');

  // Proxy Curve
  const proxy = useEndpoint();
  const [prRef, setPrRef] = useState('2025-01-15');
  const [prTenors, setPrTenors] = useState(sampleTenors);
  const [prValues, setPrValues] = useState(sampleValues);
  const [prSpread, setPrSpread] = useState('50');

  // Time Shift
  const ts = useEndpoint();
  const [tsRef, setTsRef] = useState('2025-01-15');
  const [tsTenors, setTsTenors] = useState(sampleTenors);
  const [tsValues, setTsValues] = useState(sampleValues);
  const [tsDays, setTsDays] = useState('90');
  const [tsQuery, setTsQuery] = useState('1, 2, 5, 10');

  return (
    <>
      <EndpointCard title="Parallel Shift" description="Shift entire curve by basis points">
        <Row>
          <Field label="Ref Date"><Input value={psRef} onChange={setPsRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={psTenors} onChange={setPsTenors} /></Field>
          <Field label="Values" width="250px"><Input value={psValues} onChange={setPsValues} /></Field>
          <Field label="Shift (bps)"><Input value={psBps} onChange={setPsBps} type="number" /></Field>
          <Field label="Space"><Select value={psSpace} onChange={setPsSpace} options={['zero_rate', 'discount', 'forward']} /></Field>
          <RunButton loading={ps.loading} onClick={() => ps.run(() => transformApi.parallelShift(psRef, psTenors.split(',').map(Number), psValues.split(',').map(Number), Number(psBps), psSpace))} />
        </Row>
        <ResultPanel data={ps.result} error={ps.error} />
      </EndpointCard>

      <EndpointCard title="Twist" description="Apply twist (steepening/flattening) around pivot">
        <Row>
          <Field label="Ref Date"><Input value={twRef} onChange={setTwRef} /></Field>
          <Field label="Tenors" width="200px"><Input value={twTenors} onChange={setTwTenors} /></Field>
          <Field label="Values" width="200px"><Input value={twValues} onChange={setTwValues} /></Field>
          <Field label="Pivot (yrs)"><Input value={twPivot} onChange={setTwPivot} type="number" /></Field>
          <Field label="Short bps"><Input value={twShort} onChange={setTwShort} type="number" /></Field>
          <Field label="Long bps"><Input value={twLong} onChange={setTwLong} type="number" /></Field>
          <RunButton loading={tw.loading} onClick={() => tw.run(() => transformApi.twist(twRef, twTenors.split(',').map(Number), twValues.split(',').map(Number), Number(twPivot), Number(twShort), Number(twLong)))} />
        </Row>
        <ResultPanel data={tw.result} error={tw.error} />
      </EndpointCard>

      <EndpointCard title="Butterfly" description="Butterfly shift — wings vs belly">
        <Row>
          <Field label="Ref Date"><Input value={bfRef} onChange={setBfRef} /></Field>
          <Field label="Tenors" width="200px"><Input value={bfTenors} onChange={setBfTenors} /></Field>
          <Field label="Values" width="200px"><Input value={bfValues} onChange={setBfValues} /></Field>
          <Field label="Belly (yrs)"><Input value={bfBelly} onChange={setBfBelly} type="number" /></Field>
          <Field label="Wing bps"><Input value={bfWing} onChange={setBfWing} type="number" /></Field>
          <Field label="Belly bps"><Input value={bfBellyShift} onChange={setBfBellyShift} type="number" /></Field>
          <RunButton loading={bf.loading} onClick={() => bf.run(() => transformApi.butterfly(bfRef, bfTenors.split(',').map(Number), bfValues.split(',').map(Number), Number(bfBelly), Number(bfWing), Number(bfBellyShift)))} />
        </Row>
        <ResultPanel data={bf.result} error={bf.error} />
      </EndpointCard>

      <EndpointCard title="Key Rate Shift" description="Shift curve at a specific tenor with width">
        <Row>
          <Field label="Ref Date"><Input value={krRef} onChange={setKrRef} /></Field>
          <Field label="Tenors" width="200px"><Input value={krTenors} onChange={setKrTenors} /></Field>
          <Field label="Values" width="200px"><Input value={krValues} onChange={setKrValues} /></Field>
          <Field label="Tenor (yrs)"><Input value={krTenor} onChange={setKrTenor} type="number" /></Field>
          <Field label="Shift bps"><Input value={krBps} onChange={setKrBps} type="number" /></Field>
          <Field label="Width (yrs)"><Input value={krWidth} onChange={setKrWidth} type="number" /></Field>
          <RunButton loading={kr.loading} onClick={() => kr.run(() => transformApi.keyRateShift(krRef, krTenors.split(',').map(Number), krValues.split(',').map(Number), Number(krTenor), Number(krBps), Number(krWidth)))} />
        </Row>
        <ResultPanel data={kr.result} error={kr.error} />
      </EndpointCard>

      <EndpointCard title="Roll Curve" description="Roll curve forward by N days">
        <Row>
          <Field label="Ref Date"><Input value={rlRef} onChange={setRlRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={rlTenors} onChange={setRlTenors} /></Field>
          <Field label="Values" width="250px"><Input value={rlValues} onChange={setRlValues} /></Field>
          <Field label="Days"><Input value={rlDays} onChange={setRlDays} type="number" /></Field>
          <RunButton loading={roll.loading} onClick={() => roll.run(() => transformApi.roll(rlRef, rlTenors.split(',').map(Number), rlValues.split(',').map(Number), Number(rlDays)))} />
        </Row>
        <ResultPanel data={roll.result} error={roll.error} />
      </EndpointCard>

      <EndpointCard title="Scale Curve" description="Multiply all curve values by a factor">
        <Row>
          <Field label="Ref Date"><Input value={scRef} onChange={setScRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={scTenors} onChange={setScTenors} /></Field>
          <Field label="Values" width="250px"><Input value={scValues} onChange={setScValues} /></Field>
          <Field label="Factor"><Input value={scFactor} onChange={setScFactor} type="number" /></Field>
          <RunButton loading={sc.loading} onClick={() => sc.run(() => transformApi.scale(scRef, scTenors.split(',').map(Number), scValues.split(',').map(Number), Number(scFactor)))} />
        </Row>
        <ResultPanel data={sc.result} error={sc.error} />
      </EndpointCard>

      <EndpointCard title="Composite Curve" description="Combine base + forecast curves">
        <Row>
          <Field label="Ref Date"><Input value={compRef} onChange={setCompRef} /></Field>
          <Field label="Base Tenors" width="180px"><Input value={compBT} onChange={setCompBT} /></Field>
          <Field label="Base Values" width="180px"><Input value={compBV} onChange={setCompBV} /></Field>
          <Field label="Fcast Tenors" width="180px"><Input value={compFT} onChange={setCompFT} /></Field>
          <Field label="Fcast Values" width="180px"><Input value={compFV} onChange={setCompFV} /></Field>
          <RunButton loading={comp.loading} onClick={() => comp.run(() => constructionApi.composite(compRef, compBT.split(',').map(Number), compBV.split(',').map(Number), compFT.split(',').map(Number), compFV.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={comp.result} error={comp.error} />
      </EndpointCard>

      <EndpointCard title="Proxy Curve" description="Base curve + constant spread">
        <Row>
          <Field label="Ref Date"><Input value={prRef} onChange={setPrRef} /></Field>
          <Field label="Tenors" width="250px"><Input value={prTenors} onChange={setPrTenors} /></Field>
          <Field label="Values" width="250px"><Input value={prValues} onChange={setPrValues} /></Field>
          <Field label="Spread (bps)"><Input value={prSpread} onChange={setPrSpread} type="number" /></Field>
          <RunButton loading={proxy.loading} onClick={() => proxy.run(() => constructionApi.proxy(prRef, prTenors.split(',').map(Number), prValues.split(',').map(Number), Number(prSpread)))} />
        </Row>
        <ResultPanel data={proxy.result} error={proxy.error} />
      </EndpointCard>

      <EndpointCard title="Time Shift" description="Shift curve reference date and re-evaluate">
        <Row>
          <Field label="Ref Date"><Input value={tsRef} onChange={setTsRef} /></Field>
          <Field label="Tenors" width="200px"><Input value={tsTenors} onChange={setTsTenors} /></Field>
          <Field label="Values" width="200px"><Input value={tsValues} onChange={setTsValues} /></Field>
          <Field label="Shift Days"><Input value={tsDays} onChange={setTsDays} type="number" /></Field>
          <Field label="Query Tenors" width="150px"><Input value={tsQuery} onChange={setTsQuery} /></Field>
          <RunButton loading={ts.loading} onClick={() => ts.run(() => constructionApi.timeShift(tsRef, tsTenors.split(',').map(Number), tsValues.split(',').map(Number), Number(tsDays), tsQuery.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ts.result} error={ts.error} />
      </EndpointCard>
    </>
  );
}
