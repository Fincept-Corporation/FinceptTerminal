import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { timeseriesApi } from '../quantlibStatisticsApi';

export default function TimeSeriesPanel() {
  const sampleData = '100, 102, 101, 103, 105, 104, 106, 108, 107, 110, 112, 111, 114, 113, 116';
  const sampleReturns = '0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015, 0.02, -0.01, 0.005, -0.02, 0.03';

  // AR Fit
  const arF = useEndpoint();
  const [arData, setArData] = useState(sampleData);
  const [arOrder, setArOrder] = useState('1');

  // AR Forecast
  const arFc = useEndpoint();
  const [arfData, setArfData] = useState(sampleData);
  const [arfOrder, setArfOrder] = useState('1');
  const [arfSteps, setArfSteps] = useState('5');

  // MA Fit
  const maF = useEndpoint();
  const [maData, setMaData] = useState(sampleData);
  const [maOrder, setMaOrder] = useState('1');

  // ARIMA Fit
  const arimaF = useEndpoint();
  const [arimaData, setArimaData] = useState(sampleData);
  const [arimaP, setArimaP] = useState('1');
  const [arimaD, setArimaD] = useState('0');
  const [arimaQ, setArimaQ] = useState('0');

  // ARIMA Forecast
  const arimaFc = useEndpoint();
  const [arimafData, setArimafData] = useState(sampleData);
  const [arimafP, setArimafP] = useState('1');
  const [arimafD, setArimafD] = useState('0');
  const [arimafQ, setArimafQ] = useState('0');
  const [arimafSteps, setArimafSteps] = useState('5');

  // GARCH Fit
  const garchF = useEndpoint();
  const [garchRet, setGarchRet] = useState(sampleReturns);
  const [garchP, setGarchP] = useState('1');
  const [garchQ, setGarchQ] = useState('1');

  // GARCH Forecast
  const garchFc = useEndpoint();
  const [garchfRet, setGarchfRet] = useState(sampleReturns);
  const [garchfP, setGarchfP] = useState('1');
  const [garchfQ, setGarchfQ] = useState('1');
  const [garchfSteps, setGarchfSteps] = useState('5');

  // EGARCH Fit
  const egarchF = useEndpoint();
  const [egarchRet, setEgarchRet] = useState(sampleReturns);
  const [egarchP, setEgarchP] = useState('1');
  const [egarchQ, setEgarchQ] = useState('1');

  // GJR-GARCH Fit
  const gjrF = useEndpoint();
  const [gjrRet, setGjrRet] = useState(sampleReturns);
  const [gjrP, setGjrP] = useState('1');
  const [gjrQ, setGjrQ] = useState('1');

  return (
    <>
      <EndpointCard title="AR Fit" description="Autoregressive model fitting">
        <Row>
          <Field label="Data (comma-sep)" width="400px"><Input value={arData} onChange={setArData} /></Field>
          <Field label="Order"><Input value={arOrder} onChange={setArOrder} type="number" /></Field>
          <RunButton loading={arF.loading} onClick={() => arF.run(() => timeseriesApi.arFit(arData.split(',').map(Number), Number(arOrder)))} />
        </Row>
        <ResultPanel data={arF.result} error={arF.error} />
      </EndpointCard>

      <EndpointCard title="AR Forecast" description="Autoregressive out-of-sample forecast">
        <Row>
          <Field label="Data (comma-sep)" width="400px"><Input value={arfData} onChange={setArfData} /></Field>
          <Field label="Order"><Input value={arfOrder} onChange={setArfOrder} type="number" /></Field>
          <Field label="Steps"><Input value={arfSteps} onChange={setArfSteps} type="number" /></Field>
          <RunButton loading={arFc.loading} onClick={() => arFc.run(() => timeseriesApi.arForecast(arfData.split(',').map(Number), Number(arfOrder), Number(arfSteps)))} />
        </Row>
        <ResultPanel data={arFc.result} error={arFc.error} />
      </EndpointCard>

      <EndpointCard title="MA Fit" description="Moving Average model fitting">
        <Row>
          <Field label="Data (comma-sep)" width="400px"><Input value={maData} onChange={setMaData} /></Field>
          <Field label="Order"><Input value={maOrder} onChange={setMaOrder} type="number" /></Field>
          <RunButton loading={maF.loading} onClick={() => maF.run(() => timeseriesApi.maFit(maData.split(',').map(Number), Number(maOrder)))} />
        </Row>
        <ResultPanel data={maF.result} error={maF.error} />
      </EndpointCard>

      <EndpointCard title="ARIMA Fit" description="ARIMA(p, d, q) model fitting">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={arimaData} onChange={setArimaData} /></Field>
          <Field label="p"><Input value={arimaP} onChange={setArimaP} type="number" /></Field>
          <Field label="d"><Input value={arimaD} onChange={setArimaD} type="number" /></Field>
          <Field label="q"><Input value={arimaQ} onChange={setArimaQ} type="number" /></Field>
          <RunButton loading={arimaF.loading} onClick={() => arimaF.run(() => timeseriesApi.arimaFit(arimaData.split(',').map(Number), Number(arimaP), Number(arimaD), Number(arimaQ)))} />
        </Row>
        <ResultPanel data={arimaF.result} error={arimaF.error} />
      </EndpointCard>

      <EndpointCard title="ARIMA Forecast" description="ARIMA out-of-sample forecast">
        <Row>
          <Field label="Data (comma-sep)" width="300px"><Input value={arimafData} onChange={setArimafData} /></Field>
          <Field label="p"><Input value={arimafP} onChange={setArimafP} type="number" /></Field>
          <Field label="d"><Input value={arimafD} onChange={setArimafD} type="number" /></Field>
          <Field label="q"><Input value={arimafQ} onChange={setArimafQ} type="number" /></Field>
          <Field label="Steps"><Input value={arimafSteps} onChange={setArimafSteps} type="number" /></Field>
          <RunButton loading={arimaFc.loading} onClick={() => arimaFc.run(() => timeseriesApi.arimaForecast(arimafData.split(',').map(Number), Number(arimafP), Number(arimafD), Number(arimafQ), Number(arimafSteps)))} />
        </Row>
        <ResultPanel data={arimaFc.result} error={arimaFc.error} />
      </EndpointCard>

      <EndpointCard title="GARCH Fit" description="GARCH(p, q) volatility model fitting">
        <Row>
          <Field label="Returns (comma-sep)" width="400px"><Input value={garchRet} onChange={setGarchRet} /></Field>
          <Field label="p"><Input value={garchP} onChange={setGarchP} type="number" /></Field>
          <Field label="q"><Input value={garchQ} onChange={setGarchQ} type="number" /></Field>
          <RunButton loading={garchF.loading} onClick={() => garchF.run(() => timeseriesApi.garchFit(garchRet.split(',').map(Number), Number(garchP), Number(garchQ)))} />
        </Row>
        <ResultPanel data={garchF.result} error={garchF.error} />
      </EndpointCard>

      <EndpointCard title="GARCH Forecast" description="GARCH volatility out-of-sample forecast">
        <Row>
          <Field label="Returns (comma-sep)" width="350px"><Input value={garchfRet} onChange={setGarchfRet} /></Field>
          <Field label="p"><Input value={garchfP} onChange={setGarchfP} type="number" /></Field>
          <Field label="q"><Input value={garchfQ} onChange={setGarchfQ} type="number" /></Field>
          <Field label="Steps"><Input value={garchfSteps} onChange={setGarchfSteps} type="number" /></Field>
          <RunButton loading={garchFc.loading} onClick={() => garchFc.run(() => timeseriesApi.garchForecast(garchfRet.split(',').map(Number), Number(garchfP), Number(garchfQ), Number(garchfSteps)))} />
        </Row>
        <ResultPanel data={garchFc.result} error={garchFc.error} />
      </EndpointCard>

      <EndpointCard title="EGARCH Fit" description="Exponential GARCH asymmetric volatility model">
        <Row>
          <Field label="Returns (comma-sep)" width="400px"><Input value={egarchRet} onChange={setEgarchRet} /></Field>
          <Field label="p"><Input value={egarchP} onChange={setEgarchP} type="number" /></Field>
          <Field label="q"><Input value={egarchQ} onChange={setEgarchQ} type="number" /></Field>
          <RunButton loading={egarchF.loading} onClick={() => egarchF.run(() => timeseriesApi.egarchFit(egarchRet.split(',').map(Number), Number(egarchP), Number(egarchQ)))} />
        </Row>
        <ResultPanel data={egarchF.result} error={egarchF.error} />
      </EndpointCard>

      <EndpointCard title="GJR-GARCH Fit" description="GJR-GARCH asymmetric leverage model">
        <Row>
          <Field label="Returns (comma-sep)" width="400px"><Input value={gjrRet} onChange={setGjrRet} /></Field>
          <Field label="p"><Input value={gjrP} onChange={setGjrP} type="number" /></Field>
          <Field label="q"><Input value={gjrQ} onChange={setGjrQ} type="number" /></Field>
          <RunButton loading={gjrF.loading} onClick={() => gjrF.run(() => timeseriesApi.gjrGarchFit(gjrRet.split(',').map(Number), Number(gjrP), Number(gjrQ)))} />
        </Row>
        <ResultPanel data={gjrF.result} error={gjrF.error} />
      </EndpointCard>
    </>
  );
}
