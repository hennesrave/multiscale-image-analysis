#pragma once
#include <qdialog.h>

class Database;
class Feature;

class QVBoxLayout;

// ----- FeatureManager ----- //

class FeatureManager : public QDialog
{
    Q_OBJECT
public:
    FeatureManager( Database& database );
    static void execute( Database& database );

private:
    void feature_appended( QSharedPointer<QObject> object );
    void feature_removed( QSharedPointer<QObject> object );

    Database& _database;
    QVBoxLayout* _features_layout = nullptr;
    std::unordered_map<const Feature*, QWidget*> _feature_widgets;
};