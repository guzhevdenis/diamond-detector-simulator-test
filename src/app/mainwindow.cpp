#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "amplifiermodelwidget.h"
#include "template_loader.h"
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
    , m_templateLoader(new TemplateLoader)
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
    delete m_templateLoader;
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

    // Pulse template
    controlsLayout->addWidget(new QLabel(tr("Pulse template:"), this), row, 0);
    m_templatePathEdit = new QLineEdit(this);
    m_loadTemplateButton = new QPushButton(tr("Load Pulse Template"), this);
    controlsLayout->addWidget(m_templatePathEdit, row, 1);
    controlsLayout->addWidget(m_loadTemplateButton, row, 2);
    ++row;

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
    m_ampDistCombo->addItem(tr("From file (if loaded)")); // index 0
    m_ampDistCombo->addItem(tr("Normal"));                // index 1
    m_ampDistCombo->addItem(tr("Exponential"));           // index 2
    m_ampDistCombo->addItem(tr("Log-normal"));            // index 3
    controlsLayout->addWidget(m_ampDistCombo, row, 1);
    ++row;

    // Normal parameters
    controlsLayout->addWidget(new QLabel(tr("Normal mean / sigma:"), this), row, 0);
    m_normMeanSpin = new QDoubleSpinBox(this);
    m_normMeanSpin->setRange(-1e6, 1e6);
    m_normMeanSpin->setDecimals(6);
    m_normMeanSpin->setValue(1.0);
    m_normSigmaSpin = new QDoubleSpinBox(this);
    m_normSigmaSpin->setRange(1e-6, 1e6);
    m_normSigmaSpin->setDecimals(6);
    m_normSigmaSpin->setValue(0.1);
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
    m_durationSpin->setDecimals(6);
    m_durationSpin->setValue(1e-3);
    controlsLayout->addWidget(m_durationSpin, row, 1);
    ++row;

    // Sample rate
    controlsLayout->addWidget(new QLabel(tr("Sample rate [Hz]:"), this), row, 0);
    m_sampleRateSpin = new QDoubleSpinBox(this);
    m_sampleRateSpin->setRange(1.0, 1e12);
    m_sampleRateSpin->setDecimals(0);
    // The recorded template is sampled at 800 MHz
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

    // Offset
    controlsLayout->addWidget(new QLabel(tr("Offset:"), this), row, 0);
    m_offsetSpin = new QDoubleSpinBox(this);
    m_offsetSpin->setRange(-1e3, 1e3);
    m_offsetSpin->setDecimals(6);
    m_offsetSpin->setValue(0.0);
    controlsLayout->addWidget(m_offsetSpin, row, 1);
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

    genLayout->addLayout(controlsLayout);

    // Charts
    auto *chartsLayout = new QGridLayout();

    m_signalPlotWidget = new SignalPlotWidget(this);
    m_signalPlotWidget->setAxisLabels(tr("Time [ns]"), tr("Amplitude"));
    m_signalPlotWidget->setTitle(tr("Generated Signal"));

    m_templateChartView = new QChartView(new QChart(), this);
    m_spectrumChartView = new QChartView(new QChart(), this);

    m_templateChartView->chart()->setTitle(tr("Pulse Template"));
    m_spectrumChartView->chart()->setTitle(tr("Amplitude Spectrum"));

    // Enable rubber-band zoom for spectrum and template charts
    m_templateChartView->setRubberBand(QChartView::RectangleRubberBand);
    m_spectrumChartView->setRubberBand(QChartView::RectangleRubberBand);

    chartsLayout->addWidget(m_signalPlotWidget,   0, 0);
    chartsLayout->addWidget(m_templateChartView, 0, 1);
    chartsLayout->addWidget(m_spectrumChartView, 1, 0);

    genLayout->addLayout(chartsLayout);

    tabWidget->addTab(genPage, tr("Signal Generator"));

    // -------- Amplifier model tab --------
    auto *ampWidget = new AmplifierModelWidget(this);

    // -------- Spectrum analysis tab --------
    auto *spectrumWidget = new SpectrumAnalysisWidget(this);
    ampWidget->setSpectrumAnalysisWidget(spectrumWidget);

    tabWidget->addTab(ampWidget, tr("Amplifier Model"));
    tabWidget->addTab(spectrumWidget, tr("Spectrum Analysis"));

    connect(m_loadTemplateButton, &QPushButton::clicked,
            this, &MainWindow::onLoadPulseTemplate);
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

void MainWindow::onLoadPulseTemplate()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Select pulse template file"),
                QString(),
                tr("Text files (*.txt);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        m_templatePathEdit->setText(fileName);
        const auto &tpl = m_templateLoader->pulseTemplate(fileName);
        if (tpl.empty()) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to load pulse template."));
        } else {
            updateTemplateChart(tpl);
        }
    }
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
    xmlWriter.writeCharacters(QString::number(m_sampleRateSpin->value() * m_durationSpin->value(), 'f', 0));
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
    xmlWriter.writeCharacters(m_sampleRateSpin->text());
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

    QString path = m_templatePathEdit->text().trimmed();
    if (path.isEmpty()) {
        path = QStringLiteral("DiamondDetectorSignal_amplified.txt");
        m_templatePathEdit->setText(path);
    }

    const auto &pulse = m_templateLoader->pulseTemplate(path);
    if (pulse.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("Pulse template is empty or could not be loaded."));
        return;
    }

    updateTemplateChart(pulse);

    const double durationSec = m_durationSpin->value();
    const double rateHz = m_eventRateSpin->value();
    const double sampleRateHz = m_sampleRateSpin->value();
    const double noiseStdDev = m_noiseLevelSpin->value();
    const double offset = m_offsetSpin->value();
    
    std::unique_ptr<Distribution> dist;

    if (m_timeDistCombo->currentIndex() == 0) {
        dist = std::make_unique<FixedIntervalDistribution>();
    } else {
        dist = std::make_unique<PoissonDistribution>();
    }
    
    m_eventSampler->setDistribution(std::move(dist));

    const auto eventTimes = m_eventSampler->generateTimes(rateHz, durationSec, m_rng);
    if (eventTimes.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("No events generated. Check rate and duration."));
        return;
    }

    // Configure amplitude distribution every time:
    // - If "From file (if loaded)" is selected and a file-based spectrum
    //   (Samples or Histogram) is available, keep it.
    // - Otherwise, always apply the selected built-in distribution so that
    //   changed parameters are reflected every time Generate is pressed.
    const int ampMode = m_ampDistCombo->currentIndex();
    const auto specMode = m_amplitudeSpectrum->mode();
    const bool hasFileSpectrum =
            (specMode == AmplitudeSpectrum::Mode::Samples ||
             specMode == AmplitudeSpectrum::Mode::Histogram);

    if (ampMode == 0) {
        // From file (if loaded). If no valid file spectrum is present,
        // leave the current built-in configuration as-is.
        if (!hasFileSpectrum && !m_amplitudeSpectrum->hasData()) {
            // No spectrum at all: fall back to a simple normal distribution.
            m_amplitudeSpectrum->setNormal(m_normMeanSpin->value(), m_normSigmaSpin->value());
        }
    } else if (ampMode == 1) {
        m_amplitudeSpectrum->setNormal(m_normMeanSpin->value(), m_normSigmaSpin->value());
    } else if (ampMode == 2) {
        m_amplitudeSpectrum->setExponential(m_expLambdaSpin->value());
    } else {
        m_amplitudeSpectrum->setLogNormal(m_logMuSpin->value(), m_logSigmaSpin->value());
    }

    const auto signal = m_synthService->synthesize(
                pulse,
                eventTimes,
                *m_amplitudeSpectrum,
                durationSec,
                sampleRateHz,
                noiseStdDev,
                offset,
                m_rng);

    if (signal.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("Generated signal is empty."));
        return;
    }

    // Store last signal for CSV and WLF export
    m_lastSignal = signal;
    m_lastSampleRateHz = sampleRateHz;

    updateSignalChart(signal, sampleRateHz);
    updateAmplitudeSpectrumChart();

}

void MainWindow::updateAmplitudeSpectrumChart()
{
    QChart *chart = m_spectrumChartView->chart();
    chart->removeAllSeries();
    chart->removeAxis(chart->axisX());
    chart->removeAxis(chart->axisY());

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

void MainWindow::updateTemplateChart(const std::vector<double> &pulseTemplate)
{
    if (!m_templateChartView || !m_templateChartView->chart())
        return;

    QChart *chart = m_templateChartView->chart();
    chart->removeAllSeries();
    chart->removeAxis(chart->axisX());
    chart->removeAxis(chart->axisY());

    auto *series = new QLineSeries(chart);

    const std::size_t n = pulseTemplate.size();
    if (n == 0) {
        chart->setTitle(tr("Pulse Template"));
        return;
    }

    // Use time in ns based on known sample rate 800 MHz
    const double sampleRateHz = 8e8;
    const double dtNs = 1e9 / sampleRateHz; // 1.25 ns per sample

    for (std::size_t i = 0; i < n; ++i) {
        const double tNs = static_cast<double>(i) * dtNs;
        series->append(tNs, pulseTemplate[i]);
    }

    chart->addSeries(series);

    auto *axisX = new QValueAxis(chart);
    axisX->setTitleText(tr("Time [ns]"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis(chart);
    axisY->setTitleText(tr("Normalized amplitude"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->setTitle(tr("Pulse Template"));
}
