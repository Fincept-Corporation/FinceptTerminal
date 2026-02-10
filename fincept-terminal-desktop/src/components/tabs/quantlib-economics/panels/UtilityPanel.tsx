import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { utilityApi } from '../quantlibEconomicsApi';

const utilTypes = ['cara', 'crra', 'log', 'quadratic'];

export default function UtilityPanel() {
  // CARA
  const cara = useEndpoint();
  const [caraA, setCaraA] = useState('0.5');
  const [caraW, setCaraW] = useState('100');

  // CRRA
  const crra = useEndpoint();
  const [crraG, setCrraG] = useState('2');
  const [crraW, setCrraW] = useState('100');

  // Log
  const log = useEndpoint();
  const [logW, setLogW] = useState('100');

  // Prospect Theory
  const pt = useEndpoint();
  const [ptAlpha, setPtAlpha] = useState('0.88');
  const [ptBeta, setPtBeta] = useState('0.88');
  const [ptLambda, setPtLambda] = useState('2.25');
  const [ptW, setPtW] = useState('0');
  const [ptO, setPtO] = useState('50');

  // Quadratic
  const quad = useEndpoint();
  const [quadW, setQuadW] = useState('100');
  const [quadB, setQuadB] = useState('0.01');

  // Expected Utility
  const eu = useEndpoint();
  const [euType, setEuType] = useState('crra');
  const [euOut, setEuOut] = useState('50, 100, 150');
  const [euProb, setEuProb] = useState('0.3, 0.4, 0.3');
  const [euParam, setEuParam] = useState('2');

  // Certainty Equivalent
  const ce = useEndpoint();
  const [ceType, setCeType] = useState('crra');
  const [ceOut, setCeOut] = useState('50, 100, 150');
  const [ceProb, setCeProb] = useState('0.3, 0.4, 0.3');
  const [ceParam, setCeParam] = useState('2');

  // CE Approximation
  const ceA = useEndpoint();
  const [ceAMean, setCeAMean] = useState('100');
  const [ceAVar, setCeAVar] = useState('25');
  const [ceARA, setCeARA] = useState('0.5');

  // Risk Premium
  const rp = useEndpoint();
  const [rpType, setRpType] = useState('crra');
  const [rpOut, setRpOut] = useState('50, 100, 150');
  const [rpProb, setRpProb] = useState('0.3, 0.4, 0.3');
  const [rpParam, setRpParam] = useState('2');

  // Stochastic Dominance
  const sd = useEndpoint();
  const [sdA, setSdA] = useState('0.1, 0.3, 0.6, 0.8, 1.0');
  const [sdB, setSdB] = useState('0.2, 0.4, 0.5, 0.9, 1.0');
  const [sdOrd, setSdOrd] = useState('1');

  return (
    <>
      <EndpointCard title="CARA Utility" description="Constant Absolute Risk Aversion utility">
        <Row>
          <Field label="Risk Aversion"><Input value={caraA} onChange={setCaraA} type="number" /></Field>
          <Field label="Wealth"><Input value={caraW} onChange={setCaraW} type="number" /></Field>
          <RunButton loading={cara.loading} onClick={() => cara.run(() => utilityApi.cara(Number(caraA), Number(caraW)))} />
        </Row>
        <ResultPanel data={cara.result} error={cara.error} />
      </EndpointCard>

      <EndpointCard title="CRRA Utility" description="Constant Relative Risk Aversion utility">
        <Row>
          <Field label="Gamma"><Input value={crraG} onChange={setCrraG} type="number" /></Field>
          <Field label="Wealth"><Input value={crraW} onChange={setCrraW} type="number" /></Field>
          <RunButton loading={crra.loading} onClick={() => crra.run(() => utilityApi.crra(Number(crraG), Number(crraW)))} />
        </Row>
        <ResultPanel data={crra.result} error={crra.error} />
      </EndpointCard>

      <EndpointCard title="Log Utility" description="Logarithmic utility function">
        <Row>
          <Field label="Wealth"><Input value={logW} onChange={setLogW} type="number" /></Field>
          <RunButton loading={log.loading} onClick={() => log.run(() => utilityApi.log(Number(logW)))} />
        </Row>
        <ResultPanel data={log.result} error={log.error} />
      </EndpointCard>

      <EndpointCard title="Prospect Theory" description="Kahneman-Tversky prospect theory value">
        <Row>
          <Field label="Alpha"><Input value={ptAlpha} onChange={setPtAlpha} type="number" /></Field>
          <Field label="Beta"><Input value={ptBeta} onChange={setPtBeta} type="number" /></Field>
          <Field label="Lambda"><Input value={ptLambda} onChange={setPtLambda} type="number" /></Field>
          <Field label="Wealth"><Input value={ptW} onChange={setPtW} type="number" /></Field>
          <Field label="Outcome"><Input value={ptO} onChange={setPtO} type="number" /></Field>
          <RunButton loading={pt.loading} onClick={() => pt.run(() => utilityApi.prospectTheory(Number(ptAlpha), Number(ptBeta), Number(ptLambda), Number(ptW), Number(ptO)))} />
        </Row>
        <ResultPanel data={pt.result} error={pt.error} />
      </EndpointCard>

      <EndpointCard title="Quadratic Utility" description="Quadratic utility function">
        <Row>
          <Field label="Wealth"><Input value={quadW} onChange={setQuadW} type="number" /></Field>
          <Field label="b"><Input value={quadB} onChange={setQuadB} type="number" /></Field>
          <RunButton loading={quad.loading} onClick={() => quad.run(() => utilityApi.quadratic(Number(quadW), Number(quadB)))} />
        </Row>
        <ResultPanel data={quad.result} error={quad.error} />
      </EndpointCard>

      <EndpointCard title="Expected Utility" description="Expected utility of a lottery">
        <Row>
          <Field label="Utility Type"><Select value={euType} onChange={setEuType} options={utilTypes} /></Field>
          <Field label="Outcomes" width="200px"><Input value={euOut} onChange={setEuOut} /></Field>
          <Field label="Probabilities" width="200px"><Input value={euProb} onChange={setEuProb} /></Field>
          <Field label="Param"><Input value={euParam} onChange={setEuParam} type="number" /></Field>
          <RunButton loading={eu.loading} onClick={() => eu.run(() => utilityApi.expectedUtility(euType, euOut.split(',').map(Number), euProb.split(',').map(Number), Number(euParam)))} />
        </Row>
        <ResultPanel data={eu.result} error={eu.error} />
      </EndpointCard>

      <EndpointCard title="Certainty Equivalent" description="Certainty equivalent of a lottery">
        <Row>
          <Field label="Utility Type"><Select value={ceType} onChange={setCeType} options={utilTypes} /></Field>
          <Field label="Outcomes" width="200px"><Input value={ceOut} onChange={setCeOut} /></Field>
          <Field label="Probabilities" width="200px"><Input value={ceProb} onChange={setCeProb} /></Field>
          <Field label="Param"><Input value={ceParam} onChange={setCeParam} type="number" /></Field>
          <RunButton loading={ce.loading} onClick={() => ce.run(() => utilityApi.certaintyEquivalent(ceType, ceOut.split(',').map(Number), ceProb.split(',').map(Number), Number(ceParam)))} />
        </Row>
        <ResultPanel data={ce.result} error={ce.error} />
      </EndpointCard>

      <EndpointCard title="CE Approximation" description="Arrow-Pratt certainty equivalent approximation">
        <Row>
          <Field label="Mean"><Input value={ceAMean} onChange={setCeAMean} type="number" /></Field>
          <Field label="Variance"><Input value={ceAVar} onChange={setCeAVar} type="number" /></Field>
          <Field label="Risk Aversion"><Input value={ceARA} onChange={setCeARA} type="number" /></Field>
          <RunButton loading={ceA.loading} onClick={() => ceA.run(() => utilityApi.ceApprox(Number(ceAMean), Number(ceAVar), Number(ceARA)))} />
        </Row>
        <ResultPanel data={ceA.result} error={ceA.error} />
      </EndpointCard>

      <EndpointCard title="Risk Premium" description="Risk premium of a lottery">
        <Row>
          <Field label="Utility Type"><Select value={rpType} onChange={setRpType} options={utilTypes} /></Field>
          <Field label="Outcomes" width="200px"><Input value={rpOut} onChange={setRpOut} /></Field>
          <Field label="Probabilities" width="200px"><Input value={rpProb} onChange={setRpProb} /></Field>
          <Field label="Param"><Input value={rpParam} onChange={setRpParam} type="number" /></Field>
          <RunButton loading={rp.loading} onClick={() => rp.run(() => utilityApi.riskPremium(rpType, rpOut.split(',').map(Number), rpProb.split(',').map(Number), Number(rpParam)))} />
        </Row>
        <ResultPanel data={rp.result} error={rp.error} />
      </EndpointCard>

      <EndpointCard title="Stochastic Dominance" description="Test stochastic dominance between CDFs">
        <Row>
          <Field label="CDF A" width="250px"><Input value={sdA} onChange={setSdA} /></Field>
          <Field label="CDF B" width="250px"><Input value={sdB} onChange={setSdB} /></Field>
          <Field label="Order"><Select value={sdOrd} onChange={setSdOrd} options={['1', '2']} /></Field>
          <RunButton loading={sd.loading} onClick={() => sd.run(() => utilityApi.stochasticDominance(sdA.split(',').map(Number), sdB.split(',').map(Number), Number(sdOrd)))} />
        </Row>
        <ResultPanel data={sd.result} error={sd.error} />
      </EndpointCard>
    </>
  );
}
