import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { clusteringApi } from '../quantlibMlApi';

export default function ClusteringPanel() {
  // K-Means
  const km = useEndpoint();
  const [kmX, setKmX] = useState('[[1,2],[1.5,1.8],[5,8],[8,8],[1,0.6],[9,11]]');
  const [kmK, setKmK] = useState('2');
  const [kmIter, setKmIter] = useState('300');

  // PCA
  const pca = useEndpoint();
  const [pcaX, setPcaX] = useState('[[1,2,3],[4,5,6],[7,8,9],[10,11,12]]');
  const [pcaComp, setPcaComp] = useState('2');

  // DBSCAN
  const dbs = useEndpoint();
  const [dbsX, setDbsX] = useState('[[1,2],[1.5,1.8],[5,8],[8,8],[1,0.6],[9,11]]');
  const [dbsEps, setDbsEps] = useState('3');
  const [dbsMin, setDbsMin] = useState('2');

  // Hierarchical
  const hier = useEndpoint();
  const [hierX, setHierX] = useState('[[1,2],[1.5,1.8],[5,8],[8,8],[1,0.6],[9,11]]');
  const [hierK, setHierK] = useState('2');
  const [hierLink, setHierLink] = useState('ward');

  // Isolation Forest
  const iso = useEndpoint();
  const [isoX, setIsoX] = useState('[[1,2],[1.5,1.8],[5,8],[8,8],[1,0.6],[100,200]]');
  const [isoTrees, setIsoTrees] = useState('100');
  const [isoCont, setIsoCont] = useState('0.1');

  return (
    <>
      <EndpointCard title="K-Means Clustering" description="K-Means partitioning">
        <Row>
          <Field label="X (JSON 2D)" width="350px"><Input value={kmX} onChange={setKmX} /></Field>
          <Field label="K Clusters"><Input value={kmK} onChange={setKmK} type="number" /></Field>
          <Field label="Max Iter"><Input value={kmIter} onChange={setKmIter} type="number" /></Field>
          <RunButton loading={km.loading} onClick={() => km.run(() => clusteringApi.kmeans(JSON.parse(kmX), Number(kmK), Number(kmIter)))} />
        </Row>
        <ResultPanel data={km.result} error={km.error} />
      </EndpointCard>

      <EndpointCard title="PCA" description="Principal Component Analysis">
        <Row>
          <Field label="X (JSON 2D)" width="350px"><Input value={pcaX} onChange={setPcaX} /></Field>
          <Field label="N Components"><Input value={pcaComp} onChange={setPcaComp} type="number" /></Field>
          <RunButton loading={pca.loading} onClick={() => pca.run(() => clusteringApi.pca(JSON.parse(pcaX), Number(pcaComp)))} />
        </Row>
        <ResultPanel data={pca.result} error={pca.error} />
      </EndpointCard>

      <EndpointCard title="DBSCAN" description="Density-Based Spatial Clustering">
        <Row>
          <Field label="X (JSON 2D)" width="350px"><Input value={dbsX} onChange={setDbsX} /></Field>
          <Field label="Eps"><Input value={dbsEps} onChange={setDbsEps} type="number" /></Field>
          <Field label="Min Samples"><Input value={dbsMin} onChange={setDbsMin} type="number" /></Field>
          <RunButton loading={dbs.loading} onClick={() => dbs.run(() => clusteringApi.dbscan(JSON.parse(dbsX), Number(dbsEps), Number(dbsMin)))} />
        </Row>
        <ResultPanel data={dbs.result} error={dbs.error} />
      </EndpointCard>

      <EndpointCard title="Hierarchical Clustering" description="Agglomerative hierarchical clustering">
        <Row>
          <Field label="X (JSON 2D)" width="350px"><Input value={hierX} onChange={setHierX} /></Field>
          <Field label="K Clusters"><Input value={hierK} onChange={setHierK} type="number" /></Field>
          <Field label="Linkage"><Select value={hierLink} onChange={setHierLink} options={['ward', 'complete', 'average', 'single']} /></Field>
          <RunButton loading={hier.loading} onClick={() => hier.run(() => clusteringApi.hierarchical(JSON.parse(hierX), Number(hierK), hierLink))} />
        </Row>
        <ResultPanel data={hier.result} error={hier.error} />
      </EndpointCard>

      <EndpointCard title="Isolation Forest" description="Anomaly detection via Isolation Forest">
        <Row>
          <Field label="X (JSON 2D)" width="350px"><Input value={isoX} onChange={setIsoX} /></Field>
          <Field label="N Trees"><Input value={isoTrees} onChange={setIsoTrees} type="number" /></Field>
          <Field label="Contamination"><Input value={isoCont} onChange={setIsoCont} type="number" /></Field>
          <RunButton loading={iso.loading} onClick={() => iso.run(() => clusteringApi.isolationForest(JSON.parse(isoX), Number(isoTrees), Number(isoCont)))} />
        </Row>
        <ResultPanel data={iso.result} error={iso.error} />
      </EndpointCard>
    </>
  );
}
