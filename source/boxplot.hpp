#pragma once
#include "utility.hpp"

#include <qobject.h>
#include <qsharedpointer.h>

class Feature;
class Segmentation;

// ----- GroupedBoxplot ----- //

class GroupedBoxplot : public QObject
{
    Q_OBJECT
public:
    struct Statistics
    {
        double minimum = 0.0;
        double maximum = 0.0;
        double average = 0.0;
        double standard_deviation = 0.0;
        double lower_quartile = 0.0;
        double upper_quartile = 0.0;
        double median = 0.0;
    };

    GroupedBoxplot();

    QSharedPointer<const Segmentation> segmentation() const;
    void update_segmentation( QSharedPointer<const Segmentation> segmentation );

    QSharedPointer<const Feature> feature() const;
    void update_feature( QSharedPointer<const Feature> feature );

    const Promise<Array<Statistics>>& statistics() const;

signals:
    void segmentation_changed( QSharedPointer<const Segmentation> segmentation );
    void feature_changed( QSharedPointer<const Feature> feature );

private:
    void compute_statistics( Array<Statistics>& statistics ) const;

    QWeakPointer<const Segmentation> _segmentation;
    QWeakPointer<const Feature> _feature;

    Promise<Array<Statistics>> _statistics;
};