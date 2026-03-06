#include "segmentation_manager.hpp"

#include "database.hpp"

#include <qcolordialog.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistwidget.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <qwidgetaction.h>

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

        auto button_merge = new QToolButton {};
        button_merge->setIcon( QIcon { ":/merge.svg" } );
        button_merge->setToolTip( "Merge Segment" );

        auto button_remove = new QToolButton {};
        button_remove->setIcon( QIcon { ":/delete.svg" } );
        button_remove->setToolTip( "Remove Segment" );

        auto layout = new QHBoxLayout {};
        layout->setContentsMargins( 0, 0, 0, 0 );
        layout->setSpacing( 2 );
        layout->addWidget( button_color );
        layout->addWidget( lineedit_identifier );
        layout->addWidget( button_merge );
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
        QObject::connect( button_merge, &QToolButton::clicked, this, [this, pointer = QWeakPointer { segment }, button_merge, button_remove]
        {
            if( auto source = pointer.lock() )
            {
                auto context_menu = QMenu {};

                auto label_target_segment = new QLabel { "Target Segment" };
                label_target_segment->setAlignment( Qt::AlignCenter );

                auto widget_action = new QWidgetAction { &context_menu };
                widget_action->setDefaultWidget( label_target_segment );

                context_menu.addAction( widget_action );
                context_menu.addSeparator();

                const auto segmentation     = _database.segmentation();
                const auto source_number    = source->number();

                for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
                {
                    const auto target           = segmentation->segment( segment_number );
                    const auto target_number    = target->number();

                    if( target != source )
                    {
                        auto pixmap = QPixmap { 16, 16 };
                        pixmap.fill( target->color().qcolor() );
                        auto action = context_menu.addAction( QIcon { pixmap }, target->identifier(), [segmentation, source_number, target_number, button_remove]
                        {
                            {
                                auto editor = segmentation->editor();
                                for( uint32_t element_index = 0; element_index < segmentation->element_count(); ++element_index )
                                {
                                    if( segmentation->segment_number( element_index ) == source_number )
                                    {
                                        editor.update_value( element_index, target_number );
                                    }
                                }
                            }
                            button_remove->click();
                        } );
                    }
                }

                context_menu.exec( button_merge->mapToGlobal( QPoint { 0, button_merge->height() } ) );
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

                    //while( auto item = layout->takeAt( 0 ) )
                    //{
                    //    delete item->widget();
                    //    delete item;
                    //}
                    //segments_layout->removeItem( layout );
                    //delete layout;
                }
            }
        } );

        QObject::connect( segment.get(), &Segment::color_changed, this, update_button_color );
        QObject::connect( segment.get(), &Segment::identifier_changed, this, [lineedit_identifier] ( const QString& identifier )
        {
            lineedit_identifier->setText( identifier );
        } );

        QObject::connect( _database.segmentation().get(), &Segmentation::segment_removed, this, [this, pointer = QWeakPointer { segment }, segments_layout, layout]( const QSharedPointer<Segment>& removed_segment )
        {
            if( auto segment = pointer.lock() )
            {
                if( segment == removed_segment )
                {
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
    };

    auto button_create_segment = new QPushButton { "Create Segment" };
    button_create_segment->setStyleSheet( "QPushButton { padding: 2px 10px 2px 10px; }" );

    auto button_merge_segments = new QPushButton { "Merge Segments" };
    button_merge_segments->setStyleSheet( "QPushButton { padding: 2px 10px 2px 10px; }" );

    auto controls = new QHBoxLayout {};
    controls->setContentsMargins( 0, 0, 0, 0 );
    controls->setSpacing( 5 );
    controls->addWidget( button_create_segment );
    controls->addWidget( button_merge_segments );

    auto layout = new QVBoxLayout { this };
    layout->setContentsMargins( 20, 10, 20, 10 );
    layout->setSpacing( 20 );
    layout->addLayout( segments_layout );
    layout->addStretch( 1 );
    layout->addLayout( controls );

    QObject::connect( button_create_segment, &QPushButton::clicked, this, [this, append_segment]
    {
        _database.update_active_segment( _database.segmentation()->append_segment() );
    } );
    QObject::connect( button_merge_segments, &QPushButton::clicked, this, [this]
    {
        const auto segmentation = _database.segmentation();

        auto segments_list = new QListWidget {};
        segments_list->setSelectionMode( QAbstractItemView::MultiSelection );

        for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
        {
            const auto segment = segmentation->segment( segment_number );

            auto pixmap = QPixmap { 16, 16 };
            pixmap.fill( segment->color().qcolor() );

            auto item = new QListWidgetItem { QIcon { pixmap }, segment->identifier() };
            item->setData( Qt::UserRole, segment_number );

            segments_list->addItem( item );
        }

        auto button_merge = new QPushButton { "Merge" };

        auto layout = new QVBoxLayout {};
        layout->setContentsMargins( 10, 10, 10, 10 );
        layout->setSpacing( 10 );

        layout->addWidget( segments_list );
        layout->addWidget( button_merge );

        auto dialog = QDialog {};
        dialog.setWindowTitle( "Merge Segments..." );
        dialog.setLayout( layout );

        QObject::connect( button_merge, &QPushButton::clicked, this, [&]
        {
            auto source_segment_numbers = std::vector<uint32_t> {};
            for( const auto item : segments_list->selectedItems() )
            {
                source_segment_numbers.push_back( item->data( Qt::UserRole ).toUInt() );
            }

            if( source_segment_numbers.size() < 2 )
            {
                QMessageBox::warning( &dialog, "Merge Segments...", "Please select at least two segments to merge." );
                return;
            }

            auto target_segment = segmentation->append_segment();
            const auto target_segment_number = target_segment->number();
            {
                auto editor = segmentation->editor();
                for( uint32_t element_index = 0; element_index < segmentation->element_count(); ++element_index )
                {
                    const auto segment_number = segmentation->segment_number( element_index );

                    auto contained = false;
                    for( const auto source_segment_number : source_segment_numbers )
                    {
                        if( segmentation->segment_number( element_index ) == source_segment_number )
                        {
                            editor.update_value( element_index, target_segment_number );
                            break;
                        }
                    }
                }
            }

            for( size_t i = source_segment_numbers.size(); i-- > 0; )
            {
                segmentation->remove_segment( segmentation->segment( source_segment_numbers[i] ) );
            }

            _database.update_active_segment( target_segment );

            dialog.accept();
        } );

        dialog.exec();
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