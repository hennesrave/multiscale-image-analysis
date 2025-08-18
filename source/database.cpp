#include "database.hpp"

#include "segmentation_manager.hpp"

#include <qfiledialog.h>
#include <qmessagebox.h>

Database::Database( QSharedPointer<Dataset> dataset ) : _dataset { dataset }
{
    _segmentation = QSharedPointer<Segmentation>::create( _dataset->element_count() );
    _features = QSharedPointer<Storage<Feature>>::create();
    _colormaps = QSharedPointer<Storage<Colormap>>::create();

    for( int i = 0; i < 5; ++i )
        _segmentation->append_segment();
    _active_segment = _segmentation->segment( 1 );
}

QSharedPointer<Dataset> Database::dataset() const noexcept
{
    return _dataset;
}
QSharedPointer<Segmentation> Database::segmentation() const noexcept
{
    return _segmentation;
}
QSharedPointer<Storage<Feature>> Database::features() const noexcept
{
    return _features;
}
QSharedPointer<Storage<Colormap>> Database::colormaps() const noexcept
{
    return _colormaps;
}

QSharedPointer<Segment> Database::active_segment() const
{
    if( !_active_segment )
    {
        _active_segment = _segmentation->segment( 1 );
    }
    return _active_segment.lock();
}
void Database::update_active_segment( QSharedPointer<Segment> segment )
{
    if( _active_segment != segment )
    {
        if( segment )
        {
            _active_segment = segment;
        }
        else
        {
            _active_segment = _segmentation->segment( 1 );
        }
        emit active_segment_changed( _active_segment );
    }
}

std::optional<uint32_t> Database::highlighted_element_index() const noexcept
{
    return _highlighted_element_index;
}
void Database::update_highlighted_element_index( std::optional<uint32_t> index )
{
    if( _highlighted_element_index != index )
    {
        emit highlighted_element_index_changed( _highlighted_element_index = index );
    }
}

std::optional<uint32_t> Database::highlighted_channel_index() const noexcept
{
    return _highlighted_channel_index;
}
void Database::update_highlighted_channel_index( std::optional<uint32_t> index )
{
    if( _highlighted_channel_index != index )
    {
        emit highlighted_channel_index_changed( _highlighted_channel_index = index );
    }
}

void Database::populate_segmentation_menu( QMenu& context_menu )
{
    for( uint32_t segment_number = 1; segment_number < _segmentation->segment_count(); ++segment_number )
    {
        const auto segment = _segmentation->segment( segment_number );

        auto pixmap = QPixmap { 16, 16 };
        pixmap.fill( segment->color().qcolor() );
        auto action = context_menu.addAction( QIcon { pixmap }, segment->identifier(), [this, segment]
        {
            this->update_active_segment( segment );
        } );

        auto font = action->font();
        font.setBold( segment == this->active_segment() );
        action->setFont( font );
    }

    context_menu.addSeparator();
    context_menu.addAction( "Create Segment", [this]
    {
        this->update_active_segment( _segmentation->append_segment() );
    } );
    context_menu.addAction( "Remove Segment", [this]
    {
        if( _segmentation->segment_count() <= 2 )
        {
            QMessageBox::information( nullptr, "", "Cannot remove the last segment." );
        }
        else
        {
            _segmentation->remove_segment( this->active_segment() );
        }
    } );

    auto segmentation_menu = context_menu.addMenu( "Segmentation" );
    segmentation_menu->addAction( "Manage", [this] { SegmentationManager::execute( *this ); } );
    segmentation_menu->addAction( "Import", [this]
    {
        const auto filepath = QFileDialog::getOpenFileName( nullptr, "Import Segmentation", "", "JSON Files (*.json)", nullptr );
        if( !filepath.isEmpty() )
        {
            auto file = QFile { filepath };
            if( file.open( QFile::ReadOnly | QFile::Text ) )
            {
                auto stream = QTextStream { &file };
                const auto json = nlohmann::json::parse( stream.readAll().toStdString() );
                _segmentation->deserialize( json );
            }
            else
            {
                QMessageBox::critical( nullptr, "", "Failed to open file." );
            }
        }
    } );
    segmentation_menu->addAction( "Export", [this]
    {
        const auto filepath = QFileDialog::getSaveFileName( nullptr, "Export Segmentation", "", "JSON Files (*.json)", nullptr );
        if( !filepath.isEmpty() )
        {
            auto file = QFile { filepath };
            if( file.open( QFile::WriteOnly | QFile::Text ) )
            {
                auto stream = QTextStream { &file };
                const auto json = _segmentation->serialize();
                stream << QString::fromStdString( json.dump( 4 ) );
            }
            else
            {
                QMessageBox::critical( nullptr, "", "Failed to open file." );
            }
        }
    } );
}
