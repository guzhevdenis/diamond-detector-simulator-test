#ifndef VALUES_PROVIDER_H
#define VALUES_PROVIDER_H

class ValuesProvider {
public:
    virtual ~ValuesProvider() = default;
    virtual double getDurationValue() const = 0;
    virtual double getSampleRateValue() const = 0;
};

#endif // VALUES_PROVIDER_H
