#include "workspace.hpp"

#include "database.hpp"
#include "utility.hpp"

#include "feature_selector.hpp"

#include "boxplot_viewer.hpp"
#include "colormap_viewer.hpp"
#include "embedding_viewer.hpp"
#include "histogram_viewer.hpp"
#include "image_viewer.hpp"
#include "spectrum_viewer.hpp"

#include <qlayout.h>
#include <qsplitter.h>
#include <qstackedlayout.h>
#include <qtimer.h>

Workspace::Workspace( Database& database ) : _database { database }
{
    const auto dataset = _database.dataset();

    if( dataset->spatial_metadata() )
    {
        Console::info( "Creating widgets..." );
        _boxplot_viewer = new BoxplotViewer { database };
        _colormap_viewer = new ColormapViewer { database.colormaps(), database.features() };
        _embedding_viewer = new EmbeddingViewer { database };
        _histogram_viewer = new HistogramViewer { database };
        _image_viewer = new ImageViewer { database };
        _spectrum_viewer = new SpectrumViewer { database };

        Console::info( "Initializing workspace layout..." );
        auto image_viewer_container = new QWidget {};
        auto image_viewer_layout = new QVBoxLayout { image_viewer_container };
        image_viewer_layout->setContentsMargins( 0, 0, 0, 0 );
        image_viewer_layout->setSpacing( 0 );
        image_viewer_layout->addWidget( _colormap_viewer );
        image_viewer_layout->addWidget( _image_viewer, 1 );

        auto histogram_feature_selector = new FeatureSelector { database.features() };

        auto histogram_viewer_container = new QWidget {};
        auto histogram_viewer_layout = new QVBoxLayout { histogram_viewer_container };
        histogram_viewer_layout->setContentsMargins( 0, 3, 0, 0 );
        histogram_viewer_layout->setSpacing( 0 );
        histogram_viewer_layout->addWidget( histogram_feature_selector, 0, Qt::AlignCenter );
        histogram_viewer_layout->addWidget( _histogram_viewer, 1 );

        auto splitter_histogram_boxplot = new QSplitter { Qt::Vertical };
        splitter_histogram_boxplot->addWidget( histogram_viewer_container );
        splitter_histogram_boxplot->addWidget( _boxplot_viewer );
        splitter_histogram_boxplot->setSizes( { 10000, 10000 } );

        auto splitter_image_spectrum_histogram_boxplot = new QSplitter { Qt::Horizontal };
        splitter_image_spectrum_histogram_boxplot->addWidget( image_viewer_container );
        splitter_image_spectrum_histogram_boxplot->addWidget( _embedding_viewer );
        splitter_image_spectrum_histogram_boxplot->addWidget( splitter_histogram_boxplot );
        splitter_image_spectrum_histogram_boxplot->setSizes( { 30000, 30000, 20000 } );

        auto splitter_vertical = new QSplitter { Qt::Vertical };
        splitter_vertical->addWidget( splitter_image_spectrum_histogram_boxplot );
        splitter_vertical->addWidget( _spectrum_viewer );
        splitter_vertical->setSizes( { 30000, 10000 } );

        auto separator = new QFrame {};
        separator->setFrameShape( QFrame::HLine );
        separator->setFrameShadow( QFrame::Sunken );

        auto layout = new QStackedLayout { this };
        layout->addWidget( splitter_vertical );

        Console::info( "Initialzing callbacks..." );
        QObject::connect( _colormap_viewer, &ColormapViewer::colormap_changed, this, [this] ( QSharedPointer<Colormap> colormap )
        {
            _image_viewer->update_colormap( colormap );
        } );
        QObject::connect( database.features().get(), &CollectionObject::object_appended, [this] ( QSharedPointer<QObject> object )
        {
            if( const auto feature = object.objectCast<Feature>() )
            {
                if( _database.features()->object_count() == 1 )
                {
                    _histogram_viewer->update_feature( feature );
                    _boxplot_viewer->update_feature( feature );
                }
            }
        } );
    }
    else
    {
        _boxplot_viewer = new BoxplotViewer { database };
        // _colormap_viewer = new ColormapViewer { database.colormaps(), database.features() };
        _embedding_viewer = new EmbeddingViewer { database };
        _histogram_viewer = new HistogramViewer { database };
        // _image_viewer = new ImageViewer { database };
        _spectrum_viewer = new SpectrumViewer { database };

        Console::info( "Initializing workspace layout..." );
        auto splitter_embedding_histogram_boxplot = new QSplitter { Qt::Horizontal };
        splitter_embedding_histogram_boxplot->addWidget( _embedding_viewer );
        splitter_embedding_histogram_boxplot->addWidget( _histogram_viewer );
        splitter_embedding_histogram_boxplot->addWidget( _boxplot_viewer );
        splitter_embedding_histogram_boxplot->setSizes( { 10000, 10000, 10000 } );

        auto splitter_vertical = new QSplitter { Qt::Vertical };
        splitter_vertical->addWidget( splitter_embedding_histogram_boxplot );
        splitter_vertical->addWidget( _spectrum_viewer );
        splitter_vertical->setSizes( { 10000, 10000 } );

        auto separator = new QFrame {};
        separator->setFrameShape( QFrame::HLine );
        separator->setFrameShadow( QFrame::Sunken );

        auto layout = new QStackedLayout { this };
        layout->addWidget( splitter_vertical );

        Console::info( "Initialzing callbacks..." );
        QObject::connect( database.features().get(), &CollectionObject::object_appended, [this] ( QSharedPointer<QObject> object )
        {
            if( const auto feature = object.objectCast<Feature>() )
            {
                if( _database.features()->object_count() == 1 )
                {
                    _histogram_viewer->update_feature( feature );
                    _boxplot_viewer->update_feature( feature );
                }
            }
        } );
    }

    // Trigger computations
    emit dataset->intensities_changed();

    // Create default feature
    const auto& statistics = dataset->statistics();
    const auto channel_index = static_cast<uint32_t>( std::max_element( statistics.channel_averages.begin(), statistics.channel_averages.end() ) - statistics.channel_averages.begin() );

    Console::info( std::format( "Dataset has {} channels, using channel index {} for default feature.", dataset->channel_count(), channel_index ) );
    const auto feature = QSharedPointer<DatasetChannelsFeature> { new DatasetChannelsFeature {
        dataset,
        Range<uint32_t> { channel_index, channel_index },
        DatasetChannelsFeature::Reduction::eAccumulate,
        DatasetChannelsFeature::BaselineCorrection::eNone
    } };

    _database.colormaps()->append( QSharedPointer<Colormap1D>::create( ColormapTemplate::turbo.clone() ) );
    _database.features()->append( feature );
}