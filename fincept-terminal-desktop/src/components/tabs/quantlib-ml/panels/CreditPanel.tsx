import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { creditApi } from '../quantlibMlApi';

export default function CreditPanel() {
  // Logistic Regression
  const lr = useEndpoint();
  const [lrX, setLrX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [lrY, setLrY] = useState('[0, 0, 1, 1]');
  const [lrPX, setLrPX] = useState('[[2,3],[6,7]]');

  // Discrimination
  const disc = useEndpoint();
  const [discTrue, setDiscTrue] = useState('0, 0, 1, 1, 1');
  const [discScore, setDiscScore] = useState('0.1, 0.3, 0.6, 0.8, 0.9');

  // WoE Binning
  const woe = useEndpoint();
  const [woeFeature, setWoeFeature] = useState('1, 2, 3, 4, 5, 6, 7, 8');
  const [woeTarget, setWoeTarget] = useState('0, 0, 0, 1, 0, 1, 1, 1');
  const [woeBins, setWoeBins] = useState('3');

  // Migration
  const mig = useEndpoint();
  const [migTrans, setMigTrans] = useState('[[0.9,0.08,0.02],[0.05,0.85,0.1],[0.01,0.04,0.95]]');
  const [migPeriods, setMigPeriods] = useState('2');

  // Calibration
  const cal = useEndpoint();
  const [calTrue, setCalTrue] = useState('0, 0, 1, 1, 1');
  const [calScore, setCalScore] = useState('0.1, 0.3, 0.6, 0.8, 0.9');
  const [calMethod, setCalMethod] = useState('isotonic');

  // Scorecard
  const sc = useEndpoint();
  const [scX, setScX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [scY, setScY] = useState('[0, 0, 1, 1]');
  const [scBins, setScBins] = useState('3');

  // Performance
  const perf = useEndpoint();
  const [perfTrue, setPerfTrue] = useState('0, 0, 1, 1, 1');
  const [perfScore, setPerfScore] = useState('0.1, 0.3, 0.6, 0.8, 0.9');

  // Two-Stage LGD
  const tsLgd = useEndpoint();
  const [tsX, setTsX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [tsLgdVal, setTsLgdVal] = useState('[0.2, 0.5, 0.0, 0.8]');
  const [tsCure, setTsCure] = useState('[0, 0, 1, 0]');

  // Beta LGD
  const bLgd = useEndpoint();
  const [bX, setBX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [bLgdVal, setBLgdVal] = useState('[0.2, 0.5, 0.1, 0.8]');

  return (
    <>
      <EndpointCard title="Logistic Regression" description="Credit scoring logistic regression">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={lrX} onChange={setLrX} /></Field>
          <Field label="y (JSON)" width="200px"><Input value={lrY} onChange={setLrY} /></Field>
          <Field label="Predict X (JSON)" width="200px"><Input value={lrPX} onChange={setLrPX} /></Field>
          <RunButton loading={lr.loading} onClick={() => lr.run(() => creditApi.logisticRegression(JSON.parse(lrX), JSON.parse(lrY), JSON.parse(lrPX)))} />
        </Row>
        <ResultPanel data={lr.result} error={lr.error} />
      </EndpointCard>

      <EndpointCard title="Discrimination" description="Discrimination metrics (AUC, Gini, KS)">
        <Row>
          <Field label="y_true (comma-sep)" width="250px"><Input value={discTrue} onChange={setDiscTrue} /></Field>
          <Field label="y_score (comma-sep)" width="250px"><Input value={discScore} onChange={setDiscScore} /></Field>
          <RunButton loading={disc.loading} onClick={() => disc.run(() => creditApi.discrimination(discTrue.split(',').map(Number), discScore.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={disc.result} error={disc.error} />
      </EndpointCard>

      <EndpointCard title="WoE Binning" description="Weight of Evidence binning">
        <Row>
          <Field label="Feature (comma-sep)" width="250px"><Input value={woeFeature} onChange={setWoeFeature} /></Field>
          <Field label="Target (comma-sep)" width="250px"><Input value={woeTarget} onChange={setWoeTarget} /></Field>
          <Field label="N Bins"><Input value={woeBins} onChange={setWoeBins} type="number" /></Field>
          <RunButton loading={woe.loading} onClick={() => woe.run(() => creditApi.woeBinning(woeFeature.split(',').map(Number), woeTarget.split(',').map(Number), Number(woeBins)))} />
        </Row>
        <ResultPanel data={woe.result} error={woe.error} />
      </EndpointCard>

      <EndpointCard title="Migration Matrix" description="Credit rating transition / migration">
        <Row>
          <Field label="Transitions (JSON 2D)" width="400px"><Input value={migTrans} onChange={setMigTrans} /></Field>
          <Field label="Periods"><Input value={migPeriods} onChange={setMigPeriods} type="number" /></Field>
          <RunButton loading={mig.loading} onClick={() => mig.run(() => creditApi.migration(JSON.parse(migTrans), Number(migPeriods)))} />
        </Row>
        <ResultPanel data={mig.result} error={mig.error} />
      </EndpointCard>

      <EndpointCard title="Calibration" description="Probability calibration (Platt / Isotonic)">
        <Row>
          <Field label="y_true (comma-sep)" width="200px"><Input value={calTrue} onChange={setCalTrue} /></Field>
          <Field label="y_score (comma-sep)" width="200px"><Input value={calScore} onChange={setCalScore} /></Field>
          <Field label="Method"><Select value={calMethod} onChange={setCalMethod} options={['isotonic', 'platt']} /></Field>
          <RunButton loading={cal.loading} onClick={() => cal.run(() => creditApi.calibration(calTrue.split(',').map(Number), calScore.split(',').map(Number), calMethod))} />
        </Row>
        <ResultPanel data={cal.result} error={cal.error} />
      </EndpointCard>

      <EndpointCard title="Scorecard" description="Build credit scorecard from features">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={scX} onChange={setScX} /></Field>
          <Field label="y (JSON)" width="200px"><Input value={scY} onChange={setScY} /></Field>
          <Field label="N Bins"><Input value={scBins} onChange={setScBins} type="number" /></Field>
          <RunButton loading={sc.loading} onClick={() => sc.run(() => creditApi.scorecard(JSON.parse(scX), JSON.parse(scY), Number(scBins)))} />
        </Row>
        <ResultPanel data={sc.result} error={sc.error} />
      </EndpointCard>

      <EndpointCard title="Performance" description="Credit model performance metrics">
        <Row>
          <Field label="y_true (comma-sep)" width="250px"><Input value={perfTrue} onChange={setPerfTrue} /></Field>
          <Field label="y_score (comma-sep)" width="250px"><Input value={perfScore} onChange={setPerfScore} /></Field>
          <RunButton loading={perf.loading} onClick={() => perf.run(() => creditApi.performance(perfTrue.split(',').map(Number), perfScore.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={perf.result} error={perf.error} />
      </EndpointCard>

      <EndpointCard title="Two-Stage LGD" description="Two-stage cure/loss LGD model">
        <Row>
          <Field label="X (JSON 2D)" width="200px"><Input value={tsX} onChange={setTsX} /></Field>
          <Field label="LGD (JSON)" width="200px"><Input value={tsLgdVal} onChange={setTsLgdVal} /></Field>
          <Field label="Cure (JSON)" width="150px"><Input value={tsCure} onChange={setTsCure} /></Field>
          <RunButton loading={tsLgd.loading} onClick={() => tsLgd.run(() => creditApi.twoStageLgd(JSON.parse(tsX), JSON.parse(tsLgdVal), JSON.parse(tsCure)))} />
        </Row>
        <ResultPanel data={tsLgd.result} error={tsLgd.error} />
      </EndpointCard>

      <EndpointCard title="Beta LGD" description="Beta-distributed LGD model">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={bX} onChange={setBX} /></Field>
          <Field label="LGD (JSON)" width="250px"><Input value={bLgdVal} onChange={setBLgdVal} /></Field>
          <RunButton loading={bLgd.loading} onClick={() => bLgd.run(() => creditApi.betaLgd(JSON.parse(bX), JSON.parse(bLgdVal)))} />
        </Row>
        <ResultPanel data={bLgd.result} error={bLgd.error} />
      </EndpointCard>
    </>
  );
}
