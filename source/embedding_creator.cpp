#include "embedding_creator.hpp"

#include "database.hpp"
#include "python.hpp"
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

std::filesystem::path EmbeddingCreator::execute( const Database& database )
{
    auto embedding_creator = EmbeddingCreator { database };
    if( embedding_creator.exec() == QDialog::Accepted )
    {
        return embedding_creator.filepath();
    }
    return std::filesystem::path {};
}

EmbeddingCreator::EmbeddingCreator( const Database& database ) : QDialog {}, _database { database }
{
    const auto dataset = _database.dataset();
    const auto segmentation = _database.segmentation();
    const auto features = _database.features();

    this->setWindowTitle( "Create Embedding..." );
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

    auto features_list = new QListWidget {};
    features_list->setSelectionMode( QAbstractItemView::MultiSelection );
    for( qsizetype feature_index = 0; feature_index < features->object_count(); ++feature_index )
        features_list->addItem( features->object( feature_index )->identifier() );

    auto normalization = new QComboBox {};
    normalization->addItem( "Z-score" );
    normalization->addItem( "Min-Max" );
    normalization->addItem( "None" );

    auto features_contribution = new QDoubleSpinBox {};
    features_contribution->setRange( 0.0, 1.0 );
    features_contribution->setSingleStep( 0.05 );
    features_contribution->setValue( 0.5 );

    auto filepath_label = new QLabel {};
    filepath_label->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    filepath_label->setTextInteractionFlags( Qt::TextSelectableByMouse );
    filepath_label->setStyleSheet( "QLabel { background: #fafafa; border-radius: 3px; }" );
    filepath_label->setMaximumWidth( 300 );
    filepath_label->setContentsMargins( 5, 3, 5, 3 );

    auto filepath_button = new QToolButton {};
    filepath_button->setIcon( QIcon { ":/edit.svg" } );
    filepath_button->setCursor( Qt::ArrowCursor );

    auto filepath_layout = new QHBoxLayout {};
    filepath_layout->setContentsMargins( 0, 0, 0, 0 );
    filepath_layout->setSpacing( 10 );
    filepath_layout->addWidget( filepath_label, 1 );
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
    umap_neighbors->setRange( 2, static_cast<int>( dataset->element_count() - 1 ) );
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

    layout->addRow( "Filter", segment_selector );
    layout->addRow( "Channel Filter", channels );
    layout->addRow( "", channels_select_layout );
    layout->addRow( "Additional Features", features_list );
    layout->addRow( "Normalization", normalization );
    layout->addRow( "Features Contribution", features_contribution );
    layout->addRow( "Filepath", filepath_layout );
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
    QObject::connect( normalization, &QComboBox::currentTextChanged, [features_contribution, normalization]
    {
        features_contribution->setEnabled( normalization->currentText() != "None" );
    } );

    QObject::connect( filepath_button, &QToolButton::clicked, [filepath_label]
    {
        const auto filters = QStringList { "*.csv", "*.mia" };
        const auto filters_string = filters.join( ";;" );
        auto selected_filter = QString { "*.mia" };
        auto filepath = QFileDialog::getSaveFileName( nullptr, "Choose Filepath...", "", filters_string, &selected_filter );

        filepath_label->setText( filepath );
    } );

    QObject::connect( button_create, &QPushButton::clicked, [=] ()
    {
        if( filepath_label->text().isEmpty() )
        {
            QMessageBox::warning( nullptr, "Create Embedding...", "Please select a valid filepath" );
            return;
        }

        auto interpreter        = py::interpreter {};
        const auto dataset      = _database.dataset();
        const auto segmentation = _database.segmentation();
        const auto features     = _database.features();

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
            QMessageBox::critical( nullptr, "Create Embedding...", "Cannot create an embedding for this dataset" );
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

        auto additional_features = std::vector<py::memoryview> {};
        for( const auto item : features_list->selectedItems() )
        {
            const auto feature_index = features_list->row( item );
            const auto feature = features->object( feature_index );
            additional_features.push_back( py::memoryview::from_buffer(
                feature->values().data(),
                { feature->element_count() },
                { sizeof( double ) }
            ) );
        }

        using namespace py::literals;
        auto locals = py::dict {
            "dataset"_a = dataset_memoryview,
            "segmentation"_a = segmentation_memoryview,
            "segment_number"_a = segment_number,
            "channel_indices"_a = channel_indices,
            "additional_features"_a = additional_features,
            "features_contribution"_a = features_contribution->value(),
            "normalization"_a = normalization->currentText().toStdString(),
            "filepath"_a = filepath_label->text().toStdString(),
            "algorithm"_a = algorithm->currentText().toStdString(),

            "pca_random_state"_a = pca_random_state->value(),

            "umap_neighbors"_a = static_cast<int>( umap_neighbors->value() ),
            "umap_minimum_distance"_a = umap_minimum_distance->value(),
            "umap_metric"_a = umap_metric->currentText().toLower().toStdString(),
            "umap_random_state"_a = umap_random_state->value(),
            "umap_subsampling"_a = umap_subsampling->value(),

            "tsne_perplexity"_a = tsne_perplexity->value(),
            "tsne_random_state"_a = tsne_random_state->value(),

            "error"_a = std::string {},
            "model"_a = py::none {}
        };

        try
        {
            py::exec( R"(
try:
    import csv
    import numpy as np

    dataset         = np.asarray( dataset, copy=False )
    segmentation    = np.asarray( segmentation, copy=False )

    print( f"[Embedding] Dataset: ({dataset.shape}, {dataset.dtype}) " )
    print( f"[Embedding] Segmentation: ({segmentation.shape}, {segmentation.dtype}), segment number: {segment_number} " )

    element_indices     = ( np.argwhere( segmentation == segment_number ) if segment_number >= 0 else np.arange( segmentation.shape[0] ) ).astype( np.uint32 ).flatten()
    channel_indices     = np.array( channel_indices, dtype=np.int64 )

    element_count       = element_indices.shape[0]
    channel_count       = channel_indices.shape[0]
    feature_count       = len( additional_features )
    dimension_count     = channel_count + feature_count
    print( f"[Embedding] Element count: {element_count}, Channel count: {channel_count}, Feature count: {feature_count} " )

    if element_count == 0 or dimension_count == 0:
        raise ValueError( "Filtered dataset cannot be empty" )

    filtered_dataset        = np.zeros( ( element_count, dimension_count ), dtype=np.float32 )

    # === Copy data in channel-wise for lower memory consumption ==== #
    for i, channel_index in enumerate( channel_indices ):
        filtered_dataset[:, i] = dataset[element_indices, channel_index]

    # === Copy additional features === #
    for i, feature in enumerate( additional_features ):
        filtered_dataset[:, channel_count + i] = np.asarray( feature, copy=False )[element_indices]
    
    print( f"[Embedding] Filtered dataset:  ({filtered_dataset.shape}, {filtered_dataset.dtype}) " )
    
    # === Adjust features weight based on the number of channels and features === #
    features_weight = 1.0
    if channel_count > 0 and feature_count > 0:
        features_weight = np.sqrt( features_contribution / ( 1.0 - np.clip( features_contribution, 0.0, 0.999 ) ) * channel_count / feature_count )

    if normalization != "None":
        print( f"[Embedding] Normalizing dataset... " )
        
        # === Normalize the channels together and each feature individually === #
        groups = [slice( 0, channel_count )]
        for feature_index in range( feature_count ):
            dimension_index = channel_count + feature_index
            groups.append( slice( dimension_index, dimension_index + 1 ) )
          
        for group_index, group_slice in enumerate( groups ):
            group_dataview = filtered_dataset[:, group_slice]

            # === Use in-place operations to reduce memory consumption === #
            if normalization == "Z-Score":
                group_mean        = np.mean( group_dataview )
                group_std         = np.std( group_dataview )
                np.subtract( group_dataview, group_mean, out=group_dataview )
                np.divide( group_dataview, group_std, out=group_dataview )

            elif normalization == "Min-Max":
                group_minimum     = np.min( group_dataview )
                group_maximum     = np.max( group_dataview )
                group_range       = group_maximum - group_minimum
                np.subtract( group_dataview, group_minimum, out=group_dataview )
                np.divide( group_dataview, group_range, out=group_dataview )
            
            if group_index > 0:
                # === Scale features to have the same weight as channels === #
                np.multiply( group_dataview, features_weight, out=group_dataview )
    
    model_element_indices = element_indices.copy()

    if algorithm == "PCA":
        from sklearn.decomposition import PCA
        model = PCA(
            n_components    = 2,
            random_state    = pca_random_state
        )
        embedding = model.fit_transform( filtered_dataset ).astype( np.float32 )
        print( f"[Embedding] Explained variance: {model.explained_variance_ratio_} (total: {np.sum(model.explained_variance_ratio_):.3f}) " )

    elif algorithm == "UMAP":
        import umap
        model = umap.UMAP(
            n_components	= 2,
            n_neighbors		= umap_neighbors,
            min_dist		= umap_minimum_distance,
            metric			= umap_metric,                
            random_state	= umap_random_state,
            n_jobs			= 1,
            verbose			= True
        )
        
        if umap_subsampling == 1.0:
            embedding               = model.fit_transform( filtered_dataset ).astype( np.float32 )
        else:
            np.random.seed( 42 )
            subsampling_filter      = np.random.choice( (True, False), filtered_dataset.shape[0], p=(umap_subsampling, 1.0 - umap_subsampling) )
            subsampling_dataset     = filtered_dataset[subsampling_filter, :]
            print( f"[Embedding] Subsampling dataset: {subsampling_dataset.shape}" )

            model_element_indices = model_element_indices[subsampling_filter]

            model.fit( subsampling_dataset )
            embedding = model.transform( filtered_dataset ).astype( np.float32 )

    elif algorithm == "t-SNE":
        from sklearn.manifold import TSNE
        model = TSNE(
            n_components    = 2,
            perplexity      = tsne_perplexity,
            verbose         = 2
        )
                            
        embedding = model.fit_transform( filtered_dataset ).astype( np.float32 )
                        
    else:
        raise ValueError( f"Unknown embedding algorithm: {algorithm}" )
                        
    print( f"[Embedding] Finished computing embedding: {embedding.shape}" )
    embedding = embedding - np.mean( embedding, axis=0 )
    embedding = embedding / np.max( np.abs( embedding ) )

    model = ( model, model_element_indices )

except Exception as exception:
    error = str( exception ))", py::globals(), locals );
        }
        catch( py::error_already_set& error )
        {
            locals["error"] = error.what();
        }

        if( const auto error = locals["error"].cast<std::string>(); error.empty() )
        {
            const auto filepath = std::filesystem::path { filepath_label->text().toStdWString() };
            const auto extension = filepath.extension();
            if( extension == ".csv" )
            {
                auto stream = std::ofstream { filepath, std::ios::out };
                if( !stream )
                {
                    QMessageBox::critical( nullptr, "", "Failed to open output file", QMessageBox::Ok );
                    this->reject();
                    return;
                }

                const auto element_indices = locals["element_indices"].cast<py::array_t<uint32_t>>();
                const auto embedding = locals["embedding"].cast<py::array_t<float>>();

                for( py::ssize_t i = 0; i < element_indices.size(); ++i )
                {
                    stream << element_indices.at( i ) << ',' << embedding.at( i, 0 ) << ',' << embedding.at( i, 1 ) << '\n';
                }
            }
            else if( extension == ".mia" )
            {
                auto stream = MIAFileStream { filepath, std::ios::out };
                if( !stream )
                {
                    QMessageBox::critical( nullptr, "", "Failed to open output file", QMessageBox::Ok );
                    this->reject();
                    return;
                }

                const auto element_indices = locals["element_indices"].cast<py::array_t<uint32_t>>();
                const auto embedding = locals["embedding"].cast<py::array_t<float>>();

                stream.write( std::string { "Embedding" } );
                stream.write( static_cast<uint32_t>( element_indices.size() ) );
                stream.write( element_indices.data(), element_indices.nbytes() );
                stream.write( embedding.data(), embedding.nbytes() );
            }
            else
            {
                Console::error( "Unsupported file format: " + extension.string() );
                QMessageBox::critical( nullptr, "", "Unsupported file format", QMessageBox::Ok );
                this->reject();
                return;
            }

            _filepath = filepath;
            _model = locals["model"];
            this->accept();
        }
        else
        {
            Console::error( std::format( "Python error during embedding creation: {}", error ) );
            QMessageBox::critical( nullptr, "", "Failed to create embedding", QMessageBox::Ok );
            this->reject();
        }
    } );
}

const std::filesystem::path& EmbeddingCreator::filepath() const noexcept
{
    return _filepath;
}
py::object EmbeddingCreator::model() const noexcept
{
    return _model;
}