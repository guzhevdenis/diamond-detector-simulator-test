#include "fpga_processing_dialog.h"
#include "qcustomplot.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QAbstractTableModel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QFont>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QTableView>
#include <QHeaderView>
#include <QTimer>
#include <QSplitter>
#include <QScrollArea>
#include <QMessageBox>
#include <algorithm>
#include <cmath>
#include <numeric>
namespace {
QString number(double v) { return std::isfinite(v) ? QString::number(v,'g',8) : QStringLiteral("N/A"); }
class ResultsModel : public QAbstractTableModel {
    std::shared_ptr<const Fpga::Result> r;
    bool gates;
public:
    ResultsModel(std::shared_ptr<const Fpga::Result> value, bool isGates, QObject *parent)
        : QAbstractTableModel(parent), r(std::move(value)), gates(isGates) {}
    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(gates ? r->gates.size() : r->pulses.size());
    }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 12; }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) return {};
        if (orientation == Qt::Vertical) return section+1;
        const QStringList pulseHeaders = {"StartTime [ticks]", "StartTime [ns]", "Peak [code]", "Peak [V]", "Integral [sum codes]", "Integral [V ns]", "Length [ticks]", "Length [ns]", "Crossings", "PileUp", "Done [ticks]", "Baseline [V]"};
        const QStringList gateHeaders = {"Gate", "Windows", "Completed", "BusyTicks", "Crossings", "AboveTicks", "PileUpWindows", "Measured [1/s]", "Corrected [1/s]", "Corrected2 [1/s]", "Dead [%]", "PileUp [%]"};
        return (gates ? gateHeaders : pulseHeaders).value(section);
    }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const int c = index.column();
        if (gates) {
            const auto &g = r->gates[index.row()];
            const double values[] = {double(g.number),double(g.windows),double(g.completed),double(g.busy),double(g.crossings),double(g.above),double(g.pileUp),g.rates.measured,g.rates.corrected,g.rates.corrected2,100*g.rates.deadFraction,100*g.rates.pileUpFraction};
            return number(values[c]);
        }
        const auto &p = r->pulses[index.row()];
        if (c == 9) return p.pileUp ? "Yes" : "No";
        const double values[] = {double(p.startTime),p.startTime*5.0,double(p.peak),p.peak*Fpga::voltsPerCode,double(p.integral),p.integral*Fpga::voltsPerCode*1.25,double(p.length),p.length*5.0,double(p.crossings),0,double(p.doneTick),p.baseline*Fpga::voltsPerCode};
        return number(values[c]);
    }
};
QCustomPlot *plot(QWidget *parent, const QString &x, const QString &y) {
    auto *p = new QCustomPlot(parent);
    p->setMinimumSize(240,180);
    p->xAxis->setLabel(x); p->yAxis->setLabel(y);
    p->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    p->axisRect()->setRangeDrag(Qt::Horizontal);
    p->axisRect()->setRangeZoom(Qt::Horizontal);
    QObject::connect(p,&QCustomPlot::mouseDoubleClick,p,[p] { p->rescaleAxes(); p->replot(); });
    p->setToolTip("Wheel: zoom; drag: pan; double-click: reset.");
    return p;
}
void line(QCustomPlot *p, const QVector<double> &x, const QVector<double> &y, const QString &name, QColor color) {
    auto *g = p->addGraph(); g->setName(name); g->setPen(QPen(color)); g->setData(x,y,true);
}
void finish(QCustomPlot *p) { p->legend->setVisible(p->graphCount()>1); p->rescaleAxes(); p->replot(); }
}
FpgaProcessingDialog::FpgaProcessingDialog(std::shared_ptr<const std::vector<double>> signal, double fs, QWidget *parent)
    : QDialog(parent), m_signal(std::move(signal)), m_fs(fs), m_watcher(new QFutureWatcher<std::shared_ptr<Fpga::Result>>(this)) {
    setWindowTitle(tr("FPGA Data Processing Simulation")); resize(1180,800);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    auto *note = new QLabel(tr("NI 5772: 800 MHz, 12-bit ADC, +/-1 V; P = 1 tick; baseline n = -12. Simulation follows the document; DMA losses and spectrum-bank hardware conflicts are not simulated."),this);
    note->setWordWrap(true); layout->addWidget(note);
    auto *settings = new QWidget(this); auto *forms = new QHBoxLayout(settings);
    auto *left = new QFormLayout; auto *right = new QFormLayout;
    forms->addLayout(left); forms->addLayout(right);
    auto integer = [&](QFormLayout *f, const QString &name, int low, int high, int value) {
        auto *v = new QSpinBox(settings); v->setRange(low,high); v->setValue(value); f->addRow(name,v); return v;
    };
    m_threshold = integer(left,tr("Threshold [codes]:"),0,INT32_MAX,50);
    auto *thresholdV = new QLabel(this); left->addRow(tr("Threshold [V]:"),thresholdV);
    auto voltage = [this,thresholdV] { thresholdV->setText(number(m_threshold->value()*Fpga::voltsPerCode)); };
    connect(m_threshold,qOverload<int>(&QSpinBox::valueChanged),this,[voltage] { voltage(); }); voltage();
    m_tail = integer(left,tr("TailTicks (5 ns each):"),0,65535,3);
    m_gate = integer(left,tr("Gate [us]:"),16,1000000,1000);
    m_hold = new QCheckBox(tr("BaselineHold (initial baseline = 0)"),settings); left->addRow(m_hold);
    m_maxLength = new QDoubleSpinBox(settings); m_maxLength->setDecimals(0); m_maxLength->setRange(1,4294967295.0); m_maxLength->setValue(4294967295.0); left->addRow(tr("MaxLength [ticks], max = disabled:"),m_maxLength);
    auto *maxLengthNs = new QLabel(settings);
    maxLengthNs->setWordWrap(true);
    maxLengthNs->setTextInteractionFlags(Qt::TextSelectableByMouse);
    left->addRow(tr("MaxLength [ns]:"), maxLengthNs);
    auto updateMaxLengthNs = [this, maxLengthNs] {
        const double ticks = m_maxLength->value();
        QString value = QString::number(ticks * (Fpga::tickSeconds * 1e9), 'f', 0);
        if (ticks == double(UINT32_MAX)) value += tr(" (disabled)");
        maxLengthNs->setText(value);
    };
    connect(m_maxLength, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateMaxLengthNs] { updateMaxLengthNs(); });
    updateMaxLengthNs();
    m_peakMin = integer(right,tr("FPGA PeakMin [codes]:"),INT32_MIN,INT32_MAX,0);
    m_integralMin = integer(right,tr("FPGA IntegralMin [sum codes]:"),INT32_MIN,INT32_MAX,0);
    m_peakShift = integer(right,tr("FPGA PeakShift (512 bins):"),0,30,6);
    m_integralShift = integer(right,tr("FPGA IntegralShift (1024 bins):"),0,30,13);
    auto width = [&](const QString &name,double value) { auto *v = new QDoubleSpinBox(settings); v->setRange(0.001,1e12); v->setDecimals(3); v->setValue(value); right->addRow(name,v); return v; };
    m_hostPeakWidth = width(tr("Host Peak bin width [codes]:"),64);
    m_hostIntegralWidth = width(tr("Host Integral bin width:"),512);
    m_hostPeakMin = width(tr("Host PeakMin [codes]:"),0);
    m_hostPeakMin->setRange(INT32_MIN,INT32_MAX); m_hostPeakMin->setValue(0);
    m_hostIntegralMin = width(tr("Host IntegralMin [sum codes]:"),0);
    m_hostIntegralMin->setRange(INT32_MIN,INT32_MAX); m_hostIntegralMin->setValue(0);
    auto *scroll = new QScrollArea(this); scroll->setWidgetResizable(true); scroll->setWidget(settings); scroll->setMaximumHeight(235); layout->addWidget(scroll);
    auto *actions = new QHBoxLayout;
    m_run = new QPushButton(tr("Run simulation"),this); m_stop = new QPushButton(tr("Cancel"),this); m_stop->setEnabled(false);
    m_bar = new QProgressBar(this); m_bar->setRange(0,100);
    actions->addWidget(m_run); actions->addWidget(m_stop); actions->addWidget(m_bar,1); layout->addLayout(actions);
    m_status = new QLabel(this); m_status->setWordWrap(true); layout->addWidget(m_status);
    m_summary = new QLabel(this); m_summary->setWordWrap(true); m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_summary);
    auto *tabs = new QTabWidget(this); layout->addWidget(tabs,1);
    auto *pulsePage = new QWidget(this); auto *pulseLayout = new QVBoxLayout(pulsePage);
    auto *overview = new QPushButton(tr("Full record overview"),pulsePage); pulseLayout->addWidget(overview);
    m_wave = plot(pulsePage,tr("Time [ns]"),tr("Voltage [V]")); pulseLayout->addWidget(m_wave,2);
    m_pulses = new QTableView(pulsePage); m_pulses->setSelectionBehavior(QAbstractItemView::SelectRows); m_pulses->setSelectionMode(QAbstractItemView::SingleSelection); pulseLayout->addWidget(m_pulses,1);
    tabs->addTab(pulsePage,tr("Windows and waveform"));
    auto *spectraPage = new QWidget(this); auto *spectraLayout = new QVBoxLayout(spectraPage); auto *spectraTools = new QHBoxLayout;
    m_spectrumMode = new QComboBox(spectraPage); m_percent = new QCheckBox(tr("Percentages (including out-of-range in denominator)"),spectraPage);
    spectraTools->addWidget(m_spectrumMode); spectraTools->addWidget(m_percent); spectraLayout->addLayout(spectraTools);
    m_spectrumInfo = new QLabel(spectraPage); m_spectrumInfo->setWordWrap(true); spectraLayout->addWidget(m_spectrumInfo);
    auto *spectraRow = new QSplitter(Qt::Horizontal,spectraPage);
    m_peakPlot = plot(spectraRow,tr("Peak [V]"),tr("Count")); m_integralPlot = plot(spectraRow,tr("Integral [V ns]"),tr("Count"));
    spectraRow->addWidget(m_peakPlot); spectraRow->addWidget(m_integralPlot); spectraLayout->addWidget(spectraRow);
    tabs->addTab(spectraPage,tr("Measured spectra"));
    auto *ratePage = new QWidget(this); auto *rateLayout = new QVBoxLayout(ratePage);
    auto *rateRow = new QSplitter(Qt::Horizontal,ratePage); m_rates = plot(rateRow,tr("Time [ms]"),tr("Rate [1/s]")); m_fractions = plot(rateRow,tr("Time [ms]"),tr("Fraction [%]")); rateRow->addWidget(m_rates); rateRow->addWidget(m_fractions); rateLayout->addWidget(rateRow,1);
    m_gates = new QTableView(ratePage); rateLayout->addWidget(m_gates,1); tabs->addTab(ratePage,tr("Gates and rates"));
    // Match table headings to the actual tab-bar font, including DPI scaling.
    const QFont headerFont = tabs->tabBar()->font();
    for (auto *table : {m_pulses, m_gates}) {
        table->horizontalHeader()->setFont(headerFont);
        table->verticalHeader()->setFont(headerFont);
        table->horizontalHeader()->setHighlightSections(false);
        table->verticalHeader()->setHighlightSections(false);
    }
    connect(overview,&QPushButton::clicked,this,[this] { showWaveform(); });
    connect(m_spectrumMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this] { showSpectra(); });
    connect(m_percent,&QCheckBox::toggled,this,[this] { showSpectra(); });
    connect(m_run,&QPushButton::clicked,this,[this] { start(); });
    connect(m_stop,&QPushButton::clicked,this,[this] { m_cancel->store(true); m_status->setText(tr("Cancelling...")); });
    auto *timer = new QTimer(this); timer->setInterval(200); connect(timer,&QTimer::timeout,this,[this] { m_bar->setValue(m_progress->load()); }); timer->start();
    connect(m_watcher,&QFutureWatcher<std::shared_ptr<Fpga::Result>>::finished,this,[this,settings] {
        m_run->setEnabled(true); m_stop->setEnabled(false); settings->setEnabled(true);
        try {
            auto result = m_watcher->result();
            if (result) { m_result = std::move(result); showResults(); }
            else m_status->setText(QString::fromStdString(*m_failure));
        }
        catch (const std::exception &e) { m_status->setText(QString::fromUtf8(e.what())); }
        catch (...) { m_status->setText(tr("Simulation failed. Check available memory.")); }
    });
    connect(m_run,&QPushButton::clicked,settings,[settings] { settings->setEnabled(false); });
    QTimer::singleShot(0,this,[this,settings] { settings->setEnabled(false); start(); });
}
FpgaProcessingDialog::~FpgaProcessingDialog() { m_cancel->store(true); }
void FpgaProcessingDialog::start() {
    if (m_watcher->isRunning()) return;
    Fpga::Settings s;
    s.threshold=m_threshold->value(); s.tailTicks=static_cast<uint16_t>(m_tail->value()); s.gateTicks=uint32_t(m_gate->value())*200;
    s.baselineHold=m_hold->isChecked(); s.maxLength=static_cast<uint32_t>(m_maxLength->value());
    s.peakMin=m_peakMin->value(); s.integralMin=m_integralMin->value(); s.peakShift=m_peakShift->value(); s.integralShift=m_integralShift->value();
    s.hostPeakWidth=m_hostPeakWidth->value(); s.hostIntegralWidth=m_hostIntegralWidth->value();
    s.hostPeakMin=m_hostPeakMin->value(); s.hostIntegralMin=m_hostIntegralMin->value();
    m_cancel->store(false); m_progress->store(0); m_run->setEnabled(false); m_stop->setEnabled(true);
    m_status->setText(tr("Processing the captured generated signal. Previous results remain visible until completion."));
    m_failure = std::make_shared<std::string>();
    const auto failure=m_failure;
    const auto signal=m_signal; const auto cancel=m_cancel; const auto progress=m_progress; const double fs=m_fs;
    m_watcher->setFuture(QtConcurrent::run([signal,s,cancel,progress,fs,failure]() -> std::shared_ptr<Fpga::Result> {
        try { return std::make_shared<Fpga::Result>(Fpga::process(*signal,fs,s,cancel.get(),progress.get())); }
        catch (const std::exception &e) { *failure=e.what(); return {}; }
    }));
}
void FpgaProcessingDialog::showResults() {
    const auto &r=*m_result;
    m_status->setText(tr("Complete. Partial gate: %1 ticks (not published); incomplete ADC group: %2 samples; open window at stop: %3. DMA delivery is not modeled.")
        .arg(r.partialGateTicks).arg(r.trailingSamples).arg(r.unfinishedWindow ? tr("yes") : tr("no")));
    const auto maxPeak=std::max_element(r.pulses.begin(),r.pulses.end(),[](const Fpga::Pulse &a,const Fpga::Pulse &b) { return a.peak<b.peak; });
    m_summary->setText(tr("Windows opened: %1 | Completed records: %2 | Pile-up: %3 | Accepted windows: %4 | Complete gates: %5 | Saturated samples: %6 | Final baseline: %7 V | Max measured Peak: %8 V")
        .arg(r.windows).arg(r.completed).arg(r.pileUp).arg(r.accepted).arg(r.gates.size()).arg(r.saturatedSamples).arg(number(r.finalBaseline*Fpga::voltsPerCode)).arg(maxPeak==r.pulses.end() ? tr("N/A") : number(maxPeak->peak*Fpga::voltsPerCode)));
    for (auto *table : {m_pulses,m_gates}) { auto *old=table->model(); table->setModel(new ResultsModel(m_result,table==m_gates,table)); delete old; table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); }
    connect(m_pulses->selectionModel(),&QItemSelectionModel::currentRowChanged,this,[this](const QModelIndex &i) { showWaveform(i.row()); });
    m_spectrumMode->blockSignals(true); m_spectrumMode->clear();
    m_spectrumMode->addItem(tr("LabVIEW host: all completed records"),-2);
    m_spectrumMode->addItem(tr("FPGA: all complete gates, pile-up excluded"),-1);
    for (std::size_t i=0;i<r.gates.size();++i) m_spectrumMode->addItem(tr("FPGA gate %1").arg(r.gates[i].number),static_cast<int>(i));
    m_spectrumMode->setCurrentIndex(1); m_spectrumMode->blockSignals(false);
    QVector<double> t,a,b,c,d,e;
    for (const auto &g:r.gates) { t.push_back((g.number+1)*r.settings.gateTicks*5e-6); a.push_back(g.rates.measured); b.push_back(g.rates.corrected); c.push_back(g.rates.corrected2); d.push_back(100*g.rates.deadFraction); e.push_back(100*g.rates.pileUpFraction); }
    m_rates->clearGraphs(); line(m_rates,t,a,tr("Measured"),Qt::blue); line(m_rates,t,b,tr("Corrected"),Qt::red); line(m_rates,t,c,tr("Corrected2"),Qt::darkGreen); finish(m_rates);
    m_fractions->clearGraphs(); line(m_fractions,t,d,tr("Dead time"),Qt::blue); line(m_fractions,t,e,tr("Pile-up"),Qt::red); finish(m_fractions);
    showWaveform(); showSpectra();
}
void FpgaProcessingDialog::showWaveform(int row) {
    if (!m_result) return;
    m_wave->clearGraphs(); m_wave->clearItems();
    QVector<double> x,low,high,base,threshold;
    if (row >= 0 && std::size_t(row)<m_result->pulses.size()) {
        const auto &p=m_result->pulses[row];
        const std::size_t first=p.firstSampleTick>8 ? (p.firstSampleTick-8)*4 : 0;
        const std::size_t end=std::min(m_signal->size(),std::size_t((p.lastSampleTick+9)*4));
        const std::size_t stride=std::max<std::size_t>(1,(end-first+49999)/50000);
        for (std::size_t i=first;i<end;i+=stride) {
            x.push_back(i*1.25); low.push_back(Fpga::quantize((*m_signal)[i])*Fpga::voltsPerCode);
            base.push_back(p.baseline*Fpga::voltsPerCode); threshold.push_back((double(p.baseline)+m_result->settings.threshold)*Fpga::voltsPerCode);
        }
        line(m_wave,x,low,tr("ADC samples"),Qt::blue);
        auto *band=new QCPItemRect(m_wave); band->topLeft->setType(QCPItemPosition::ptPlotCoords); band->bottomRight->setType(QCPItemPosition::ptPlotCoords);
        band->topLeft->setCoords(p.firstSampleTick*5.0,1.1); band->bottomRight->setCoords((p.lastSampleTick+1)*5.0,-1.1);
        band->setBrush(QColor(255,200,0,35)); band->setPen(Qt::NoPen);
        m_wave->setToolTip(tr("Highlighted integration window. Baseline line is the baseline snapshot at window start; overview shows its evolution."));
    } else {
        for (const auto &p:m_result->trace) { x.push_back(p.timeNs); low.push_back(p.minimumV); high.push_back(p.maximumV); base.push_back(p.baselineV); threshold.push_back(p.baselineV+m_result->settings.threshold*Fpga::voltsPerCode); }
        line(m_wave,x,low,tr("ADC min"),Qt::blue); line(m_wave,x,high,tr("ADC max"),Qt::darkCyan);
    }
    line(m_wave,x,base,tr("Baseline"),Qt::darkGreen); line(m_wave,x,threshold,tr("Threshold"),Qt::red); finish(m_wave);
}
void FpgaProcessingDialog::showSpectra() {
    if (!m_result || m_spectrumMode->currentIndex()<0) return;
    const auto &r=*m_result; const int mode=m_spectrumMode->currentData().toInt();
    QVector<double> px,py,ix,iy; uint64_t count=0,po=0,io=0;
    if (mode==-2) { count=r.completed; po=r.hostPeakOut; io=r.hostIntegralOut; }
    else if (mode==-1) { for (const auto &g:r.gates) count+=g.accepted; po=r.fpgaPeakOut; io=r.fpgaIntegralOut; }
    else { const auto &g=r.gates[mode]; count=g.accepted; po=g.peakOut; io=g.integralOut; }
    const double multiplier=m_percent->isChecked() ? (count ? 100.0/count : 0.0) : 1.0;
    for (int i=0;i<512;++i) {
        px.push_back((mode==-2 ? r.settings.hostPeakMin+(i+.5)*r.settings.hostPeakWidth : r.settings.peakMin+(i+.5)*std::ldexp(1.0,r.settings.peakShift))*Fpga::voltsPerCode);
        py.push_back((mode==-2 ? r.hostPeak[i] : mode==-1 ? r.fpgaPeak[i] : r.gates[mode].peakBins[i])*multiplier);
    }
    for (int i=0;i<1024;++i) {
        ix.push_back((mode==-2 ? r.settings.hostIntegralMin+(i+.5)*r.settings.hostIntegralWidth : r.settings.integralMin+(i+.5)*std::ldexp(1.0,r.settings.integralShift))*Fpga::voltsPerCode*1.25);
        iy.push_back((mode==-2 ? r.hostIntegral[i] : mode==-1 ? r.fpgaIntegral[i] : r.gates[mode].integralBins[i])*multiplier);
    }
    m_spectrumInfo->setText(tr("Included records: %1 | Peak out of range: %2 | Integral out of range: %3. FPGA spectra are logical accumulations for complete gates, before DMA delivery.").arg(count).arg(po).arg(io));
    m_peakPlot->clearGraphs(); m_integralPlot->clearGraphs();
    line(m_peakPlot,px,py,QString(),Qt::blue); line(m_integralPlot,ix,iy,QString(),Qt::blue);
    for (auto *p:{m_peakPlot,m_integralPlot}) { p->graph(0)->setLineStyle(QCPGraph::lsStepCenter); p->yAxis->setLabel(m_percent->isChecked() ? tr("Share [%]") : tr("Count")); finish(p); }
}
