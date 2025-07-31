#include "feature_manager.hpp"

#include "database.hpp"

#include <qapplication.h>
#include <qcolordialog.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>

FeatureManager::FeatureManager( Database& database ) : QDialog {}, _database { database }
{
    this->setWindowTitle( "Feature Manager" );
    this->setMinimumSize( 300, 400 );

    _features_layout = new QVBoxLayout {};
    _features_layout->setContentsMargins( 0, 0, 0, 0 );
    _features_layout->setSpacing( 2 );

    auto button_create_feature = new QPushButton { "Create Feature" };

    auto controls = new QHBoxLayout {};
    controls->setContentsMargins( 0, 0, 0, 0 );
    controls->setSpacing( 5 );
    controls->addWidget( button_create_feature );

    auto layout = new QVBoxLayout { this };
    layout->setContentsMargins( 20, 10, 20, 10 );
    layout->setSpacing( 5 );
    layout->addLayout( _features_layout );
    layout->addStretch( 1 );
    layout->addLayout( controls );

    QObject::connect( button_create_feature, &QPushButton::clicked, this, [this, button_create_feature]
    {
        auto context_menu = QMenu {};

        context_menu.addAction( "Channel Feature", [this]
        {
            auto feature = new DatasetChannelsFeature {
                _database.dataset(),
                Range<uint32_t>{ 0, 0 },
                DatasetChannelsFeature::Reduction::eAccumulate,
                DatasetChannelsFeature::BaselineCorrection::eNone
            };
            _database.features()->append( QSharedPointer<Feature> { feature } );
        } );
        context_menu.addAction( "Combination Feature", [this]
        {
            auto feature = new CombinationFeature {
                QSharedPointer<const Feature> {},
                QSharedPointer<const Feature> {},
                CombinationFeature::Operation::eAddition
            };
            _database.features()->append( QSharedPointer<Feature> { feature } );
        } );

        context_menu.setFixedWidth( button_create_feature->width() );
        context_menu.exec( button_create_feature->mapToGlobal( QPoint { 0, button_create_feature->height() } ) );
    } );

    const auto features = _database.features();
    QObject::connect( features.get(), &CollectionObject::object_appended, this, &FeatureManager::feature_appended );
    QObject::connect( features.get(), &CollectionObject::object_removed, this, &FeatureManager::feature_removed );
}
void FeatureManager::execute( Database& database )
{
    auto feature_manager = FeatureManager { database };
    feature_manager.exec();
}

void FeatureManager::feature_appended( QSharedPointer<QObject> object )
{
    const auto feature_pointer = object.staticCast<Feature>().get();

    auto header = new QHBoxLayout {};
    header->setContentsMargins( 0, 0, 0, 0 );
    header->setSpacing( 5 );

    auto properties = new QVBoxLayout {};
    properties->setContentsMargins( 20, 0, 0, 0 );
    properties->setSpacing( 2 );

    // TODO

    auto container_widget = new QWidget {};
    auto container_layout = new QVBoxLayout { container_widget };
    container_layout->setContentsMargins( 0, 0, 0, 0 );
    container_layout->setSpacing( 2 );
    container_layout->addLayout( header );
    container_layout->addLayout( properties );

    _features_layout->addWidget( container_widget );
    _feature_widgets[feature_pointer] = container_widget;
}
void FeatureManager::feature_removed( QSharedPointer<QObject> object )
{
    const auto feature_pointer = object.staticCast<Feature>().get();
    _feature_widgets[feature_pointer]->deleteLater();
    _feature_widgets.erase( feature_pointer );
}
