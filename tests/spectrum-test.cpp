#include "include/mediaspectrum.h"
#include "include/spectrumanalyzer.h"
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <limits>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    SpectrumAnalyzer::Samples samples{};
    for (double value : SpectrumAnalyzer::analyze(samples,48000)) if (value!=0) return 2;
    auto strongest=[&](double frequency) {
        for (int i=0;i<SpectrumAnalyzer::Size;++i) samples[i]=float(0.5*std::sin(2*std::numbers::pi*frequency*i/48000));
        const auto bands=SpectrumAnalyzer::analyze(samples,48000);
        for (double value : bands) if (!std::isfinite(value) || value<0 || value>1) return -1;
        return int(std::max_element(bands.begin(),bands.end())-bands.begin());
    };
    const int low=strongest(500), high=strongest(8000);
    if (low<0 || high<=low+5) return 3;
    samples.fill(std::numeric_limits<float>::quiet_NaN());
    for (double value : SpectrumAnalyzer::analyze(samples,48000)) if (value!=0) return 4;
    qInfo() << "SPECTRUM FFT PASS silence + 500Hz/8kHz bands + NaN + bounds";
    MediaSpectrum spectrum;
    spectrum.setRequested(true); spectrum.setEnabled(true);
    QTimer::singleShot(2200,&app,[&] {
        if (!spectrum.available() || spectrum.bands().size()!=SpectrumAnalyzer::Bands) {
            qCritical() << "SPECTRUM loopback failed" << spectrum.statusText(); app.exit(5); return;
        }
        spectrum.setEnabled(false);
        if (!spectrum.bands().isEmpty() || spectrum.available()) { app.exit(6); return; }
        qInfo() << "SPECTRUM LOOPBACK PASS real default endpoint + 32 bands + stop/clear (no test tone played)";
        app.quit();
    });
    return app.exec();
}
