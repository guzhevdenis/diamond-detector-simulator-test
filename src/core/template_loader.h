#ifndef TEMPLATE_LOADER_H
#define TEMPLATE_LOADER_H

#include <vector>
#include <QString>

class TemplateLoader
{
public:
    TemplateLoader() = default;

    const std::vector<double> &pulseTemplate(const QString &path = QString());
    static void normalizePeak(std::vector<double> &data);


private:
    std::vector<double> m_cachedTemplate;
    QString m_cachedPath;
    bool m_loaded = false;
};

#endif // TEMPLATE_LOADER_H

