#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cstdint> // for uint32_t, int16_t
#include <cmath>
#include <cstring>
#include <algorithm>

#include <Eigen/Dense>     // 矩阵运算
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace SeismicIO {

    // SEGY 读取
    std::vector<std::vector<float>> readSegyFile2D(std::string inputfile);

    // SEGY 写入
    // 支持直接从 2D vector 写入，或者从展平的 1D vector (通过重载实现) 写入
    void writeSegyFile2D(const std::vector<std::vector<float>>& dataArray, const std::string outputfile, float dt );
    void writeSegyFile2D(const std::vector<float>& flatData, int nTraces, int nSamples, const std::string& outputfile, float dt );

    // 简单文本写入 (CSV style)
    void writeTextFile2D(const std::vector<std::vector<float>>& dataArray, const std::string& outputfile);

    // --- 内部辅助函数 (通常放在 .cpp 的匿名命名空间，或者设为 private，这里为了方便直接暴露) ---
    uint32_t swap4byte(uint32_t value);
    int16_t swap2byte(int16_t value);
    float ibm2float(uint32_t x);

} // namespace SeismicIO

namespace SeismicUtils {

    std::vector<float> generateRickerWavelet(float f, float dt, int nt, float delay);
    Eigen::VectorXd calculateDiffCoefficients(int o, char s = 'r');
    std::vector<std::vector<float>> Grid_resample(const std::vector<std::vector<float>>& dataArray,
        float target_spacing,
        float original_spacing);
    std::vector<std::vector<float>> addPmlToGrid(const std::vector<std::vector<float>>& grid_input, int npml);
    std::vector<std::vector<float>> transposeMatrix(const std::vector<std::vector<float>>& matrix);
    std::vector<std::vector<float>> concatMatrix2D(const std::vector<std::vector<float>>& d1, const std::vector<std::vector<float>>& d2, int flag);
}