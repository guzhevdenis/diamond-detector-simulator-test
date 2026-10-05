#ifndef AMPLIFIERMODELWIDGET_H
#define AMPLIFIERMODELWIDGET_H

#include <QWidget>
#include <vector>
#include <random>
#include <QPushButton>
#include <QFile>

class ValuesProvider;

QT_BEGIN_NAMESPACE
namespace Ui {
class AmplifierModelWidget;
}
QT_END_NAMESPACE

class AmplifierModelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AmplifierModelWidget(ValuesProvider* provider, QWidget *parent = nullptr);
    void setSpectrumAnalysisWidget(class SpectrumAnalysisWidget *widget);
    ~AmplifierModelWidget() override;

private slots:
    void onLoadCsv();
    void onRunAmplifier();
    void onResetZoom();
    void onExportAmpWlf();
    void onExportAmpCsv();

private:
    void plotSpectrum(class QChartView *view,
                      const std::vector<double> &centers,
                      const std::vector<double> &counts,
                      const QString &title);

    Ui::AmplifierModelWidget *ui;
    ValuesProvider* m_provider;

    std::vector<double> m_inputTimes;
    std::vector<double> m_inputSignal;
    double              m_inputSampleRateHz = 0.0;
    std::vector<double> m_outputSignal;

    class SpectrumAnalysisWidget *m_spectrumWidget = nullptr;

    std::mt19937        m_rng;

    void generateAmpWlfFile(QFile &file, QString wlfFileName, QString dataTxtFilePath);

    QPushButton *m_exportAmplifiedSigButton_Wlf;
    QPushButton *m_exportAmplifiedSigButton_Csv;

    std::vector<double> m_lastAmpSignal;
    double m_lastAmpSampleRateHz = 0.0;
};

#endif // AMPLIFIERMODELWIDGET_H

