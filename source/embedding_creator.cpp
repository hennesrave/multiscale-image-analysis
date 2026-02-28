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

// ----- EmbeddingCreator ----- //

std::vector<std::unique_ptr<Database>>* EmbeddingCreator::database_registry = nullptr;

std::filesystem::path EmbeddingCreator::execute()
{
    auto embedding_creator = EmbeddingCreator {};
    if( embedding_creator.exec() == QDialog::Accepted )
    {
        return embedding_creator.filepath();
    }
    return std::filesystem::path {};
}

EmbeddingCreator::EmbeddingCreator() : QDialog {}
{
    const auto segmentation = EmbeddingCreator::database_registry->front()->segmentation();

    this->setWindowTitle( "Create Embedding..." );
    this->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    // Initialize general properties
    auto segment_selector = new SegmentSelector { segmentation };

    auto normalization = new QComboBox {};
    normalization->addItem( "Z-score" );
    normalization->addItem( "Min-Max" );
    normalization->addItem( "None" );

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

    auto export_model = new QComboBox {};
    export_model->addItem( "No" );
    export_model->addItem( "Yes" );

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
    umap_neighbors->setRange( 2, static_cast<int>( segmentation->element_count() - 1 ) );
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

    struct DatasetWidgets
    {
        QListWidget* channels_list;
        QListWidget* features_list;
    };

    auto dataset_selector   = new QComboBox {};
    auto dataset_properties = new QStackedWidget {};
    auto dataset_widgets    = std::vector<DatasetWidgets> {};

    for( size_t database_index = 0; database_index < EmbeddingCreator::database_registry->size(); ++database_index )
    {
        const auto& database    = EmbeddingCreator::database_registry->at( database_index );
        const auto dataset      = database->dataset();
        const auto features     = database->features();

        auto channels_list = new QListWidget {};
        channels_list->setSelectionMode( QAbstractItemView::MultiSelection );
        for( uint32_t channel_index = 0; channel_index < dataset->channel_count(); ++channel_index )
            channels_list->addItem( "Channel " + dataset->channel_identifier( channel_index ) );

        if( database.get() == EmbeddingCreator::database_registry->front().get() )
        {
            channels_list->selectAll();
        }

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

        // Initialize outside widgets

        dataset_selector->addItem( "[" + QString::number( database_index + 1 ) + "] " + dataset->identifier() );

        if( EmbeddingCreator::database_registry->size() == 1 )
        {
            layout->addRow( "Channels", channels_list );
            layout->addRow( "", channels_select_layout );
            layout->addRow( "Features", features_list );

            dataset_widgets.push_back( { channels_list, features_list } );
        }
        else
        {
            auto layout = new QFormLayout {};
            layout->setContentsMargins( 0, 0, 0, 0 );
            layout->addRow( "Channels", channels_list );
            layout->addRow( "", channels_select_layout );
            layout->addRow( "Features", features_list );

            auto container = new QWidget {};
            container->setLayout( layout );
            dataset_properties->addWidget( container );
            dataset_widgets.push_back( { channels_list, features_list } );
        }

        QObject::connect( channels_select_all, &QPushButton::clicked, channels_list, &QListWidget::selectAll );
        QObject::connect( channels_deselect_all, &QPushButton::clicked, channels_list, &QListWidget::clearSelection );
        QObject::connect( channels_select_from_features, &QPushButton::clicked, [&database, channels_list]
        {
            channels_list->clearSelection();
            for( const auto object : *database->features() ) if( auto feature = object.objectCast<DatasetChannelsFeature>() )
            {
                const auto channel_range = feature->channel_range();
                for( uint32_t channel_index = channel_range.x; channel_index <= channel_range.y; ++channel_index )
                {
                    if( auto item = channels_list->item( static_cast<int>( channel_index ) ) )
                    {
                        item->setSelected( true );
                    }
                }
            }
        } );
    }

    if( EmbeddingCreator::database_registry->size() > 1 )
    {
        layout->addRow( "", dataset_selector );
        layout->addRow( dataset_properties );

        QObject::connect( dataset_selector, &QComboBox::currentIndexChanged, dataset_properties, &QStackedWidget::setCurrentIndex );
    }

    layout->addRow( "Normalization", normalization );
    layout->addRow( "Filepath", filepath_layout );
    layout->addRow( "Export Model", export_model );
    layout->addRow( "Algorithm", algorithm );
    layout->addRow( algorithm_properties );
    layout->addRow( button_create );

    QObject::connect( algorithm, &QComboBox::currentIndexChanged, algorithm_properties, &QStackedWidget::setCurrentIndex );
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

        const auto filepath = std::filesystem::path { filepath_label->text().toStdWString() };
        const auto extension = filepath.extension();
        if( extension == ".csv" && export_model->currentText() == "Yes" )
        {
            QMessageBox::warning( nullptr, "Create Embedding...", "Exporting the model is not supported for CSV format. Please select MIA format or disable model export." );
            return;
        }

        auto interpreter = py::interpreter {};

        // Segmentation
        const auto segmentation_memoryview = py::memoryview::from_buffer(
            segmentation->segment_numbers().data(),
            { segmentation->element_count() },
            { sizeof( uint32_t ) }
        );

        const auto selected_segment = segment_selector->selected_segment();
        const auto segment_number   = selected_segment ? static_cast<int32_t>( selected_segment->number() ) : -1;

        // Datasets
        auto datasets_memoryviews           = std::vector<py::object> {};
        auto datasets_channels_indices      = std::vector<std::vector<uint32_t>> {};
        auto datasets_features_memoryviews  = std::vector<std::vector<py::memoryview>> {};
        auto datasets_channels_weights      = std::vector<double> {};
        auto datasets_features_weights      = std::vector<double> {};
        auto modality_count                 = 0;

        for( size_t database_index = 0; database_index < EmbeddingCreator::database_registry->size(); ++database_index )
        {
            const auto& database    = EmbeddingCreator::database_registry->at( database_index );
            const auto dataset      = database->dataset();
            const auto features     = database->features();

            const auto& widgets         = dataset_widgets[database_index];
            const auto channels_list    = widgets.channels_list;
            const auto features_list    = widgets.features_list;

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
            if( !channels_list->selectedItems().isEmpty() && !dataset_memoryview.has_value() )
            {
                Console::error( std::format( "Cannot create an embedding for dataset {}: '{}'", database_index, dataset->identifier().toStdString() ) );
                QMessageBox::critical( nullptr, "Create Embedding...", "Cannot create an embedding for this dataset" );
                this->reject();
                return;
            }

            auto channels_indices = std::vector<uint32_t> {};
            for( const auto item : channels_list->selectedItems() )
                channels_indices.push_back( channels_list->row( item ) );

            auto features_memoryviews = std::vector<py::memoryview> {};
            for( const auto item : features_list->selectedItems() )
            {
                const auto feature_index = features_list->row( item );
                const auto feature = features->object( feature_index );
                features_memoryviews.push_back( py::memoryview::from_buffer(
                    feature->values().data(),
                    { feature->element_count() },
                    { sizeof( double ) }
                ) );
            }

            if( channels_indices.size() != 0 )
            {
                ++modality_count;
            }
            if( features_memoryviews.size() != 0 )
            {
                ++modality_count;
            }

            datasets_channels_weights.push_back( channels_indices.empty()? 0.0 : 100.0 );
            datasets_features_weights.push_back( features_memoryviews.empty()? 0.0 : 100.0 );

            datasets_memoryviews.push_back( dataset_memoryview.value_or( py::none {} ) );
            datasets_channels_indices.push_back( std::move( channels_indices ) );
            datasets_features_memoryviews.push_back( features_memoryviews );
        }

        if( modality_count == 0 )
        {
            QMessageBox::warning( nullptr, "Create Embedding...", "Please select at least one channel or feature from at least one dataset." );
            return;
        }
        else if( modality_count > 1 && normalization->currentText() != "None" )
        {
            struct WeightMetadata
            {
                enum class Modality
                {
                    eChannels,
                    eFeatures
                };

                size_t dataset_index;
                Modality modality;
                QSpinBox* weight_spinbox;
                QLabel* percentage_label;
            };
            auto weights_metadata = std::vector<WeightMetadata> {};

            const auto update_percentages = [&weights_metadata]
            {
                auto weight_total = 0;
                for( const auto& metadata : weights_metadata )
                {
                    weight_total += metadata.weight_spinbox->value();
                }
                for( auto& metadata : weights_metadata )
                {
                    const auto weight = metadata.weight_spinbox->value();
                    const auto percentage = weight_total == 0 ? 0.0 : static_cast<double>( weight ) / static_cast<double>( weight_total ) * 100.0;
                    metadata.percentage_label->setText( QString::fromStdString( std::format( "{:.1f} %", percentage ) ) );
                }
            };

            auto column_labels = new QVBoxLayout {};
            column_labels->setContentsMargins( 0, 0, 0, 0 );
            column_labels->setSpacing( 5 );

            auto column_weights = new QVBoxLayout {};
            column_weights->setContentsMargins( 0, 0, 0, 0 );
            column_weights->setSpacing( 5 );

            auto column_percentages = new QVBoxLayout {};
            column_percentages->setContentsMargins( 0, 0, 0, 0 );
            column_percentages->setSpacing( 5 );

            for( size_t database_index = 0; database_index < EmbeddingCreator::database_registry->size(); ++database_index )
            {
                const auto& channels_indices        = datasets_channels_indices[database_index];
                const auto& features_memoryviews    = datasets_features_memoryviews[database_index];

                const auto options = std::vector<std::pair<WeightMetadata::Modality, size_t>> {
                    { WeightMetadata::Modality::eChannels, channels_indices.size() },
                    { WeightMetadata::Modality::eFeatures, features_memoryviews.size() }
                };

                for( const auto& [modality, count] : options )
                {
                    auto label_string = dataset_selector->itemText( static_cast<int>( database_index ) );
                    switch( modality )
                    {
                    case WeightMetadata::Modality::eChannels: label_string += " (channels)"; break;
                    case WeightMetadata::Modality::eFeatures: label_string += " (features)"; break;
                    }
                    auto label = new QLabel { label_string };

                    auto weight = new QSpinBox {};
                    weight->setRange( 0, 1000 );
                    weight->setValue( count == 0? 0 : 100 );
                    weight->setEnabled( count != 0 );

                    auto percentage = new QLabel {};

                    auto row = new QHBoxLayout {};
                    row->setContentsMargins( 0, 0, 0, 0 );
                    row->setSpacing( 5 );
                    row->addWidget( label, 1 );
                    row->addWidget( weight );
                    row->addWidget( percentage );

                    column_labels->addWidget( label );
                    column_weights->addWidget( weight );
                    column_percentages->addWidget( percentage );

                    weights_metadata.push_back( { database_index, modality, weight, percentage } );

                    QObject::connect( weight, &QSpinBox::valueChanged, update_percentages );
                }
            }

            update_percentages();

            auto columns = new QHBoxLayout {};
            columns->setContentsMargins( 0, 0, 0, 0 );
            columns->setSpacing( 10 );
            columns->addLayout( column_labels );
            columns->addLayout( column_weights );
            columns->addLayout( column_percentages );

            auto button_confirm = new QPushButton { "Confirm" };

            auto dialog = QDialog {};
            dialog.setWindowTitle( "Modality Contributions" );

            auto layout = new QVBoxLayout { &dialog };
            layout->setContentsMargins( 10, 10, 10, 10 );
            layout->setSpacing( 20 );

            layout->addLayout( columns );
            layout->addStretch( 1 );
            layout->addWidget( button_confirm );

            QObject::connect( button_confirm, &QPushButton::clicked, &dialog, &QDialog::accept );

            if( dialog.exec() == QDialog::Rejected )
            {
                return;
            }

            for( size_t metadata_index = 0; metadata_index < weights_metadata.size(); ++metadata_index )
            {
                const auto& metadata        = weights_metadata[metadata_index];
                const auto weight           = metadata.weight_spinbox->value();
                const auto dataset_index    = metadata.dataset_index;

                switch( metadata.modality )
                {
                case WeightMetadata::Modality::eChannels: datasets_channels_weights[dataset_index] = weight; break;
                case WeightMetadata::Modality::eFeatures: datasets_features_weights[dataset_index] = weight; break;
                }
            }
        }

        using namespace py::literals;
        auto locals = py::dict {
            "segmentation"_a = segmentation_memoryview,
            "segment_number"_a = segment_number,

            "datasets"_a = datasets_memoryviews,
            "datasets_channels"_a = datasets_channels_indices,
            "datasets_features"_a = datasets_features_memoryviews,
            "datasets_channels_weights"_a = datasets_channels_weights,
            "datasets_features_weights"_a = datasets_features_weights,

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
    import numpy as np

    segmentation        = np.asarray( segmentation, copy=False )
    print( f"[Embedding] Segmentation: ({segmentation.shape}, {segmentation.dtype}), segment number: {segment_number} " )

    datapoint_indices   = ( np.argwhere( segmentation == segment_number ) if segment_number >= 0 else np.arange( segmentation.shape[0] ) ).astype( np.uint32 ).flatten()
    datapoint_count     = datapoint_indices.shape[0]    
    print( f"[Embedding] Datapoint indices: ({datapoint_indices.shape}, {datapoint_indices.dtype}) " )

    if datapoint_count == 0:
        raise ValueError( "Datapoint count cannot be zero" )

    # Normalize weights
    dataset_channels_weights = np.array( datasets_channels_weights, dtype=np.float64 )
    dataset_features_weights = np.array( datasets_features_weights, dtype=np.float64 )

    modality_count  = np.count_nonzero( dataset_channels_weights ) + np.count_nonzero( dataset_features_weights )
    weights_total   = np.sum( datasets_channels_weights ) + np.sum( datasets_features_weights )
    print( f"[Embedding] Modality count: {modality_count}, weights total: {weights_total:.4f}" )

    if modality_count == 0:
        raise ValueError( "Modality count cannot be zero" )

    dataset_channels_weights = dataset_channels_weights / weights_total
    dataset_features_weights = dataset_features_weights / weights_total

    channel_count_total     = 0
    feature_count_total     = 0
    dataset_count_total     = 0

    for dataset_index in range( len( datasets ) ):
        if datasets[dataset_index] is not None:
            dataset                 = np.asarray( datasets[dataset_index], copy=False )
            datasets[dataset_index] = dataset
        
        channels                            = np.asarray( datasets_channels[dataset_index], copy=True )
        datasets_channels[dataset_index]    = channels
        channel_count_total                 = channel_count_total + channels.size

        features = datasets_features[dataset_index]
        for feature_index in range( len( features ) ):
            features[feature_index] = np.asarray( features[feature_index], copy=False )
        feature_count_total = feature_count_total + len( features )

        if channels.size != 0 or len( features ) != 0:
            dataset_count_total += 1
        
        channels_weight = dataset_channels_weights[dataset_index]
        features_weight = dataset_features_weights[dataset_index]

        print( f"[Embedding] Dataset {dataset_index}: ({dataset.shape}, {dataset.dtype}), channels: {len(channels)} (contribution = {channels_weight:.1%}), features: {len(features)} (contribution = {features_weight:.1%}) " )

    dimension_count = channel_count_total + feature_count_total
    print( f"[Embedding] Total channel count: {channel_count_total}, total feature count: {feature_count_total}, dimension count: {dimension_count}" )

    if dimension_count == 0:
        raise ValueError( "Dimension count cannot be zero" )

    # === Create a combined dataset === #
    combined_dataset = np.zeros( ( datapoint_count, dimension_count ), dtype=np.float32 )

    current_channel_index = 0
    current_feature_index = channel_count_total

    for dataset_index in range( len( datasets ) ):
        dataset     = datasets[dataset_index]
        channels    = datasets_channels[dataset_index]
        features    = datasets_features[dataset_index]

        for i, channel_index in enumerate( channels ):
            combined_dataset[:, current_channel_index + i] = dataset[datapoint_indices, channel_index]
        current_channel_index += channels.size

        for i, feature in enumerate( features ):
            combined_dataset[:, current_feature_index + i] = feature[datapoint_indices]
        current_feature_index += len( features )
    
    print( f"[Embedding] Combined dataset: ({combined_dataset.shape}, {combined_dataset.dtype}) " )

    # === Normalize dataset ==== #
    if normalization != "None":
        print( f"[Embedding] Normalizing dataset with method: {normalization} " )

        normalization_groups = []

        current_channel_index = 0
        current_feature_index = channel_count_total

        for dataset_index in range( len( datasets ) ):
            channels    = datasets_channels[dataset_index]
            features    = datasets_features[dataset_index]

            channels_weight = dataset_channels_weights[dataset_index]
            features_weight = dataset_features_weights[dataset_index]   

            if len( channels ) > 0:
                channels_weight = channels_weight / np.sqrt( len( channels ) )
                normalization_groups.append( (
                    slice( current_channel_index, current_channel_index + channels.size ),
                    channels_weight,
                    dataset_index,
                    "channels"
                ) )
                current_channel_index += len( channels )
            
            if len( features ) > 0:
                features_weight = features_weight / np.sqrt( len( features ) )    

                for i in range( len( features ) ):
                    normalization_groups.append( (
                        slice( current_feature_index, current_feature_index + 1 ),
                        features_weight,
                        dataset_index,
                        f"feature {i}"
                    ) )
                    current_feature_index += 1

        for group_slice, weight, dataset_index, label in normalization_groups:
            group_dataview = combined_dataset[:, group_slice]

            if normalization == "Z-score":
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
            
            np.multiply( group_dataview, weight, out=group_dataview )

            print( f"[Embedding] Normalized dataset {dataset_index} ({label}) in {group_slice} with weight = {weight:.4}. Total variance after normalization: {np.var( group_dataview ):.6f} " )

    # === Train model and compute embedding ==== #
    model                   = None
    model_datapoint_indices = datapoint_indices.copy()

    if algorithm == "PCA":
        from sklearn.decomposition import PCA
        model = PCA(
            n_components    = 2,
            random_state    = pca_random_state
        )
        embedding = model.fit_transform( combined_dataset ).astype( np.float32 )
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
            embedding               = model.fit_transform( combined_dataset ).astype( np.float32 )
        else:
            np.random.seed( 42 )
            subsampling_filter      = np.random.choice( (True, False), combined_dataset.shape[0], p=(umap_subsampling, 1.0 - umap_subsampling) )
            subsampling_dataset     = combined_dataset[subsampling_filter, :]
            print( f"[Embedding] Subsampling dataset: {subsampling_dataset.shape}" )
            
            model_datapoint_indices = model_datapoint_indices[subsampling_filter]
            
            model.fit( subsampling_dataset )
            embedding = model.transform( combined_dataset ).astype( np.float32 )

    elif algorithm == "t-SNE":
        from sklearn.manifold import TSNE
        model = TSNE(
            n_components    = 2,
            perplexity      = tsne_perplexity,
            verbose         = 2
        )
                            
        embedding = model.fit_transform( combined_dataset ).astype( np.float32 )
                        
    else:
        raise ValueError( f"Unknown embedding algorithm: {algorithm}" )

    print( f"[Embedding] Finished computing embedding: {embedding.shape}" )
    embedding = embedding - np.mean( embedding, axis=0 )
    embedding = embedding / np.max( np.abs( embedding ) )
    
    model = (model, model_datapoint_indices)

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

                const auto datapoint_indices = locals["datapoint_indices"].cast<py::array_t<uint32_t>>();
                const auto embedding = locals["embedding"].cast<py::array_t<float>>();

                for( py::ssize_t i = 0; i < datapoint_indices.size(); ++i )
                {
                    stream << datapoint_indices.at( i ) << ',' << embedding.at( i, 0 ) << ',' << embedding.at( i, 1 ) << '\n';
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

                const auto datapoint_indices    = locals["datapoint_indices"].cast<py::array_t<uint32_t>>();
                const auto embedding            = locals["embedding"].cast<py::array_t<float>>();
                const auto model                = locals["model"].cast<py::object>();

                stream.write( std::string { "Embedding" } );
                stream.write( static_cast<uint32_t>( datapoint_indices.size() ) );
                stream.write( datapoint_indices.data(), datapoint_indices.nbytes() );
                stream.write( embedding.data(), embedding.nbytes() );

                if( export_model->currentText() == "Yes" )
                {
                    stream.write( model );
                }
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