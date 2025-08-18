#pragma once
#include "utility.hpp"

#include <qobject.h>

class Dataset;

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
    Override<QString>& override_identifier() noexcept;

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

// ----- DatasetChannelsFeature ----- //

class DatasetChannelsFeature : public Feature
{
    Q_OBJECT
public:
    enum class Reduction
    {
        eAccumulate,
        eIntegrate
    };

    enum class BaselineCorrection
    {
        eNone,
        eMinimum,
        eLinear
    };

    DatasetChannelsFeature( QSharedPointer<const Dataset> dataset, Range<uint32_t> channel_range, Reduction reduction, BaselineCorrection baseline_correction );

    // Feature interface
    uint32_t element_count() const noexcept override;

    // Properties
    QSharedPointer<const Dataset> dataset() const;

    Range<uint32_t> channel_range() const noexcept;
    void update_channel_range( Range<uint32_t> channels );

    Reduction reduction() const noexcept;
    void update_reduction( Reduction reduction );

    BaselineCorrection baseline_correction() const noexcept;
    void update_baseline_correction( BaselineCorrection baseline_correction );

signals:
    void channel_range_changed( Range<uint32_t> channel_range );
    void reduction_changed( Reduction reduction );
    void baseline_correction_changed( BaselineCorrection baseline_correction );

private:
    void update_identifier();
    Array<double> compute_values() const override;

    QWeakPointer<const Dataset> _dataset;
    Range<uint32_t> _channel_range;
    Reduction _reduction { Reduction::eAccumulate };
    BaselineCorrection _baseline_correction { BaselineCorrection::eNone };
};

// ----- CombinationFeature ----- //

class CombinationFeature : public Feature
{
    Q_OBJECT
public:
    enum class Operation
    {
        eAddition,
        eSubtraction,
        eMultiplication,
        eDivision
    };

    CombinationFeature( QSharedPointer<const Feature> first_feature, QSharedPointer<const Feature> second_feature, Operation operation );

    uint32_t element_count() const noexcept override;

    QSharedPointer<const Feature> first_feature() const;
    void update_first_feature( QSharedPointer<const Feature> first_feature );

    QSharedPointer<const Feature> second_feature() const;
    void update_second_feature( QSharedPointer<const Feature> second_feature );

    Operation operation() const noexcept;
    void update_operation( Operation operation );

signals:
    void first_feature_changed( QSharedPointer<const Feature> first_feature );
    void second_feature_changed( QSharedPointer<const Feature> second_feature );
    void operation_changed( Operation operation );

private:
    void update_identifier();
    Array<double> compute_values() const override;

    QWeakPointer<const Feature> _first_feature;
    QWeakPointer<const Feature> _second_feature;
    Operation _operation { Operation::eAddition };
};