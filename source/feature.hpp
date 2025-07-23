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

    const Promise<Array<double>>& values() const noexcept;
    const Promise<Extremes>& extremes() const noexcept;
    const Promise<Moments>& moments() const noexcept;
    const Promise<Quantiles>& quantiles() const noexcept;
    const Promise<Array<uint32_t>>& sorted_indices() const noexcept;

signals:
    void identifier_changed( const QString& identifier );

protected:
    virtual void compute_values( Array<double>& values ) const = 0;
    void compute_extremes( Extremes& extremes ) const;
    void compute_moments( Moments& moments ) const;
    void compute_quantiles( Quantiles& quantiles ) const;
    void compute_sorted_indices( Array<uint32_t>& sorted_indices ) const;

    Override<QString> _identifier;
    Promise<Array<double>> _values;
    Promise<Extremes> _extremes;
    Promise<Moments> _moments;
    Promise<Quantiles> _quantiles;
    Promise<Array<uint32_t>> _sorted_indices;
};

// ----- ElementFilterFeature ----- //

class ElementFilterFeature : public Feature
{
    Q_OBJECT
public:
    ElementFilterFeature( QSharedPointer<const Feature> feature, std::vector<uint32_t> element_indices );

    uint32_t element_count() const noexcept override;

private:
    void compute_values( Array<double>& values ) const override;

    QWeakPointer<const Feature> _feature;
    std::vector<uint32_t> _element_indices;
};