#include "segmentation_manager.hpp"

#include "database.hpp"

#include <qapplication.h>
#include <qcolordialog.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>

SegmentationManager::SegmentationManager( Database& database ) : QDialog {}, _database { database }
{
    this->setWindowTitle( "Segmentation Manager" );

    auto segments_layout = new QVBoxLayout {};
    segments_layout->setContentsMargins( 0, 0, 0, 0 );
    segments_layout->setSpacing( 2 );

    const auto append_segment = [this, segments_layout] ( QSharedPointer<Segment> segment )
    {
        auto button_color = new QToolButton {};
        const auto update_button_color = [button_color, pointer = QWeakPointer { segment }]
        {
            if( auto segment = pointer.lock() )
            {
                auto pixmap = QPixmap { 16, 16 };
                pixmap.fill( segment->color().qcolor() );
                button_color->setIcon( QIcon { pixmap } );
            }
        };
        update_button_color();

        auto lineedit_identifier = new QLineEdit { segment->identifier() };

        auto button_remove = new QToolButton {};
        button_remove->setIcon( QIcon { ":/delete.svg" } );

        auto layout = new QHBoxLayout {};
        layout->setContentsMargins( 0, 0, 0, 0 );
        layout->setSpacing( 2 );
        layout->addWidget( button_color );
        layout->addWidget( lineedit_identifier );
        layout->addWidget( button_remove );

        segments_layout->addLayout( layout );

        QObject::connect( button_color, &QToolButton::clicked, this, [this, pointer = QWeakPointer { segment }]
        {
            if( auto segment = pointer.lock() )
            {
                if( const auto color = QColorDialog::getColor( segment->color().qcolor(), this, "Select Segment Color" ); color.isValid() )
                {
                    segment->update_color( vec4<float> { color.redF(), color.greenF(), color.blueF(), color.alphaF() } );
                }
            }
        } );
        QObject::connect( lineedit_identifier, &QLineEdit::textChanged, this, [this, pointer = QWeakPointer { segment }]( const QString& text )
        {
            if( auto segment = pointer.lock() )
            {
                segment->update_identifier( text );
            }
        } );
        QObject::connect( button_remove, &QToolButton::clicked, this, [this, pointer = QWeakPointer { segment }, segments_layout, layout]
        {
            if( auto segment = pointer.lock() )
            {
                if( _database.segmentation()->segment_count() <= 2 )
                {
                    QMessageBox::information( this, "Segmentation Manager", "Cannot remove the last segment." );
                }
                else
                {
                    _database.segmentation()->remove_segment( segment );

                    while( auto item = layout->takeAt( 0 ) )
                    {
                        delete item->widget();
                        delete item;
                    }
                    segments_layout->removeItem( layout );
                    delete layout;
                }
            }
        } );

        QObject::connect( segment.get(), &Segment::color_changed, this, update_button_color );
        QObject::connect( segment.get(), &Segment::identifier_changed, this, [lineedit_identifier] ( const QString& identifier )
        {
            lineedit_identifier->setText( identifier );
        } );
    };

    auto button_create_segment = new QPushButton { "Create Segment" };

    auto controls = new QHBoxLayout {};
    controls->setContentsMargins( 0, 0, 0, 0 );
    controls->setSpacing( 5 );
    controls->addWidget( button_create_segment );

    auto layout = new QVBoxLayout { this };
    layout->setContentsMargins( 20, 10, 20, 10 );
    layout->setSpacing( 5 );
    layout->addLayout( segments_layout );
    layout->addStretch( 1 );
    layout->addLayout( controls );

    QObject::connect( button_create_segment, &QPushButton::clicked, this, [this, append_segment]
    {
        _database.update_active_segment( _database.segmentation()->append_segment() );
    } );

    const auto segmentation = _database.segmentation();
    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
    {
        append_segment( segmentation->segment( segment_number ) );
    }
    QObject::connect( segmentation.get(), &Segmentation::segment_appended, this, append_segment );
}
void SegmentationManager::execute( Database& database )
{
    auto segmentation_manager = SegmentationManager { database };
    segmentation_manager.exec();
}