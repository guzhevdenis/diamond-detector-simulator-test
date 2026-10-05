#ifndef AMPLITUDE_SPECTRUM_H
#define AMPLITUDE_SPECTRUM_H

#include <vector>
#include <QString>
#include <random>

class AmplitudeSpectrum
{
public:
    enum class Mode {
        None,
        Samples,
        Histogram,
        Normal,
        Exponential,
        LogNormal
    };

    AmplitudeSpectrum() = default;

    bool loadFromFile(const QString &path);

    double sample(std::mt19937 &rng) const;

    bool hasData() const { return m_mode != Mode::None; }
    Mode mode() const { return m_mode; }

    // Built-in distributions
    void setNormal(double mean, double sigma);
    void setExponential(double lambda);
    void setLogNormal(double mu, double sigma);

    // For visualization
    void histogramFromSamples(std::size_t binCount,
                              std::vector<double> &binCenters,
                              std::vector<double> &binCounts) const;

    const std::vector<double> &histAmplitudes() const { return m_histAmplitudes; }
    const std::vector<double> &histWeights() const { return m_histWeights; }

private:
    Mode m_mode = Mode::None;

    // Samples mode
    std::vector<double> m_samples;

    // Histogram mode
    std::vector<double> m_histAmplitudes;
    std::vector<double> m_histWeights;
    std::vector<double> m_histCdf;

    // Built-in parameters
    double m_normalMean = 1.0;
    double m_normalSigma = 0.1;

    double m_expLambda = 1.0;

    double m_logMu = 0.0;
    double m_logSigma = 0.25;
};

#endif // AMPLITUDE_SPECTRUM_H

