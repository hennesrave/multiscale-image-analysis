#pragma once
#include "utility.hpp"

#include <qobject.h>

// ----- Feature ----- //

class Feature : public QObject
{
    Q_OBJECT
public:
    struct Extremes
    {
        double minimum;
        double maximum;
    };

    struct Moments
    {
        double average;
        double standard_deviation;
    };

    struct Quantiles
    {
        double lower_quartile;
        double median;
        double upper_quartile;
    };

    Feature();

    virtual uint32_t element_count() const noexcept = 0;

    const QString& identifier() const noexcept;
    void update_identifier( const QString& identifier );

    const Array<double>& values() const noexcept;
    const Extremes& extremes() const noexcept;
    const Moments& moments() const noexcept;
    const Quantiles& quantiles() const noexcept;
    const Array<uint32_t>& sorted_indices() const noexcept;

signals:
    void identifier_changed( const QString& identifier );
    void values_changed();
    void extremes_changed();
    void moments_changed();
    void quantiles_changed();
    void sorted_indices_changed();

protected:
    virtual Array<double> compute_values() const = 0;
    Extremes compute_extremes() const;
    Moments compute_moments() const;
    Quantiles compute_quantiles() const;
    Array<uint32_t> compute_sorted_indices() const;

    Override<QString> _identifier;
    Computed<Array<double>> _values;
    Computed<Extremes> _extremes;
    Computed<Moments> _moments;
    Computed<Quantiles> _quantiles;
    Computed<Array<uint32_t>> _sorted_indices;
};

// ----- ElementFilterFeature ----- //

class ElementFilterFeature : public Feature
{
    Q_OBJECT
public:
    ElementFilterFeature( QSharedPointer<const Feature> feature, std::vector<uint32_t> element_indices );

    uint32_t element_count() const noexcept override;

private:
    Array<double> compute_values() const override;

    QWeakPointer<const Feature> _feature;
    std::vector<uint32_t> _element_indices;
};