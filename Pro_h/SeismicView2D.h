#pragma once
#include <QWidget>
#include <vector>
#include "qcustomplot.h"

QWidget* createSeismicView(
    const std::vector<std::vector<float>>& data,
    const QString& title,
    QWidget* parent = nullptr, float dt = 0.001f
);
