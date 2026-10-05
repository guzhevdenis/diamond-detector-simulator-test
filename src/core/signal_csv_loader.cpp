#include "signal_csv_loader.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

bool SignalCsvLoader::load(const QString &path,
                           double fallbackSampleRateHz,
                           SignalCsvData &out,
                           QString &error) const
{
    out = SignalCsvData{};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QObject::tr("Cannot open file: %1").arg(path);
        return false;
    }

    QTextStream in(&file);
    bool headerSkipped = false;
    std::vector<double> times;
    std::vector<double> amplitudes;

    // Pre-reserve space based on a rough line-count estimate to reduce reallocations
    const qint64 fileSizeBytes = file.size();
    if (fileSizeBytes > 0) {
        // Assume an average of ~24 bytes per line (time,amplitude + newline)
        const std::size_t approxLines = static_cast<std::size_t>(fileSizeBytes / 24);
        if (approxLines > 0) {
            amplitudes.reserve(approxLines);
            times.reserve(approxLines);
        }
    }

    static const QRegularExpression separatorPattern(QStringLiteral("\\s*,\\s*|\\s+"));

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        const QStringList parts = line.split(separatorPattern, Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        // Try to detect and skip header
        if (!headerSkipped) {
            bool allNumeric = true;
            for (const QString &p : parts) {
                bool ok = false;
                p.toDouble(&ok);
                if (!ok) {
                    allNumeric = false;
                    break;
                }
            }
            if (!allNumeric) {
                headerSkipped = true;
                continue;
            }
            headerSkipped = true;
        }

        bool ok1 = false;
        bool ok2 = false;
        const double v1 = parts.at(0).toDouble(&ok1);
        if (!ok1)
            continue;

        if (parts.size() >= 2) {
            const double v2 = parts.at(1).toDouble(&ok2);
            if (ok2) {
                times.push_back(v1);
                amplitudes.push_back(v2);
            } else {
                amplitudes.push_back(v1);
            }
        } else {
            amplitudes.push_back(v1);
        }
    }

    if (amplitudes.empty()) {
        error = QObject::tr("No valid numeric samples found in file: %1").arg(path);
        return false;
    }

    out.amplitudes = std::move(amplitudes);

    if (!times.empty()) {
        out.times = std::move(times);
        // Infer sample rate from times (simple average dt)
        if (out.times.size() >= 2) {
            const std::size_t n = out.times.size();
            const double t0 = out.times.front();
            const double t1 = out.times.back();
            const double dt = (t1 - t0) / static_cast<double>(n - 1);
            if (dt > 0.0)
                out.sampleRateHz = 1.0 / dt;
        }
    } else {
        out.sampleRateHz = fallbackSampleRateHz;
    }

    if (out.sampleRateHz <= 0.0) {
        error = QObject::tr("Sample rate could not be determined. Please specify it explicitly.");
        return false;
    }

    return true;
}

