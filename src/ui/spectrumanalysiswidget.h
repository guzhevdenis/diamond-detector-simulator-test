#ifndef SPECTRUMANALYSISWIDGET_H
#define SPECTRUMANALYSISWIDGET_H

#include <QWidget>

class QChartView;
class SignalPlotWidget;
class QCheckBox;

struct AmplitudeSpectrumResult;

class SpectrumAnalysisWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumAnalysisWidget(QWidget *parent = nullptr);
    ~SpectrumAnalysisWidget() override;

    void updateSpectra(const AmplitudeSpectrumResult &input,
                       const AmplitudeSpectrumResult &output);

private:
    void plotSpectrum(QChartView *view,
                      const std::vector<double> &centers,
                      const std::vector<double> &counts,
                      const QString &title);

    QChartView *m_inputSpectrumChartView = nullptr;
    QChartView *m_outputSpectrumChartView = nullptr;
    SignalPlotWidget *m_combinedSpectrumPlot = nullptr;

    QCheckBox *m_logXCheck = nullptr;
    QCheckBox *m_logYCheck = nullptr;
};

#endif // SPECTRUMANALYSISWIDGET_H

