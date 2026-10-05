#include "template_loader.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <cmath>

const std::vector<double> &TemplateLoader::pulseTemplate(const QString &path)
{
    const QString effectivePath = path.isEmpty()
            ? QStringLiteral("DiamondDetectorSignal_amplified.txt")
            : path;

    if (m_loaded && effectivePath == m_cachedPath) {
        return m_cachedTemplate;
    }

    m_cachedTemplate.clear();
    m_loaded = false;
    m_cachedPath.clear();

    QFile file(effectivePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "TemplateLoader: cannot open" << effectivePath;
        return m_cachedTemplate;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        bool ok = false;
        const double v = line.toDouble(&ok);
        if (ok)
            m_cachedTemplate.push_back(v);
    }

    if (m_cachedTemplate.empty()) {
        qWarning() << "TemplateLoader: file is empty or invalid:" << effectivePath;
        return m_cachedTemplate;
    }

    normalizePeak(m_cachedTemplate);

    m_loaded = true;
    m_cachedPath = effectivePath;
    return m_cachedTemplate;
}


void TemplateLoader::normalizePeak(std::vector<double> &data)
{
    if (data.empty())
        return;

    double maxAbs = 0.0;
    for (double v : data) {
        maxAbs = std::max(maxAbs, std::abs(v));
    }
    if (maxAbs <= 0.0)
        return;

    const double inv = 1.0 / maxAbs;
    for (double &v : data) {
        v *= inv;
    }
}


