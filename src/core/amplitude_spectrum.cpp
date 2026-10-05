#include "amplitude_spectrum.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <QRegularExpression>

bool AmplitudeSpectrum::loadFromFile(const QString &path)
{
    m_mode = Mode::None;
    m_samples.clear();
    m_histAmplitudes.clear();
    m_histWeights.clear();
    m_histCdf.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "AmplitudeSpectrum: cannot open" << path;
        return false;
    }

    QTextStream in(&file);
    bool decidedMode = false;
    bool firstLine = true;

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        if (firstLine) {
            if (parts.size() >= 2) {
                m_mode = Mode::Histogram;
            } else {
                m_mode = Mode::Samples;
            }
            decidedMode = true;
            firstLine = false;
        }

        if (!decidedMode) {
            continue;
        }

        if (m_mode == Mode::Samples) {
            bool ok = false;
            const double a = parts.at(0).toDouble(&ok);
            if (ok)
                m_samples.push_back(a);
        } else {
            bool okA = false;
            bool okW = false;
            const double a = parts.at(0).toDouble(&okA);
            const double w = parts.at(1).toDouble(&okW);
            if (okA && okW && w > 0.0) {
                m_histAmplitudes.push_back(a);
                m_histWeights.push_back(w);
            }
        }
    }

    if (m_mode == Mode::Samples && m_samples.empty()) {
        qWarning() << "AmplitudeSpectrum: no valid samples in" << path;
        m_mode = Mode::None;
        return false;
    }

    if (m_mode == Mode::Histogram && (m_histAmplitudes.empty() || m_histWeights.empty())) {
        qWarning() << "AmplitudeSpectrum: no valid histogram data in" << path;
        m_mode = Mode::None;
        return false;
    }

    if (m_mode == Mode::Histogram) {
        m_histCdf.resize(m_histWeights.size());
        double sum = 0.0;
        for (std::size_t i = 0; i < m_histWeights.size(); ++i) {
            sum += m_histWeights[i];
            m_histCdf[i] = sum;
        }
        if (sum <= 0.0) {
            qWarning() << "AmplitudeSpectrum: histogram weights sum to zero";
            m_mode = Mode::None;
            return false;
        }
        for (double &v : m_histCdf) {
            v /= sum;
        }
    }

    return true;
}

double AmplitudeSpectrum::sample(std::mt19937 &rng) const
{
    // File-based modes
    if (m_mode == Mode::Samples) {
        if (m_samples.empty())
            return 1.0;
        std::uniform_int_distribution<std::size_t> dist(0, m_samples.size() - 1);
        return m_samples[dist(rng)];
    }

    if (m_mode == Mode::Histogram) {
        if (m_histAmplitudes.empty() || m_histCdf.empty())
            return 1.0;
        std::uniform_real_distribution<double> u(0.0, 1.0);
        const double x = u(rng);
        auto it = std::lower_bound(m_histCdf.begin(), m_histCdf.end(), x);
        const std::size_t idx = static_cast<std::size_t>(std::distance(m_histCdf.begin(), it));
        if (idx >= m_histAmplitudes.size())
            return m_histAmplitudes.back();
        return m_histAmplitudes[idx];
    }

    // Built-in modes
    if (m_mode == Mode::Normal) {
        std::normal_distribution<double> d(m_normalMean, m_normalSigma);
        return d(rng);
    }

    if (m_mode == Mode::Exponential) {
        if (m_expLambda <= 0.0)
            return 0.0;
        std::exponential_distribution<double> d(m_expLambda);
        return d(rng);
    }

    if (m_mode == Mode::LogNormal) {
        std::lognormal_distribution<double> d(m_logMu, m_logSigma);
        return d(rng);
    }

    return 1.0;
}

void AmplitudeSpectrum::setNormal(double mean, double sigma)
{
    m_mode = Mode::Normal;
    m_normalMean = mean;
    m_normalSigma = sigma > 0.0 ? sigma : 1e-6;
}

void AmplitudeSpectrum::setExponential(double lambda)
{
    m_mode = Mode::Exponential;
    m_expLambda = lambda > 0.0 ? lambda : 1e-6;
}

void AmplitudeSpectrum::setLogNormal(double mu, double sigma)
{
    m_mode = Mode::LogNormal;
    m_logMu = mu;
    m_logSigma = sigma > 0.0 ? sigma : 1e-6;
}

void AmplitudeSpectrum::histogramFromSamples(std::size_t binCount,
                                             std::vector<double> &binCenters,
                                             std::vector<double> &binCounts) const
{
    binCenters.clear();
    binCounts.clear();

    if (m_mode != Mode::Samples || m_samples.empty() || binCount == 0)
        return;

    const auto [minIt, maxIt] = std::minmax_element(m_samples.begin(), m_samples.end());
    const double minA = *minIt;
    const double maxA = *maxIt;
    if (minA == maxA)
        return;

    const double width = (maxA - minA) / static_cast<double>(binCount);
    binCenters.resize(binCount);
    binCounts.assign(binCount, 0.0);

    for (std::size_t i = 0; i < binCount; ++i) {
        const double center = minA + (i + 0.5) * width;
        binCenters[i] = center;
    }

    for (double a : m_samples) {
        std::size_t idx = static_cast<std::size_t>((a - minA) / width);
        if (idx >= binCount)
            idx = binCount - 1;
        binCounts[idx] += 1.0;
    }
}

