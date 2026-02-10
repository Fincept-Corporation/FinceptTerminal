import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { bondApi, convexityAdjApi } from '../quantlibSolverApi';

export default function BondPanel() {
  // Bond Yield
  const by = useEndpoint();
  const [byPrice, setByPrice] = useState('95');
  const [byCoupon, setByCoupon] = useState('5');
  const [byMat, setByMat] = useState('10');
  const [byFace, setByFace] = useState('100');
  const [byFreq, setByFreq] = useState('2');
  const [byGuess, setByGuess] = useState('0.05');

  // Duration
  const dur = useEndpoint();
  const [durC, setDurC] = useState('5');
  const [durM, setDurM] = useState('10');
  const [durY, setDurY] = useState('0.05');
  const [durF, setDurF] = useState('100');
  const [durFr, setDurFr] = useState('2');

  // Modified Duration
  const md = useEndpoint();
  const [mdC, setMdC] = useState('5');
  const [mdM, setMdM] = useState('10');
  const [mdY, setMdY] = useState('0.05');
  const [mdF, setMdF] = useState('100');
  const [mdFr, setMdFr] = useState('2');

  // Convexity
  const cv = useEndpoint();
  const [cvC, setCvC] = useState('5');
  const [cvM, setCvM] = useState('10');
  const [cvY, setCvY] = useState('0.05');
  const [cvF, setCvF] = useState('100');
  const [cvFr, setCvFr] = useState('2');

  // Convexity Adjustment
  const ca = useEndpoint();
  const [caFwd, setCaFwd] = useState('0.05');
  const [caVol, setCaVol] = useState('0.2');
  const [caT1, setCaT1] = useState('1');
  const [caT2, setCaT2] = useState('2');
  const [caModel, setCaModel] = useState('simple');
  const [caMR, setCaMR] = useState('0.03');

  return (
    <>
      <EndpointCard title="Bond Yield" description="Solve for yield to maturity given price">
        <Row>
          <Field label="Price"><Input value={byPrice} onChange={setByPrice} type="number" /></Field>
          <Field label="Coupon"><Input value={byCoupon} onChange={setByCoupon} type="number" /></Field>
          <Field label="Maturity"><Input value={byMat} onChange={setByMat} type="number" /></Field>
          <Field label="Face"><Input value={byFace} onChange={setByFace} type="number" /></Field>
          <Field label="Frequency"><Input value={byFreq} onChange={setByFreq} type="number" /></Field>
          <Field label="Guess"><Input value={byGuess} onChange={setByGuess} type="number" /></Field>
          <RunButton loading={by.loading} onClick={() => by.run(() => bondApi.yield(Number(byPrice), Number(byCoupon), Number(byMat), Number(byFace), Number(byFreq), Number(byGuess)))} />
        </Row>
        <ResultPanel data={by.result} error={by.error} />
      </EndpointCard>

      <EndpointCard title="Duration" description="Macaulay duration of a bond">
        <Row>
          <Field label="Coupon"><Input value={durC} onChange={setDurC} type="number" /></Field>
          <Field label="Maturity"><Input value={durM} onChange={setDurM} type="number" /></Field>
          <Field label="YTM"><Input value={durY} onChange={setDurY} type="number" /></Field>
          <Field label="Face"><Input value={durF} onChange={setDurF} type="number" /></Field>
          <Field label="Frequency"><Input value={durFr} onChange={setDurFr} type="number" /></Field>
          <RunButton loading={dur.loading} onClick={() => dur.run(() => bondApi.duration(Number(durC), Number(durM), Number(durY), Number(durF), Number(durFr)))} />
        </Row>
        <ResultPanel data={dur.result} error={dur.error} />
      </EndpointCard>

      <EndpointCard title="Modified Duration" description="Modified duration of a bond">
        <Row>
          <Field label="Coupon"><Input value={mdC} onChange={setMdC} type="number" /></Field>
          <Field label="Maturity"><Input value={mdM} onChange={setMdM} type="number" /></Field>
          <Field label="YTM"><Input value={mdY} onChange={setMdY} type="number" /></Field>
          <Field label="Face"><Input value={mdF} onChange={setMdF} type="number" /></Field>
          <Field label="Frequency"><Input value={mdFr} onChange={setMdFr} type="number" /></Field>
          <RunButton loading={md.loading} onClick={() => md.run(() => bondApi.modifiedDuration(Number(mdC), Number(mdM), Number(mdY), Number(mdF), Number(mdFr)))} />
        </Row>
        <ResultPanel data={md.result} error={md.error} />
      </EndpointCard>

      <EndpointCard title="Convexity" description="Bond convexity measure">
        <Row>
          <Field label="Coupon"><Input value={cvC} onChange={setCvC} type="number" /></Field>
          <Field label="Maturity"><Input value={cvM} onChange={setCvM} type="number" /></Field>
          <Field label="YTM"><Input value={cvY} onChange={setCvY} type="number" /></Field>
          <Field label="Face"><Input value={cvF} onChange={setCvF} type="number" /></Field>
          <Field label="Frequency"><Input value={cvFr} onChange={setCvFr} type="number" /></Field>
          <RunButton loading={cv.loading} onClick={() => cv.run(() => bondApi.convexity(Number(cvC), Number(cvM), Number(cvY), Number(cvF), Number(cvFr)))} />
        </Row>
        <ResultPanel data={cv.result} error={cv.error} />
      </EndpointCard>

      <EndpointCard title="Convexity Adjustment" description="Forward rate convexity adjustment">
        <Row>
          <Field label="Forward Rate"><Input value={caFwd} onChange={setCaFwd} type="number" /></Field>
          <Field label="Volatility"><Input value={caVol} onChange={setCaVol} type="number" /></Field>
          <Field label="T1"><Input value={caT1} onChange={setCaT1} type="number" /></Field>
          <Field label="T2"><Input value={caT2} onChange={setCaT2} type="number" /></Field>
          <Field label="Model"><Select value={caModel} onChange={setCaModel} options={['simple', 'hull_white']} /></Field>
          <Field label="Mean Rev"><Input value={caMR} onChange={setCaMR} type="number" /></Field>
          <RunButton loading={ca.loading} onClick={() => ca.run(() => convexityAdjApi.adjust(Number(caFwd), Number(caVol), Number(caT1), Number(caT2), caModel, Number(caMR)))} />
        </Row>
        <ResultPanel data={ca.result} error={ca.error} />
      </EndpointCard>
    </>
  );
}
