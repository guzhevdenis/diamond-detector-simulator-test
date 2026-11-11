#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "amplifiermodelwidget.h"
#include <cmath>
#include <stdexcept>
#include <QScrollArea>
#include "amplitude_spectrum.h"
#include "event_time_sampler.h"
#include "signal_synthesis_service.h"
#include "signal_plot_widget.h"
#include "spectrumanalysiswidget.h"

#include "Distribution.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QPrinter>

#include <QDateTime>
#include <QXmlStreamWriter>
#include <QXmlStreamAttribute>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_amplitudeSpectrum(new AmplitudeSpectrum)
    , m_eventSampler(new EventTimeSampler)
    , m_synthService(new SignalSynthesisService)
{
    ui->setupUi(this);

    std::random_device rd;
    m_rng.seed(rd());

    buildUi();
}

MainWindow::~MainWindow()
{
    delete m_amplitudeSpectrum;
    delete m_eventSampler;
    delete m_synthService;
    delete ui;
}

void MainWindow::buildUi()
{
    QWidget *central = ui->centralwidget;
    if (!central) {
        central = new QWidget(this);
        setCentralWidget(central);
    }

    auto *mainLayout = new QVBoxLayout(central);

    auto *tabWidget = new QTabWidget(central);
    mainLayout->addWidget(tabWidget);

    // -------- Signal generator tab --------
    auto *genPage = new QWidget(this);
    auto *genLayout = new QVBoxLayout(genPage);

    // Top controls
    auto *controlsLayout = new QGridLayout();

    int row = 0;

    // Specification v1.0: adjustable A, tau_r, tau_f, beta.
    auto addParameter = [&](const QString &label, double low, double high,
                            double value, int decimals) {
        auto *spin = new QDoubleSpinBox(this);
        spin->setDecimals(decimals);
        spin->setRange(low, high);
        spin->setValue(value);
        controlsLayout->addWidget(new QLabel(label, this), row, 0);
        controlsLayout->addWidget(spin, row++, 1);
        return spin;
    };
    m_pulseAmplitudeSpin = addParameter(tr("Pulse amplitude A:"), 0, 1e9, 12000, 3);
    m_riseTimeSpin = addParameter(tr("Rise constant tau_r [ns]:"), 0.001, 1e6, 6, 3);
    m_fallTimeSpin = addParameter(tr("Fall constant tau_f [ns]:"), 0.001, 1e6, 20, 3);
    m_betaSpin = addParameter(tr("Negative tail coefficient beta:"), 0, 1000, 0.08, 6);
    m_pretriggerSpin = addParameter(tr("Pre-trigger [ns]:"), 0, 1e9, 100, 3);
    controlsLayout->addWidget(new QLabel(tr("Fixed: tau_u = 100 ns; baseline = 0. A is the positive peak height."), this), row++, 0, 1, 3);

    // Amplitude spectrum
    controlsLayout->addWidget(new QLabel(tr("Amplitude spectrum:"), this), row, 0);
    m_spectrumPathEdit = new QLineEdit(this);
    m_loadSpectrumButton = new QPushButton(tr("Load Amplitude Spectrum"), this);
    controlsLayout->addWidget(m_spectrumPathEdit, row, 1);
    controlsLayout->addWidget(m_loadSpectrumButton, row, 2);
    ++row;

    // Time distribution
    controlsLayout->addWidget(new QLabel(tr("Time distribution:"), this), row, 0);
    m_timeDistCombo = new QComboBox(this);
    m_timeDistCombo->addItem(tr("Fixed Interval"));
    m_timeDistCombo->addItem(tr("Poisson"));
    controlsLayout->addWidget(m_timeDistCombo, row, 1);
    ++row;

    // Amplitude distribution
    controlsLayout->addWidget(new QLabel(tr("Amplitude distribution:"), this), row, 0);
    m_ampDistCombo = new QComboBox(this);
    m_ampDistCombo->addItem(tr("From file")); // index 0
    m_ampDistCombo->addItem(tr("Normal"));                // index 1
    m_ampDistCombo->addItem(tr("Exponential"));           // index 2
    m_ampDistCombo->addItem(tr("Log-normal"));            // index 3
    m_ampDistCombo->addItem(tr("Constant A (pulse model)")); // index 4
    m_ampDistCombo->setCurrentIndex(4);
    controlsLayout->addWidget(m_ampDistCombo, row, 1);
    ++row;

    // Normal parameters
    controlsLayout->addWidget(new QLabel(tr("Normal mean / sigma:"), this), row, 0);
    m_normMeanSpin = new QDoubleSpinBox(this);
    m_normMeanSpin->setRange(0, 1e9);
    m_normMeanSpin->setDecimals(6);
    m_normMeanSpin->setValue(12000);
    m_normSigmaSpin = new QDoubleSpinBox(this);
    m_normSigmaSpin->setRange(1e-6, 1e6);
    m_normSigmaSpin->setDecimals(6);
    m_normSigmaSpin->setValue(1200);
    controlsLayout->addWidget(m_normMeanSpin, row, 1);
    controlsLayout->addWidget(m_normSigmaSpin, row, 2);
    ++row;

    // Exponential parameter
    controlsLayout->addWidget(new QLabel(tr("Exponential lambda:"), this), row, 0);
    m_expLambdaSpin = new QDoubleSpinBox(this);
    m_expLambdaSpin->setRange(1e-6, 1e6);
    m_expLambdaSpin->setDecimals(6);
    m_expLambdaSpin->setValue(1.0);
    controlsLayout->addWidget(m_expLambdaSpin, row, 1);
    ++row;

    // Log-normal parameters
    controlsLayout->addWidget(new QLabel(tr("Log-normal mu / sigma:"), this), row, 0);
    m_logMuSpin = new QDoubleSpinBox(this);
    m_logMuSpin->setRange(-1e6, 1e6);
    m_logMuSpin->setDecimals(6);
    m_logMuSpin->setValue(0.0);
    m_logSigmaSpin = new QDoubleSpinBox(this);
    m_logSigmaSpin->setRange(1e-6, 1e6);
    m_logSigmaSpin->setDecimals(6);
    m_logSigmaSpin->setValue(0.25);
    controlsLayout->addWidget(m_logMuSpin, row, 1);
    controlsLayout->addWidget(m_logSigmaSpin, row, 2);
    ++row;

    // Event rate
    controlsLayout->addWidget(new QLabel(tr("Event rate [Hz]:"), this), row, 0);
    m_eventRateSpin = new QDoubleSpinBox(this);
    m_eventRateSpin->setRange(0.0, 1e9);
    m_eventRateSpin->setDecimals(3);
    m_eventRateSpin->setValue(1e6); // example default
    controlsLayout->addWidget(m_eventRateSpin, row, 1);
    ++row;

    // Duration
    controlsLayout->addWidget(new QLabel(tr("Duration [s]:"), this), row, 0);
    m_durationSpin = new QDoubleSpinBox(this);
    m_durationSpin->setRange(0.0, 10.0);
    m_durationSpin->setDecimals(9);
    m_durationSpin->setValue(1e-3);
    controlsLayout->addWidget(m_durationSpin, row, 1);
    ++row;

    // Sample rate
    controlsLayout->addWidget(new QLabel(tr("Sample rate [Hz]:"), this), row, 0);
    m_sampleRateSpin = new QDoubleSpinBox(this);
    m_sampleRateSpin->setRange(1.0, 1e12);
    m_sampleRateSpin->setDecimals(0);
    // Output sampling frequency is independent of model normalization.
    m_sampleRateSpin->setValue(8e8);
    controlsLayout->addWidget(m_sampleRateSpin, row, 1);
    ++row;

    // Noise level
    controlsLayout->addWidget(new QLabel(tr("Noise stddev:"), this), row, 0);
    m_noiseLevelSpin = new QDoubleSpinBox(this);
    m_noiseLevelSpin->setRange(0.0, 1e3);
    m_noiseLevelSpin->setDecimals(6);
    m_noiseLevelSpin->setValue(0.0);
    controlsLayout->addWidget(m_noiseLevelSpin, row, 1);
    ++row;

    // Random seed
    controlsLayout->addWidget(new QLabel(tr("Random seed:"), this), row, 0);
    m_seedSpin = new QSpinBox(this);
    m_seedSpin->setRange(0, std::numeric_limits<int>::max());
    m_seedSpin->setValue(12345);
    controlsLayout->addWidget(m_seedSpin, row, 1);
    ++row;

    // Generate / Reset zoom / Export CSV / Export WLF
    auto *exportButtonsLayout = new QHBoxLayout();

    m_generateButton = new QPushButton(tr("Generate"), this);
    m_resetZoomButton = new QPushButton(tr("Reset Signal Zoom"), this);
    m_exportCsvButton = new QPushButton(tr("Export Signal to CSV"), this);
    m_exportWlfButton = new QPushButton(tr("Export Signal to WLF"), this);
    controlsLayout->addWidget(m_generateButton, row, 0);
    exportButtonsLayout->addWidget(m_exportWlfButton, 1);
    exportButtonsLayout->addWidget(m_exportCsvButton, 1);
    controlsLayout->addWidget(m_resetZoomButton, row, 2);

    controlsLayout->addLayout(exportButtonsLayout, row, 1, 1, 1);

    auto *controlsWidget = new QWidget(this);
    controlsWidget->setLayout(controlsLayout);
    auto *controlsScroll = new QScrollArea(this);
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setWidget(controlsWidget);
    genLayout->addWidget(controlsScroll, 1);

    // Charts
    auto *chartsLayout = new QGridLayout();

    m_signalPlotWidget = new SignalPlotWidget(this);
    m_signalPlotWidget->setAxisLabels(tr("Time [ns]"), tr("Amplitude"));
    m_signalPlotWidget->setTitle(tr("Generated Signal"));

    m_pulseChartView = new QChartView(new QChart(), this);
    m_spectrumChartView = new QChartView(new QChart(), this);

    m_pulseChartView->chart()->setTitle(tr("Pulse Model"));
    m_spectrumChartView->chart()->setTitle(tr("Amplitude Spectrum"));

    // Enable rubber-band zoom for spectrum and pulse model charts
    m_pulseChartView->setRubberBand(QChartView::RectangleRubberBand);
    m_spectrumChartView->setRubberBand(QChartView::RectangleRubberBand);

    chartsLayout->addWidget(m_signalPlotWidget,   0, 0);
    chartsLayout->addWidget(m_pulseChartView, 0, 1);
    chartsLayout->addWidget(m_spectrumChartView, 1, 0);

    genLayout->addLayout(chartsLayout, 1);

    tabWidget->addTab(genPage, tr("Signal Generator"));

    // -------- Amplifier model tab --------
    auto *ampWidget = new AmplifierModelWidget(this);

    // -------- Spectrum analysis tab --------
    auto *spectrumWidget = new SpectrumAnalysisWidget(this);
    ampWidget->setSpectrumAnalysisWidget(spectrumWidget);

    tabWidget->addTab(ampWidget, tr("Amplifier Model"));
    tabWidget->addTab(spectrumWidget, tr("Spectrum Analysis"));

    for (auto *spin : {m_pulseAmplitudeSpin, m_riseTimeSpin, m_fallTimeSpin, m_betaSpin, m_pretriggerSpin, m_sampleRateSpin})
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this] { updatePulseChart(); });
    auto updateAmplitudeControls = [this] {
        const int mode = m_ampDistCombo->currentIndex();
        m_pulseAmplitudeSpin->setEnabled(mode == 4);
        m_normMeanSpin->setEnabled(mode == 1);
        m_normSigmaSpin->setEnabled(mode == 1);
        m_expLambdaSpin->setEnabled(mode == 2);
        m_logMuSpin->setEnabled(mode == 3);
        m_logSigmaSpin->setEnabled(mode == 3);
        m_spectrumPathEdit->setEnabled(mode == 0);
        updateAmplitudeSpectrumChart();
    };
    connect(m_ampDistCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [updateAmplitudeControls](int) { updateAmplitudeControls(); });
    m_pulseAmplitudeSpin->setToolTip(tr("Used for Constant A mode and the single-pulse preview. Distributions supply each event's A directly."));
    m_pretriggerSpin->setToolTip(tr("Leading baseline interval. The end of the array is reserved for tail decay."));
    m_generateButton->setToolTip(tr("Generates pulses from the analytic model; no pulse template file is needed."));
    m_exportCsvButton->setEnabled(false);
    m_exportWlfButton->setEnabled(false);
    updateAmplitudeControls();
    updatePulseChart();
    connect(m_loadSpectrumButton, &QPushButton::clicked,
            this, &MainWindow::onLoadAmplitudeSpectrum);
    connect(m_generateButton, &QPushButton::clicked,
            this, &MainWindow::onGenerate);
    connect(m_resetZoomButton, &QPushButton::clicked,
            this, &MainWindow::onResetSignalZoom);
    connect(m_exportCsvButton, &QPushButton::clicked,
            this, &MainWindow::onExportCsv);
    connect(m_exportWlfButton, &QPushButton::clicked,
            this, &MainWindow::onExportWlf);
}

double MainWindow::getDurationValue() const
{
    return m_durationSpin->value();
}

double MainWindow::getSampleRateValue() const
{
    return m_sampleRateSpin->value();
}

void MainWindow::onLoadAmplitudeSpectrum()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Select amplitude spectrum file"),
                QString(),
                tr("Text files (*.txt);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    m_spectrumPathEdit->setText(fileName);
    if (!m_amplitudeSpectrum->loadFromFile(fileName)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load amplitude spectrum."));
        return;
    }

    // Loaded spectrum overrides built-in modes
    m_ampDistCombo->setCurrentIndex(0);

    updateAmplitudeSpectrumChart();
}

void MainWindow::onResetSignalZoom()
{
    if (m_signalPlotWidget)
        m_signalPlotWidget->resetView();
}
void MainWindow::onExportCsv()
{
    if (m_lastSignal.empty() || m_lastSampleRateHz <= 0.0) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("No signal available. Please generate a signal first."));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
                this,
                tr("Export signal to CSV"),
                QStringLiteral("signal.csv"),
                tr("CSV files (*.csv);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("Cannot open file for writing."));
        return;
    }

    QTextStream out(&file);
    out << "time_s,amplitude\n";

    const double dt = 1.0 / m_lastSampleRateHz;
    const std::size_t n = m_lastSignal.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        out << QString::number(t, 'g', 16) << ','
            << QString::number(m_lastSignal[i], 'g', 16) << '\n';
    }
}

void MainWindow::generateWlfFile(QFile &file, QString wlfFileName, QString dataTxtFilePath) {
    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement("AWGSingleWaveform");
    xmlWriter.writeDefaultNamespace("urn:awgnet-schema");

    xmlWriter.writeStartElement("Waveform");

    xmlWriter.writeStartElement("Name");
    xmlWriter.writeCharacters(wlfFileName);
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Index");
    xmlWriter.writeCharacters("1");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Segment");

    xmlWriter.writeStartElement("Name");
    xmlWriter.writeCharacters("Segment1");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Index");
    xmlWriter.writeCharacters("0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Repetitions");
    xmlWriter.writeCharacters("1");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Duration");
    xmlWriter.writeCharacters(QString::number(static_cast<qulonglong>(m_lastSignal.size())));
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Component");

    xmlWriter.writeStartElement("Name");
    xmlWriter.writeCharacters("Component1");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Index");
    xmlWriter.writeCharacters("0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("ComponentSignal");
    xmlWriter.writeCharacters("13");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Operation");
    xmlWriter.writeCharacters("0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Parameters");

    xmlWriter.writeStartElement("Parameter");
    xmlWriter.writeCharacters("0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Value");
    xmlWriter.writeCharacters(QString::number(m_lastSampleRateHz, 'g', 16));
    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement(); //close Parameters

    xmlWriter.writeStartElement("Parameters");

    xmlWriter.writeStartElement("Parameter");
    xmlWriter.writeCharacters("16");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Value");
    xmlWriter.writeCharacters(dataTxtFilePath);
    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement(); //close Parameters

    xmlWriter.writeEndElement(); //close Component

    xmlWriter.writeEndElement(); //close Segment

    xmlWriter.writeStartElement("FilterAndNoise");
    xmlWriter.writeCharacters("0");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("WaveformMode");
    xmlWriter.writeCharacters("1");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndElement(); //close Waveform
    xmlWriter.writeEndElement(); //close AWG...
    xmlWriter.writeEndDocument();
}

void MainWindow::onExportWlf()
{
    if (m_lastSignal.empty() || m_lastSampleRateHz <= 0.0) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("No signal available. Please generate a signal first."));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export signal to WLF"),
        QStringLiteral("Waveform.wlf"),
        tr("Waveform Library File (*.wlf);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("Cannot open file for writing."));
        return;
    }

    QFileInfo wlfFileFolder(fileName);
    QString wlfFilePath = wlfFileFolder.absolutePath();
    QString wlfFileName = wlfFileFolder.baseName();

    QString dateTimeStr = QDateTime::currentDateTime().toString("ddMMyyyy-hhmmss");

    QFile dataTxtFile(wlfFilePath + "/" + dateTimeStr + "-data.txt");
    if (!dataTxtFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("Cannot open data txt file for writing."));
        return;
    }

    QTextStream outData(&dataTxtFile);

    const std::size_t n = m_lastSignal.size();
    for (std::size_t i = 0; i < n; ++i) {
        outData << QString::number(m_lastSignal[i], 'g', 16) << '\n';
    }

    QFileInfo dataTxtFileInfo(dataTxtFile);
    QString dataTxtFilePath = dataTxtFileInfo.absoluteFilePath();
    dataTxtFilePath = QDir::toNativeSeparators(dataTxtFilePath);

    dataTxtFile.close();

    generateWlfFile(file, wlfFileName, dataTxtFilePath);

    file.close();
}

void MainWindow::onGenerate()
{
    m_rng.seed(static_cast<std::mt19937::result_type>(m_seedSpin->value()));

    try {
        const PulseModel pulse(pulseParameters());
        updatePulseChart();

        const double durationSec = m_durationSpin->value();
        const double rateHz = m_eventRateSpin->value();
        const double sampleRateHz = m_sampleRateSpin->value();
        const double noiseStdDev = m_noiseLevelSpin->value();
        if (durationSec * sampleRateHz > 20000000)
            throw std::invalid_argument("Signal exceeds the limit of 20 million samples.");
        const double pretriggerSec = m_pretriggerSpin->value() * 1e-9;
        const double eventWindow = durationSec - pretriggerSec - pulse.settlingTimeNs() * 1e-9;
        if (eventWindow <= 0)
            throw std::invalid_argument("Increase duration to include the pre-trigger and pulse tail.");
        if (rateHz * eventWindow > 1000000)
            throw std::invalid_argument("Too many events: reduce the rate or duration.");
    
        std::unique_ptr<Distribution> dist;

        if (m_timeDistCombo->currentIndex() == 0) {
            dist = std::make_unique<FixedIntervalDistribution>();
        } else {
            dist = std::make_unique<PoissonDistribution>();
        }
    
        m_eventSampler->setDistribution(std::move(dist));

        auto eventTimes = m_eventSampler->generateTimes(rateHz, eventWindow, m_rng);
        for (double &t : eventTimes) t += pretriggerSec;

        const int ampMode = m_ampDistCombo->currentIndex();
        if (ampMode == 0) {
            // Reload the selected spectrum after switching back from built-in modes.
            if (!m_amplitudeSpectrum->loadFromFile(m_spectrumPathEdit->text().trimmed()))
                throw std::invalid_argument("Load a valid amplitude spectrum or select Constant A.");
        } else if (ampMode == 1) {
            m_amplitudeSpectrum->setNormal(m_normMeanSpin->value(), m_normSigmaSpin->value());
        } else if (ampMode == 2) {
            m_amplitudeSpectrum->setExponential(m_expLambdaSpin->value());
        } else if (ampMode == 3) {
            m_amplitudeSpectrum->setLogNormal(m_logMuSpin->value(), m_logSigmaSpin->value());
        }
        std::function<double(std::mt19937 &)> sampler;
        if (ampMode != 4)
            sampler = [this](std::mt19937 &rng) { return m_amplitudeSpectrum->sample(rng); };
        const auto signal = m_synthService->synthesize(pulse, eventTimes, durationSec,
                                                    sampleRateHz, noiseStdDev, m_rng, sampler);

        if (signal.empty()) {
            QMessageBox::warning(this, tr("Error"), tr("Generated signal is empty."));
            return;
        }

        // Store last signal for CSV and WLF export
        m_lastSignal = signal;
        m_lastSampleRateHz = sampleRateHz;
        m_exportCsvButton->setEnabled(true);
        m_exportWlfButton->setEnabled(true);

        updateSignalChart(signal, sampleRateHz);
        updateAmplitudeSpectrumChart();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Generation error"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::updateAmplitudeSpectrumChart()
{
    QChart *chart = m_spectrumChartView->chart();
    chart->removeAllSeries();
    for (auto *axis : chart->axes()) { chart->removeAxis(axis); delete axis; }

    if (m_ampDistCombo->currentIndex() == 4) {
        chart->setTitle(tr("Constant amplitude A = %1").arg(m_pulseAmplitudeSpin->value()));
        return;
    }
    if (!m_amplitudeSpectrum->hasData()) {
        chart->setTitle(tr("Amplitude Spectrum (built-in or default)"));
        return;
    }

    auto *series = new QLineSeries(chart);

    if (m_amplitudeSpectrum->mode() == AmplitudeSpectrum::Mode::Histogram) {
        const auto &amps = m_amplitudeSpectrum->histAmplitudes();
        const auto &weights = m_amplitudeSpectrum->histWeights();
        for (std::size_t i = 0; i < amps.size(); ++i) {
            series->append(amps[i], weights[i]);
        }
    } else if (m_amplitudeSpectrum->mode() == AmplitudeSpectrum::Mode::Samples) {
        std::vector<double> centers;
        std::vector<double> counts;
        m_amplitudeSpectrum->histogramFromSamples(32, centers, counts);
        for (std::size_t i = 0; i < centers.size(); ++i) {
            series->append(centers[i], counts[i]);
        }
    } else {
        // Built-in distributions: approximate with histogram from sampled values
        const std::size_t sampleCount = 1000;
        std::vector<double> samples;
        samples.reserve(sampleCount);
        std::mt19937 tmp(static_cast<std::mt19937::result_type>(m_seedSpin->value()));
        for (std::size_t i = 0; i < sampleCount; ++i) {
            samples.push_back(m_amplitudeSpectrum->sample(tmp));
        }

        if (!samples.empty()) {
            const auto [minIt, maxIt] = std::minmax_element(samples.begin(), samples.end());
            const double minA = *minIt;
            const double maxA = *maxIt;
            if (minA != maxA) {
                const std::size_t binCount = 32;
                const double width = (maxA - minA) / static_cast<double>(binCount);
                std::vector<double> counts(binCount, 0.0);
                for (double a : samples) {
                    std::size_t idx = static_cast<std::size_t>((a - minA) / width);
                    if (idx >= binCount)
                        idx = binCount - 1;
                    counts[idx] += 1.0;
                }
                for (std::size_t i = 0; i < binCount; ++i) {
                    const double center = minA + (i + 0.5) * width;
                    series->append(center, counts[i]);
                }
            }
        }
    }

    chart->addSeries(series);

    auto *axisX = new QValueAxis(chart);
    axisX->setTitleText(tr("Amplitude"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis(chart);
    axisY->setTitleText(tr("Weight / count"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->setTitle(tr("Amplitude Spectrum"));
}


void MainWindow::updateSignalChart(const std::vector<double> &signal,
                                   double sampleRateHz)
{
    if (m_signalPlotWidget)
        m_signalPlotWidget->setSignal(signal, sampleRateHz);
}

PulseParameters MainWindow::pulseParameters() const
{
    return {m_pulseAmplitudeSpin->value(), m_riseTimeSpin->value(),
            m_fallTimeSpin->value(), m_betaSpin->value()};
}

void MainWindow::updatePulseChart()
{
    QChart *chart = m_pulseChartView->chart();
    chart->removeAllSeries();
    for (auto *axis : chart->axes()) { chart->removeAxis(axis); delete axis; }
    try {
        const PulseModel pulse(pulseParameters());
        auto *series = new QLineSeries(chart);
        const double pre = m_pretriggerSpin->value();
        // Preview uses the output time grid; cap displayed points for responsiveness.
        const double fs = m_sampleRateSpin->value();
        const double endNs = pre + pulse.settlingTimeNs();
        const double count = std::ceil(endNs * fs * 1e-9) + 1;
        const double stride = std::max(1.0, std::ceil(count / 10000.0));
        for (double k = 0; k < count; k += stride) {
            const double t = k / fs * 1e9;
            series->append(t, pulse.value(t - pre));
        }
        chart->addSeries(series);
        auto *axisX = new QValueAxis(chart);
        axisX->setTitleText(tr("Time [ns]"));
        chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        auto *axisY = new QValueAxis(chart);
        axisY->setTitleText(tr("Amplitude"));
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        chart->setTitle(tr("Pulse Model (A = %1, tau_u = 100 ns)").arg(pulse.parameters().amplitude));
    } catch (const std::exception &e) {
        chart->setTitle(QString::fromUtf8(e.what()));
    }
}
