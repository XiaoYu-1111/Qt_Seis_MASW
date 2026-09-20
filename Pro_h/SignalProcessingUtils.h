#pragma once
#include <vector>
#include <iostream>  // <--- 添加这一行
#include <complex>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <Eigen/Dense>
#include <unsupported/Eigen/FFT>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//复数转换辅助函数
std::vector<std::vector<float>> complexToFloat(
	const std::vector<std::vector<std::complex<double>>>& in);

std::vector<std::vector<float>>
doubleToFloat2D(const std::vector<std::vector<double>>& in);

//ST
std::vector<std::vector<std::complex<double>>> st_transform(const std::vector<double>& t,
    const std::vector<double>& x,
    double fmin,
    double fmax,
    double df);
//STFT
std::vector<std::vector<double>>
stft_fft(const std::vector<double>& x,
	double fs,
	int winSize,
	int hop,
	int nfft);

// 辅助函数：生成汉宁窗 (Hann Window)
std::vector<double> get_hann_window(int len);
//Welch功率谱密度估计
void compute_welch_psd(const std::vector<double>& x, double fs, int nperseg,
	std::vector<double>& freqs_out, std::vector<double>& psd_out);

/**
 * @brief 计算希尔伯特包络 (Hilbert Envelope)
 * @param input 原始实数信号
 * @return 包络信号 (模值)
 */
std::vector<double> compute_hilbert_envelope(const std::vector<double>& input);

/**
 * @brief 连续小波变换 (CWT) - 基于 Morlet 小波
 * @param sig 输入信号
 * @param fs 采样率
 * @param f_min 最小分析频率
 * @param f_max 最大分析频率
 * @param num_freqs 频率点数 (决定纵轴分辨率)
 * @param omega0 Morlet 参数 (通常取 6.0, 类似地震子波)
 * @return 结果矩阵 [频率行][时间列] (复数结果)
 */
std::vector<std::vector<std::complex<double>>> cwt_morlet(
	const std::vector<double>& sig,
	double fs,
	double f_min,
	double f_max,
	int num_freqs,
	double omega0 );
std::vector<std::vector<std::complex<double>>> cwt_morlet_safe(
	const std::vector<double>& sig,
	double fs,
	double f_min,
	double f_max,
	int num_freqs,
	double omega0);

struct VmdResult {
	std::vector<std::vector<double>> modes; // 分解出的 K 个模态 (时域)
	std::vector<double> center_freqs;       // 对应的 K 个中心频率 (最终迭代结果)
};
/**
 * @brief 变分模态分解 (VMD)
 * @param signal 输入信号
 * @param fs 采样率
 * @param K 分解的模态个数 (通常取 3~8)
 * @param alpha 带宽约束 (通常取 2000, 越大数据越光滑/带宽越窄)
 * @param tau 噪声容限 (通常 0 表示无噪声，强噪环境可设为 0.1~0.5)
 * @param tol 收敛阈值 (例如 1e-7)
 * @return VmdResult
 */
 VmdResult compute_vmd(const std::vector<double>& signal, double fs,
	int K, double alpha, double tau ,  double tol );

 std::vector<std::vector<float>> computePhaseShiftDispersion(
	 const std::vector<std::vector<float>>& seismicData,
	 float dt, float dx, float x0,  // <--- 增加 x0 参数
	 float f_min, float f_max, int nf,
	 float v_min, float v_max, int nv);
