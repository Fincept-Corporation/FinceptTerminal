import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { validationApi, metricsApi } from '../quantlibMlApi';

export default function ValidationPanel() {
  // Calibration Report
  const calR = useEndpoint();
  const [calTrue, setCalTrue] = useState('0, 0, 1, 1, 1, 0, 1, 0');
  const [calScore, setCalScore] = useState('0.1, 0.2, 0.7, 0.8, 0.9, 0.3, 0.6, 0.4');

  // Discrimination Report
  const discR = useEndpoint();
  const [discTrue, setDiscTrue] = useState('0, 0, 1, 1, 1, 0, 1, 0');
  const [discScore, setDiscScore] = useState('0.1, 0.2, 0.7, 0.8, 0.9, 0.3, 0.6, 0.4');

  // Interpretability
  const interp = useEndpoint();
  const [interpPredict, setInterpPredict] = useState('linear');
  const [interpX, setInterpX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [interpY, setInterpY] = useState('[0, 0, 1, 1]');
  const [interpMethod, setInterpMethod] = useState('permutation');

  // Stability
  const stab = useEndpoint();
  const [stabExp, setStabExp] = useState('0.1, 0.2, 0.3, 0.15, 0.25');
  const [stabAct, setStabAct] = useState('0.12, 0.18, 0.35, 0.13, 0.22');
  const [stabBins, setStabBins] = useState('5');

  // Regression Metrics
  const regM = useEndpoint();
  const [regTrue, setRegTrue] = useState('1.5, 3.5, 5.5, 7.5');
  const [regPred, setRegPred] = useState('1.4, 3.6, 5.3, 7.8');

  // Classification Metrics
  const clsM = useEndpoint();
  const [clsTrue, setClsTrue] = useState('0, 0, 1, 1, 1, 0');
  const [clsPred, setClsPred] = useState('0, 1, 1, 1, 0, 0');

  return (
    <>
      <EndpointCard title="Calibration Report" description="Hosmer-Lemeshow, Brier score, calibration curve">
        <Row>
          <Field label="y_true (comma-sep)" width="300px"><Input value={calTrue} onChange={setCalTrue} /></Field>
          <Field label="y_score (comma-sep)" width="300px"><Input value={calScore} onChange={setCalScore} /></Field>
          <RunButton loading={calR.loading} onClick={() => calR.run(() => validationApi.calibrationReport(calTrue.split(',').map(Number), calScore.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={calR.result} error={calR.error} />
      </EndpointCard>

      <EndpointCard title="Discrimination Report" description="AUC, Gini, KS statistic, ROC/CAP curves">
        <Row>
          <Field label="y_true (comma-sep)" width="300px"><Input value={discTrue} onChange={setDiscTrue} /></Field>
          <Field label="y_score (comma-sep)" width="300px"><Input value={discScore} onChange={setDiscScore} /></Field>
          <RunButton loading={discR.loading} onClick={() => discR.run(() => validationApi.discriminationReport(discTrue.split(',').map(Number), discScore.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={discR.result} error={discR.error} />
      </EndpointCard>

      <EndpointCard title="Interpretability" description="Permutation / SHAP feature importance">
        <Row>
          <Field label="Model" width="100px"><Select value={interpPredict} onChange={setInterpPredict} options={['linear', 'tree', 'logistic']} /></Field>
          <Field label="X (JSON 2D)" width="200px"><Input value={interpX} onChange={setInterpX} /></Field>
          <Field label="y (JSON)" width="150px"><Input value={interpY} onChange={setInterpY} /></Field>
          <Field label="Method"><Select value={interpMethod} onChange={setInterpMethod} options={['permutation', 'shap']} /></Field>
          <RunButton loading={interp.loading} onClick={() => interp.run(() => validationApi.interpretability(interpPredict, JSON.parse(interpX), JSON.parse(interpY), interpMethod))} />
        </Row>
        <ResultPanel data={interp.result} error={interp.error} />
      </EndpointCard>

      <EndpointCard title="Population Stability" description="PSI (Population Stability Index)">
        <Row>
          <Field label="Expected (comma-sep)" width="250px"><Input value={stabExp} onChange={setStabExp} /></Field>
          <Field label="Actual (comma-sep)" width="250px"><Input value={stabAct} onChange={setStabAct} /></Field>
          <Field label="N Bins"><Input value={stabBins} onChange={setStabBins} type="number" /></Field>
          <RunButton loading={stab.loading} onClick={() => stab.run(() => validationApi.stability(stabExp.split(',').map(Number), stabAct.split(',').map(Number), Number(stabBins)))} />
        </Row>
        <ResultPanel data={stab.result} error={stab.error} />
      </EndpointCard>

      <EndpointCard title="Regression Metrics" description="MAE, MSE, RMSE, R-squared">
        <Row>
          <Field label="y_true (comma-sep)" width="250px"><Input value={regTrue} onChange={setRegTrue} /></Field>
          <Field label="y_pred (comma-sep)" width="250px"><Input value={regPred} onChange={setRegPred} /></Field>
          <RunButton loading={regM.loading} onClick={() => regM.run(() => metricsApi.regression(regTrue.split(',').map(Number), regPred.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={regM.result} error={regM.error} />
      </EndpointCard>

      <EndpointCard title="Classification Metrics" description="Accuracy, precision, recall, F1, confusion matrix">
        <Row>
          <Field label="y_true (comma-sep)" width="250px"><Input value={clsTrue} onChange={setClsTrue} /></Field>
          <Field label="y_pred (comma-sep)" width="250px"><Input value={clsPred} onChange={setClsPred} /></Field>
          <RunButton loading={clsM.loading} onClick={() => clsM.run(() => metricsApi.classification(clsTrue.split(',').map(Number), clsPred.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={clsM.result} error={clsM.error} />
      </EndpointCard>
    </>
  );
}
