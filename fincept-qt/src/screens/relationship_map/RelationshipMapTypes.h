// src/screens/relationship_map/RelationshipMapTypes.h
// Relationship Map — umbrella header kept for its includers (datahub/DataHubMetaTypes.h
// registers RelationshipData through it). The data itself lives in RelationshipMapData.h
// (pure QtCore); the graph model in RelationshipGraphModel.h.
//
// ui/theme/Theme.h and the Qt headers below stay included: files that include
// DataHubMetaTypes.h have always received them through this header.
#pragma once

#include "screens/relationship_map/RelationshipMapData.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QCoreApplication>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
