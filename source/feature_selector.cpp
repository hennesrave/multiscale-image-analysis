#include "feature_selector.hpp"

#include "feature.hpp"

#include "logger.hpp"

#include <qcombobox.h>
#include <qstackedlayout.h>

FeatureSelector::FeatureSelector( QSharedPointer<const Collection<Feature>> features ) : QWidget {}, _features { features }
{
    _combobox = new QComboBox {};
    _combobox->setPlaceholderText( "Select Feature..." );
    _combobox->setSizeAdjustPolicy( QComboBox::AdjustToContents );
    _combobox->setMaximumWidth( 200 );

    auto layout = new QStackedLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->addWidget( _combobox );

    QObject::connect( features.get(), &Collection<Feature>::object_appended, this, &FeatureSelector::object_appended );
    QObject::connect( features.get(), &Collection<Feature>::object_removed, this, &FeatureSelector::object_removed );
    QObject::connect( _combobox, &QComboBox::currentIndexChanged, this, [this] ( int index )
    {
        emit selected_feature_changed( _combobox->itemData( index ).value<QWeakPointer<Feature>>().lock() );
    } );

    for( qsizetype feature_index = 0; feature_index < features->object_count(); ++feature_index )
    {
        this->object_appended( features->object( feature_index ) );
    }
}

void FeatureSelector::object_appended( QSharedPointer<QObject> object )
{
    if( auto feature = object.objectCast<Feature>() )
    {
        _combobox->addItem( feature->identifier(), QVariant::fromValue( QWeakPointer<Feature>( feature ) ) );
        _combobox->setItemData( _combobox->count() - 1, feature->identifier(), Qt::ToolTipRole );
        QObject::connect( feature.get(), &Feature::identifier_changed, this, [this, pointer = QWeakPointer { feature }]( const QString& identifier )
        {
            if( const auto index = _combobox->findData( QVariant::fromValue( pointer ) ); index != -1 )
            {
                _combobox->setItemText( index, identifier );
            }
        } );

        if( _combobox->currentIndex() == -1 )
        {
            _combobox->setCurrentIndex( _combobox->count() - 1 );
        }
    }
}
void FeatureSelector::object_removed( QSharedPointer<QObject> object )
{
    if( auto feature = object.objectCast<Feature>() )
    {
        _combobox->removeItem( _combobox->findData( QVariant::fromValue( QWeakPointer<Feature>( feature ) ) ) );
    }
}