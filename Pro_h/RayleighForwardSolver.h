#pragma once
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <functional>
#include <unsupported/Eigen/FFT>

// 地层模型结构体
struct LayerModel {
    std::vector<double> H;   // 各层厚度 (m)，最后一层(半空间)厚度无需提供，长度为 n-1
    std::vector<double> VS;  // 各层横波速度 (m/s)，长度为 n
    std::vector<double> VP;  // 各层纵波速度 (m/s)，长度为 n
    std::vector<double> Rho; // 各层密度 (kg/m^3)，长度为 n
};

class RayleighForwardSolver {
public:
    /**
     * @brief 求解层状介质基阶瑞雷面波理论频散曲线 (对应 MATLAB 的 calcbase)
     * @param freqs  分析频率向量 (Hz)，建议由低到高排序
     * @param model  地层模型参数
     * @return 对应频率下的相速度向量 (m/s)
     */
    static std::vector<double> calcBaseDispersion(const std::vector<double>& freqs, const LayerModel& model);

    /**
     * @brief 利用理论频散曲线合成多道地震面波炮集 (对应 MATLAB 的频域相位调制+IFFT法)
     * @param freqs       频率序列
     * @param phaseVel    对应的理论相速度 (m/s)
     * @param dt          时间采样率 (s)
     * @param nt          采样点数
     * @param offsets     各道物理偏移距 (m)
     * @param f_dominant  震源子波主频 (Hz)
     * @return 二维合成地震数据 [道数 nx][采样点 nt]
     */
    static std::vector<std::vector<float>> synthesizeSurfaceWaveGather(
        const std::vector<double>& freqs,
        const std::vector<double>& phaseVel,
        double dt, int nt,
        const std::vector<double>& offsets,
        double f_dominant = 15.0);

private:
    // 特征超越方程判别式 (对应 MATLAB 的 fastcalc)
    static double fastCalc(double c, double f, const LayerModel& model);

    // C++ 实现 MATLAB 的 fzero (Brent-Dekker 求根算法)
    static double brentRoot(const std::function<double(double)>& f, double x_guess, double x_min, double x_max);
};