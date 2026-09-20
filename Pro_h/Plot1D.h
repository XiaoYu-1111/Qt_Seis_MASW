#pragma once
#include "qcustomplot.h"
#include <QtWidgets/QMainWindow>
#include <QTimer>
#include <QFileDialog> // 别忘了头文件
#include <QInputDialog>
#include <QScrollArea>

#include <complex>
#include <omp.h>           // 并行计算
#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>



namespace Data_show
{
    class Plot1D : public QCustomPlot
    {
        Q_OBJECT

    public:
        explicit Plot1D(QWidget* parent = nullptr);

        void setData(const std::vector<float>& vec, const QString& title);

        // [新增]：支持传入 X(频率) 和 Y(幅度)
        void setData(const std::vector<double>& x, const std::vector<double>& y, const QString& title);

        // Plot1D.h
    public:
        // 专门用于实时更新，不改样式，不改标题，只改数据
        void updateDataOnly(const std::vector<double>& x, const std::vector<double>& y);
        void m_setName(const QString& name);

        void setSampleRate(double fs); // 设置采样频率
        double sampleRate() const;

    protected:
        // 事件系统
        void mousePressEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void contextMenuEvent(QContextMenuEvent* event) override;

    private:
        QCPGraph* m_mainGraph;
        QCPTextElement* m_titleElement = nullptr; // 用于存储标题对象
        double m_sampleRate = 1000.0; // 默认 1000 Hz
    private:
        void applyScientificStyle(); // 应用期刊风格样式
    };
}
