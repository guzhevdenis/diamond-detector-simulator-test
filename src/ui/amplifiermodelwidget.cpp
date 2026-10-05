#include "amplifiermodelwidget.h"
#include "ui_amplifiermodelwidget.h"

#include "amplifier_model.h"
#include "signal_csv_loader.h"
#include "amplitude_spectrum_analyzer.h"
#include "signal_plot_widget.h"
#include "spectrumanalysiswidget.h"
#include "values_provider.h"

#include <QFileDialog>
#include <QMessageBox>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtConcurrent>


AmplifierModelWidget::AmplifierModelWidget(ValuesProvider* provider, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AmplifierModelWidget)
    , m_provider(provider)
{
    ui->setupUi(this);

    // Make the charts area grow vertically with the window.
    // Controls group keeps its preferred height, charts take remaining space.
    if (ui->verticalLayout) {
        ui->verticalLayout->setStretch(0, 0); // groupBoxControls
        ui->verticalLayout->setStretch(1, 1); // gridLayoutCharts
    }

    std::random_device rd;
    m_rng.seed(rd());

    // Set initial chart titles and zoom mode
    ui->inputChartView->setAxisLabels(tr("Time [ns]"), tr("Amplitude"));
    ui->inputChartView->setTitle(tr("Input Signal"));
    ui->outputChartView->setAxisLabels(tr("Time [ns]"), tr("Amplitude"));
    ui->outputChartView->setTitle(tr("Output Signal"));

    m_exportAmplifiedSigButton_Wlf = new QPushButton(tr("Export signal to WLF"), this);
    m_exportAmplifiedSigButton_Csv = new QPushButton(tr("Export signal to CSV"), this);

    connect(ui->loadCsvButton, &QPushButton::clicked,
            this, &AmplifierModelWidget::onLoadCsv);
    connect(ui->runButton, &QPushButton::clicked,
            this, &AmplifierModelWidget::onRunAmplifier);
    connect(ui->resetZoomButton, &QPushButton::clicked,
            this, &AmplifierModelWidget::onResetZoom);
    connect(m_exportAmplifiedSigButton_Wlf, &QPushButton::clicked,
            this, &AmplifierModelWidget::onExportAmpWlf);
    connect(m_exportAmplifiedSigButton_Csv, &QPushButton::clicked,
            this, &AmplifierModelWidget::onExportAmpCsv);

    auto *exportAmpButtonsLayout = new QHBoxLayout();

    exportAmpButtonsLayout->addWidget(m_exportAmplifiedSigButton_Wlf, 1);
    exportAmpButtonsLayout->addWidget(m_exportAmplifiedSigButton_Csv, 1);
    ui->gridLayoutControls->addLayout(exportAmpButtonsLayout, 14, 1, 1, 1);

    // We no longer show spectra in this widget; they are displayed in the
    // dedicated "Spectrum Analysis" tab. Remove spectrum charts from layout.
    if (ui->gridLayoutCharts) {
        ui->gridLayoutCharts->removeWidget(ui->inputSpectrumChartView);
        ui->gridLayoutCharts->removeWidget(ui->outputSpectrumChartView);
    }
    ui->inputSpectrumChartView->deleteLater();
    ui->outputSpectrumChartView->deleteLater();
}

AmplifierModelWidget::~AmplifierModelWidget()
{
    delete ui;
}

void AmplifierModelWidget::setSpectrumAnalysisWidget(SpectrumAnalysisWidget *widget)
{
    m_spectrumWidget = widget;
}

void AmplifierModelWidget::onLoadCsv()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("Select input signal CSV"),
                QString(),
                tr("CSV files (*.csv);;All files (*.*)"));
    if (fileName.isEmpty())
        return;

    ui->inputPathEdit->setText(fileName);

    // Run CSV loading in a worker thread to keep UI responsive
    SignalCsvLoader loader;
    const double fallbackRate = ui->inputSampleRateSpin->value();

    setEnabled(false);

    auto future = QtConcurrent::run([loader, fileName, fallbackRate]() {
        SignalCsvData data;
        QString error;
        const bool ok = loader.load(fileName, fallbackRate, data, error);
        return std::make_pair(ok, std::make_pair(data, error));
    });

    auto *watcher = new QFutureWatcher<std::pair<bool, std::pair<SignalCsvData, QString>>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const auto result = watcher->future().result();
        watcher->deleteLater();
        setEnabled(true);

        const bool ok = result.first;
        const SignalCsvData &data = result.second.first;
        const QString &error = result.second.second;

        if (!ok) {
            QMessageBox::warning(this, tr("Load error"), error);
            return;
        }

        m_inputSignal = data.amplitudes;
        m_inputTimes = data.times;
        m_inputSampleRateHz = data.sampleRateHz;
        ui->inputSampleRateSpin->setValue(m_inputSampleRateHz);

        if (m_inputTimes.empty()) {
            const std::size_t n = m_inputSignal.size();
            m_inputTimes.resize(n);
            const double dt = 1.0 / m_inputSampleRateHz;
            for (std::size_t i = 0; i < n; ++i)
                m_inputTimes[i] = static_cast<double>(i) * dt;
        }

        ui->inputChartView->setSignalWithTimes(m_inputTimes, m_inputSignal);
    });

    watcher->setFuture(future);
}

void AmplifierModelWidget::onRunAmplifier()
{
    if (m_inputSignal.empty() || m_inputSampleRateHz <= 0.0) {
        QMessageBox::warning(this, tr("Amplifier error"),
                             tr("No input signal loaded. Please load a CSV first."));
        return;
    }

    AmplifierModelConfig cfg;
    cfg.gain1 = ui->gain1Spin->value();
    cfg.gain2 = ui->gain2Spin->value();
    cfg.tauCrNs = ui->tauCrSpinNs->value();
    cfg.tauRcNs = ui->tauRcSpinNs->value();
    cfg.rcOrder = ui->rcOrderSpin->value();
    cfg.noiseSigma = ui->noiseSigmaSpin->value();
    cfg.saturationEnabled = ui->saturationCheck->isChecked();
    cfg.saturationMin = ui->satMinSpin->value();
    cfg.saturationMax = ui->satMaxSpin->value();

    AmplifierModel model;
    std::mt19937 localRng = m_rng;
    m_outputSignal = model.process(m_inputSignal,
                                   m_inputSampleRateHz,
                                   cfg,
                                   localRng);

    if (m_outputSignal.empty()) {
        QMessageBox::warning(this, tr("Amplifier error"),
                             tr("Amplifier produced an empty signal."));
        return;
    }

    ui->outputChartView->setSignalWithTimes(m_inputTimes, m_outputSignal);

    m_lastAmpSignal = m_outputSignal;
    m_lastAmpSampleRateHz = m_inputSampleRateHz;

    AmplitudeSpectrumAnalyzer analyzer;
    const double thresholdIn = ui->peakThresholdInSpin->value();
    const double windowInSec = ui->peakWindowInSpinNs->value() * 1e-9;
    const double thresholdOut = ui->peakThresholdOutSpin->value();
    const double windowOutSec = ui->peakWindowOutSpinNs->value() * 1e-9;
    const int binCount = ui->histBinsSpin->value();
    const double specStartSec = ui->specWindowStartSpinNs->value() * 1e-9;
    const double specEndSec = ui->specWindowEndSpinNs->value() * 1e-9;

    setEnabled(false);

    auto future = QtConcurrent::run([analyzer,
                                     times = m_inputTimes,
                                     input = m_inputSignal,
                                     output = m_outputSignal,
                                     sampleRate = m_inputSampleRateHz,
                                     thresholdIn,
                                     windowInSec,
                                     thresholdOut,
                                     windowOutSec,
                                     binCount,
                                     specStartSec,
                                     specEndSec]() {
        AmplitudeSpectrumResult inSpec = analyzer.compute(times,
                                                          input,
                                                          sampleRate,
                                                          thresholdIn,
                                                          windowInSec,
                                                          binCount,
                                                          specStartSec,
                                                          specEndSec);
        AmplitudeSpectrumResult outSpec = analyzer.compute(times,
                                                           output,
                                                           sampleRate,
                                                           thresholdOut,
                                                           windowOutSec,
                                                           binCount,
                                                           specStartSec,
                                                           specEndSec);
        return std::make_pair(inSpec, outSpec);
    });

    auto *watcher = new QFutureWatcher<std::pair<AmplitudeSpectrumResult, AmplitudeSpectrumResult>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const auto result = watcher->future().result();
        watcher->deleteLater();
        setEnabled(true);

        const auto &inputSpec = result.first;
        const auto &outputSpec = result.second;

        if (m_spectrumWidget) {
            m_spectrumWidget->updateSpectra(inputSpec, outputSpec);
        }
    });

    watcher->setFuture(future);
}

void AmplifierModelWidget::onResetZoom()
{
    if (ui->inputChartView)
        ui->inputChartView->resetView();
    if (ui->outputChartView)
        ui->outputChartView->resetView();
}

void AmplifierModelWidget::generateAmpWlfFile(QFile &file, QString wlfFileName, QString dataTxtFilePath) {
    if(!m_provider) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("Cannot get sampel rate or duration value"));
        return;
    }
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
    xmlWriter.writeCharacters(QString::number(m_provider->getSampleRateValue() * m_provider->getDurationValue(), 'f', 0));
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
    xmlWriter.writeCharacters(QString::number(m_provider->getSampleRateValue(), 'f', 0));
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

void AmplifierModelWidget::onExportAmpWlf() {
    if (m_lastAmpSignal.empty() || m_lastAmpSampleRateHz <= 0.0) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("No signal available. Please generate a signal first."));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export amplified signal to WLF"),
        QStringLiteral("ampWaveform.wlf"),
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

    QFile dataTxtFile(wlfFilePath + "/" + dateTimeStr + "-ampData.txt");
    if (!dataTxtFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("Cannot open data txt file for writing."));
        return;
    }

    QTextStream outData(&dataTxtFile);

    const std::size_t n = m_lastAmpSignal.size();
    for (std::size_t i = 0; i < n; ++i) {
        outData << QString::number(m_lastAmpSignal[i], 'g', 16) << '\n';
    }

    QFileInfo dataTxtFileInfo(dataTxtFile);
    QString dataTxtFilePath = dataTxtFileInfo.absoluteFilePath();
    dataTxtFilePath = QDir::toNativeSeparators(dataTxtFilePath);

    dataTxtFile.close();

    generateAmpWlfFile(file, wlfFileName, dataTxtFilePath);

    file.close();
}

void AmplifierModelWidget::onExportAmpCsv() {
    if (m_lastAmpSignal.empty() || m_lastAmpSampleRateHz <= 0.0) {
        QMessageBox::warning(this, tr("Export error"),
                             tr("No signal available. Please generate a signal first."));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export amplified signal to CSV"),
        QStringLiteral("ampSignal.csv"),
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

    const double dt = 1.0 / m_lastAmpSampleRateHz;
    const std::size_t n = m_lastAmpSignal.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        out << QString::number(t, 'g', 16) << ','
            << QString::number(m_lastAmpSignal[i], 'g', 16) << '\n';
    }

    file.close();
}