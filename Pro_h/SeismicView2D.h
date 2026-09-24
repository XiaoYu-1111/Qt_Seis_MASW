#pragma once

#include <QWidget>
#include <vector>
#include <QString>

// 移除重型 qcustomplot.h，仅前置声明，加速编译
class QCustomPlot;

/**
 * @brief 全功能 2D 地震道集查看器 (支持 Wiggle 波形叠加变密度热力图)
 * @param data    二维道集矩阵 [道数 nx][采样点 nz]
 * @param title   窗口标题
 * @param parent  父组件指针
 * @param dt      时间采样率 (s)，如 0.001s。若为 1.0f 则自动按样点序号显示
 */
QWidget* createSeismicView(
    const std::vector<std::vector<float>>& data,
    const QString& title,
    QWidget* parent = nullptr,
    float dt = 0.001f
);