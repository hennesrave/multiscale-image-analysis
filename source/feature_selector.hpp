#pragma once
#include "collection.hpp"

#include <qwidget.h>

class Feature;

class QComboBox;

class FeatureSelector : public QWidget
{
    Q_OBJECT
public:
    FeatureSelector( QSharedPointer<const Collection<Feature>> features, const std::function<bool( QSharedPointer<Feature> )>& filter = {} );

    QSharedPointer<Feature> selected_feature() const;
    void update_selected_feature( QSharedPointer<Feature> feature );

signals:
    void selected_feature_changed( QSharedPointer<Feature> feature );

private:
    void object_appended( QSharedPointer<QObject> object );
    void object_removed( QSharedPointer<QObject> object );

    QSharedPointer<const Collection<Feature>> _features;
    std::function<bool( QSharedPointer<Feature> )> _filter;
    QComboBox* _combobox = nullptr;
};