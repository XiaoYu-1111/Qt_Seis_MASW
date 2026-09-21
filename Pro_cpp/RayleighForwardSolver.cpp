#include "Pro_h/RayleighForwardSolver.h"
#include <iostream>

const double PI = 3.14159265358979323846;

// =========================================================
// 1. fastcalc 的 C++ 严格等价实现
// =========================================================
double RayleighForwardSolver::fastCalc(double x, double f, const LayerModel& model)
{
    int n = model.VS.size();
    if (n < 2) return 0.0;

    double k = 2.0 * PI * f / x;
    double xs = x * x;

    // --- 第 n 层 (半空间，底层) ---
    int last = n - 1;
    double btv = model.VS[last] * model.VS[last];
    double bt = model.Rho[last] * btv;
    double af = model.VP[last] * model.VP[last];

    double g = xs / (2.0 * btv);
    double t = 1.0 - g;
    double r = xs / af - 1.0;
    double s = 2.0 * g - 1.0;

    std::complex<double> r1 = std::sqrt(std::complex<double>(r, 0.0));
    std::complex<double> s1 = std::sqrt(std::complex<double>(s, 0.0));
    std::complex<double> ps = r1 * s1;

    double bt1 = bt;
    std::complex<double> x1 = 1.0 + ps;
    std::complex<double> x2 = t + ps;
    std::complex<double> x3 = -t * t - ps;
    std::complex<double> x4 = std::complex<double>(0.0, 1.0) * s1 * g;
    std::complex<double> x5 = -std::complex<double>(0.0, 1.0) * r1 * g;

    // --- 从第 n-1 层循环推导到第 1 层 (从下至上) ---
    for (int ii = n - 2; ii >= 0; --ii) {
        btv = model.VS[ii] * model.VS[ii];
        bt = model.Rho[ii] * btv;
        af = model.VP[ii] * model.VP[ii];
        g = xs / (2.0 * btv);
        t = 1.0 - g;
        r = xs / af - 1.0;
        s = 2.0 * g - 1.0;

        r1 = std::sqrt(std::complex<double>(r, 0.0));
        s1 = std::sqrt(std::complex<double>(s, 0.0));

        double l = bt1 / bt;
        bt1 = bt;
        x1 /= l;
        x3 *= l;

        double k1 = k * model.H[ii];
        std::complex<double> p = k1 * r1;
        std::complex<double> q = k1 * s1;

        std::complex<double> tx1 = t * x1;
        std::complex<double> ttx1 = t * tx1;
        std::complex<double> tx2 = t * x2;
        std::complex<double> p1 = x1 - 2.0 * x2 - x3;
        std::complex<double> p2 = -ttx1 + 2.0 * tx2 + x3;
        std::complex<double> p3 = g * x4;
        std::complex<double> p4 = g * x5;
        std::complex<double> p5 = -tx1 + 2.0 * x2 + x3;

        std::complex<double> q1, q2, q3, q4;

        if (x >= model.VP[ii]) {
            std::complex<double> c = (std::abs(r1) == 0.0) ? std::complex<double>(k1, 0.0) : (std::sin(p) / r1);
            std::complex<double> d = std::sin(q) / s1;
            std::complex<double> a = std::cos(p);
            std::complex<double> b = std::cos(q);

            std::complex<double> ab = a * b;
            std::complex<double> ad = a * d;
            std::complex<double> cd = c * d;
            std::complex<double> bc = b * c;
            std::complex<double> ads = ad * s;
            std::complex<double> bcr = bc * r;
            std::complex<double> cds = cd * s;
            std::complex<double> cdr = cd * r;
            std::complex<double> cdrs = cdr * s;

            q1 = ab * p1 + cd * p2 - ad * p3 + bc * p4;
            q2 = cdrs * p1 + ab * p2 + bcr * p3 - ads * p4;
            q3 = ads * p1 - bc * p2 + ab * p3 + cds * p4;
            q4 = -bcr * p1 + ad * p2 + cdr * p3 + ab * p4;
        }
        else if (x < model.VP[ii] && x >= model.VS[ii]) {
            std::complex<double> d = (std::abs(s1) == 0.0) ? std::complex<double>(k1, 0.0) : (std::sin(q) / s1);
            double ar1 = std::abs(r1);
            double ark1 = ar1 * k1;
            double ee = std::exp(-2.0 * ark1);
            std::complex<double> b = std::cos(q);
            std::complex<double> c((1.0 - ee) / (1.0 + ee) / ar1, 0.0);

            std::complex<double> ds = d * s;
            std::complex<double> br = b * r;
            std::complex<double> dr = d * r;
            std::complex<double> drs = dr * s;

            q1 = b * p1 + c * d * p2 - d * p3 + b * c * p4;
            q2 = c * drs * p1 + b * p2 + c * br * p3 - ds * p4;
            q3 = ds * p1 - b * c * p2 + b * p3 + c * ds * p4;
            q4 = -c * br * p1 + d * p2 + c * dr * p3 + b * p4;

            if (ark1 < 20.0) p5 /= std::cos(p);
            else p5 = 0.0;
        }
        else { // x < model.VS[ii] (最核心分支)
            double ar1 = std::abs(r1);
            double ark1 = ar1 * k1;
            double ee = std::exp(-2.0 * ark1);

            double as1 = std::abs(s1);
            double ask1 = as1 * k1;
            double ees = std::exp(-2.0 * ask1);

            std::complex<double> c((1.0 - ee) / (1.0 + ee) / ar1, 0.0);
            std::complex<double> d((1.0 - ees) / (1.0 + ees) / as1, 0.0);

            std::complex<double> ds = d * s;
            std::complex<double> cr = c * r;
            std::complex<double> cd = c * d;
            std::complex<double> cds = c * ds;
            std::complex<double> cdr = cr * d;
            std::complex<double> cdrs = ds * cr;

            q1 = p1 + cd * p2 - d * p3 + c * p4;
            q2 = cdrs * p1 + p2 + cr * p3 - ds * p4;
            q3 = ds * p1 - c * p2 + p3 + cds * p4;
            q4 = -cr * p1 + d * p2 + cdr * p3 + p4;

            if ((ask1 + ark1) < 20.0) p5 = p5 / std::cos(p) / std::cos(q);
            else p5 = 0.0;
        }

        std::complex<double> tq1 = t * q1;
        std::complex<double> ttq1 = t * tq1;
        std::complex<double> tp5 = t * p5;

        x1 = q1 - q2 + 2.0 * p5;
        x2 = tq1 - q2 + p5 + tp5;
        x3 = -ttq1 + q2 - 2.0 * tp5;
        x4 = g * q3;
        x5 = g * q4;
    }

    return x3.real();
}

// =========================================================
// 2. 经典 Brent-Dekker 求根算法 (C++ 等价替代 MATLAB fzero)
// =========================================================
double RayleighForwardSolver::brentRoot(const std::function<double(double)>& func, double x_guess, double x_min, double x_max)
{
    double step = 2.0;
    double a = std::max(x_min, x_guess - step);
    double b = std::min(x_max, x_guess + step);
    double fa = func(a);
    double fb = func(b);

    // 自动寻找变号区间 [a, b]
    int search_iter = 0;
    while (fa * fb > 0.0 && search_iter < 80) {
        step *= 1.4;
        a = std::max(x_min, x_guess - step);
        b = std::min(x_max, x_guess + step);
        fa = func(a);
        fb = func(b);
        search_iter++;
    }

    if (fa * fb > 0.0) return x_guess; // 未找到变号根，回退初始猜测

    // Brent 核心迭代
    double c = a, fc = fa;
    double d = b - a, e = d;
    double tol = 1e-5;

    for (int iter = 0; iter < 100; ++iter) {
        if (fb * fc > 0.0) { c = a; fc = fa; d = b - a; e = d; }
        if (std::abs(fc) < std::abs(fb)) {
            a = b; b = c; c = a;
            fa = fb; fb = fc; fc = fa;
        }
        double m = 0.5 * (c - b);
        if (std::abs(m) <= tol || fb == 0.0) return b;

        if (std::abs(e) >= tol && std::abs(fa) > std::abs(fb)) {
            double s_val = fb / fa;
            double p, q;
            if (a == c) {
                p = 2.0 * m * s_val;
                q = 1.0 - s_val;
            }
            else {
                q = fa / fc;
                double r = fb / fc;
                p = s_val * (2.0 * m * q * (q - r) - (b - a) * (r - 1.0));
                q = (q - 1.0) * (r - 1.0) * (s_val - 1.0);
            }
            if (p > 0.0) q = -q;
            p = std::abs(p);
            if (2.0 * p < std::min(3.0 * m * q - std::abs(tol * q), std::abs(e * q))) {
                e = d; d = p / q;
            }
            else {
                d = m; e = m;
            }
        }
        else {
            d = m; e = m;
        }
        a = b; fa = fb;
        b += (std::abs(d) > tol) ? d : ((m > 0) ? tol : -tol);
        fb = func(b);
    }
    return b;
}

// =========================================================
// 3. 理论频散曲线计算 (高频逆向追踪)
// =========================================================
std::vector<double> RayleighForwardSolver::calcBaseDispersion(const std::vector<double>& freqs, const LayerModel& model)
{
    int N = freqs.size();
    if (N == 0) return {};

    std::vector<double> pv(N, 0.0);
    double min_vs = *std::min_element(model.VS.begin(), model.VS.end());
    double max_vs = *std::max_element(model.VS.begin(), model.VS.end());

    // 从最高频逆向追踪到最低频
    double current_guess = 0.91 * min_vs;

    for (int i = N - 1; i >= 0; --i) {
        double f = freqs[i];
        auto secularFunc = [&](double c) {
            return fastCalc(c, f, model);
            };
        // 限制在合理的相速度区间寻找
        current_guess = brentRoot(secularFunc, current_guess, 0.5 * min_vs, 1.2 * max_vs);
        pv[i] = current_guess;
    }

    return pv;
}

// =========================================================
// 4. 频散地震记录时域合成 (IFFT 相位调制)
// =========================================================
std::vector<std::vector<float>> RayleighForwardSolver::synthesizeSurfaceWaveGather(
    const std::vector<double>& freqs,
    const std::vector<double>& phaseVel,
    double dt, int nt,
    const std::vector<double>& offsets,
    double f_dominant)
{
    int nTraces = offsets.size();
    int nf = freqs.size();
    if (nTraces == 0 || nf == 0 || nt == 0) return {};

    // 1. 生成雷克子波振幅谱 A(f)
    std::vector<double> amp(nf, 0.0);
    for (int i = 0; i < nf; ++i) {
        double f = freqs[i];
        // Ricker 频域解析振幅谱: A(f) = (2 / sqrt(pi)) * (f^2 / fm^3) * exp(-f^2 / fm^2)
        double ratio = (f * f) / (f_dominant * f_dominant);
        amp[i] = ratio * std::exp(1.0 - ratio);
    }

    // 2. 构造逆傅里叶变换 (IFFT) 容器
    int nfft = 1;
    while (nfft < nt) nfft <<= 1;
    if (nfft < 2048) nfft = 2048;

    double df = 1.0 / (nfft * dt);
    int nFreqBins = nfft / 2 + 1;

    Eigen::FFT<double> fft;
    std::vector<std::vector<float>> gather(nTraces, std::vector<float>(nt, 0.0f));

#pragma omp parallel for
    for (int tr = 0; tr < nTraces; ++tr) {
        std::vector<std::complex<double>> spectrum(nfft, std::complex<double>(0.0, 0.0));
        double dist = offsets[tr];

        for (int i = 0; i < nf; ++i) {
            double f = freqs[i];
            double c = phaseVel[i];
            if (c <= 1e-3 || f <= 0.0) continue;

            int k_fft = static_cast<int>(std::round(f / df));
            if (k_fft >= nFreqBins) continue;

            // 物理相移: phi = -2 * pi * f * x / c(f)
            double phase = -2.0 * PI * f * dist / c;
            std::complex<double> phaseShift(std::cos(phase), std::sin(phase));

            // 正频率
            spectrum[k_fft] = amp[i] * phaseShift;
            // 负频率 (共轭对称，保证 IFFT 后为纯实数时域信号)
            if (k_fft > 0 && k_fft < nfft / 2) {
                spectrum[nfft - k_fft] = std::conj(spectrum[k_fft]);
            }
        }

        // 逆变换回时域
        std::vector<double> timeSignal;
        fft.inv(timeSignal, spectrum);

        // 截取前 nt 个时间采样点
        for (int t = 0; t < nt; ++t) {
            gather[tr][t] = static_cast<float>(timeSignal[t]);
        }
    }

    return gather;
}