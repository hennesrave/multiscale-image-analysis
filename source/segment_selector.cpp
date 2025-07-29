#include "segment_selector.hpp"

#include "segmentation.hpp"

#include <qcombobox.h>
#include <qlayout.h>

// ----- SegmentSelector ----- //

SegmentSelector::SegmentSelector( QSharedPointer<const Segmentation> segmentation ) : QWidget {}, _segmentation { segmentation }
{
    _combobox = new QComboBox {};
    _combobox->setPlaceholderText( "Select Segment..." );
    _combobox->setSizeAdjustPolicy( QComboBox::AdjustToContents );

    _combobox->addItem( "Entire Dataset", QVariant::fromValue( QSharedPointer<Segment>() ) );
    _combobox->setCurrentIndex( 0 );

    auto layout = new QHBoxLayout { this };
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->addWidget( _combobox );

    QObject::connect( segmentation.get(), &Segmentation::segment_appended, this, &SegmentSelector::segment_appended );
    QObject::connect( segmentation.get(), &Segmentation::segment_removed, this, &SegmentSelector::segment_removed );
    QObject::connect( _combobox, &QComboBox::currentIndexChanged, this, [this] ( int index )
    {
        const auto segment = _combobox->itemData( index ).value<QSharedPointer<Segment>>();
        emit selected_segment_changed( _selected_segment = segment );
    } );

    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
    {
        this->segment_appended( segmentation->segment( segment_number ) );
    }
}

QSharedPointer<Segment> SegmentSelector::selected_segment() const
{
    return _combobox->itemData( _combobox->currentIndex() ).value<QSharedPointer<Segment>>();
}

void SegmentSelector::segment_appended( QSharedPointer<Segment> segment )
{
    if( segment->number() > 0 )
    {
        auto pixmap = QPixmap { 16, 16 };
        pixmap.fill( segment->color().qcolor() );

        _combobox->addItem( QIcon { pixmap }, segment->identifier(), QVariant::fromValue( segment ) );
        QObject::connect( segment.data(), &Segment::identifier_changed, this, [this, segment] ( const QString& identifier )
        {
            if( const auto index = _combobox->findData( QVariant::fromValue( segment ) ); index >= 0 )
            {
                _combobox->setItemText( index, identifier );
            }
        } );
    }
}
void SegmentSelector::segment_removed( QSharedPointer<Segment> segment )
{
    if( const auto index = _combobox->findData( QVariant::fromValue( segment ) ); index >= 0 )
    {
        _combobox->removeItem( index );
    }
}