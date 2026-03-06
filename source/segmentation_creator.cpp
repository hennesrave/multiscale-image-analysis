#include "segmentation_creator.hpp"

#include "database.hpp"
#include "python.hpp"
#include "segmentation.hpp"
#include "segment_selector.hpp"

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qdesktopservices.h>
#include <qformlayout.h>
#include <qlayout.h>
#include <qlistwidget.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qstackedwidget.h>
#include <qtoolbutton.h>
#include <qurl.h>

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

    auto target_combobox = new QComboBox {};
    target_combobox->addItem( "Dataset" );
    target_combobox->addItem( "Embedding" );

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

    auto target_dataset_widget = new QWidget {};
    auto target_dataset_layout = new QFormLayout { target_dataset_widget };
    target_dataset_layout->setContentsMargins( 0, 0, 0, 0 );
    target_dataset_layout->addRow( "Channel Filter", channels );
    target_dataset_layout->addRow( "", channels_select_layout );

    auto target_properties = new QStackedWidget {};
    target_properties->setContentsMargins( 20, 0, 0, 0 );
    target_properties->addWidget( target_dataset_widget );
    target_properties->addWidget( new QWidget {} );

    auto algorithm_combobox = new QComboBox {};
    algorithm_combobox->addItem( "K-means" );
    algorithm_combobox->addItem( "HDBSCAN" );
    algorithm_combobox->addItem( "Leiden" );

    auto algorithm_toolbutton = new QToolButton {};
    algorithm_toolbutton->setIcon( QIcon { ":/question.svg" } );
    algorithm_toolbutton->setCursor( Qt::ArrowCursor );

    auto algorithm_layout = new QHBoxLayout {};
    algorithm_layout->setContentsMargins( 0, 0, 0, 0 );
    algorithm_layout->setSpacing( 5 );
    algorithm_layout->addWidget( algorithm_combobox );
    algorithm_layout->addWidget( algorithm_toolbutton );

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

    // Initialize HDBSCAN properties
    auto hdbscan_min_cluster_size = new QSpinBox {};
    hdbscan_min_cluster_size->setRange( 2, static_cast<int>( dataset->element_count() ) );
    hdbscan_min_cluster_size->setValue( 5 );

    auto hdbscan_min_samples = new QSpinBox {};
    hdbscan_min_samples->setRange( 1, static_cast<int>( dataset->element_count() ) );
    hdbscan_min_samples->setValue( 5 );

    auto hdbscan_cluster_selection_epsilon = new QDoubleSpinBox {};
    hdbscan_cluster_selection_epsilon->setRange( 0.0, 3.0 );
    hdbscan_cluster_selection_epsilon->setSingleStep( 0.05 );
    hdbscan_cluster_selection_epsilon->setValue( 0.0 );

    auto hdbscan_subsampling = new QDoubleSpinBox {};
    hdbscan_subsampling->setRange( 0.01, 1.0 );
    hdbscan_subsampling->setSingleStep( 0.01 );
    hdbscan_subsampling->setValue( 1.0 );

    auto hdbscan_widget = new QWidget {};
    auto hdbscan_layout = new QFormLayout { hdbscan_widget };
    hdbscan_layout->setContentsMargins( 0, 0, 0, 0 );
    hdbscan_layout->addRow( "Minimum Cluster Size", hdbscan_min_cluster_size );
    hdbscan_layout->addRow( "Minimum Samples", hdbscan_min_samples );
    hdbscan_layout->addRow( "Cluster Selection Epsilon", hdbscan_cluster_selection_epsilon );
    hdbscan_layout->addRow( "Subsampling", hdbscan_subsampling );

    // Initialize Leiden properties

    auto leiden_resolution_parameter = new QDoubleSpinBox {};
    leiden_resolution_parameter->setRange( 0.01, 10.0 );
    leiden_resolution_parameter->setSingleStep( 0.01 );
    leiden_resolution_parameter->setValue( 1.0 );

    auto leiden_random_state = new QSpinBox {};
    leiden_random_state->setRange( std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max() );
    leiden_random_state->setValue( 42 );

    auto leiden_widget = new QWidget {};
    auto leiden_layout = new QFormLayout { leiden_widget };
    leiden_layout->setContentsMargins( 0, 0, 0, 0 );
    leiden_layout->addRow( "Resolution Parameter", leiden_resolution_parameter );
    leiden_layout->addRow( "Random State", leiden_random_state );

    auto algorithm_properties = new QStackedWidget {};
    algorithm_properties->setContentsMargins( 20, 0, 0, 0 );
    algorithm_properties->addWidget( kmeans_widget );
    algorithm_properties->addWidget( hdbscan_widget );
    algorithm_properties->addWidget( leiden_widget );

    auto button_create = new QPushButton { "Create" };

    auto layout = new QFormLayout { this };

    layout->addRow( "Filter", segment_selector );
    layout->addRow( "Target", target_combobox );
    layout->addRow( target_properties );
    layout->addRow( "Algorithm", algorithm_layout );
    layout->addRow( algorithm_properties );
    layout->addRow( button_create );

    QObject::connect( target_combobox, &QComboBox::currentIndexChanged, target_properties, &QStackedWidget::setCurrentIndex );
    QObject::connect( algorithm_combobox, &QComboBox::currentIndexChanged, algorithm_properties, &QStackedWidget::setCurrentIndex );
    QObject::connect( algorithm_toolbutton, &QToolButton::clicked, [=]
    {
        if( algorithm_combobox->currentText() == "K-means" )
        {
            QDesktopServices::openUrl( QUrl { "https://scikit-learn.org/stable/modules/generated/sklearn.cluster.KMeans.html" } );
        }
        else if( algorithm_combobox->currentText() == "HDBSCAN" )
        {
            QDesktopServices::openUrl( QUrl { "https://hdbscan.readthedocs.io/en/latest/parameter_selection.html" } );
        }
        else if( algorithm_combobox->currentText() == "Leiden" )
        {
            QDesktopServices::openUrl( QUrl { "https://leidenalg.readthedocs.io/en/stable/api_reference.html#leidenalg.RBConfigurationVertexPartition" } );
        }
        else
        {
            QMessageBox::warning( nullptr, "Create Segmentation...", "Unavailable" );
        }
    } );
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
        const auto segmentation = _database.segmentation();
        const auto dataset = _database.dataset();
        const auto embedding = _database.embedding();

        if( target_combobox->currentText() == "Embedding" && !embedding )
        {
            QMessageBox::critical( nullptr, "Create Segmentation...", "No embedding available" );
            return;
        }

        if( algorithm_combobox->currentText() == "Leiden" && target_combobox->currentText() != "Embedding" )
        {
            QMessageBox::critical( nullptr, "Create Segmentation...", "Leiden algorithm requires an embedding target" );
            return;
        }

        const auto segmentation_memoryview = py::memoryview::from_buffer(
            segmentation->segment_numbers().data(),
            { segmentation->element_count() },
            { sizeof( uint32_t ) }
        );

        const auto selected_segment = segment_selector->selected_segment();
        const auto segment_number = selected_segment ? static_cast<int32_t>( selected_segment->number() ) : -1;

        if( algorithm_combobox->currentText() == "Leiden" && selected_segment )
        {
            QMessageBox::critical( nullptr, "Create Segmentation...", "Leiden algorithm requires entire dataset" );
            return;
        }

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

        auto channel_indices = std::vector<size_t> {};
        for( const auto item : channels->selectedItems() )
            channel_indices.push_back( channels->row( item ) );

        auto embedding_coordinates_memoryview = std::optional<py::memoryview> {};
        auto embedding_indices_memoryview = std::optional<py::memoryview> {};

        if( embedding )
        {
            const auto& indices = embedding->indices();
            embedding_indices_memoryview = py::memoryview::from_buffer(
                indices.data(),
                { indices.size() },
                { sizeof( uint32_t ) }
            );

            const auto& coordinates = embedding->coordinates();
            embedding_coordinates_memoryview = py::memoryview::from_buffer(
                reinterpret_cast<const float*>( coordinates.data() ),
                { coordinates.size(), size_t { 2 } },
                { 2 * sizeof( float ), sizeof( float ) }
            );
        }


        using namespace py::literals;
        auto locals = py::dict {
            "segmentation"_a = segmentation_memoryview,
            "segment_number"_a = segment_number,
            "target"_a = target_combobox->currentText().toStdString(),

            "dataset"_a = dataset_memoryview,
            "channel_indices"_a = channel_indices,

            "embedding"_a = embedding_coordinates_memoryview,
            "embedding_indices"_a = embedding_indices_memoryview,

            "algorithm"_a = algorithm_combobox->currentText().toStdString(),

            "kmeans_cluster_count"_a = static_cast<int>( kmeans_cluster_count->value() ),
            "kmeans_random_state"_a = kmeans_random_state->value(),

            "hdbscan_min_cluster_size"_a = static_cast<int>( hdbscan_min_cluster_size->value() ),
            "hdbscan_min_samples"_a = static_cast<int>( hdbscan_min_samples->value() ),
            "hdbscan_cluster_selection_epsilon"_a = hdbscan_cluster_selection_epsilon->value(),
            "hdbscan_subsampling"_a = hdbscan_subsampling->value(),

            "leiden_resolution_parameter"_a = leiden_resolution_parameter->value(),
            "leiden_random_state"_a = leiden_random_state->value(),
            "leiden_model"_a = embedding? embedding->model() : py::none {},

            "error"_a = std::string {},
            "unassigned_count"_a = 0
        };

        try
        {
            py::exec( R"(
try:
    import csv
    import numpy as np

    segmentation    = np.asarray( segmentation, copy=False )
    print( f"[Segmentation] Segmentation: ({segmentation.shape}, {segmentation.dtype}) " )

    element_indices = ( np.argwhere( segmentation == segment_number ) if segment_number >= 0 else np.arange( segmentation.shape[0] ) ).astype( np.uint32 ).flatten()
    print( f"[Segmentation] Element indices: ({element_indices.shape}, {element_indices.dtype}) " )

    algorithm_input = None
    if target == "Dataset":
        dataset         = np.asarray( dataset, copy=False )
        channel_indices = np.array( channel_indices, dtype=np.int64 )
        
        if len( element_indices ) == dataset.shape[0] and len( channel_indices ) == dataset.shape[1]:
            algorithm_input = dataset
        else:
            algorithm_input = dataset[np.ix_(element_indices, channel_indices)]

    elif target == "Embedding":
        if embedding is None:
            raise ValueError( "No embedding available" )

        indices         = np.asarray( embedding_indices, copy=False )
        element_indices = np.intersect1d( element_indices, indices, assume_unique=True )
        indices         = np.where( np.isin( indices, element_indices ) )[0]
        print( f"[Segmentation] Element indices after intersecting with embedding indices: ({element_indices.shape}, {element_indices.dtype}) " )

        embedding   = np.asarray( embedding, copy=False )
        
        if len( indices ) == embedding.shape[0]:
            algorithm_input = embedding
        else:
            algorithm_input = embedding[indices]
    else:
        raise ValueError( f"Unknown target: {target}" )

    print( f"[Segmentation] Algorithm input: ({algorithm_input.shape}, {algorithm_input.dtype}) " )
    if np.any( algorithm_input.shape == 0 ):
        raise ValueError( "Algorithm input cannot be empty" )
    
    segment_numbers = np.zeros( ( segmentation.shape[0], ), dtype=np.uint32 )

    if algorithm == "K-means":
        print( f"[Segmentation] Running K-means... " )

        from sklearn.cluster import KMeans
        clustering = KMeans(
            n_clusters      = kmeans_cluster_count,
            random_state    = kmeans_random_state,
            init            = 'k-means++',
            max_iter        = 1000,
            verbose         = 2
        ).fit( algorithm_input )
        segment_numbers[element_indices] = clustering.labels_ + 1

    elif algorithm == "HDBSCAN":
        print( f"[Segmentation] Running HDBSCAN... " )

        import hdbscan

        if hdbscan_subsampling == 1.0:
            average_standard_deviation = np.mean( np.std( algorithm_input, axis=0 ) )
            hdbscan_cluster_selection_epsilon = float( hdbscan_cluster_selection_epsilon * average_standard_deviation )

            clustering = hdbscan.HDBSCAN(
                min_cluster_size            = hdbscan_min_cluster_size,
                min_samples                 = hdbscan_min_samples,
                cluster_selection_epsilon   = hdbscan_cluster_selection_epsilon,
                metric                      = "euclidean",
                core_dist_n_jobs            = 1
            ).fit( algorithm_input )
            segment_numbers[element_indices] = np.maximum( clustering.labels_ + 1, 0 )
        
        else:
            np.random.seed( 42 )
            subsampling_filter          = np.random.choice( (True, False), algorithm_input.shape[0], p=(hdbscan_subsampling, 1.0 - hdbscan_subsampling) )
            subsampling_algorithm_input = algorithm_input[subsampling_filter, :]
            print( f"[Segmentation] Subsampling algorithm input: ({subsampling_algorithm_input.shape}, {subsampling_algorithm_input.dtype}) " )

            average_standard_deviation = np.mean( np.std( subsampling_algorithm_input, axis=0 ) )
            hdbscan_cluster_selection_epsilon = float( hdbscan_cluster_selection_epsilon * average_standard_deviation )

            clustering = hdbscan.HDBSCAN(
                min_cluster_size            = hdbscan_min_cluster_size,
                min_samples                 = hdbscan_min_samples,
                cluster_selection_epsilon   = hdbscan_cluster_selection_epsilon,
                metric                      = "euclidean",
                core_dist_n_jobs            = 1
            ).fit( subsampling_algorithm_input )

            segment_numbers[element_indices[subsampling_filter]] = np.maximum( clustering.labels_ + 1, 0 )
    
    elif algorithm == "Leiden":
        print( f"[Segmentation] Running Leiden... " )
        import igraph
        import umap

        if leiden_model is None or not isinstance( leiden_model[0], umap.UMAP ):
            raise ValueError( "Leiden algorithm requires a UMAP embedding" )

        leiden_model, model_datapoint_indices = leiden_model
        model_datapoint_indices = np.intersect1d( element_indices, model_datapoint_indices, assume_unique=True )

        print( f"[Segmentation] Building graph... " )
        coo_graph   = leiden_model.graph_.tocoo()
        sources     = coo_graph.row
        targets     = coo_graph.col
        weights     = coo_graph.data

        graph               = igraph.Graph( edges=list( zip( sources, targets ) ), directed=False )
        graph.es["weight"]  = weights

        print( f"[Segmentation] Running Leiden... " )

        import leidenalg
        partition = leidenalg.find_partition(
            graph,
            leidenalg.RBConfigurationVertexPartition,
            weights="weight",
            resolution_parameter=leiden_resolution_parameter,
            seed=leiden_random_state
        )

        labels = np.array( partition.membership, dtype=np.uint32 ) + 1
        segment_numbers[model_datapoint_indices] = labels

    else:
        raise ValueError( f"Unknown segmentation algorithm: {algorithm}" )

    segment_numbers_maximum = np.max( segment_numbers )
    print( f"[Segmentation] Segments found: {segment_numbers_maximum}" )

    if segment_numbers_maximum == 0:
        raise ValueError( "No segments found" )
    
    unassigned_count = np.sum( segment_numbers[element_indices] == 0 )
    print( f"[Segmentation] Unassigned: {unassigned_count}" )

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

            if( segment_numbers_maximum > 20 )
            {
                const auto response = QMessageBox::question(
                    nullptr,
                    "Create Segmentation...",
                    QString::fromStdString( std::format( "The algorithm produced {} segments, which may lead to performance issues. Continue?", segment_numbers_maximum ) ),
                    QMessageBox::Yes | QMessageBox::No
                );
                if( response == QMessageBox::No )
                {
                    this->reject();
                    return;
                }
            }

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