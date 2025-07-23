#include "embedding_creator.hpp"

#include "dataset.hpp"
#include "logger.hpp"
#include "python.hpp"
#include "segment_selector.hpp"
#include "segmentation.hpp"

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

QString EmbeddingCreator::execute( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation )
{
    auto embedding_creator = EmbeddingCreator { dataset, segmentation };
    if( embedding_creator.exec() == QDialog::Accepted )
    {
        return embedding_creator.filepath();
    }
    return QString {};
}

EmbeddingCreator::EmbeddingCreator( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation ) : QDialog {}, _dataset { dataset }, _segmentation { segmentation }
{
    this->setWindowTitle( "Create Embedding..." );

    // Initialize general properties
    auto segment_selector = new SegmentSelector { segmentation };

    auto channels = new QListWidget {};
    channels->setSelectionMode( QAbstractItemView::MultiSelection );
    for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
        channels->addItem( "Channel " + dataset->channel_identifier( channel_index ) );
    channels->selectAll();

    auto channels_select_all = new QPushButton { "Select All" };
    auto channels_deselect_all = new QPushButton { "Deselect All" };

    auto channels_select_layout = new QHBoxLayout {};
    channels_select_layout->setContentsMargins( 0, 0, 0, 0 );
    channels_select_layout->setSpacing( 10 );
    channels_select_layout->addWidget( channels_select_all );
    channels_select_layout->addWidget( channels_deselect_all );

    auto normalization = new QComboBox {};
    normalization->addItem( "Z-score" );
    normalization->addItem( "Min-Max" );
    normalization->addItem( "None" );

    auto filepath = new QLabel {};
    filepath->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    filepath->setTextInteractionFlags( Qt::TextSelectableByMouse );
    filepath->setStyleSheet( "QLabel { background: #fafafa; border-radius: 3px; }" );
    filepath->setMaximumWidth( 300 );
    filepath->setContentsMargins( 5, 3, 5, 3 );

    auto filepath_button = new QToolButton {};
    filepath_button->setIcon( QIcon { ":/edit.svg" } );
    filepath_button->setCursor( Qt::ArrowCursor );

    auto filepath_layout = new QHBoxLayout {};
    filepath_layout->setContentsMargins( 0, 0, 0, 0 );
    filepath_layout->setSpacing( 10 );
    filepath_layout->addWidget( filepath, 1 );
    filepath_layout->addWidget( filepath_button );

    auto algorithm = new QComboBox {};
    algorithm->addItem( "PCA" );
    algorithm->addItem( "UMAP" );
    algorithm->addItem( "t-SNE" );

    // Initialize PCA properties
    auto pca_random_state = new QSpinBox {};
    pca_random_state->setRange( std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max() );
    pca_random_state->setValue( 42 );

    auto pca_widget = new QWidget {};
    auto pca_layout = new QFormLayout { pca_widget };
    pca_layout->setContentsMargins( 0, 0, 0, 0 );
    pca_layout->addRow( "Random State", pca_random_state );

    // Initialize UMAP properties
    auto umap_neighbors = new QSpinBox {};
    umap_neighbors->setRange( 2, static_cast<int>( _dataset->element_count() - 1 ) );
    umap_neighbors->setValue( 15 );

    auto umap_minimum_distance = new QDoubleSpinBox {};
    umap_minimum_distance->setRange( 0.0, 1.0 );
    umap_minimum_distance->setSingleStep( 0.01 );
    umap_minimum_distance->setValue( 0.1 );

    auto umap_metric = new QComboBox {};
    umap_metric->addItem( "Euclidean" );
    umap_metric->addItem( "Cosine" );

    auto umap_random_state = new QSpinBox {};
    umap_random_state->setRange( std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max() );
    umap_random_state->setValue( 42 );

    auto umap_subsampling = new QDoubleSpinBox {};
    umap_subsampling->setRange( 0.01, 1.0 );
    umap_subsampling->setSingleStep( 0.01 );
    umap_subsampling->setValue( 1.0 );

    auto umap_widget = new QWidget {};
    auto umap_layout = new QFormLayout { umap_widget };
    umap_layout->setContentsMargins( 0, 0, 0, 0 );
    umap_layout->addRow( "Neighbors", umap_neighbors );
    umap_layout->addRow( "Minimum Distance", umap_minimum_distance );
    umap_layout->addRow( "Metric", umap_metric );
    umap_layout->addRow( "Random State", umap_random_state );
    umap_layout->addRow( "Subsampling", umap_subsampling );

    // Initialize t-SNE properties
    auto tsne_perplexity = new QDoubleSpinBox {};
    tsne_perplexity->setRange( 5.0, 50.0 );
    tsne_perplexity->setSingleStep( 0.1 );
    tsne_perplexity->setValue( 30.0 );

    auto tsne_random_state = new QSpinBox {};
    tsne_random_state->setRange( std::numeric_limits<int>::lowest(), std::numeric_limits<int>::max() );
    tsne_random_state->setValue( 42 );

    auto tsne_widget = new QWidget {};
    auto tsne_layout = new QFormLayout { tsne_widget };
    tsne_layout->setContentsMargins( 0, 0, 0, 0 );
    tsne_layout->addRow( "Perplexity", tsne_perplexity );
    tsne_layout->addRow( "Random State", tsne_random_state );

    auto algorithm_properties = new QStackedWidget {};
    algorithm_properties->setContentsMargins( 20, 0, 0, 0 );
    algorithm_properties->addWidget( pca_widget );
    algorithm_properties->addWidget( umap_widget );
    algorithm_properties->addWidget( tsne_widget );

    auto button_create = new QPushButton { "Create" };

    auto layout = new QFormLayout { this };

    layout->addRow( "Element Filter", segment_selector );
    layout->addRow( "Channel Filter", channels );
    layout->addRow( "", channels_select_layout );
    layout->addRow( "Normalization", normalization );
    layout->addRow( "Filepath", filepath_layout );
    layout->addRow( "Algorithm", algorithm );
    layout->addRow( algorithm_properties );
    layout->addRow( button_create );

    QObject::connect( algorithm, &QComboBox::currentIndexChanged, algorithm_properties, &QStackedWidget::setCurrentIndex );
    QObject::connect( channels_select_all, &QPushButton::clicked, channels, &QListWidget::selectAll );
    QObject::connect( channels_deselect_all, &QPushButton::clicked, channels, &QListWidget::clearSelection );

    QObject::connect( filepath_button, &QToolButton::clicked, [filepath]
    {
        filepath->setText( QFileDialog::getSaveFileName( nullptr, "Choose Filepath...", "", "*.csv" ) );
    } );

    QObject::connect( button_create, &QPushButton::clicked, [=] ()
    {
        if( filepath->text().isEmpty() )
        {
            QMessageBox::warning( nullptr, "Create Embedding...", "Please select a valid filepath." );
            return;
        }

        auto interpreter = py::interpreter {};

        auto dataset_memoryview = std::optional<py::memoryview> {};
        _dataset->visit( [&dataset_memoryview] ( const auto& dataset )
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
            QMessageBox::critical( nullptr, "Create Embedding...", "Cannot create an embedding for this dataset." );
            this->reject();
            return;
        }

        const auto segmentation_memoryview = py::memoryview::from_buffer(
            _segmentation->values().data(),
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
            "normalization"_a = normalization->currentText().toStdString(),
            "filepath"_a = filepath->text().toStdString(),
            "algorithm"_a = algorithm->currentText().toStdString(),
            "error"_a = std::string {},

            "pca_random_state"_a = pca_random_state->value(),

            "umap_neighbors"_a = static_cast<int>( umap_neighbors->value() ),
            "umap_minimum_distance"_a = umap_minimum_distance->value(),
            "umap_metric"_a = umap_metric->currentText().toLower().toStdString(),
            "umap_random_state"_a = umap_random_state->value(),
            "umap_subsampling"_a = umap_subsampling->value(),

            "tsne_perplexity"_a = tsne_perplexity->value(),
            "tsne_random_state"_a = tsne_random_state->value()
        };

        try
        {
            py::exec( R"(
try:
    import csv
    import numpy as np

    dataset         = np.asarray( dataset, copy=False )
    segmentation    = np.asarray( segmentation, copy=False )
    print( f"[Embedding] Dataset:           ({dataset.shape}, {dataset.dtype}), Segmentation: ({segmentation.shape}, {segmentation.dtype}) " )

    element_indices     = (np.argwhere( segmentation == segment_number ) if segment_number >= 0 else np.arange( segmentation.shape[0] )).flatten()
    channel_indices     = np.array( channel_indices, dtype=np.int64 )
    filtered_dataset    = dataset[np.ix_(element_indices, channel_indices)].copy()
    print( f"[Embedding] Filtered dataset:  ({dataset.shape}, {dataset.dtype}) " )
                        
    if np.any( filtered_dataset.shape == 0 ):
        raise ValueError( "Filtered dataset cannot be empty" )
                        
    if normalization != "None":
        print( f"[Embedding] Normalizing dataset... " )
        if normalization == "Z-Score":
            filtered_dataset_mean       = np.mean( filtered_dataset, axis=0 )
            filtered_dataset_std        = np.std( filtered_dataset, axis=0 )
            filtered_dataset            = ( filtered_dataset - filtered_dataset_mean ) / filtered_dataset_std
        elif normalization == "Min-Max":
            filtered_dataset_minimum   = np.min( filtered_dataset )
            filtered_dataset_maximum   = np.max( filtered_dataset )
            filtered_dataset            = ( filtered_dataset - filtered_dataset_minimum ) / ( filtered_dataset_maximum - filtered_dataset_minimum )
                        
    if algorithm == "PCA":
        from sklearn.decomposition import PCA
        pca = PCA(
            n_components    = 2,
            random_state    = pca_random_state
        )
        embedding = pca.fit_transform( filtered_dataset ).astype( np.float32 )

    elif algorithm == "UMAP":
        import umap
        umap = umap.UMAP(
            n_components	= 2,
            n_neighbors		= umap_neighbors,
            min_dist		= umap_minimum_distance,
            metric			= umap_metric,                
            random_state	= umap_random_state,
            verbose			= True
        )
                        
        if umap_subsampling == 1.0:
            embedding               = umap.fit_transform( filtered_dataset ).astype( np.float32 )
        else:
            np.random.seed( 42 )
            subsampling_filter      = np.random.choice( (True, False), filtered_dataset.shape[0], p=(umap_subsampling, 1.0 - umap_subsampling) )
            subsampling_dataset     = filtered_dataset[subsampling_filter, :]
            print( f"[Embedding] Subsampling dataset: {subsampling_dataset.shape}" )
                        
            umap.fit( subsampling_dataset )
            embedding = umap.transform( filtered_dataset ).astype( np.float32 )

    elif algorithm == "t-SNE":
        from sklearn.manifold import TSNE
        tsne = TSNE(
            n_components    = 2,
            perplexity      = tsne_perplexity,
            verbose         = 2
        )
                            
        embedding = tsne.fit_transform( filtered_dataset ).astype( np.float32 )
                        
    else:
        raise ValueError( f"Unknown embedding algorithm: {algorithm}" )
                        
    print( f"[Embedding] Finished computing embedding: {embedding.shape}" )
    embedding = embedding - np.mean( embedding, axis=0 )
    embedding = embedding / np.max( np.abs( embedding ) )
                        
    with open( filepath, "w", newline='' ) as file:
        writer = csv.writer( file )
                            
        for index, element_index in enumerate( element_indices ):                            
            writer.writerow([element_index, embedding[index, 0], embedding[index, 1]])
    print( f"[Embedding] Finished writing result: {filepath}" )
except Exception as exception:
    error = str( exception ))", py::globals(), locals );
        }
        catch( py::error_already_set& error )
        {
            locals["error"] = error.what();
        }

        if( const auto error = locals["error"].cast<std::string>(); error.empty() )
        {
            _filepath = filepath->text();
            this->accept();
        }
        else
        {
            Logger::error() << "Python error during embedding creation: " << error;
            QMessageBox::critical( nullptr, "Create Embedding...", "Failed to create embedding.", QMessageBox::Ok );
            this->reject();
        }
    } );
}

const QString& EmbeddingCreator::filepath() const noexcept
{
    return _filepath;
}