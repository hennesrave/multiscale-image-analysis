#include "dataset_exporter.hpp"

#include "database.hpp"
#include "dataset.hpp"
#include "filestream.hpp"

#include "segment_selector.hpp"

#include <qcheckbox.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qlayout.h>
#include <qlistwidget.h>
#include <qmessagebox.h>
#include <qpushbutton.h>

void DatasetExporter::execute_dialog( const Database& database, const QSharedPointer<Dataset>& dataset )
{
    const auto filepath = std::filesystem::path { QFileDialog::getSaveFileName( nullptr, "Export Dataset...", "", "*.mia;;*.csv" ).toStdWString() };
    if( !filepath.empty() )
    {
        const auto extension = filepath.extension();

        if( extension == ".mia" )
        {
            auto stream = MIAFileStream { filepath, std::ios::out };
            if( stream )
            {
                stream.write( dataset );
            }
            else
            {
                QMessageBox::critical( nullptr, "", "Failed to open file" );
            }
        }
        else if( extension == ".csv" )
        {
            auto dialog = DatasetExporterCSV { database, dataset, filepath };
            dialog.exec();
        }
    }
}

// ----- DatasetExporterCSV ----- //

DatasetExporterCSV::DatasetExporterCSV( const Database& database, const QSharedPointer<Dataset>& dataset, const std::filesystem::path& filepath )
    : QDialog {}, _database { database }, _dataset { dataset }, _filepath { filepath }
{
    this->setWindowTitle( "Export Dataset..." );
    this->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    // Initialize general properties
    auto segment_selector = new SegmentSelector { _database.segmentation() };

    auto channels = new QListWidget {};
    channels->setSelectionMode( QAbstractItemView::MultiSelection );
    for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        channels->addItem( "Channel " + dataset->channel_identifier( channel_index ) );
    channels->selectAll();

    auto channels_select_all = new QPushButton { "Select All" };
    auto channels_deselect_all = new QPushButton { "Deselect All" };
    auto channels_select_from_features = new QPushButton { "Select from Features" };

    auto channels_select_layout = new QHBoxLayout {};
    channels_select_layout->setContentsMargins( 0, 0, 0, 0 );
    channels_select_layout->setSpacing( 5 );
    channels_select_layout->addWidget( channels_select_all );
    channels_select_layout->addWidget( channels_deselect_all );
    channels_select_layout->addWidget( channels_select_from_features );

    auto button_export = new QPushButton { "Export" };

    auto layout = new QFormLayout { this };

    layout->addRow( "Filter", segment_selector );
    layout->addRow( "Channel Filter", channels );
    layout->addRow( "", channels_select_layout );
    layout->addRow( button_export );

    QObject::connect( channels_select_all, &QPushButton::clicked, channels, &QListWidget::selectAll );
    QObject::connect( channels_deselect_all, &QPushButton::clicked, channels, &QListWidget::clearSelection );
    QObject::connect( channels_select_from_features, &QPushButton::clicked, [this, channels]
    {
        channels->clearSelection();
        for( const auto object : *_database.features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
        {
            const auto channel_range = feature->channel_range();
            for( uint32_t channel_index = channel_range.x; channel_index <= channel_range.y; ++channel_index )
            {
                if( auto item = channels->item( static_cast<int>( channel_index ) ) )
                {
                    item->setSelected( true );
                }
            }
        }
    } );

    QObject::connect( button_export, &QPushButton::clicked, [=] ()
    {
        auto file = QFile { _filepath };
        if( !file.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            QMessageBox::critical( nullptr, "Export Dataset...", "Failed to open file" );
            this->reject();
            return;
        }

        auto stream = QTextStream { &file };
        stream << "index";

        const auto spatial_metadata = _dataset->spatial_metadata();
        if( spatial_metadata )
        {
            stream << ",x,y";
        }

        stream << ",segment_identifier,segment_color";

        auto channel_indices = std::vector<uint32_t> {};
        for( const auto item : channels->selectedItems() )
            channel_indices.push_back( channels->row( item ) );

        for( size_t i = 0; i < channel_indices.size(); ++i )
        {
            stream << ',' + _dataset->channel_identifier( channel_indices[i] );
        }
        stream << '\n';

        const auto selected_segment = segment_selector->selected_segment();
        const auto segment_number = selected_segment ? static_cast<int32_t>( selected_segment->number() ) : -1;
        const auto segmentation = _database.segmentation();

        if( selected_segment )
        {
            if( selected_segment->element_count() == 0 )
            {
                QMessageBox::warning( nullptr, "Export Dataset...", "The selected segment is empty" );
                return;
            }
        }

        _dataset->visit( [&] ( const auto& dataset )
        {
            const auto& intensities = dataset.intensities();

            for( uint32_t element_index = 0; element_index < dataset.element_count(); ++element_index )
            {
                if( !selected_segment || segmentation->segment_number( element_index ) == segment_number )
                {
                    stream << element_index;

                    if( spatial_metadata )
                    {
                        const auto coordinates = spatial_metadata->coordinates( element_index );
                        stream << ',' << coordinates.x << ',' << coordinates.y;
                    }

                    const auto element_segment_number = segmentation->segment_number( element_index );
                    const auto element_segment = segmentation->segment( element_segment_number );

                    stream << ',' << element_segment->identifier() << ',' << element_segment->color().qcolor().name();

                    for( const auto channel_index : channel_indices )
                    {
                        const auto value = static_cast<double>( intensities.value( { element_index, channel_index } ) );
                        stream << ',' << value;
                    }
                    stream << '\n';
                }
            }
        } );

        this->accept();
    } );
}
