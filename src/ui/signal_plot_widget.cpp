#include "signal_plot_widget.h"

#include "qcustomplot.h"

#include <QVBoxLayout>
#include <QResizeEvent>
#include <utility>

SignalPlotWidget::SignalPlotWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    m_graph = m_plot->addGraph();

    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_plot->axisRect()->setRangeDrag(Qt::Horizontal);
    m_plot->axisRect()->setRangeZoom(Qt::Horizontal);

    m_plot->legend->setVisible(false);
    setToolTip(tr("Wheel: zoom time axis; drag: pan; double-click: reset."));
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, [this] { resetView(); });

    connect(m_plot->xAxis, SIGNAL(rangeChanged(QCPRange,QCPRange)),
            this, SLOT(onXAxisRangeChanged(QCPRange,QCPRange)));
}

SignalPlotWidget::~SignalPlotWidget() = default;

void SignalPlotWidget::setSignal(const std::vector<double> &samples,
                                 double sampleRateHz)
{
    setSharedSignal(std::make_shared<const std::vector<double>>(samples), sampleRateHz);
}

void SignalPlotWidget::setSharedSignal(std::shared_ptr<const std::vector<double>> samples,
                                      double sampleRateHz)
{
    m_mode = SignalPlotMode::TimeSignal;
    m_signal = samples ? std::move(samples) : std::make_shared<const std::vector<double>>();
    m_sampleRateHz = sampleRateHz;
    m_timesSeconds.clear();
    m_hasSpectrum = false;
    m_hasCombinedSpectrum = false;

    updateFullRange();
    rebuildVisibleData();
}

void SignalPlotWidget::setSignalWithTimes(const std::vector<double> &timesSeconds,
                                          const std::vector<double> &samples)
{
    m_mode = SignalPlotMode::TimeSignal;
    m_signal = std::make_shared<const std::vector<double>>(samples);
    m_timesSeconds = timesSeconds;
    m_sampleRateHz = 0.0; // not used when explicit times are present
    m_hasSpectrum = false;
    m_hasCombinedSpectrum = false;

    updateFullRange();
    rebuildVisibleData();
}

void SignalPlotWidget::setSpectrum(const std::vector<double> &centers,
                                   const std::vector<double> &counts)
{
    m_mode = SignalPlotMode::SpectrumSingle;

    m_specX = QVector<double>(centers.begin(), centers.end());
    m_specY = QVector<double>(counts.begin(), counts.end());
    m_hasSpectrum = !m_specX.isEmpty() && m_specX.size() == m_specY.size();
    m_hasCombinedSpectrum = false;

    if (!m_hasSpectrum) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    auto minMaxX = std::minmax_element(m_specX.constBegin(), m_specX.constEnd());
    auto minMaxY = std::minmax_element(m_specY.constBegin(), m_specY.constEnd());

    m_specMinX = *minMaxX.first;
    m_specMaxX = *minMaxX.second;
    m_specMinY = *minMaxY.first;
    m_specMaxY = *minMaxY.second;

    if (m_specMinX == m_specMaxX) {
        m_specMinX -= 0.5;
        m_specMaxX += 0.5;
    }
    if (m_specMinY == m_specMaxY) {
        m_specMinY -= 0.5;
        m_specMaxY += 0.5;
    }

    const double padY = 0.05 * (m_specMaxY - m_specMinY + 1e-12);

    m_plot->xAxis->setRange(m_specMinX, m_specMaxX);
    m_plot->yAxis->setRange(m_specMinY - padY, m_specMaxY + padY);

    m_graph->setData(m_specX, m_specY, true);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::setCombinedSpectrum(const std::vector<double> &centersInput,
                                           const std::vector<double> &countsInput,
                                           const std::vector<double> &centersOutput,
                                           const std::vector<double> &countsOutput)
{
    m_mode = SignalPlotMode::SpectrumCombined;

    m_specInX = QVector<double>(centersInput.begin(), centersInput.end());
    m_specInY = QVector<double>(countsInput.begin(), countsInput.end());
    m_specOutX = QVector<double>(centersOutput.begin(), centersOutput.end());
    m_specOutY = QVector<double>(countsOutput.begin(), countsOutput.end());

    m_hasCombinedSpectrum = !m_specInX.isEmpty() && m_specInX.size() == m_specInY.size()
                            && !m_specOutX.isEmpty() && m_specOutX.size() == m_specOutY.size();
    m_hasSpectrum = false;

    if (!m_hasCombinedSpectrum) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    auto minMaxXIn = std::minmax_element(m_specInX.constBegin(), m_specInX.constEnd());
    auto minMaxXOut = std::minmax_element(m_specOutX.constBegin(), m_specOutX.constEnd());
    m_specMinX = std::min(*minMaxXIn.first, *minMaxXOut.first);
    m_specMaxX = std::max(*minMaxXIn.second, *minMaxXOut.second);

    auto minMaxYIn = std::minmax_element(m_specInY.constBegin(), m_specInY.constEnd());
    auto minMaxYOut = std::minmax_element(m_specOutY.constBegin(), m_specOutY.constEnd());
    m_specMinY = std::min(*minMaxYIn.first, *minMaxYOut.first);
    m_specMaxY = std::max(*minMaxYIn.second, *minMaxYOut.second);

    if (m_specMinX == m_specMaxX) { m_specMinX -= 0.5; m_specMaxX += 0.5; }
    if (m_specMinY == m_specMaxY) { m_specMinY -= 0.5; m_specMaxY += 0.5; }

    const double padY = 0.05 * (m_specMaxY - m_specMinY + 1e-12);

    m_plot->xAxis->setRange(m_specMinX, m_specMaxX);
    m_plot->yAxis->setRange(m_specMinY - padY, m_specMaxY + padY);

    // Ensure second graph exists
    if (!m_graph2) {
        m_graph2 = m_plot->addGraph();
    }

    m_graph->setData(m_specInX, m_specInY, true);
    QPen outPen(Qt::red);
    outPen.setWidthF(1.5);
    m_graph2->setPen(outPen);
    m_graph2->setData(m_specOutX, m_specOutY, true);

    m_plot->legend->setVisible(true);
    m_graph->setName(tr("Input"));
    m_graph2->setName(tr("Output"));

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::setSpectrumLogY(bool enabled)
{
    m_logSpectrumY = enabled;

    if (m_mode == SignalPlotMode::TimeSignal)
        return;

    auto *axisY = m_plot->yAxis;
    if (!axisY)
        return;

    if (enabled)
        axisY->setScaleType(QCPAxis::stLogarithmic);
    else
        axisY->setScaleType(QCPAxis::stLinear);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::setSpectrumLogX(bool enabled)
{
    m_logSpectrumX = enabled;

    if (m_mode == SignalPlotMode::TimeSignal)
        return;

    auto *axisX = m_plot->xAxis;
    if (!axisX)
        return;

    if (enabled)
        axisX->setScaleType(QCPAxis::stLogarithmic);
    else
        axisX->setScaleType(QCPAxis::stLinear);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::clear()
{
    m_signal = std::make_shared<const std::vector<double>>();
    m_sampleRateHz = 0.0;
    m_fullMinTimeNs = 0.0;
    m_fullMaxTimeNs = 0.0;

    if (m_graph) {
        m_graph->data()->clear();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SignalPlotWidget::setAxisLabels(const QString &xLabel, const QString &yLabel)
{
    m_plot->xAxis->setLabel(xLabel);
    m_plot->yAxis->setLabel(yLabel);
}

void SignalPlotWidget::setTitle(const QString &title)
{
    if (!m_titleElement) {
        m_plot->plotLayout()->insertRow(0);
        m_titleElement = new QCPTextElement(m_plot, title, QFont(font().family(), 10, QFont::Bold));
        m_plot->plotLayout()->addElement(0, 0, m_titleElement);
    } else {
        m_titleElement->setText(title);
    }
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::resetView()
{
    if (m_mode == SignalPlotMode::SpectrumSingle || m_mode == SignalPlotMode::SpectrumCombined) {
        const bool hasAny = (m_mode == SignalPlotMode::SpectrumSingle && m_hasSpectrum) ||
                            (m_mode == SignalPlotMode::SpectrumCombined && m_hasCombinedSpectrum);
        if (!hasAny)
            return;

        const double padY = 0.05 * (m_specMaxY - m_specMinY + 1e-12);
        m_plot->xAxis->setRange(m_specMinX, m_specMaxX);
        m_plot->yAxis->setRange(m_specMinY - padY, m_specMaxY + padY);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    if (m_signal->empty())
        return;

    m_updating = true;

    m_plot->xAxis->setRange(m_fullMinTimeNs, m_fullMaxTimeNs);

    // Y range from full signal
    auto minmax = std::minmax_element(m_signal->begin(), m_signal->end());
    const double minY = *minmax.first;
    const double maxY = *minmax.second;
    const double padding = 0.05 * (maxY - minY + 1e-12);
    m_plot->yAxis->setRange(minY - padding, maxY + padding);

    m_updating = false;

    rebuildVisibleData();
}

void SignalPlotWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    rebuildVisibleData();
}

void SignalPlotWidget::onXAxisRangeChanged(const QCPRange &newRange, const QCPRange &)
{
    if (m_updating)
        return;

    Q_UNUSED(newRange);
    rebuildVisibleData();
}

void SignalPlotWidget::rebuildVisibleData()
{
    if (!m_graph)
        return;

    if (m_mode == SignalPlotMode::SpectrumSingle) {
        if (!m_hasSpectrum)
            return;
        m_graph->setData(m_specX, m_specY, true);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    } else if (m_mode == SignalPlotMode::SpectrumCombined) {
        if (!m_hasCombinedSpectrum)
            return;
        if (!m_graph2) {
            m_graph2 = m_plot->addGraph();
        }
        m_graph->setData(m_specInX, m_specInY, true);
        m_graph2->setData(m_specOutX, m_specOutY, true);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    if (m_signal->empty())
        return;

    const int pixelWidth = m_plot->viewport().width();
    if (pixelWidth <= 0)
        return;

    const QCPRange range = m_plot->xAxis->range();
    const double tMinNs = range.lower;
    const double tMaxNs = range.upper;

    if (tMaxNs <= tMinNs)
        return;

    QVector<double> x;
    QVector<double> y;
    buildMinMaxEnvelope(tMinNs, tMaxNs, pixelWidth, x, y);

    // Update the graph data for the current visible time range.
    // Y-axis range is managed globally in updateFullRange/resetView based
    // on the full signal amplitudes to avoid collapsed or tiny ranges.
    m_graph->setData(x, y, true);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SignalPlotWidget::updateFullRange()
{
    if (m_mode == SignalPlotMode::SpectrumSingle || m_mode == SignalPlotMode::SpectrumCombined) {
        // Spectrum ranges are handled in setSpectrum/resetView
        return;
    }

    if (m_signal->empty()) {
        m_fullMinTimeNs = 0.0;
        m_fullMaxTimeNs = 0.0;
        return;
    }

    const std::size_t n = m_signal->size();

    if (!m_timesSeconds.empty() && m_timesSeconds.size() == n) {
        // Use explicit time axis
        const double t0 = m_timesSeconds.front();
        const double t1 = m_timesSeconds.back();
        const double scale = 1e9; // display in ns
        m_fullMinTimeNs = t0 * scale;
        m_fullMaxTimeNs = t1 * scale;
    } else if (m_sampleRateHz > 0.0) {
        const double dtNs = 1e9 / m_sampleRateHz;
        m_fullMinTimeNs = 0.0;
        m_fullMaxTimeNs = static_cast<double>(n - 1) * dtNs;
    } else {
        m_fullMinTimeNs = 0.0;
        m_fullMaxTimeNs = static_cast<double>(n);
    }

    m_updating = true;
    m_plot->xAxis->setRange(m_fullMinTimeNs, m_fullMaxTimeNs);

    auto minmax = std::minmax_element(m_signal->begin(), m_signal->end());
    const double minY = *minmax.first;
    const double maxY = *minmax.second;
    const double padding = 0.05 * (maxY - minY + 1e-12);
    m_plot->yAxis->setRange(minY - padding, maxY + padding);
    m_updating = false;
}

void SignalPlotWidget::buildMinMaxEnvelope(double tMinNs,
                                           double tMaxNs,
                                           int pixelWidth,
                                           QVector<double> &x,
                                           QVector<double> &y) const
{
    const std::size_t n = m_signal->size();
    if (n == 0)
        return;

    const double scale = 1e9; // seconds -> ns

    // Map visible time range to sample indices
    std::size_t iStart = 0;
    std::size_t iEnd = n;

    if (!m_timesSeconds.empty() && m_timesSeconds.size() == n) {
        // Use binary search on explicit times (converted to ns)
        const double tMinSec = tMinNs / scale;
        const double tMaxSec = tMaxNs / scale;

        auto itStart = std::lower_bound(m_timesSeconds.begin(), m_timesSeconds.end(), tMinSec);
        auto itEnd = std::upper_bound(m_timesSeconds.begin(), m_timesSeconds.end(), tMaxSec);

        iStart = static_cast<std::size_t>(std::distance(m_timesSeconds.begin(), itStart));
        iEnd = static_cast<std::size_t>(std::distance(m_timesSeconds.begin(), itEnd));
    } else if (m_sampleRateHz > 0.0) {
        const double dtNs = 1e9 / m_sampleRateHz;

        if (tMinNs > 0.0)
            iStart = static_cast<std::size_t>(tMinNs / dtNs);

        iEnd = n - 1;
        if (tMaxNs > 0.0) {
            const std::size_t tmp = static_cast<std::size_t>(tMaxNs / dtNs) + 1;
            if (tmp < n)
                iEnd = tmp;
        }
    }

    if (iStart >= n)
        iStart = n - 1;
    if (iEnd <= iStart + 1)
        iEnd = std::min<std::size_t>(n, iStart + 2);

    const std::size_t count = iEnd - iStart;
    if (count == 0)
        return;

    const int bucketCount = std::max(1, pixelWidth);
    const double bucketSize = static_cast<double>(count) / static_cast<double>(bucketCount);

    x.clear();
    y.clear();
    x.reserve(bucketCount * 2);
    y.reserve(bucketCount * 2);

    for (int b = 0; b < bucketCount; ++b) {
        const double startF = static_cast<double>(b) * bucketSize;
        const double endF = static_cast<double>(b + 1) * bucketSize;

        std::size_t bStart = iStart + static_cast<std::size_t>(startF);
        std::size_t bEnd   = iStart + static_cast<std::size_t>(endF);

        if (bStart >= iEnd)
            break;
        if (bEnd <= bStart + 1)
            bEnd = std::min<std::size_t>(iEnd, bStart + 2);
        if (bEnd > iEnd)
            bEnd = iEnd;

        double minVal = (*m_signal)[bStart];
        double maxVal = (*m_signal)[bStart];
        std::size_t idxMin = bStart;
        std::size_t idxMax = bStart;

        for (std::size_t i = bStart + 1; i < bEnd; ++i) {
            const double v = (*m_signal)[i];
            if (v < minVal) {
                minVal = v;
                idxMin = i;
            }
            if (v > maxVal) {
                maxVal = v;
                idxMax = i;
            }
        }

        // Preserve temporal order of min/max within the bucket
        auto appendPoint = [&](std::size_t idx, double value) {
            double tNs = 0.0;
            if (!m_timesSeconds.empty() && m_timesSeconds.size() == n) {
                tNs = m_timesSeconds[idx] * scale;
            } else if (m_sampleRateHz > 0.0) {
                const double dtNs = 1e9 / m_sampleRateHz;
                tNs = static_cast<double>(idx) * dtNs;
            } else {
                tNs = static_cast<double>(idx);
            }
            x.push_back(tNs);
            y.push_back(value);
        };

        if (idxMin == idxMax) {
            appendPoint(idxMin, minVal);
        } else if (idxMin < idxMax) {
            appendPoint(idxMin, minVal);
            appendPoint(idxMax, maxVal);
        } else {
            appendPoint(idxMax, maxVal);
            appendPoint(idxMin, minVal);
        }
    }
}

