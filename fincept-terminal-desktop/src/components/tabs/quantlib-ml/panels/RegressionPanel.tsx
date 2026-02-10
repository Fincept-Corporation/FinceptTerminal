import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { regressionApi } from '../quantlibMlApi';

export default function RegressionPanel() {
  // Linear Fit
  const fit = useEndpoint();
  const [fitX, setFitX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [fitY, setFitY] = useState('[1.5, 3.5, 5.5, 7.5]');
  const [fitMethod, setFitMethod] = useState('ols');
  const [fitAlpha, setFitAlpha] = useState('1');
  const [fitL1, setFitL1] = useState('0.5');

  // Tree Regression
  const tree = useEndpoint();
  const [treeX, setTreeX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [treeY, setTreeY] = useState('[1.5, 3.5, 5.5, 7.5]');
  const [treeMethod, setTreeMethod] = useState('decision_tree');
  const [treeDepth, setTreeDepth] = useState('3');
  const [treeEst, setTreeEst] = useState('100');
  const [treeLr, setTreeLr] = useState('0.1');

  // EAD
  const ead = useEndpoint();
  const [eadX, setEadX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [eadY, setEadY] = useState('[100, 200, 150, 300]');

  // LGD
  const lgd = useEndpoint();
  const [lgdX, setLgdX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [lgdY, setLgdY] = useState('[0.2, 0.5, 0.3, 0.8]');

  // Ensemble
  const ens = useEndpoint();
  const [ensX, setEnsX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [ensY, setEnsY] = useState('[1.5, 3.5, 5.5, 7.5]');
  const [ensMethod, setEnsMethod] = useState('random_forest');
  const [ensEst, setEnsEst] = useState('100');
  const [ensDepth, setEnsDepth] = useState('3');

  return (
    <>
      <EndpointCard title="Linear Regression Fit" description="OLS / Ridge / Lasso / ElasticNet">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={fitX} onChange={setFitX} /></Field>
          <Field label="y (JSON)" width="200px"><Input value={fitY} onChange={setFitY} /></Field>
          <Field label="Method"><Select value={fitMethod} onChange={setFitMethod} options={['ols', 'ridge', 'lasso', 'elasticnet']} /></Field>
          <Field label="Alpha"><Input value={fitAlpha} onChange={setFitAlpha} type="number" /></Field>
          <Field label="L1 Ratio"><Input value={fitL1} onChange={setFitL1} type="number" /></Field>
          <RunButton loading={fit.loading} onClick={() => fit.run(() => regressionApi.fit(JSON.parse(fitX), JSON.parse(fitY), fitMethod, Number(fitAlpha), Number(fitL1)))} />
        </Row>
        <ResultPanel data={fit.result} error={fit.error} />
      </EndpointCard>

      <EndpointCard title="Tree Regression" description="Decision Tree / Random Forest / GBM">
        <Row>
          <Field label="X (JSON 2D)" width="200px"><Input value={treeX} onChange={setTreeX} /></Field>
          <Field label="y (JSON)" width="200px"><Input value={treeY} onChange={setTreeY} /></Field>
          <Field label="Method"><Select value={treeMethod} onChange={setTreeMethod} options={['decision_tree', 'random_forest', 'gradient_boosting']} /></Field>
          <Field label="Max Depth"><Input value={treeDepth} onChange={setTreeDepth} type="number" /></Field>
          <Field label="N Estimators"><Input value={treeEst} onChange={setTreeEst} type="number" /></Field>
          <Field label="Learning Rate"><Input value={treeLr} onChange={setTreeLr} type="number" /></Field>
          <RunButton loading={tree.loading} onClick={() => tree.run(() => regressionApi.tree(JSON.parse(treeX), JSON.parse(treeY), treeMethod, Number(treeDepth), Number(treeEst), Number(treeLr)))} />
        </Row>
        <ResultPanel data={tree.result} error={tree.error} />
      </EndpointCard>

      <EndpointCard title="EAD Model" description="Exposure at Default regression">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={eadX} onChange={setEadX} /></Field>
          <Field label="y (JSON)" width="250px"><Input value={eadY} onChange={setEadY} /></Field>
          <RunButton loading={ead.loading} onClick={() => ead.run(() => regressionApi.ead(JSON.parse(eadX), JSON.parse(eadY)))} />
        </Row>
        <ResultPanel data={ead.result} error={ead.error} />
      </EndpointCard>

      <EndpointCard title="LGD Model" description="Loss Given Default regression">
        <Row>
          <Field label="X (JSON 2D)" width="250px"><Input value={lgdX} onChange={setLgdX} /></Field>
          <Field label="y (JSON)" width="250px"><Input value={lgdY} onChange={setLgdY} /></Field>
          <RunButton loading={lgd.loading} onClick={() => lgd.run(() => regressionApi.lgd(JSON.parse(lgdX), JSON.parse(lgdY)))} />
        </Row>
        <ResultPanel data={lgd.result} error={lgd.error} />
      </EndpointCard>

      <EndpointCard title="Ensemble Regression" description="Random Forest / Gradient Boosting ensemble">
        <Row>
          <Field label="X (JSON 2D)" width="200px"><Input value={ensX} onChange={setEnsX} /></Field>
          <Field label="y (JSON)" width="200px"><Input value={ensY} onChange={setEnsY} /></Field>
          <Field label="Method"><Select value={ensMethod} onChange={setEnsMethod} options={['random_forest', 'gradient_boosting', 'adaboost']} /></Field>
          <Field label="N Estimators"><Input value={ensEst} onChange={setEnsEst} type="number" /></Field>
          <Field label="Max Depth"><Input value={ensDepth} onChange={setEnsDepth} type="number" /></Field>
          <RunButton loading={ens.loading} onClick={() => ens.run(() => regressionApi.ensemble(JSON.parse(ensX), JSON.parse(ensY), ensMethod, Number(ensEst), Number(ensDepth)))} />
        </Row>
        <ResultPanel data={ens.result} error={ens.error} />
      </EndpointCard>
    </>
  );
}
