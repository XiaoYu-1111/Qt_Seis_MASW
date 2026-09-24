#include "Pro_h/RayleighInversionSolver.h"
#include <iostream>
#include <numeric>
#include <cmath>

LayerModel RayleighInversionSolver::makeLayerModel(const Eigen::VectorXd& m, const InversionParams& params)
{
    LayerModel model;
    model.H = params.layerH;
    int n = m.size();
    model.VS.resize(n);
    model.VP.resize(n);
    model.Rho.resize(n);

    for (int i = 0; i < n; ++i) {
        model.VS[i] = m(i);
        model.VP[i] = m(i) * params.vpVsRatio;
        model.Rho[i] = params.density;
    }
    return model;
}

double RayleighInversionSolver::computeObjective(
    const std::vector<double>& freqs,
    const std::vector<double>& obsVel,
    const Eigen::VectorXd& m,
    const InversionParams& params,
    const Eigen::MatrixXd& L,
    std::vector<double>& outCalcVel)
{
    LayerModel model = makeLayerModel(m, params);
    outCalcVel = RayleighForwardSolver::calcBaseDispersion(freqs, model);

    int M = freqs.size();
    double dataMisfit = 0.0;
    for (int i = 0; i < M; ++i) {
        double diff = outCalcVel[i] - obsVel[i];
        dataMisfit += diff * diff;
    }
    dataMisfit *= 0.5;

    // Tikhonov 一阶粗糙度正则化项: 1/2 * lambda * ||L * m||^2
    Eigen::VectorXd Lm = L * m;
    double roughness = 0.5 * params.lambdaReg * Lm.squaredNorm();

    return dataMisfit + roughness;
}

InversionResult RayleighInversionSolver::runInversion(
    const std::vector<double>& freqs,
    const std::vector<double>& obsVel,
    const InversionParams& params)
{
    InversionResult result;
    int M = freqs.size();
    if (M < 3 || obsVel.size() != freqs.size()) {
        result.success = false;
        return result;
    }

    result.freqs = freqs;
    result.obsVel = obsVel;

    int N = params.layerH.size() + 1; // 6 层

    // 1. 构建一阶平滑算子 L
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N - 1, N);
    for (int i = 0; i < N - 1; ++i) {
        L(i, i) = -1.0;
        L(i, i + 1) = 1.0;
    }
    Eigen::MatrixXd LtL = L.transpose() * L;

    // 2. 初始模型：浅层取观测速度小值，深层取观测速度大值，平滑过渡
    double vMin = *std::min_element(obsVel.begin(), obsVel.end());
    double vMax = *std::max_element(obsVel.begin(), obsVel.end());

    Eigen::VectorXd m(N);
    for (int i = 0; i < N; ++i) {
        double ratio = (N > 1) ? (double)i / (N - 1) : 0.0;
        double initVal = (vMin * 1.0) + ratio * (vMax * 1.15 - vMin * 1.0);
        m(i) = std::clamp(initVal, params.vsMin, params.vsMax);
    }

    std::vector<double> calcVel;
    double phi = computeObjective(freqs, obsVel, m, params, L, calcVel);
    double mu = 1.0; // 初始适度阻尼

    // 3. 迭代循环 (Levenberg-Marquardt)
    int iter = 0;
    for (iter = 0; iter < params.maxIter; ++iter) {
        // A. 差分求雅可比矩阵 J
        Eigen::MatrixXd J(M, N);
        for (int j = 0; j < N; ++j) {
            double delta = std::max(1.0, 0.005 * m(j));
            Eigen::VectorXd m_pert = m;
            m_pert(j) += delta;

            LayerModel pertModel = makeLayerModel(m_pert, params);
            std::vector<double> pertVel = RayleighForwardSolver::calcBaseDispersion(freqs, pertModel);

            for (int i = 0; i < M; ++i) {
                J(i, j) = (pertVel[i] - calcVel[i]) / delta;
            }
        }

        // B. 【核心修复】：动态平衡数据项与平滑项量级
        Eigen::MatrixXd JtJ = J.transpose() * J;
        double scaleFactor = JtJ.trace() / std::max(1.0, LtL.trace());
        double lambdaEff = params.lambdaReg * scaleFactor; // 保证平滑项起效

        Eigen::MatrixXd H = JtJ + lambdaEff * LtL + mu * Eigen::MatrixXd::Identity(N, N);

        Eigen::VectorXd r(M);
        for (int i = 0; i < M; ++i) r(i) = obsVel[i] - calcVel[i];

        Eigen::VectorXd g = J.transpose() * r - lambdaEff * LtL * m;

        // C. 解步长
        Eigen::VectorXd delta_m = H.ldlt().solve(g);

        // D. 【核心保护】：限制单步最大变化量不超过 15%，防止冲出轨道
        for (int i = 0; i < N; ++i) {
            double maxStep = m(i) * 0.15;
            delta_m(i) = std::clamp(delta_m(i), -maxStep, maxStep);
        }

        Eigen::VectorXd m_trial = m + delta_m;
        for (int i = 0; i < N; ++i) {
            m_trial(i) = std::clamp(m_trial(i), params.vsMin, params.vsMax);
        }

        std::vector<double> trialCalcVel;
        double phi_trial = computeObjective(freqs, obsVel, m_trial, params, L, trialCalcVel);

        if (phi_trial < phi) {
            m = m_trial;
            phi = phi_trial;
            calcVel = trialCalcVel;
            mu = std::max(1e-4, mu * 0.5);

            if (delta_m.norm() < 0.2) break;
        }
        else {
            mu = std::min(1e4, mu * 4.0);
        }
    }

    // 4. 收尾
    result.iterations = iter + 1;
    result.vs.resize(N);
    for (int i = 0; i < N; ++i) result.vs[i] = m(i);
    result.calcVel = calcVel;

    double sumSq = 0.0;
    for (int i = 0; i < M; ++i) {
        double d = calcVel[i] - obsVel[i];
        sumSq += d * d;
    }
    result.rmse = std::sqrt(sumSq / M);
    result.success = true;

    return result;
}