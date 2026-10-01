#pragma once
#include <array>
#include <algorithm>
#include <complex>
#include <cmath>
#include <numbers>

namespace SpectrumAnalyzer {
constexpr int Size = 2048, Bands = 32;
using Samples = std::array<float, Size>;
inline std::array<double, Bands> analyze(const Samples &samples, int sampleRate) {
    std::array<double, Bands> result{};
    if (sampleRate < 8000) return result;
    std::array<std::complex<double>, Size> bins{};
    double mean = 0;
    for (float sample : samples) mean += std::isfinite(sample) ? sample : 0;
    mean /= Size;
    for (int i=0; i<Size; ++i) {
        const double sample = std::isfinite(samples[i]) ? samples[i] : 0;
        bins[i] = (sample-mean) * (0.5-0.5*std::cos(2*std::numbers::pi*i/(Size-1)));
    }
    for (int i=1, j=0; i<Size; ++i) {
        int bit=Size>>1;
        for (; j&bit; bit>>=1) j^=bit;
        j^=bit; if (i<j) std::swap(bins[i],bins[j]);
    }
    for (int length=2; length<=Size; length*=2) {
        const auto step=std::polar(1.0,-2*std::numbers::pi/length);
        for (int i=0; i<Size; i+=length) {
            std::complex<double> weight=1;
            for (int j=0; j<length/2; ++j) {
                const auto a=bins[i+j], b=bins[i+j+length/2]*weight;
                bins[i+j]=a+b; bins[i+j+length/2]=a-b; weight*=step;
            }
        }
    }
    const double low=40, high=std::min(16000.0,sampleRate*0.48);
    for (int band=0; band<Bands; ++band) {
        const double from=low*std::pow(high/low,double(band)/Bands);
        const double to=low*std::pow(high/low,double(band+1)/Bands);
        const int first=std::clamp(int(std::ceil(from*Size/sampleRate)),1,Size/2-1);
        const int last=std::clamp(int(std::ceil(to*Size/sampleRate))-1,first,Size/2-1);
        double energy=0;
        for (int bin=first; bin<=last; ++bin) energy+=std::norm(bins[bin]);
        const double amplitude=std::sqrt(energy)/(Size/4.0);
        result[band]=amplitude < 0.000316 ? 0 : std::clamp((20*std::log10(amplitude)+70)/70,0.0,1.0);
    }
    return result;
}
}
