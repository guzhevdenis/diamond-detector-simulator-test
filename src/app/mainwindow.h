#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <random>
#include <vector>
#include <memory>
#include <QtCharts>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>

#include "values_provider.h"
#include "pulse_model.h"

class SignalPlotWidget;

class QAction;
class QLineEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QChartView;
class QCheckBox;
class AmplifierModelWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class AmplitudeSpectrum;
class EventTimeSampler;
class SignalSynthesisService;

class MainWindow : public QMainWindow, public ValuesProvider
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    double getDurationValue() const override;
    double getSampleRateValue() const override;

    ~MainWindow() override;

private slots:
    void onLoadAmplitudeSpectrum();
    void onGenerate();
    void onResetSignalZoom();
    void onExportCsv();
    void onExportWlf();

private:
    void buildUi();
    void updateAmplitudeSpectrumChart();
    void updateSignalChart(std::shared_ptr<const std::vector<double>> signal, double sampleRateHz);
    void updatePulseChart();
    PulseParameters pulseParameters() const;

    void generateWlfFile(QFile &file, QString wlfFileName, QString dataTxtFilePath);

    Ui::MainWindow *ui;

    AmplitudeSpectrum     *m_amplitudeSpectrum   = nullptr;
    EventTimeSampler      *m_eventSampler        = nullptr;
    SignalSynthesisService *m_synthService       = nullptr;

    std::mt19937 m_rng;

    // Controls
    QDoubleSpinBox *m_pulseAmplitudeSpin = nullptr;
    QDoubleSpinBox *m_riseTimeSpin = nullptr;
    QDoubleSpinBox *m_fallTimeSpin = nullptr;
    QDoubleSpinBox *m_betaSpin = nullptr;
    QDoubleSpinBox *m_pretriggerSpin = nullptr;

    QLineEdit   *m_spectrumPathEdit   = nullptr;
    QPushButton *m_loadSpectrumButton = nullptr;

    QComboBox   *m_timeDistCombo      = nullptr;
    QDoubleSpinBox *m_eventRateSpin   = nullptr;
    QDoubleSpinBox *m_durationSpin    = nullptr;
    QDoubleSpinBox *m_sampleRateSpin  = nullptr;
    QDoubleSpinBox *m_noiseLevelSpin  = nullptr;
    QSpinBox       *m_seedSpin        = nullptr;

    QPushButton *m_generateButton     = nullptr;
    QPushButton *m_resetZoomButton    = nullptr;
    QAction     *m_exportCsvAction    = nullptr;
    QAction     *m_exportWlfAction    = nullptr;

    // Amplitude distribution (built-in)
    QComboBox      *m_ampDistCombo    = nullptr;
    QDoubleSpinBox *m_normMeanSpin    = nullptr;
    QDoubleSpinBox *m_normSigmaSpin   = nullptr;
    QDoubleSpinBox *m_expLambdaSpin   = nullptr;
    QDoubleSpinBox *m_logMuSpin       = nullptr;
    QDoubleSpinBox *m_logSigmaSpin    = nullptr;

    // Charts
    SignalPlotWidget *m_signalPlotWidget = nullptr;
    SignalPlotWidget *m_pulsePlotWidget = nullptr;
    QChartView *m_spectrumChartView  = nullptr;

    // Last generated signal (for CSV and WLF export)
    std::shared_ptr<const std::vector<double>> m_lastSignal = std::make_shared<const std::vector<double>>();
    double m_lastSampleRateHz = 0.0;
};
#endif // MAINWINDOW_H
