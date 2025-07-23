#pragma once
#include "utility.hpp"

#include <qobject.h>
#include <qsharedpointer.h>

class Feature;
class Segmentation;

// ----- Histogram ----- //

class Histogram : public QObject
{
    Q_OBJECT
public:
    Histogram( uint32_t bincount );

    QSharedPointer<const Feature> feature() const;
    void update_feature( QSharedPointer<const Feature> feature );

    uint32_t bincount() const noexcept;
    void update_bincount( uint32_t bincount );

    const Promise<Array<double>>& edges() const;
    const Promise<Array<uint32_t>>& counts() const;

signals:
    void feature_changed( QSharedPointer<const Feature> feature );
    void bincount_changed( uint32_t bincount );

private:
    void compute_edges( Array<double>& edges ) const;
    void compute_counts( Array<uint32_t>& counts ) const;

    QWeakPointer<const Feature> _feature;
    uint32_t _bincount;

    Promise<Array<double>> _edges;
    Promise<Array<uint32_t>> _counts;
};

// ----- StackedHistogram ----- //

class StackedHistogram : public QObject
{
    Q_OBJECT
public:
    StackedHistogram( uint32_t bincount );

    QSharedPointer<const Segmentation> segmentation() const;
    void update_segmentation( QSharedPointer<const Segmentation> segmentation );

    QSharedPointer<const Feature> feature() const;
    void update_feature( QSharedPointer<const Feature> feature );

    uint32_t bincount() const noexcept;
    void update_bincount( uint32_t bincount );

    const Promise<Array<double>>& edges() const;
    const Promise<Array<Array<uint32_t>>>& counts() const;

signals:
    void segmentation_changed( QSharedPointer<const Segmentation> segmentation );
    void feature_changed( QSharedPointer<const Feature> feature );
    void bincount_changed( uint32_t bincount );

private:
    void compute_edges( Array<double>& edges ) const;
    void compute_counts( Array<Array<uint32_t>>& counts ) const;

    QWeakPointer<const Segmentation> _segmentation;
    QWeakPointer<const Feature> _feature;
    uint32_t _bincount;

    Promise<Array<double>> _edges;
    Promise<Array<Array<uint32_t>>> _counts;
};