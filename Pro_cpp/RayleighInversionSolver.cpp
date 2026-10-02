#include "Pro_h/RayleighInversionSolver.h"
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>

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
    if (M < 3 || obsVel.size() != freqs.size() || params.layerH.empty() ||
        !std::isfinite(params.vsMin) || !std::isfinite(params.vsMax) ||
        params.vsMin <= 0.0 || params.vsMax < params.vsMin ||
        !std::isfinite(params.lambdaReg) || params.lambdaReg < 0.0 || params.maxIter <= 0) {
        result.success = false;
        return result;
    }

    for (int i = 0; i < M; ++i) {
        if (!std::isfinite(freqs[i]) || freqs[i] <= 0.0 ||
            !std::isfinite(obsVel[i]) || obsVel[i] <= 0.0) {
            result.success = false;
            return result;
        }
    }
    for (double thickness : params.layerH) {
        if (!std::isfinite(thickness) || thickness <= 0.0) {
            result.success = false;
            return result;
        }
    }

    result.freqs = freqs;
    result.obsVel = obsVel;

    int N = params.layerH.size() + 1;

    // 1. 一阶平滑算子
    Eigen::MatrixXd L = Eigen::MatrixXd::Zero(N - 1, N);
    for (int i = 0; i < N - 1; ++i) {
        L(i, i) = -1.0;
        L(i, i + 1) = 1.0;
    }
    Eigen::MatrixXd LtL = L.transpose() * L;

    // 2. 观测速度极值
    double vMin = *std::min_element(obsVel.begin(), obsVel.end());
    double vMax = *std::max_element(obsVel.begin(), obsVel.end());

    // =========================================================
    // 【核心物理锁 1】：基底横波速度必须能支撑最大观测相速度！
    // 瑞雷波速 ≈ 0.92 Vs，因此基底 Vs 绝对不能低于 vMax / 0.92
    // =========================================================
    double basementMinVs = std::max(params.vsMin, (vMax / 0.90));
    if (basementMinVs > params.vsMax) {
        result.success = false;
        return result;
    }

    Eigen::VectorXd m(N);
    for (int i = 0; i < N; ++i) {
        double ratio = (N > 1) ? (double)i / (N - 1) : 0.0;
        double initVal = (vMin * 1.05) + ratio * (basementMinVs * 1.05 - vMin * 1.05);
        double lower = (i == N - 1) ? basementMinVs : params.vsMin;
        m(i) = std::clamp(initVal, lower, params.vsMax);
    }

    std::vector<double> calcVel;
    double phi = computeObjective(freqs, obsVel, m, params, L, calcVel);
    double mu = 2.0; // 增强初始阻尼，防止第一步冲出轨道

    // 3. 阻尼非线性反演循环
    int iter = 0;
    for (iter = 0; iter < params.maxIter; ++iter) {
        // A. 数值差分计算雅可比敏感度矩阵 J
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

        // B. 自适应平滑项匹配
        Eigen::MatrixXd JtJ = J.transpose() * J;
        double lambdaEff = params.lambdaReg; // Match the regularization term used in computeObjective.

        Eigen::MatrixXd H = JtJ + lambdaEff * LtL + mu * Eigen::MatrixXd::Identity(N, N);

        Eigen::VectorXd r(M);
        for (int i = 0; i < M; ++i) r(i) = obsVel[i] - calcVel[i];

        Eigen::VectorXd g = J.transpose() * r - lambdaEff * LtL * m;

        Eigen::VectorXd delta_m = H.ldlt().solve(g);

        // C. 限制单步最大变化量不超过 12%
        for (int i = 0; i < N; ++i) {
            double maxStep = m(i) * 0.12;
            delta_m(i) = std::clamp(delta_m(i), -maxStep, maxStep);
        }

        Eigen::VectorXd m_trial = m + delta_m;

        // D. 施加物理有界保护
        for (int i = 0; i < N; ++i) {
            double lower = (i == N - 1) ? basementMinVs : params.vsMin;
            if (i > 0) {
                lower = std::max(lower, m_trial(i - 1) * 0.90);
            }
            m_trial(i) = std::clamp(m_trial(i), lower, params.vsMax);
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
            mu = std::min(1e5, mu * 4.0);
        }
    }

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