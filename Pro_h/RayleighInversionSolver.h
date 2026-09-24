#pragma once

#include <vector>
#include <Eigen/Dense>
#include "Pro_h/RayleighForwardSolver.h"

// 反演配置参数
struct InversionParams {
    std::vector<double> layerH = { 2.0, 3.0, 5.0, 8.0, 12.0 }; // 前 N-1 层厚度 (米)
    double vsMin = 100.0;       // 横波速度下界 (m/s)
    double vsMax = 1500.0;      // 横波速度上界 (m/s)
    double lambdaReg = 0.02;    // Tikhonov 一阶平滑正则化阻尼因子
    int maxIter = 25;           // 最大迭代次数
    double vpVsRatio = 2.0;     // Vp/Vs 泊松介质假定比值
    double density = 2000.0;    // 地层密度 (kg/m^3)
};

// 反演输出成果
struct InversionResult {
    std::vector<double> vs;        // 反演出的各层横波速度 (m/s)
    std::vector<double> calcVel;   // 最优模型计算出的理论频散相速度 (m/s)
    std::vector<double> freqs;     // 对应的频点 (Hz)
    std::vector<double> obsVel;    // 观测/拾取的实测相速度 (m/s)
    double rmse = 0.0;             // 均方根拟合残差 (m/s)
    int iterations = 0;            // 实际收敛迭代次数
    bool success = false;          // 是否成功收敛
};

class RayleighInversionSolver {
public:
    /**
     * @brief 纯 C++ 原生 1D 瑞雷面波相速度反演求解器 (带 Tikhonov 平滑约束的 Levenberg-Marquardt 算法)
     * @param freqs      观测频点序列 (Hz)
     * @param obsVel     观测相速度序列 (m/s)
     * @param params     反演地层厚度与约束参数
     * @return InversionResult 反演最优地层模型与拟合指标
     */
    static InversionResult runInversion(
        const std::vector<double>& freqs,
        const std::vector<double>& obsVel,
        const InversionParams& params = InversionParams());

private:
    // 计算目标泛函 Phi(m) = 1/2 * ||d_obs - d_calc||^2 + 1/2 * lambda * ||L*m||^2
    static double computeObjective(
        const std::vector<double>& freqs,
        const std::vector<double>& obsVel,
        const Eigen::VectorXd& m,
        const InversionParams& params,
        const Eigen::MatrixXd& L,
        std::vector<double>& outCalcVel);

    // 将 Vs 向量打包成 LayerModel 供正演调用
    static LayerModel makeLayerModel(const Eigen::VectorXd& m, const InversionParams& params);
};