#ifndef SIGNAL_PLOT_WIDGET_H
#define SIGNAL_PLOT_WIDGET_H

#include <QWidget>
#include <vector>
#include <memory>

class QCustomPlot;
class QCPGraph;
class QCPRange;

enum class SignalPlotMode {
    TimeSignal,
    SpectrumSingle,
    SpectrumCombined
};

class SignalPlotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalPlotWidget(QWidget *parent = nullptr);
    ~SignalPlotWidget() override;

    // Load full signal (uniformly sampled) into the viewer.
    void setSignal(const std::vector<double> &samples, double sampleRateHz);
    // Share immutable generated data with the exporter instead of duplicating it.
    void setSharedSignal(std::shared_ptr<const std::vector<double>> samples, double sampleRateHz);

    // Load signal together with explicit time stamps (in seconds).
    // Used when non-trivial timing comes from data files.
    void setSignalWithTimes(const std::vector<double> &timesSeconds,
                            const std::vector<double> &samples);

    // Load amplitude spectrum data (e.g. histogram: centers & counts).
    // X values are used as-is; no time-axis logic is applied here.
    void setSpectrum(const std::vector<double> &centers,
                     const std::vector<double> &counts);

    // Load two spectra at once for overlay (e.g. input vs output).
    void setCombinedSpectrum(const std::vector<double> &centersInput,
                             const std::vector<double> &countsInput,
                             const std::vector<double> &centersOutput,
                             const std::vector<double> &countsOutput);

    // Enable/disable logarithmic scale on Y axis in spectrum modes.
    void setSpectrumLogY(bool enabled);
    // Enable/disable logarithmic scale on X axis in spectrum modes.
    void setSpectrumLogX(bool enabled);

    void clear();

    void setAxisLabels(const QString &xLabel, const QString &yLabel);
    void setTitle(const QString &title);

    // Reset zoom to show the full signal range.
    void resetView();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onXAxisRangeChanged(const QCPRange &newRange, const QCPRange &oldRange);

private:
    void rebuildVisibleData();
    void updateFullRange();

    void buildMinMaxEnvelope(double tMinNs,
                             double tMaxNs,
                             int pixelWidth,
                             QVector<double> &x,
                             QVector<double> &y) const;

    QCustomPlot *m_plot = nullptr;
    QCPGraph    *m_graph = nullptr;
    QCPGraph    *m_graph2 = nullptr; // used only for combined spectrum

    SignalPlotMode m_mode = SignalPlotMode::TimeSignal;

    std::shared_ptr<const std::vector<double>> m_signal = std::make_shared<const std::vector<double>>();
    double              m_sampleRateHz = 0.0;

    // Optional explicit time axis (seconds). If empty, uniform sampling is used.
    std::vector<double> m_timesSeconds;

    // Spectrum data (amplitude vs count or similar)
    QVector<double> m_specX;
    QVector<double> m_specY;
    bool            m_hasSpectrum = false;
    double          m_specMinX = 0.0;
    double          m_specMaxX = 0.0;
    double          m_specMinY = 0.0;
    double          m_specMaxY = 0.0;

    // Combined spectra (input/output)
    QVector<double> m_specInX;
    QVector<double> m_specInY;
    QVector<double> m_specOutX;
    QVector<double> m_specOutY;
    bool            m_hasCombinedSpectrum = false;

    bool            m_logSpectrumY = false;
    bool            m_logSpectrumX = false;

    // Cached full-range in time (ns)
    double m_fullMinTimeNs = 0.0;
    double m_fullMaxTimeNs = 0.0;

    bool m_updating = false;
};

#endif // SIGNAL_PLOT_WIDGET_H

