#include "feature_manager.hpp"

#include "database.hpp"
#include "feature_selector.hpp"
#include "string_input.hpp"

#include <qapplication.h>
#include <qcolordialog.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>

FeatureManager::FeatureManager( Database& database ) : QDialog {}, _database { database }
{
    this->setWindowTitle( "Feature Manager" );
    this->setMinimumSize( 400, 500 );

    _features_layout = new QVBoxLayout {};
    _features_layout->setContentsMargins( 0, 0, 0, 0 );
    _features_layout->setSpacing( 5 );

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

    for( qsizetype object_index = 0; object_index < features->object_count(); ++object_index )
    {
        this->feature_appended( features->object( object_index ) );
    }
}
void FeatureManager::execute( Database& database )
{
    auto feature_manager = FeatureManager { database };
    feature_manager.exec();
}

void FeatureManager::feature_appended( QSharedPointer<QObject> object )
{
    const auto feature = object.staticCast<Feature>();

    auto lineedit_identfier = new StringInput { feature->override_identifier() };

    auto button_remove = new QToolButton {};
    button_remove->setIcon( QIcon { ":/delete.svg" } );

    auto header = new QHBoxLayout {};
    header->setContentsMargins( 0, 0, 0, 0 );
    header->setSpacing( 5 );
    header->addWidget( lineedit_identfier );
    header->addWidget( button_remove );

    auto properties = new QVBoxLayout {};
    properties->setContentsMargins( 20, 0, 0, 0 );
    properties->setSpacing( 2 );

    if( auto combination_feature = feature.objectCast<CombinationFeature>() )
    {
        auto first = new FeatureSelector { _database.features(), [pointer = combination_feature.get()] ( QSharedPointer<const Feature> feature )
        {
            return feature.get() != pointer;
        } };
        first->update_selected_feature( combination_feature->first_feature().constCast<Feature>() );

        auto operation = new QComboBox {};
        operation->addItem( "+", QVariant::fromValue( CombinationFeature::Operation::eAddition ) );
        operation->addItem( "-", QVariant::fromValue( CombinationFeature::Operation::eSubtraction ) );
        operation->addItem( "\u00d7", QVariant::fromValue( CombinationFeature::Operation::eMultiplication ) );
        operation->addItem( "\u00f7", QVariant::fromValue( CombinationFeature::Operation::eDivision ) );
        operation->setCurrentIndex( operation->findData( QVariant::fromValue( combination_feature->operation() ) ) );

        auto second = new FeatureSelector { _database.features(), [pointer = combination_feature.get()] ( QSharedPointer<const Feature> feature )
        {
            return feature.get() != pointer;
        } };
        second->update_selected_feature( combination_feature->second_feature().constCast<Feature>() );

        QObject::connect( first, &FeatureSelector::selected_feature_changed, this, [this, combination_feature] ( QSharedPointer<const Feature> feature )
        {
            combination_feature->update_first_feature( feature );
        } );
        QObject::connect( operation, &QComboBox::currentIndexChanged, this, [this, combination_feature, operation] ( int index )
        {
            combination_feature->update_operation( static_cast<CombinationFeature::Operation>( operation->itemData( index ).value<CombinationFeature::Operation>() ) );
        } );

        QObject::connect( second, &FeatureSelector::selected_feature_changed, this, [this, combination_feature] ( QSharedPointer<const Feature> feature )
        {
            combination_feature->update_second_feature( feature );
        } );

        auto row = new QHBoxLayout {};
        row->setContentsMargins( 0, 0, 0, 0 );
        row->setSpacing( 5 );

        row->addWidget( first, 1 );
        row->addWidget( operation );
        row->addWidget( second, 1 );

        properties->addLayout( row );
    }

    auto container_widget = new QWidget {};
    auto container_layout = new QVBoxLayout { container_widget };
    container_layout->setContentsMargins( 0, 0, 0, 0 );
    container_layout->setSpacing( 2 );
    container_layout->addLayout( header );
    container_layout->addLayout( properties );

    QObject::connect( button_remove, &QToolButton::clicked, this, [this, pointer = QWeakPointer { feature }]
    {
        if( auto feature = pointer.lock() )
        {
            _database.features()->remove( feature );
        }
    } );

    _features_layout->addWidget( container_widget );
    _feature_widgets[feature.get()] = container_widget;
}
void FeatureManager::feature_removed( QSharedPointer<QObject> object )
{
    const auto feature_pointer = object.staticCast<Feature>().get();
    _feature_widgets[feature_pointer]->deleteLater();
    _feature_widgets.erase( feature_pointer );
}
