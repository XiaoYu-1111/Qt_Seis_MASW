#include "Pro_h/SignalProcessingUtils.h"

// 辅助转换代码
std::vector<std::vector<float>> complexToFloat(const std::vector<std::vector<std::complex<double>>>& st_result) {
	if (st_result.empty()) return {};

	int rows = st_result.size();
	int cols = st_result[0].size();

	// 预分配内存，防止多次拷贝
	std::vector<std::vector<float>> result(rows, std::vector<float>(cols));

	// 使用 OpenMP 加速转换过程
#pragma omp parallel for
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			// std::abs 计算复数的模 (Magnitude/Amplitude)
			// static_cast 将 double 转为 float
			result[i][j] = static_cast<float>(std::abs(st_result[i][j]));
		}
	}
	return result;
}

std::vector<std::vector<float>>
doubleToFloat2D(const std::vector<std::vector<double>>& in)
{
	if (in.empty()) return {};

	const size_t rows = in.size();
	const size_t cols = in[0].size();

	std::vector<std::vector<float>> out(rows, std::vector<float>(cols));

#pragma omp parallel for
	for (int i = 0; i < (int)rows; ++i)
	{
		for (size_t j = 0; j < cols; ++j)
			out[i][j] = static_cast<float>(in[i][j]);
	}

	return out;
}

//数据处理函数 S变换
std::vector<std::vector<std::complex<double>>> st_transform(
	const std::vector<double>& t,
	const std::vector<double>& Sig,
	double freqlow, double freqhigh, double alpha)
{
	const double PI = 3.14159265358979323846;

	// 1. 基础参数计算
	int nLevel = static_cast<int>((freqhigh - freqlow) / alpha) + 1;
	int TimeLen = t.size();
	if (TimeLen == 0) return {};

	double dt = t[1] - t[0];

	// 2. 预分配结果矩阵 (避免 push_back)
	// wcoefs[freq_index][time_index]
	std::vector<std::vector<std::complex<double>>> wcoefs(nLevel);

	int processed_count = 0;

	// 4. OpenMP 并行计算
	// shared: 共享变量, private: 私有变量
#pragma omp parallel for schedule(dynamic)
	for (int m = 0; m < nLevel; ++m) {
		// 每个线程分配内存
		wcoefs[m].resize(TimeLen);

		double f = freqlow + m * alpha;

		// 避免 f=0 导致除零 (S变换中 f=0 通常处理为直流分量或跳过)
		if (std::abs(f) < 1e-6) f = 1e-6;

		double sigma_f = 1.0 / std::abs(f);
		double factor = 1.0 / (sqrt(2 * PI) * sigma_f);
		double sigma_sq_inv = 1.0 / (sigma_f * sigma_f); // 预计算 1/sigma^2

		// 时间平移循环 (tau) - 对应 n
#pragma omp parallel for schedule(dynamic)

		for (int n = 0; n < TimeLen; ++n) {
			std::complex<double> sum(0.0, 0.0);
			double tau = t[n]; // 当前平移时间

			// 积分循环 (t) - 对应 k
			// 注意：这种直接卷积是 O(N^2)，数据量大时仍较慢，FFT方法是 O(NlogN)
			for (int k = 0; k < TimeLen; ++k) {
				double time_diff = tau - t[k]; // (tau - t) 或 (n*dt - t[k])

				// 高斯窗部分
				double exponent = -0.5 * (time_diff * time_diff) * sigma_sq_inv;
				// 如果 exponent 太小，exp 接近0，可以剪枝优化
				if (exponent < -10.0) continue;

				double gauss_val = factor * std::exp(exponent);

				// 复指数部分 e^(-i * 2 * PI * f * t[k])
				double phase = -2 * PI * f * t[k];

				// 欧拉公式: exp(ix) = cos(x) + i*sin(x)
				std::complex<double> complex_exp(std::cos(phase), std::sin(phase));

				sum += Sig[k] * gauss_val * complex_exp;
			}
			wcoefs[m][n] = sum * dt;
		}

	}

	return wcoefs;
}

//数据处理函数 STFT变换
// STFT变换
std::vector<std::vector<double>> stft_fft(
	const std::vector<double>& x,
	double fs,
	int winSize,
	int hop,
	int nfft)
{
	int N = (int)x.size();

	// 自动修正 nfft，必须大于等于 winSize 且通常为 2 的幂
	if (nfft < winSize) nfft = winSize;

	// 基础参数检查
	if (N < winSize || hop <= 0) return {};

	int nFrames = (N - winSize) / hop + 1;
	int nFreq = nfft / 2 + 1; // 单边谱 (0 到 fs/2)

	// 结果矩阵：[频率行][时间列]
	std::vector<std::vector<double>> spec(
		nFreq, std::vector<double>(nFrames, 0.0));

	// 1. 生成汉宁窗 (Hann Window)
	std::vector<double> win(winSize);
	double winSumSq = 0.0; // 用于能量归一化
	for (int i = 0; i < winSize; ++i) {
		win[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (winSize - 1)));
		winSumSq += win[i] * win[i];
	}

	// 2. 归一化系数 (Amplitude Spectrum)
	// 如果想要功率谱密度(PSD)，系数计算方式不同。这里按幅度谱计算方便可视化。
	// 对于加窗信号，恢复幅值通常除以 sum(win)/2 或者 sqrt(sum(win^2))
	// 这里使用简单的幅值恢复，使得正弦波峰值接近真实振幅
	double scale = 1.0;
	double winSum = 0.0;
	for (double w : win) winSum += w;
	if (winSum > 0) scale = 2.0 / winSum;

	// OpenMP 并行计算
#pragma omp parallel
	{
		Eigen::FFT<double> fft;
		std::vector<std::complex<double>> timeBuf(nfft);
		std::vector<std::complex<double>> freqBuf(nfft);

#pragma omp for schedule(static)
		for (int i = 0; i < nFrames; ++i)
		{
			int pos = i * hop;

			// 填充数据并加窗
			std::fill(timeBuf.begin(), timeBuf.end(), std::complex<double>(0, 0));
			for (int j = 0; j < winSize; ++j) {
				if (pos + j < N) {
					timeBuf[j] = x[pos + j] * win[j];
				}
			}

			// 执行 FFT
			fft.fwd(freqBuf, timeBuf);

			// 计算幅值 (取前 nFreq 个点)
			for (int k = 0; k < nFreq; ++k)
			{
				double mag = std::abs(freqBuf[k]) * scale;
				spec[k][i] = mag; // 这里存幅值，如果颜色太暗可以在绘图时取对数
			}
		}
	}

	return spec;
}

//Welch PSD 计算函数

// 辅助函数：生成汉宁窗 (Hann Window)
std::vector<double> get_hann_window(int len) {
	std::vector<double> win(len);
	for (int i = 0; i < len; ++i) {
		win[i] = 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * i / (len - 1)));
	}
	return win;
}

/**
 * @brief Welch功率谱密度估计
 * @param x 输入信号
 * @param fs 采样率 (Hz)
 * @param nperseg 每段长度 (窗口大小)，例如 1024, 2048
 * @param freqs_out 输出频率轴
 * @param psd_out 输出功率谱密度 (V^2/Hz)
 */
void compute_welch_psd(const std::vector<double>& x, double fs, int nperseg,
	std::vector<double>& freqs_out, std::vector<double>& psd_out)
{
	int N = x.size();
	if (N < nperseg) nperseg = N; // 保护

	// 1. 参数设置
	int noverlap = nperseg / 2;   // 50% 重叠 (标准做法)
	int step = nperseg - noverlap;
	int nfft = nperseg;           // FFT 点数通常等于窗口长度，也可以更大(补零)

	// 2. 准备窗函数和归一化系数
	std::vector<double> win = get_hann_window(nperseg);
	double win_sum_sq = 0.0;      // 窗口能量归一化因子
	for (double w : win) win_sum_sq += w * w;

	// Welch 归一化系数: scale = 2 / (fs * sum(win^2))
	// 乘2是因为我们要单边谱(One-sided)，且把负频能量加回来
	// 除以 fs 是为了得到密度 (/Hz)
	double scale = 2.0 / (fs * win_sum_sq);

	// 3. 分段计算 FFT 并累加
	int nFreq = nfft / 2 + 1;
	std::vector<double> psd_avg(nFreq, 0.0);
	int num_segments = 0;

	Eigen::FFT<double> fft;
	std::vector<double> segment(nperseg);
	std::vector<std::complex<double>> fft_out(nfft);

	for (int start = 0; start <= N - nperseg; start += step)
	{
		// 3.1 提取分段数据
		// 同时去直流 (Detrend constant) - 这是一个好习惯
		double local_mean = 0.0;
		for (int i = 0; i < nperseg; ++i) local_mean += x[start + i];
		local_mean /= nperseg;

		for (int i = 0; i < nperseg; ++i) {
			segment[i] = (x[start + i] - local_mean) * win[i];
		}

		// 3.2 FFT
		fft.fwd(fft_out, segment); // 假设 segment 长度等于 nfft，否则需补零

		// 3.3 计算功率并累加
		for (int k = 0; k < nFreq; ++k) {
			double mag = std::abs(fft_out[k]);
			psd_avg[k] += mag * mag;
		}
		num_segments++;
	}

	// 4. 平均与归一化
	freqs_out.resize(nFreq);
	psd_out.resize(nFreq);

	double freq_step = fs / nfft;

	for (int k = 0; k < nFreq; ++k)
	{
		freqs_out[k] = k * freq_step;

		// 平均：除以段数
		// 归一化：乘以前面算的 scale
		double val = psd_avg[k] / num_segments;
		val *= scale;

		// 直流分量和 Nyquist 频率不需要乘2，所以要除回来 (严谨的数学修正)
		if (k == 0 || k == nFreq - 1) val /= 2.0;

		psd_out[k] = val;
	}
}


/**
 * @brief 计算希尔伯特包络 (Hilbert Envelope)
 * @param input 原始实数信号
 * @return 包络信号 (模值)
 */
std::vector<double> compute_hilbert_envelope(const std::vector<double>& input)
{
	int N = input.size();
	if (N == 0) return {};

	Eigen::FFT<double> fft;
	std::vector<std::complex<double>> spectrum;

	// 1. FFT 变换到频域
	fft.fwd(spectrum, input);

	// 2. 构造解析信号 (Analytic Signal) 的频谱
	// 规则：
	// - 直流分量 (0) 保持不变
	// - 正频率 (1 ~ N/2) 乘以 2
	// - 负频率 (N/2+1 ~ N-1) 置为 0
	// - Nyquist 频率 (N/2) 保持不变 (如果是偶数)

	std::vector<std::complex<double>> analytic_spec(N, std::complex<double>(0, 0));

	// DC
	analytic_spec[0] = spectrum[0];

	int half = (N % 2 == 0) ? (N / 2) : ((N + 1) / 2);

	// 正频率 * 2
	for (int i = 1; i < half; ++i) {
		analytic_spec[i] = 2.0 * spectrum[i];
	}

	// Nyquist (如果是偶数长度)
	if (N % 2 == 0) {
		analytic_spec[N / 2] = spectrum[N / 2];
	}

	// 3. IFFT 反变换回时域
	std::vector<std::complex<double>> analytic_signal;
	fft.inv(analytic_signal, analytic_spec);

	// 4. 求模值 (Magnitude) 得到包络
	std::vector<double> envelope(N);
	for (int i = 0; i < N; ++i) {
		envelope[i] = std::abs(analytic_signal[i]);
	}

	return envelope;
}

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
	double omega0 = 6.0)
{
	int n = sig.size();

	// 1. 为了消除循环卷积的边缘效应，进行补零到 2 的幂次 (Next Power of 2)
	int n_padded = std::pow(2, std::ceil(std::log2(n)));
	// 如果数据太短，至少补到一定长度
	if (n_padded < 2 * n) n_padded *= 2;

	// 2. 预计算信号的 FFT
	Eigen::FFT<double> fft;
	std::vector<std::complex<double>> sig_padded(n_padded, 0.0);
	for (int i = 0; i < n; ++i) sig_padded[i] = sig[i];

	std::vector<std::complex<double>> sig_fft;
	fft.fwd(sig_fft, sig_padded);

	// 3. 构建角频率轴 (Angular Frequency Axis)
	// 对应 FFT 的频率分布: [0, 1, ..., N/2, -N/2+1, ..., -1] * (2*pi*fs/N)
	std::vector<double> omega(n_padded);
	double dw = 2.0 * M_PI * fs / n_padded;
	for (int i = 0; i < n_padded; ++i) {
		if (i <= n_padded / 2)
			omega[i] = i * dw;
		else
			omega[i] = (i - n_padded) * dw;
	}

	// 4. 准备结果矩阵
	std::vector<std::vector<std::complex<double>>> cwt_matrix(num_freqs);

	// 5. 并行循环计算每个尺度的 CWT
	// 我们按照线性频率分布来计算，这样画图时纵轴是线性的
#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < num_freqs; ++i)
	{
		// 当前分析频率
		double f = f_min + i * (f_max - f_min) / (num_freqs - 1);
		if (f <= 1e-6) f = 1e-6; // 保护

		// 将频率转换为尺度 (Scale)
		// 对于 Morlet (omega0=6), scale = (omega0 + sqrt(2 + omega0^2)) / (4 * pi * f) 
		// 简化近似公式: scale = omega0 / (2 * pi * f)
		double scale = omega0 / (2.0 * M_PI * f);

		// 构造 Morlet 小波的频域响应 (Analytical Morlet)
		// Psi_hat(w) = pi^0.25 * sqrt(2*s) * exp(-0.5 * (s*w - w0)^2) * step(w)
		// 注意：CWT 定义为 IFFT( X(w) * sqrt(s) * Psi_hat*(s*w) )
		// 为了保持能量守恒或幅度物理意义，归一化系数会有所不同。
		// 这里使用幅度归一化，使得正弦波的 CWT 幅值接近真实幅值。

		std::vector<std::complex<double>> wavelet_fft(n_padded, 0.0);

		double norm_factor = std::pow(M_PI, 0.25) * std::sqrt(2.0 * scale); // 标准因子

		for (int k = 0; k < n_padded; ++k)
		{
			// Analytic Wavelet 只有正频率部分
			if (k > 0 && k < n_padded / 2 + 1) // omega[k] > 0
			{
				double sw = scale * omega[k];
				double exponent = -0.5 * std::pow(sw - omega0, 2);

				// 优化：太小的指数直接忽略
				if (exponent > -20.0) {
					// 频域乘法: Signal_FFT * Wavelet_FFT
					// 乘以 (2/fs) 甚至更多是为了幅值恢复，这里先算标准定义
					wavelet_fft[k] = sig_fft[k] * std::exp(exponent);
				}
			}
		}

		// IFFT 变换回时域
		std::vector<std::complex<double>> time_domain_out;
		fft.inv(time_domain_out, wavelet_fft);

		// 截取原始长度并归一化
		cwt_matrix[i].resize(n);

		// 经验归一化系数，使得幅值看起来物理意义更强
		// CWT 的幅值物理意义比较复杂，通常看相对值
		double output_scale = std::sqrt(scale) * (dw / std::sqrt(2 * M_PI));

		for (int t = 0; t < n; ++t) {
			cwt_matrix[i][t] = time_domain_out[t] * output_scale;
		}
	}

	return cwt_matrix;
}

/**
 * @brief 健壮版 CWT (带内存检查)
 * @return 空矩阵表示失败
 */
std::vector<std::vector<std::complex<double>>> cwt_morlet_safe(
	const std::vector<double>& sig,
	double fs,
	double f_min,
	double f_max,
	int num_freqs,
	double omega0 = 6.0)
{
	int n = sig.size();
	if (n == 0) return {};

	try {
		// 1. 优化补零策略 (仅补到 Next Power of 2，不再无脑翻倍)
		// 虽然边缘效应会稍微明显一点，但能节省 50% 内存
		int n_padded = std::pow(2, std::ceil(std::log2(n)));

		// 2. 内存安全检查
		// 估算内存: 输入+FFT缓存 + 输出矩阵
		// 输出矩阵大小 = num_freqs * n * 16 bytes (complex<double>)
		size_t estimated_bytes = (size_t)num_freqs * n * sizeof(std::complex<double>);
		size_t fft_bytes = (size_t)n_padded * sizeof(std::complex<double>) * 2;
		size_t total_mb = (estimated_bytes + fft_bytes) / (1024 * 1024);

		// 如果预估内存超过 800MB (可根据你电脑配置调整)，则拒绝计算
		if (total_mb > 800) {
			std::cerr << "CWT Error: Memory requirement too high (" << total_mb << " MB). Reduce selection." << std::endl;
			return {};
		}

		// 3. FFT 预处理
		Eigen::FFT<double> fft;
		std::vector<std::complex<double>> sig_padded(n_padded, 0.0);
		for (int i = 0; i < n; ++i) sig_padded[i] = sig[i];

		std::vector<std::complex<double>> sig_fft;
		fft.fwd(sig_fft, sig_padded);

		// 4. 频率轴
		std::vector<double> omega(n_padded);
		double dw = 2.0 * M_PI * fs / n_padded;
		for (int i = 0; i < n_padded; ++i) {
			omega[i] = (i <= n_padded / 2) ? (i * dw) : ((i - n_padded) * dw);
		}

		// 5. 分配结果矩阵 (最易崩溃点)
		std::vector<std::vector<std::complex<double>>> cwt_matrix(num_freqs);

#pragma omp parallel for schedule(dynamic)
		for (int i = 0; i < num_freqs; ++i)
		{
			double f = f_min + i * (f_max - f_min) / (num_freqs - 1);
			if (f <= 1e-6) f = 1e-6;

			double scale = omega0 / (2.0 * M_PI * f);

			// 构造 Morlet 频域响应
			std::vector<std::complex<double>> wavelet_fft(n_padded, 0.0);
			// 归一化系数
			double norm_factor = std::pow(M_PI, 0.25) * std::sqrt(2.0 * scale);

			for (int k = 0; k < n_padded; ++k)
			{
				if (k > 0 && k < n_padded / 2 + 1)
				{
					double sw = scale * omega[k];
					double exponent = -0.5 * std::pow(sw - omega0, 2);
					if (exponent > -20.0) {
						wavelet_fft[k] = sig_fft[k] * std::exp(exponent);
					}
				}
			}

			std::vector<std::complex<double>> time_domain_out;
			// 每个线程内部都有 FFT 对象，线程安全
			Eigen::FFT<double> local_fft;
			local_fft.inv(time_domain_out, wavelet_fft);

			// 截断结果
			cwt_matrix[i].resize(n);
			double output_scale = std::sqrt(scale) * (dw / std::sqrt(2 * M_PI));
			for (int t = 0; t < n; ++t) {
				cwt_matrix[i][t] = time_domain_out[t] * output_scale;
			}
		}

		return cwt_matrix;
	}
	catch (const std::bad_alloc& e) {
		std::cerr << "CWT Crash Prevented: Memory allocation failed. " << e.what() << std::endl;
		return {}; // 返回空表示失败
	}
	catch (const std::exception& e) {
		std::cerr << "CWT Error: " << e.what() << std::endl;
		return {};
	}
}


#include <vector>
#include <complex>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <unsupported/Eigen/FFT>
#include <iostream>

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
	int K, double alpha, double tau, double tol)
{
	int T = signal.size();

	// 1. 镜像延拓 (Mirror Extension) - 减少边界效应
	// 简单起见，这里做 T/2 的镜像
	int T_half = T / 2;
	std::vector<double> f_mirrored;
	f_mirrored.reserve(T + 2 * T_half);

	// 左镜像
	for (int i = T_half - 1; i >= 0; --i) f_mirrored.push_back(signal[i]);
	// 原始信号
	for (double v : signal) f_mirrored.push_back(v);
	// 右镜像
	for (int i = T - 1; i >= T - T_half; --i) f_mirrored.push_back(signal[i]);

	int N = f_mirrored.size(); // 延拓后的长度

	// 2. FFT 到频域
	Eigen::FFT<double> fft;
	std::vector<std::complex<double>> f_hat_full;
	fft.fwd(f_hat_full, f_mirrored);

	// VMD 只需要正频率部分 (0 ~ N/2)
	int N_half = N / 2;
	std::vector<std::complex<double>> f_hat(N_half + 1);
	for (int i = 0; i <= N_half; ++i) f_hat[i] = f_hat_full[i];

	// 3. 初始化
	// omega_k: 中心频率 (初始为 0)
	std::vector<double> omega_k(K, 0.0);

	// u_hat: 模态频谱 [K][Freq]
	std::vector<std::vector<std::complex<double>>> u_hat(K, std::vector<std::complex<double>>(N_half + 1, 0.0));

	// u_hat_old: 用于检查收敛
	auto u_hat_old = u_hat;

	// lambda_hat: 拉格朗日乘子
	std::vector<std::complex<double>> lambda_hat(N_half + 1, 0.0);

	// 频率轴 (0 ~ 0.5)
	std::vector<double> freqs(N_half + 1);
	for (int i = 0; i <= N_half; ++i) freqs[i] = (double)i / N;

	// 4. ADMM 迭代
	int max_iter = 500;
	for (int n = 0; n < max_iter; ++n)
	{
		// --- 更新 u_k (模态) ---
		for (int k = 0; k < K; ++k)
		{
			// 计算 sum_{i!=k} u_hat_i
			std::vector<std::complex<double>> sum_u_others(N_half + 1, 0.0);
			for (int i = 0; i < K; ++i) {
				if (i == k) continue;
				for (int j = 0; j <= N_half; ++j) sum_u_others[j] += u_hat[i][j];
			}

			// 更新公式 (维纳滤波形式)
			for (int j = 0; j <= N_half; ++j)
			{
				std::complex<double> num = f_hat[j] - sum_u_others[j] + lambda_hat[j] / 2.0;
				double denom = 1.0 + 2.0 * alpha * std::pow(freqs[j] - omega_k[k], 2);
				u_hat[k][j] = num / denom;
			}
		}

		// --- 更新 omega_k (中心频率) ---
		for (int k = 0; k < K; ++k)
		{
			double num = 0.0;
			double denom = 0.0;
			for (int j = 0; j <= N_half; ++j)
			{
				double mag2 = std::norm(u_hat[k][j]); // |u|^2
				num += freqs[j] * mag2;
				denom += mag2;
			}
			if (denom > 1e-12) omega_k[k] = num / denom;
		}

		// --- 更新 lambda (对偶上升) ---
		// lambda <- lambda + tau * (f - sum(u_k))
		// 这里的 tau 通常在 VMD 论文里用的是 update step，如果 input tau=0，则那是噪声容限参数
		// 实际上 dual ascent step 默认可以是 0，但在噪声重建时需要更新
		// 为了简化，我们使用标准 VMD 更新逻辑 (Lagrangian multiplier update)

		std::vector<std::complex<double>> sum_u_all(N_half + 1, 0.0);
		for (int k = 0; k < K; ++k) {
			for (int j = 0; j <= N_half; ++j) sum_u_all[j] += u_hat[k][j];
		}

		for (int j = 0; j <= N_half; ++j) {
			// 这里的 tau 参数和 VMD 论文中的噪声容限含义略有混合
			// 标准实现中：lambda = lambda + tau * (f - sum_u)
			// 如果为了严格重建，我们取步长为 0 (不更新) 或者一个小值
			// 这里我们假设无噪声重建，持续更新 lambda
			lambda_hat[j] += tau * (f_hat[j] - sum_u_all[j]);
		}

		// --- 检查收敛 ---
		double diff = 0.0;
		double norm_sq = 0.0;
		for (int k = 0; k < K; ++k) {
			for (int j = 0; j <= N_half; ++j) {
				diff += std::norm(u_hat[k][j] - u_hat_old[k][j]);
				norm_sq += std::norm(u_hat_old[k][j]);
				u_hat_old[k][j] = u_hat[k][j]; // 更新旧值
			}
		}

		if (norm_sq > 0 && (diff / norm_sq) < tol) {
			break; // 收敛
		}
	}

	// 5. IFFT 重建时域信号
	VmdResult result;
	result.center_freqs.resize(K);
	for (int k = 0; k < K; ++k) result.center_freqs[k] = omega_k[k] * fs;

	result.modes.resize(K);

	for (int k = 0; k < K; ++k)
	{
		// 构造全频谱 (Hermitian Symmetric)
		std::vector<std::complex<double>> u_full(N, 0.0);
		for (int j = 0; j <= N_half; ++j) u_full[j] = u_hat[k][j];
		for (int j = 1; j < N_half; ++j)  u_full[N - j] = std::conj(u_hat[k][j]); // 共轭对称

		std::vector<double> time_domain;
		fft.inv(time_domain, u_full);

		// 截取中间原始部分 (去掉镜像延拓)
		// 注意：FFT 逆变换结果通常是实数部分
		result.modes[k].resize(T);
		for (int i = 0; i < T; ++i) {
			result.modes[k][i] = time_domain[T_half + i]; // 实部
		}
	}

	return result;
}

/**
 * @brief 移相法计算面波频散能量谱 (Phase Shift Method - Park et al., 1998)
 *
 * @param seismicData  地震道集数据 [道数 nx][采样点数 nt]
 * @param dt           时间采样率 (s)，如 0.001
 * @param dx           道间距 (m)，如 1.0
 * @param f_min        频率扫描下限 (Hz)
 * @param f_max        频率扫描上限 (Hz)
 * @param nf           频率采样点数 (通常 100~200)
 * @param v_min        相速度扫描下限 (m/s)
 * @param v_max        相速度扫描上限 (m/s)
 * @param nv           速度采样点数 (通常 150~300)
 * @return 二维能量谱矩阵 [速度 nv][频率 nf]，值范围归一化至 [0, 1]
 */
 std::vector<std::vector<float>> computePhaseShiftDispersion(
	const std::vector<std::vector<float>>& seismicData,
	float dt, float dx,float x0,
	float f_min, float f_max, int nf,
	float v_min, float v_max, int nv)
{
	int nTraces = seismicData.size();
	if (nTraces < 2 || seismicData[0].empty()) return {};

	int nt = seismicData[0].size();
	const double PI = 3.14159265358979323846;

	// 1. 补零至 2 的幂次，提高 FFT 效率与频率采样精度
	int nfft = 1;
	while (nfft < nt) nfft <<= 1;
	//if (nfft < 2048) nfft = 2048; // 保证频域分辨率
	if (nfft < 8192) nfft = 8192; // <--- 从 2048 改为 8192

	// 2. 对每一道做 FFT 并进行振幅归一化 P(x, w) = U(x, w) / |U(x, w)|
	Eigen::FFT<double> fft;
	// 存储所有道的全频域复数谱: [trace][freq_bin]
	int nFreqBins = nfft / 2 + 1;
	std::vector<std::vector<std::complex<double>>> normSpectra(nTraces, std::vector<std::complex<double>>(nFreqBins));

	double df = 1.0 / (nfft * dt); // FFT 频率分辨率

#pragma omp parallel for
	for (int tr = 0; tr < nTraces; ++tr) {
		std::vector<double> timePad(nfft, 0.0);
		for (int t = 0; t < nt; ++t) {
			timePad[t] = seismicData[tr][t];
		}

		std::vector<std::complex<double>> fwdOut;
		fft.fwd(fwdOut, timePad);

		for (int k = 0; k < nFreqBins; ++k) {
			double mag = std::abs(fwdOut[k]);
			if (mag > 1e-12) {
				normSpectra[tr][k] = fwdOut[k] / mag; // 振幅归一化
			}
			else {
				normSpectra[tr][k] = std::complex<double>(0.0, 0.0);
			}
		}
	}

	// 3. 预备输出矩阵：行对应速度 (nv)，列对应频率 (nf)
	std::vector<std::vector<float>> dispersionEnergy(nv, std::vector<float>(nf, 0.0f));

	float df_scan = (f_max - f_min) / std::max(1, nf - 1);
	float dv_scan = (v_max - v_min) / std::max(1, nv - 1);

	// 4. 并行扫描 (f, v) 空间，执行空间相移叠加 (带频域复数线性插值)
#pragma omp parallel for schedule(dynamic)
	for (int fi = 0; fi < nf; ++fi) {
		double cur_f = f_min + fi * df_scan;
		if (cur_f <= 0.0) continue;

		// =========================================================
		// 1. 计算浮点频率索引与相邻两点的插值权重
		// =========================================================
		double k_float = cur_f / df;
		int k0 = static_cast<int>(std::floor(k_float));
		int k1 = k0 + 1;
		double alpha = k_float - k0; // 插值权重 (0.0 ~ 1.0)

		// 边界越界保护
		if (k0 < 0) {
			k0 = 0;
			k1 = 0;
			alpha = 0.0;
		}
		else if (k1 >= nFreqBins) {
			k0 = nFreqBins - 1;
			k1 = nFreqBins - 1;
			alpha = 0.0;
		}

		// =========================================================
		// 2. 【核心优化】：在进入速度扫描前，先插值好当前频率所有道的复数谱
		// =========================================================
		std::vector<std::complex<double>> curNormSpectra(nTraces);
		for (int tr = 0; tr < nTraces; ++tr) {
			// 相邻两点线性加权插值
			std::complex<double> val = (1.0 - alpha) * normSpectra[tr][k0] + alpha * normSpectra[tr][k1];

			// 归一化为单位模长 (移相法标准：剔除振幅影响，仅保留纯相位)
			double mag = std::abs(val);
			if (mag > 1e-12) {
				curNormSpectra[tr] = val / mag;
			}
			else {
				curNormSpectra[tr] = std::complex<double>(0.0, 0.0);
			}
		}

		double omega = 2.0 * PI * cur_f;

		// =========================================================
		// 3. 速度扫描与空间相移叠加
		// =========================================================
		for (int vi = 0; vi < nv; ++vi) {
			double cur_v = v_min + vi * dv_scan;
			if (cur_v <= 0.0) continue;

			double k_wave = omega / cur_v; // 波数 k = w / v

			std::complex<double> stackSum(0.0, 0.0);

			for (int tr = 0; tr < nTraces; ++tr) {
				double offset = x0 + tr * dx; // 真实偏移距
				double phase = k_wave * offset;

				// 相移因子 exp(i * k * x)
				std::complex<double> shiftFactor(std::cos(phase), std::sin(phase));

				// 直接使用插值好的平滑频谱 curNormSpectra
				stackSum += curNormSpectra[tr] * shiftFactor;
			}

			// 计算叠加能量（取模并归一）
			dispersionEnergy[vi][fi] = static_cast<float>(std::abs(stackSum) / nTraces);
		}
	}

	return dispersionEnergy;
}
//Fk变换法

std::vector<std::vector<float>> computeFKDispersion(
	const std::vector<std::vector<float>>& seismicData,
	float dt, float dx,
	float f_min, float f_max, int nf,
	float v_min, float v_max, int nv)
{
	int nTraces = seismicData.size();
	if (nTraces < 2 || seismicData[0].empty()) return {};
	int nt = seismicData[0].size();
	const double PI = 3.14159265358979323846;

	// 1. 时间向补零 FFT
	int nfft_t = 1;
	while (nfft_t < nt) nfft_t <<= 1;
	if (nfft_t < 2048) nfft_t = 2048;

	double df = 1.0 / (nfft_t * dt);
	int nFreqBins = nfft_t / 2 + 1;

	Eigen::FFT<double> fft_t;
	// 存储时间变换后的 F-X 域数据: [nTraces][nFreqBins]
	std::vector<std::vector<std::complex<double>>> fxData(nTraces, std::vector<std::complex<double>>(nFreqBins));

#pragma omp parallel for
	for (int tr = 0; tr < nTraces; ++tr) {
		std::vector<double> timePad(nfft_t, 0.0);
		for (int t = 0; t < nt; ++t) timePad[t] = seismicData[tr][t];

		std::vector<std::complex<double>> fwdOut;
		fft_t.fwd(fwdOut, timePad);

		for (int k = 0; k < nFreqBins; ++k) {
			fxData[tr][k] = fwdOut[k]; // F-K 经典算法保留真实物理振幅
		}
	}

	// 2. 空间向构建 F-K 谱并映射回 (f, v)
	std::vector<std::vector<float>> fkEnergy(nv, std::vector<float>(nf, 0.0f));
	float df_scan = (f_max - f_min) / std::max(1, nf - 1);
	float dv_scan = (v_max - v_min) / std::max(1, nv - 1);

#pragma omp parallel for schedule(dynamic)
	for (int fi = 0; fi < nf; ++fi) {
		double cur_f = f_min + fi * df_scan;
		if (cur_f <= 0.0) continue;

		// 频域线性插值系数
		double k_float = cur_f / df;
		int k0 = std::clamp((int)std::floor(k_float), 0, nFreqBins - 2);
		int k1 = k0 + 1;
		double alpha = k_float - k0;

		// 提取当前频率在各道的复数振幅
		std::vector<std::complex<double>> cur_fx(nTraces);
		for (int tr = 0; tr < nTraces; ++tr) {
			cur_fx[tr] = (1.0 - alpha) * fxData[tr][k0] + alpha * fxData[tr][k1];
		}

		double omega = 2.0 * PI * cur_f;

		for (int vi = 0; vi < nv; ++vi) {
			double cur_v = v_min + vi * dv_scan;
			if (cur_v <= 0.0) continue;

			// 经典空间波数 k = omega / v
			double k_wave = omega / cur_v;

			// 空间离散傅里叶变换积分 (空间积分等价于空间向 FFT 在特定 k 的采样)
			std::complex<double> fkSum(0.0, 0.0);
			for (int tr = 0; tr < nTraces; ++tr) {
				double x_pos = tr * dx;
				double phase = k_wave * x_pos;
				std::complex<double> spatialFactor(std::cos(phase), std::sin(phase));
				fkSum += cur_fx[tr] * spatialFactor;
			}

			fkEnergy[vi][fi] = static_cast<float>(std::abs(fkSum));
		}
	}

	return fkEnergy;
}
//MVDR

std::vector<std::vector<float>> computeCaponMVDRDispersion(
	const std::vector<std::vector<float>>& seismicData,
	float dt, float dx,
	float f_min, float f_max, int nf,
	float v_min, float v_max, int nv)
{
	int nTraces = seismicData.size();
	if (nTraces < 8 || seismicData[0].empty()) return {};
	int nt = seismicData[0].size();
	const double PI = 3.14159265358979323846;

	// 1. 时间向补零 FFT (与移相法完全保持一致)
	int nfft_t = 1;
	while (nfft_t < nt) nfft_t <<= 1;
	if (nfft_t < 2048) nfft_t = 2048;

	double df = 1.0 / (nfft_t * dt);
	int nFreqBins = nfft_t / 2 + 1;
	Eigen::FFT<double> fft_t;

	std::vector<std::vector<std::complex<double>>> fxData(nTraces, std::vector<std::complex<double>>(nFreqBins));
#pragma omp parallel for
	for (int tr = 0; tr < nTraces; ++tr) {
		std::vector<double> timePad(nfft_t, 0.0);
		for (int t = 0; t < nt; ++t) timePad[t] = seismicData[tr][t];

		std::vector<std::complex<double>> fwdOut;
		fft_t.fwd(fwdOut, timePad);
		for (int k = 0; k < nFreqBins; ++k) {
			double mag = std::abs(fwdOut[k]);
			fxData[tr][k] = (mag > 1e-12) ? (fwdOut[k] / mag) : std::complex<double>(0.0, 0.0);
		}
	}

	// =========================================================
	// 2. 空间子阵列参数设置 (确保满秩: L <= N/2)
	// =========================================================
	int L = std::clamp(int(nTraces / 2), 4, 64); // 子阵列长度取总道数一半 (如96道取48)
	int M = nTraces - L + 1;                     // 子阵列滑动次数 (M >= L 保证满秩)

	std::vector<std::vector<float>> caponEnergy(nv, std::vector<float>(nf, 0.0f));
	float df_scan = (f_max - f_min) / std::max(1, nf - 1);
	float dv_scan = (v_max - v_min) / std::max(1, nv - 1);

#pragma omp parallel for schedule(dynamic)
	for (int fi = 0; fi < nf; ++fi) {
		double cur_f = f_min + fi * df_scan;
		if (cur_f <= 0.0) continue;

		// 频域相邻点线性加权插值
		double k_float = cur_f / df;
		int k0 = std::clamp((int)std::floor(k_float), 0, nFreqBins - 2);
		int k1 = k0 + 1;
		double alpha = k_float - k0;

		Eigen::VectorXcd x_full(nTraces);
		for (int tr = 0; tr < nTraces; ++tr) {
			std::complex<double> val = (1.0 - alpha) * fxData[tr][k0] + alpha * fxData[tr][k1];
			double mag = std::abs(val);
			x_full(tr) = (mag > 1e-12) ? (val / mag) : std::complex<double>(0.0, 0.0);
		}

		// =========================================================
		// 3. 构建前向-后向双向空间平滑协方差矩阵 (FBSS)
		// =========================================================
		// A. 前向滑动平滑 Rf
		Eigen::MatrixXcd Rf = Eigen::MatrixXcd::Zero(L, L);
		for (int m = 0; m < M; ++m) {
			Eigen::VectorXcd x_sub = x_full.segment(m, L);
			Rf += x_sub * x_sub.adjoint();
		}
		Rf /= double(M);

		// B. 后向空间平滑 Rb = J * conj(Rf) * J (消除相干信号干扰)
		Eigen::MatrixXcd Rb = Rf.conjugate().colwise().reverse().rowwise().reverse();

		// C. 双向平均
		Eigen::MatrixXcd R = 0.5 * (Rf + Rb);

		// =========================================================
		// 4. 稳健对角加载 (Diagonal Loading, 加 3% 白噪声提高条件数)
		// =========================================================
		double trace_R = R.trace().real();
		double epsilon = 0.03 * (trace_R / L);
		R += epsilon * Eigen::MatrixXcd::Identity(L, L);

		// 5. 协方差矩阵求逆 R^{-1} (在此频率下仅需计算一次)
		Eigen::MatrixXcd R_inv = R.inverse();

		double omega = 2.0 * PI * cur_f;

		// =========================================================
		// 6. 速度扫描成像
		// =========================================================
		for (int vi = 0; vi < nv; ++vi) {
			double cur_v = v_min + vi * dv_scan;
			if (cur_v <= 0.0) continue;

			double k_wave = omega / cur_v;

			// 构造导向矢量 a (注意：物理走时延迟符号必须为负号 -k*x !)
			Eigen::VectorXcd a(L);
			for (int l = 0; l < L; ++l) {
				double dist = l * dx;
				double phase = -k_wave * dist; // <--- 【核心修复】：必须带负号！
				a(l) = std::complex<double>(std::cos(phase), std::sin(phase));
			}

			// 计算二次型分母 a^H * R^{-1} * a
			// 采用显式矩阵乘法避免 Eigen .dot() 的歧义
			double denom = (a.adjoint() * R_inv * a).value().real();

			// Capon 功率输出 (分母越小说明与信号越匹配，能量越强)
			if (denom > 1e-12) {
				caponEnergy[vi][fi] = static_cast<float>(1.0 / denom);
			}
			else {
				caponEnergy[vi][fi] = 0.0f;
			}
		}
	}

	return caponEnergy;
}

std::vector<std::vector<float>> computeSlantStackDispersion(
	const std::vector<std::vector<float>>& seismicData,
	float dt, float dx, float x0,
	float f_min, float f_max, int nf,
	float v_min, float v_max, int nv)
{
	int nTraces = seismicData.size();
	if (nTraces < 2 || seismicData[0].empty()) return {};
	int nt = seismicData[0].size();
	const double PI = 3.14159265358979323846;

	// 1. FFT 补零长度准备
	int nfft_tau = 1;
	while (nfft_tau < nt) nfft_tau <<= 1;
	if (nfft_tau < 2048) nfft_tau = 2048;

	double df_fft = 1.0 / (nfft_tau * dt);
	int nFreqBins = nfft_tau / 2 + 1;
	Eigen::FFT<double> fft_tau;

	// 2. 扫描参数准备
	std::vector<std::vector<float>> tauPEnergy(nv, std::vector<float>(nf, 0.0f));
	float df_scan = (f_max - f_min) / std::max(1, nf - 1);
	float dv_scan = (v_max - v_min) / std::max(1, nv - 1);

	// =========================================================
	// 3. 对各个相速度 v (慢度 p = 1/v) 执行时域倾斜叠加 + 1D FFT
	// =========================================================
#pragma omp parallel for schedule(dynamic)
	for (int vi = 0; vi < nv; ++vi) {
		double cur_v = v_min + vi * dv_scan;
		if (cur_v <= 0.0) continue;

		double p_slowness = 1.0 / cur_v; // 水平慢度 (s/m)

		// A. 构建一条长度为 nt 的截距时间道 u(tau, p)
		std::vector<double> tau_trace(nfft_tau, 0.0);

		for (int k_tau = 0; k_tau < nt; ++k_tau) {
			double tau = k_tau * dt;
			double sum_val = 0.0;

			for (int tr = 0; tr < nTraces; ++tr) {
				double dist = x0 + tr * dx; // 物理偏移距
				double t_arr = tau + p_slowness * dist; // 走时斜线方程

				// 分数走时点的高精度线性插值
				double sample_idx = t_arr / dt;
				int i0 = static_cast<int>(std::floor(sample_idx));
				int i1 = i0 + 1;

				if (i0 >= 0 && i1 < nt) {
					double frac = sample_idx - i0;
					double val = (1.0 - frac) * seismicData[tr][i0] + frac * seismicData[tr][i1];
					sum_val += val;
				}
			}
			tau_trace[k_tau] = sum_val / nTraces;
		}

		// B. 沿截距时间 tau 方向做一维实数 FFT -> U(f, p)
		std::vector<std::complex<double>> tau_fft;
		fft_tau.fwd(tau_fft, tau_trace);

		// C. 提取目标频率点 f 处的振幅模长
		for (int fi = 0; fi < nf; ++fi) {
			double cur_f = f_min + fi * df_scan;
			if (cur_f <= 0.0) continue;

			// 频域相邻两点线性插值
			double k_float = cur_f / df_fft;
			int k0 = std::clamp(static_cast<int>(std::floor(k_float)), 0, nFreqBins - 2);
			int k1 = k0 + 1;
			double alpha = k_float - k0;

			std::complex<double> spec_val = (1.0 - alpha) * tau_fft[k0] + alpha * tau_fft[k1];
			tauPEnergy[vi][fi] = static_cast<float>(std::abs(spec_val));
		}
	}

	return tauPEnergy;
}