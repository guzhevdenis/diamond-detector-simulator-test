#ifndef SIGNAL_CSV_LOADER_H
#define SIGNAL_CSV_LOADER_H

#include <vector>
#include <QString>

struct SignalCsvData
{
    std::vector<double> times;      // may be empty if not available
    std::vector<double> amplitudes; // required
    double sampleRateHz = 0.0;      // inferred or provided
};

class SignalCsvLoader
{
public:
    SignalCsvLoader() = default;

    bool load(const QString &path,
              double fallbackSampleRateHz,
              SignalCsvData &out,
              QString &error) const;
};

#endif // SIGNAL_CSV_LOADER_H

