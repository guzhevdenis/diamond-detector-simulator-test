#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "amplifiermodelwidget.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <QScrollArea>
#include "amplitude_spectrum.h"
#include "signal_diagnostics.h"
#include <QCheckBox>
#include <QGroupBox>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDir>
#include "event_time_sampler.h"
#include "signal_synthesis_service.h"
#include "signal_plot_widget.h"
#include "spectrumanalysiswidget.h"

#include "Distribution.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QMessageBox>
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

    // The sidebar scrolls independently; charts use the full available height.
    auto *workspace = new QSplitter(Qt::Horizontal, genPage);
    workspace->setObjectName(QStringLiteral("generatorWorkspace"));
    workspace->setChildrenCollapsible(false);
    workspace->setHandleWidth(6);
    genLayout->addWidget(workspace, 1);

    auto *controlsScroll = new QScrollArea(workspace);
    controlsScroll->setObjectName(QStringLiteral("generatorSettings"));
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setMinimumWidth(260);
    auto *controlsWidget = new QWidget(controlsScroll);
    auto *settingsLayout = new QVBoxLayout(controlsWidget);
    settingsLayout->setContentsMargins(6, 6, 6, 6);
    settingsLayout->setSpacing(8);
    controlsScroll->setWidget(controlsWidget);

    auto addSection = [&](const QString &title, bool expanded) {
        auto *section = new QWidget(controlsWidget);
        auto *layout = new QVBoxLayout(section);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        auto *header = new QToolButton(section);
        header->setText(title);
        header->setCheckable(true);
        header->setChecked(expanded);
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *body = new QWidget(section);
        auto *form = new QFormLayout(body);
        form->setContentsMargins(4, 4, 4, 4);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        layout->addWidget(header);
        layout->addWidget(body);
        body->setVisible(expanded);
        connect(header, &QToolButton::toggled, body, [header, body](bool open) {
            body->setVisible(open);
            header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        });
        settingsLayout->addWidget(section);
        return form;
    };
    auto addParameter = [&](QFormLayout *form, const QString &label,
                            double low, double high, double value, int decimals) {
        auto *spin = new QDoubleSpinBox(controlsWidget);
        spin->setDecimals(decimals);
        spin->setRange(low, high);
        spin->setValue(value);
        spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        form->addRow(label, spin);
        return spin;
    };

    auto *pulseForm = addSection(tr("Pulse model"), true);
    m_pulseAmplitudeSpin = addParameter(pulseForm, tr("Amplitude A [V]:"), 0, 1e9, PulseParameters{}.amplitude, 6);
    m_pulseAmplitudeSpin->setSingleStep(0.01);
    m_riseTimeSpin = addParameter(pulseForm, tr("tau_r [ns]:"), 0.001, 1e6, 6, 3);
    m_fallTimeSpin = addParameter(pulseForm, tr("tau_f [ns]:"), 0.001, 1e6, 20, 3);
    m_betaSpin = addParameter(pulseForm, tr("beta:"), 0, 1000, 0.08, 6);
    auto *modelNote = new QLabel(tr("Fixed: tau_u = 100 ns; baseline = 0 V. A is the positive peak height in volts."), controlsWidget);
    modelNote->setWordWrap(true);
    pulseForm->addRow(modelNote);

    auto *eventsForm = addSection(tr("Event timing"), false);
    m_timeDistCombo = new QComboBox(controlsWidget);
    m_timeDistCombo->addItems({tr("Fixed Interval"), tr("Poisson")});
    eventsForm->addRow(tr("Distribution:"), m_timeDistCombo);
    m_eventRateSpin = addParameter(eventsForm, tr("Event rate [Hz]:"), 0, 1e9, 1e6, 3);

    auto *amplitudeForm = addSection(tr("Amplitudes"), false);
    m_ampDistCombo = new QComboBox(controlsWidget);
    // Preserve mode indices used by onGenerate().
    m_ampDistCombo->addItems({tr("From file"), tr("Normal"), tr("Exponential"),
                             tr("Log-normal"), tr("Constant A (pulse model)")});
    m_ampDistCombo->setCurrentIndex(4);
    amplitudeForm->addRow(tr("Distribution:"), m_ampDistCombo);

    // Hide whole parameter forms, including labels, for inactive distributions.
    auto addAmplitudePanel = [&] {
        auto *panel = new QWidget(controlsWidget);
        auto *form = new QFormLayout(panel);
        form->setContentsMargins(0, 0, 0, 0);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        amplitudeForm->addRow(panel);
        return form;
    };
    auto *fileForm = addAmplitudePanel();
    m_spectrumPathEdit = new QLineEdit(controlsWidget);
    m_spectrumPathEdit->setPlaceholderText(tr("Amplitude spectrum file"));
    m_loadSpectrumButton = new QPushButton(tr("Load Amplitude Spectrum"), controlsWidget);
    fileForm->addRow(m_spectrumPathEdit);
    fileForm->addRow(m_loadSpectrumButton);
    auto *normalForm = addAmplitudePanel();
    m_normMeanSpin = addParameter(normalForm, tr("Mean [V]:"), 0, 1e9, 1.0, 6);
    m_normSigmaSpin = addParameter(normalForm, tr("Sigma [V]:"), 1e-6, 1e6, 0.1, 6);
    m_normMeanSpin->setSingleStep(0.01);
    m_normSigmaSpin->setSingleStep(0.01);
    auto *exponentialForm = addAmplitudePanel();
    m_expLambdaSpin = addParameter(exponentialForm, tr("Lambda [1/V]:"), 1e-6, 1e6, 1, 6);
    auto *logNormalForm = addAmplitudePanel();
    m_logMuSpin = addParameter(logNormalForm, tr("Mu (ln(A / 1 V)):"), -1e6, 1e6, 0, 6);
    m_logSigmaSpin = addParameter(logNormalForm, tr("Sigma:"), 1e-6, 1e6, 0.25, 6);

    auto *samplingForm = addSection(tr("Sampling and duration"), false);
    m_durationSpin = addParameter(samplingForm, tr("Duration [s]:"), 0, 10, 1e-3, 9);
    m_sampleRateSpin = addParameter(samplingForm, tr("Sample rate [Hz]:"), 1, 1e12, 8e8, 0);
    m_pretriggerSpin = addParameter(samplingForm, tr("Pre-trigger [ns]:"), 0, 1e9, 100, 3);

    auto *noiseForm = addSection(tr("Noise and random seed"), false);
    m_noiseLevelSpin = addParameter(noiseForm, tr("Noise stddev [V]:"), 0, 1e3, 0, 6);
    m_noiseLevelSpin->setSingleStep(0.001);
    m_logSigmaSpin->setToolTip(tr("Standard deviation of ln(A / 1 V), dimensionless."));
    m_spectrumPathEdit->setToolTip(tr("Amplitude values in the spectrum file must be in volts."));
    m_seedSpin = new QSpinBox(controlsWidget);
    m_seedSpin->setRange(0, std::numeric_limits<int>::max());
    m_seedSpin->setValue(12345);
    noiseForm->addRow(tr("Random seed:"), m_seedSpin);
    auto *outputForm = addSection(tr("Generated signal"), false);
    m_adcClippingCheck = new QCheckBox(tr("Enable ADC Clipping"), controlsWidget);
    outputForm->addRow(m_adcClippingCheck);
    m_adcLevelSpin = addParameter(outputForm, tr("Clipping level +/- [V]:"), 0.000001, 1e9, 1.0, 6);
    m_adcLevelSpin->setSingleStep(0.1);
    m_adcLevelSpin->setEnabled(false);
    m_adcLevelSpin->setToolTip(tr("Applied after pulse summation and noise, before display, diagnostics and export."));
    connect(m_adcClippingCheck, &QCheckBox::toggled, m_adcLevelSpin, &QDoubleSpinBox::setEnabled);
    settingsLayout->addStretch();

    auto *charts = new QSplitter(Qt::Vertical, workspace);
    charts->setObjectName(QStringLiteral("generatorCharts"));
    charts->setChildrenCollapsible(false);
    charts->setHandleWidth(6);
    auto *signalRow = new QSplitter(Qt::Horizontal, charts);
    signalRow->setChildrenCollapsible(false);
    m_signalPlotWidget = new SignalPlotWidget(signalRow);
    m_signalPlotWidget->setMinimumSize(240, 160);
    m_signalPlotWidget->setAxisLabels(tr("Time [ns]"), tr("Amplitude [V]"));
    m_signalPlotWidget->setTitle(tr("Generated Signal"));

    auto *diagnosticsPanel = new QGroupBox(tr("Output diagnostics"), signalRow);
    auto *diagnosticsLayout = new QVBoxLayout(diagnosticsPanel);
    auto addDiagnostic = [&](const QString &caption) {
        auto *label = new QLabel(caption, diagnosticsPanel);
        label->setWordWrap(true);
        diagnosticsLayout->addWidget(label);
        auto *value = new QLabel(tr("Not generated"), diagnosticsPanel);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        diagnosticsLayout->addWidget(value);
        return value;
    };
    m_maximumLabel = addDiagnostic(tr("Maximum amplitude [V]"));
    m_pulseCountLabel = addDiagnostic(tr("Detected pulses"));
    m_detectionThresholdLabel = addDiagnostic(tr("Detection threshold [V]"));
    auto *diagnosticsNote = new QLabel(tr("Measured from output samples after clipping. Overlap and noise can merge or hide pulses; this is a detected peak count."), diagnosticsPanel);
    diagnosticsNote->setWordWrap(true);
    diagnosticsLayout->addWidget(diagnosticsNote);
    diagnosticsLayout->addStretch();
    signalRow->addWidget(m_signalPlotWidget);
    signalRow->addWidget(diagnosticsPanel);
    signalRow->setStretchFactor(0, 1);
    signalRow->setStretchFactor(1, 0);
    signalRow->setSizes({650, 190});

    auto *detailCharts = new QSplitter(Qt::Horizontal, charts);
    detailCharts->setChildrenCollapsible(false);
    detailCharts->setHandleWidth(6);
    m_pulsePlotWidget = new SignalPlotWidget(detailCharts);
    m_pulsePlotWidget->setMinimumSize(160, 120);
    m_pulsePlotWidget->setAxisLabels(tr("Time [ns]"), tr("Amplitude [V]"));
    m_pulsePlotWidget->setTitle(tr("Pulse Model"));
    m_spectrumPlotWidget = new SignalPlotWidget(detailCharts);
    m_spectrumPlotWidget->setTitle(tr("Amplitude Spectrum"));
    m_spectrumPlotWidget->setMinimumSize(160, 120);
    m_spectrumPlotWidget->setAxisLabels(tr("Amplitude [V]"), tr("Share [%]"));
    workspace->addWidget(controlsScroll);
    workspace->addWidget(charts);
    charts->addWidget(signalRow);
    charts->addWidget(detailCharts);
    detailCharts->addWidget(m_pulsePlotWidget);
    detailCharts->addWidget(m_spectrumPlotWidget);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setSizes({320, 800});
    charts->setStretchFactor(0, 3);
    charts->setStretchFactor(1, 2);
    charts->setSizes({420, 280});
    detailCharts->setSizes({400, 400});

    // Actions remain available when the sidebar is hidden or scrolled.
    auto *actionsLayout = new QHBoxLayout();
    auto *settingsButton = new QPushButton(tr("Hide settings"), genPage);
    settingsButton->setCheckable(true);
    settingsButton->setChecked(true);
    connect(settingsButton, &QPushButton::toggled, controlsScroll,
            [workspace, controlsScroll, settingsButton, sidebarWidth = 320](bool visible) mutable {
        if (!visible) sidebarWidth = workspace->sizes().value(0, 320);
        controlsScroll->setVisible(visible);
        if (visible)
            workspace->setSizes({sidebarWidth, std::max(1, workspace->width() - sidebarWidth - workspace->handleWidth())});
        settingsButton->setText(visible ? tr("Hide settings") : tr("Show settings"));
    });
    m_generateButton = new QPushButton(tr("Generate"), genPage);
    auto *exportButton = new QToolButton(genPage);
    exportButton->setText(tr("Export"));
    exportButton->setPopupMode(QToolButton::InstantPopup);
    auto *exportMenu = new QMenu(exportButton);
    m_exportCsvAction = exportMenu->addAction(tr("Export Signal to CSV"));
    m_exportWlfAction = exportMenu->addAction(tr("Export Signal to WLF"));
    exportButton->setMenu(exportMenu);
    actionsLayout->addWidget(settingsButton);
    actionsLayout->addWidget(m_generateButton);
    actionsLayout->addStretch();
    actionsLayout->addWidget(exportButton);
    genLayout->addLayout(actionsLayout);

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
    auto updateAmplitudeControls = [this, fileForm, normalForm, exponentialForm, logNormalForm] {
        const int mode = m_ampDistCombo->currentIndex();
        m_pulseAmplitudeSpin->setEnabled(mode == 4);
        fileForm->parentWidget()->setVisible(mode == 0);
        normalForm->parentWidget()->setVisible(mode == 1);
        exponentialForm->parentWidget()->setVisible(mode == 2);
        logNormalForm->parentWidget()->setVisible(mode == 3);
        updateAmplitudeSpectrumChart();
    };
    connect(m_ampDistCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [updateAmplitudeControls](int) { updateAmplitudeControls(); });
    m_pulseAmplitudeSpin->setToolTip(tr("Used for Constant A mode and the single-pulse preview. A is in volts; distributions supply each event's A directly in volts."));
    m_pretriggerSpin->setToolTip(tr("Leading baseline interval. The end of the array is reserved for tail decay."));
    m_generateButton->setToolTip(tr("Generates pulses from the analytic model; no pulse template file is needed."));
    m_exportCsvAction->setEnabled(false);
    m_exportWlfAction->setEnabled(false);
    for (auto *spin : {m_pulseAmplitudeSpin, m_normMeanSpin, m_normSigmaSpin,
                       m_expLambdaSpin, m_logMuSpin, m_logSigmaSpin})
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this] { updateAmplitudeSpectrumChart(); });
    connect(m_seedSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this] { updateAmplitudeSpectrumChart(); });
    connect(m_spectrumPathEdit, &QLineEdit::editingFinished, this,
            &MainWindow::updateAmplitudeSpectrumChart);
    updateAmplitudeControls();
    updatePulseChart();
    connect(m_loadSpectrumButton, &QPushButton::clicked,
            this, &MainWindow::onLoadAmplitudeSpectrum);
    connect(m_generateButton, &QPushButton::clicked,
            this, &MainWindow::onGenerate);
    connect(m_exportCsvAction, &QAction::triggered,
            this, &MainWindow::onExportCsv);
    connect(m_exportWlfAction, &QAction::triggered,
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

void MainWindow::onExportCsv()
{
    if (m_lastSignal->empty() || m_lastSampleRateHz <= 0.0) {
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
    out << "time_s,amplitude_V\n";

    const double dt = 1.0 / m_lastSampleRateHz;
    const std::size_t n = m_lastSignal->size();
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        out << QString::number(t, 'g', 16) << ','
            << QString::number((*m_lastSignal)[i], 'g', 16) << '\n';
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
    xmlWriter.writeCharacters(QString::number(static_cast<qulonglong>(m_lastSignal->size())));
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
    if (m_lastSignal->empty() || m_lastSampleRateHz <= 0.0) {
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

    const std::size_t n = m_lastSignal->size();
    for (std::size_t i = 0; i < n; ++i) {
        outData << QString::number((*m_lastSignal)[i], 'g', 16) << '\n';
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
        SignalSynthesisService::sampleCount(durationSec, sampleRateHz);
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
        auto signal = m_synthService->synthesize(pulse, eventTimes, durationSec,
                                                    sampleRateHz, noiseStdDev, m_rng, sampler,
                                                    {m_adcClippingCheck->isChecked(), m_adcLevelSpin->value()});

        if (signal.empty()) {
            QMessageBox::warning(this, tr("Error"), tr("Generated signal is empty."));
            return;
        }

        const auto diagnostics = analyzeSignal(signal);

        // Store last signal for CSV and WLF export
        m_lastSignal = std::make_shared<const std::vector<double>>(std::move(signal));
        m_lastSampleRateHz = sampleRateHz;
        m_exportCsvAction->setEnabled(true);
        m_exportWlfAction->setEnabled(true);

        m_maximumLabel->setText(QString::number(diagnostics.maximumV, 'g', 9));
        m_pulseCountLabel->setText(QString::number(static_cast<qulonglong>(diagnostics.detectedPulses)));
        m_detectionThresholdLabel->setText(QString::number(diagnostics.detectionThresholdV, 'g', 6));
        updateSignalChart(m_lastSignal, sampleRateHz);
        updateAmplitudeSpectrumChart();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, tr("Generation error"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::updateAmplitudeSpectrumChart()
{
    // Build the preview from current controls, independently of generation RNG/state.
    try {
        PercentageHistogram histogram;
        const int mode = m_ampDistCombo->currentIndex();
        AmplitudeSpectrum spectrum;
        if (mode == 4) {
            histogram = {{m_pulseAmplitudeSpin->value()}, {100.0}};
        } else if (mode == 0) {
            if (!spectrum.loadFromFile(m_spectrumPathEdit->text().trimmed())) {
                m_spectrumPlotWidget->clear();
                m_spectrumPlotWidget->setTitle(tr("Load an amplitude spectrum"));
                return;
            }
            histogram = spectrum.mode() == AmplitudeSpectrum::Mode::Histogram
                ? weightedPercentages(spectrum.histAmplitudes(), spectrum.histWeights())
                : samplePercentages(spectrum.samples());
        } else {
            if (mode == 1) spectrum.setNormal(m_normMeanSpin->value(), m_normSigmaSpin->value());
            else if (mode == 2) spectrum.setExponential(m_expLambdaSpin->value());
            else spectrum.setLogNormal(m_logMuSpin->value(), m_logSigmaSpin->value());
            std::mt19937 previewRng(static_cast<std::mt19937::result_type>(m_seedSpin->value()));
            std::vector<double> samples;
            samples.reserve(50000);
            for (int i = 0; i < 50000; ++i) samples.push_back(spectrum.sample(previewRng));
            histogram = samplePercentages(samples);
        }
        m_spectrumPlotWidget->setTitle(tr("Amplitude Spectrum"));
        m_spectrumPlotWidget->setSpectrum(histogram.centers, histogram.percentages, true);
        m_spectrumPlotWidget->setToolTip(tr("Share per bin; total = 100%. Built-in distributions use 50000 preview samples. Constant A is a 100% line. Wheel: zoom; drag: pan; double-click: reset."));
    } catch (const std::exception &e) {
        m_spectrumPlotWidget->clear();
        m_spectrumPlotWidget->setTitle(QString::fromUtf8(e.what()));
    }
}

void MainWindow::updateSignalChart(std::shared_ptr<const std::vector<double>> signal,
                                   double sampleRateHz)
{
    if (m_signalPlotWidget)
        m_signalPlotWidget->setSharedSignal(std::move(signal), sampleRateHz);
}

PulseParameters MainWindow::pulseParameters() const
{
    return {m_pulseAmplitudeSpin->value(), m_riseTimeSpin->value(),
            m_fallTimeSpin->value(), m_betaSpin->value()};
}

void MainWindow::updatePulseChart()
{
    try {
        const PulseModel pulse(pulseParameters());
        const double pre = m_pretriggerSpin->value();
        const double fs = m_sampleRateSpin->value();
        const double endNs = pre + pulse.settlingTimeNs();
        const double count = std::ceil(endNs * fs * 1e-9) + 1;
        // Bound preview memory for extreme time constants/sample rates.
        // Explicit times preserve the output-grid positions when subsampling.
        const double stride = std::max(1.0, std::ceil(count / 200000.0));
        std::vector<double> times, values;
        const auto previewCount = static_cast<std::size_t>(std::ceil(count / stride));
        times.reserve(previewCount);
        values.reserve(previewCount);
        for (double k = 0; k < count; k += stride) {
            const double timeSec = k / fs;
            times.push_back(timeSec);
            values.push_back(pulse.value(timeSec * 1e9 - pre));
        }
        m_pulsePlotWidget->setTitle(tr("Pulse Model (A = %1 V)").arg(pulse.parameters().amplitude));
        m_pulsePlotWidget->setSignalWithTimes(times, values);
        m_pulsePlotWidget->setToolTip(tr("Wheel: zoom time axis; drag: pan; double-click: reset. Preview step: %1 ns.")
                                     .arg(stride / fs * 1e9));
    } catch (const std::exception &e) {
        m_pulsePlotWidget->clear();
        m_pulsePlotWidget->setTitle(QString::fromUtf8(e.what()));
    }
}
