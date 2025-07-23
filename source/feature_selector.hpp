#pragma once
#include "collection.hpp"

#include <qwidget.h>

class Feature;

class QComboBox;

class FeatureSelector : public QWidget
{
    Q_OBJECT
public:
    FeatureSelector( QSharedPointer<const Collection<Feature>> features );

signals:
    void selected_feature_changed( QSharedPointer<Feature> feature );

private:
    void object_appended( QSharedPointer<QObject> object );
    void object_removed( QSharedPointer<QObject> object );

    QSharedPointer<const Collection<Feature>> _features;
    QWeakPointer<Feature> _selected_feature;
    QComboBox* _combobox = nullptr;
};