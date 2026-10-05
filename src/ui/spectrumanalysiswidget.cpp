#include "spectrumanalysiswidget.h"

#include "amplitude_spectrum_analyzer.h"
#include "signal_plot_widget.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

SpectrumAnalysisWidget::SpectrumAnalysisWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);

    // Controls row for combined spectrum options (log X/Y)
    auto *controlsLayout = new QHBoxLayout();
    m_logXCheck = new QCheckBox(tr("Log X"), this);
    m_logYCheck = new QCheckBox(tr("Log Y"), this);
    controlsLayout->addWidget(m_logXCheck);
    controlsLayout->addWidget(m_logYCheck);
    controlsLayout->addStretch(1);
    mainLayout->addLayout(controlsLayout);

    auto *chartsLayout = new QGridLayout();

    m_inputSpectrumChartView = new QChartView(new QChart(), this);
    m_outputSpectrumChartView = new QChartView(new QChart(), this);
    m_combinedSpectrumPlot = new SignalPlotWidget(this);

    m_inputSpectrumChartView->chart()->setTitle(tr("Input Amplitude Spectrum"));
    m_outputSpectrumChartView->chart()->setTitle(tr("Output Amplitude Spectrum"));
    m_combinedSpectrumPlot->setAxisLabels(tr("Amplitude"), tr("Count"));
    m_combinedSpectrumPlot->setTitle(tr("Input vs Output Amplitude Spectrum"));

    m_inputSpectrumChartView->setRubberBand(QChartView::RectangleRubberBand);
    m_outputSpectrumChartView->setRubberBand(QChartView::RectangleRubberBand);

    m_inputSpectrumChartView->chart()->legend()->hide();
    m_outputSpectrumChartView->chart()->legend()->hide();

    chartsLayout->addWidget(m_inputSpectrumChartView, 0, 0);
    chartsLayout->addWidget(m_outputSpectrumChartView, 0, 1);
    chartsLayout->addWidget(m_combinedSpectrumPlot, 1, 0, 1, 2);

    // Ensure vertical space is shared roughly 50/50 between
    // the top row (two separate spectra) and the bottom row
    // (combined spectrum).
    chartsLayout->setRowStretch(0, 1);
    chartsLayout->setRowStretch(1, 1);

    mainLayout->addLayout(chartsLayout);

    // Connect log-scale controls to the combined spectrum plot
    connect(m_logXCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (m_combinedSpectrumPlot)
                    m_combinedSpectrumPlot->setSpectrumLogX(checked);
            });
    connect(m_logYCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (m_combinedSpectrumPlot)
                    m_combinedSpectrumPlot->setSpectrumLogY(checked);
            });
}

SpectrumAnalysisWidget::~SpectrumAnalysisWidget() = default;

void SpectrumAnalysisWidget::plotSpectrum(QChartView *view,
                                          const std::vector<double> &centers,
                                          const std::vector<double> &counts,
                                          const QString &title)
{
    if (!view || !view->chart())
        return;

    QChart *chart = view->chart();
    chart->removeAllSeries();
    chart->removeAxis(chart->axisX());
    chart->removeAxis(chart->axisY());

    auto *series = new QLineSeries(chart);
    const std::size_t n = centers.size();
    for (std::size_t i = 0; i < n; ++i) {
        series->append(centers[i], counts[i]);
    }

    chart->addSeries(series);

    auto *axisX = new QValueAxis(chart);
    axisX->setTitleText(tr("Amplitude"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis(chart);
    axisY->setTitleText(tr("Count"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->setTitle(title);
}

void SpectrumAnalysisWidget::updateSpectra(const AmplitudeSpectrumResult &input,
                                           const AmplitudeSpectrumResult &output)
{
    plotSpectrum(m_inputSpectrumChartView,
                 input.binCenters,
                 input.binCounts,
                 tr("Input Amplitude Spectrum"));
    plotSpectrum(m_outputSpectrumChartView,
                 output.binCenters,
                 output.binCounts,
                 tr("Output Amplitude Spectrum"));

    // Combined chart: overlay both spectra using SignalPlotWidget
    if (m_combinedSpectrumPlot) {
        m_combinedSpectrumPlot->setCombinedSpectrum(input.binCenters,
                                                    input.binCounts,
                                                    output.binCenters,
                                                    output.binCounts);
    }
}

