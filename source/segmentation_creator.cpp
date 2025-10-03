#include "segmentation_creator.hpp"

#include "database.hpp"
#include "python.hpp"
#include "segmentation.hpp"
#include "segment_selector.hpp"

#include <qcombobox.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlistwidget.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qstackedwidget.h>
#include <qtoolbutton.h>

QSharedPointer<Segmentation> SegmentationCreator::execute( const Database& database )
{
    auto segmentation_creator = SegmentationCreator { database };
    if( segmentation_creator.exec() == QDialog::Accepted )
    {
        return segmentation_creator.segmentation();
    }
    return QSharedPointer<Segmentation> { nullptr };
}

SegmentationCreator::SegmentationCreator( const Database& database ) : QDialog {}, _database { database }
{
    const auto dataset = _database.dataset();
    const auto segmentation = _database.segmentation();

    this->setWindowTitle( "Create Segmentation..." );
    this->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    // Initialize general properties
    auto segment_selector = new SegmentSelector { segmentation };

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

    auto algorithm = new QComboBox {};
    algorithm->addItem( "K-means" );

    // Initialize K-means properties
    auto kmeans_cluster_count = new QSpinBox {};
    kmeans_cluster_count->setRange( 2, static_cast<int>( dataset->element_count() ) );
    kmeans_cluster_count->setValue( 5 );

    auto kmeans_random_state = new QSpinBox {};
    kmeans_random_state->setRange( std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max() );
    kmeans_random_state->setValue( 42 );

    auto kmeans_widget = new QWidget {};
    auto kmeans_layout = new QFormLayout { kmeans_widget };
    kmeans_layout->setContentsMargins( 0, 0, 0, 0 );
    kmeans_layout->addRow( "Cluster Count", kmeans_cluster_count );
    kmeans_layout->addRow( "Random State", kmeans_random_state );

    auto algorithm_properties = new QStackedWidget {};
    algorithm_properties->setContentsMargins( 20, 0, 0, 0 );
    algorithm_properties->addWidget( kmeans_widget );

    auto button_create = new QPushButton { "Create" };

    auto layout = new QFormLayout { this };

    layout->addRow( "Filter", segment_selector );
    layout->addRow( "Channel Filter", channels );
    layout->addRow( "", channels_select_layout );
    layout->addRow( "Algorithm", algorithm );
    layout->addRow( algorithm_properties );
    layout->addRow( button_create );

    QObject::connect( algorithm, &QComboBox::currentIndexChanged, algorithm_properties, &QStackedWidget::setCurrentIndex );
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

    QObject::connect( button_create, &QPushButton::clicked, [=] ()
    {
        auto interpreter = py::interpreter {};
        const auto dataset = _database.dataset();
        const auto segmentation = _database.segmentation();

        auto dataset_memoryview = std::optional<py::memoryview> {};
        dataset->visit( [&dataset_memoryview] ( const auto& dataset )
        {
            using value_type = std::remove_cvref_t<decltype( dataset )>::value_type;
            dataset_memoryview = py::memoryview::from_buffer(
                dataset.intensities().data(),
                { dataset.element_count(), dataset.channel_count() },
                { dataset.channel_count() * sizeof( value_type ), sizeof( value_type ) }
            );
        } );
        if( !dataset_memoryview.has_value() )
        {
            QMessageBox::critical( nullptr, "Create Segmentation...", "Cannot create a segmentation for this dataset" );
            this->reject();
            return;
        }

        const auto segmentation_memoryview = py::memoryview::from_buffer(
            segmentation->segment_numbers().data(),
            { segmentation->element_count() },
            { sizeof( uint32_t ) }
        );

        const auto selected_segment = segment_selector->selected_segment();
        const auto segment_number = selected_segment ? static_cast<int32_t>( selected_segment->number() ) : -1;

        auto channel_indices = std::vector<size_t> {};
        for( const auto item : channels->selectedItems() )
            channel_indices.push_back( channels->row( item ) );

        using namespace py::literals;
        auto locals = py::dict {
            "dataset"_a = dataset_memoryview,
            "segmentation"_a = segmentation_memoryview,
            "segment_number"_a = segment_number,
            "channel_indices"_a = channel_indices,
            "algorithm"_a = algorithm->currentText().toStdString(),
            "error"_a = std::string {},

            "kmeans_cluster_count"_a = static_cast<int>( kmeans_cluster_count->value() ),
            "kmeans_random_state"_a = kmeans_random_state->value()
        };

        try
        {
            py::exec( R"(
try:
    import csv
    import numpy as np

    dataset         = np.asarray( dataset, copy=False )
    segmentation    = np.asarray( segmentation, copy=False )
    print( f"[Segmentation] Dataset:           ({dataset.shape}, {dataset.dtype}), Segmentation: ({segmentation.shape}, {segmentation.dtype}) " )

    element_indices     = ( np.argwhere( segmentation == segment_number ) if segment_number >= 0 else np.arange( segmentation.shape[0] ) ).astype( np.uint32 ).flatten()
    channel_indices     = np.array( channel_indices, dtype=np.int64 )
    filtered_dataset    = dataset[np.ix_(element_indices, channel_indices)].copy()
    print( f"[Segmentation] Filtered dataset:  ({dataset.shape}, {dataset.dtype}) " )
                        
    if np.any( filtered_dataset.shape == 0 ):
        raise ValueError( "Filtered dataset cannot be empty" )
                        
    segment_numbers = np.zeros( ( segmentation.shape[0], ), dtype=np.uint32 )

    if algorithm == "K-means":
        from sklearn.cluster import KMeans
        kmeans = KMeans(
            n_clusters      = kmeans_cluster_count,
            random_state    = kmeans_random_state,
            init            = 'k-means++',
            max_iter        = 1000,
            verbose         = 2
        ).fit( filtered_dataset )
        segment_numbers[element_indices] = kmeans.labels_ + 1

    else:
        raise ValueError( f"Unknown segmentation algorithm: {algorithm}" )

    print( f"[Segmentation] Finished computing segmentation!" )
    segment_numbers_maximum = np.max( segment_numbers )

except Exception as exception:
    error = str( exception ))", py::globals(), locals );
        }
        catch( py::error_already_set& error )
        {
            locals["error"] = error.what();
        }

        if( const auto error = locals["error"].cast<std::string>(); error.empty() )
        {
            const auto segment_numbers = locals["segment_numbers"].cast<py::array_t<uint32_t>>();
            const auto segment_numbers_maximum = locals["segment_numbers_maximum"].cast<uint32_t>();

            _segmentation.reset( new Segmentation { segmentation->element_count() } );
            while( _segmentation->segment_count() < segment_numbers_maximum + 1 )
            {
                _segmentation->append_segment();
            }

            auto editor = _segmentation->editor();
            for( py::ssize_t i = 0; i < segment_numbers.size(); ++i )
            {
                editor.update_value( static_cast<uint32_t>( i ), segment_numbers.at( i ) );
            }

            this->accept();
        }
        else
        {
            Console::error( std::format( "Python error during segmentation creation: {}", error ) );
            QMessageBox::critical( nullptr, "", "Failed to create segmentation", QMessageBox::Ok );
            this->reject();
        }
    } );
}

QSharedPointer<Segmentation> SegmentationCreator::segmentation() const noexcept
{
    return _segmentation;
}