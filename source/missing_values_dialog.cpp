#include "missing_values_dialog.hpp"

#include "database.hpp"
#include "dataset.hpp"

#include <qcombobox.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qstackedwidget.h>

// ----- MissingValuesDialog ----- //

bool MissingValuesDialog::execute( QSharedPointer<Dataset> dataset )
{
    if( !dataset )
    {
        QMessageBox::warning( nullptr, "Replace Missing Values...", "No dataset available" );
        return false;
    }

    if( MissingValuesDialog::summarize( dataset ).value_count == 0 )
    {
        // QMessageBox::information( nullptr, "Replace Missing Values...", "This dataset does not contain any missing values." );
        Console::info( "This dataset does not contain any missing values." );
        return false;
    }

    auto missing_values_dialog = MissingValuesDialog { dataset };
    return missing_values_dialog.exec() == QDialog::Accepted;
}

MissingValuesDialog::MissingValuesDialog( QSharedPointer<Dataset> dataset ) : QDialog {}
{
    const auto summary = MissingValuesDialog::summarize( dataset );
    const auto spatial_metadata = dataset->spatial_metadata();

    this->setWindowTitle( "Replace Missing Values..." );
    this->setStyleSheet( "QPushButton { padding: 2px 5px 2px 5px; }" );

    const auto note = [] ( const QString& text )
    {
        auto label = new QLabel { text };
        label->setWordWrap( true );
        label->setMaximumWidth( 300 );
        return label;
    };

    // Initialize general properties
    auto summary_text = QString::number( summary.value_count ) + " missing value(s) in "
        + QString::number( summary.element_count ) + " of " + QString::number( dataset->element_count() ) + " elements";
    if( summary.complete_count > 0 )
    {
        summary_text += ", " + QString::number( summary.complete_count ) + " of which have no measured channel at all";
    }
    summary_text += spatial_metadata
        ? "\nGrid: " + QString::number( spatial_metadata->width ) + " x " + QString::number( spatial_metadata->height )
        : "\nNo spatial metadata: neighborhood based modes are unavailable";

    auto summary_label = new QLabel { summary_text };
    summary_label->setWordWrap( true );
    summary_label->setMaximumWidth( 300 );
    summary_label->setTextInteractionFlags( Qt::TextSelectableByMouse );
    summary_label->setStyleSheet( "QLabel { background: #fafafa; border-radius: 3px; }" );
    summary_label->setContentsMargins( 5, 3, 5, 3 );

    auto mode = new QComboBox {};
    mode->addItem( "Constant", static_cast<uint32_t>( Dataset::MissingValues::Mode::eConstant ) );
    mode->addItem( "Channel Mean", static_cast<uint32_t>( Dataset::MissingValues::Mode::eChannelMean ) );
    mode->addItem( "Channel Median", static_cast<uint32_t>( Dataset::MissingValues::Mode::eChannelMedian ) );
    mode->addItem( spatial_metadata ? "Local Median" : "Local Median (requires spatial data)",
        static_cast<uint32_t>( Dataset::MissingValues::Mode::eLocalMedian ) );

    if( !spatial_metadata )
    {
        // Qt::UserRole - 1 is the role the default combo box model uses to
        // enable and disable individual items.
        mode->setItemData( mode->count() - 1, false, Qt::UserRole - 1 );
    }

    // Initialize constant properties
    auto constant_value = new QDoubleSpinBox {};
    constant_value->setRange( std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max() );
    constant_value->setDecimals( 4 );
    constant_value->setValue( 0.0 );

    auto constant_widget = new QWidget {};
    auto constant_layout = new QFormLayout { constant_widget };
    constant_layout->setContentsMargins( 0, 0, 0, 0 );
    constant_layout->addRow( "Value", constant_value );

    // Initialize channel mean properties
    auto channel_mean_widget = new QWidget {};
    auto channel_mean_layout = new QFormLayout { channel_mean_widget };
    channel_mean_layout->setContentsMargins( 0, 0, 0, 0 );
    channel_mean_layout->addRow( note( "Replaced by the mean of the measured values of the same channel." ) );

    // Initialize channel median properties
    auto channel_median_widget = new QWidget {};
    auto channel_median_layout = new QFormLayout { channel_median_widget };
    channel_median_layout->setContentsMargins( 0, 0, 0, 0 );
    channel_median_layout->addRow( note( "Replaced by the median of the measured values of the same channel. Preferable to the mean for skewed data." ) );

    // Initialize local median properties
    auto local_median_radius = new QSpinBox {};
    local_median_radius->setRange( 1, 10 );
    local_median_radius->setValue( 1 );

    auto local_median_passes = new QComboBox {};
    local_median_passes->addItem( "Until filled", 0 );
    for( int i = 1; i <= 10; ++i )
    {
        local_median_passes->addItem( QString::number( i ), i );
    }

    auto local_median_fallback = new QComboBox {};
    local_median_fallback->addItem( "Channel Median", static_cast<uint32_t>( Dataset::MissingValues::Fallback::eChannelMedian ) );
    local_median_fallback->addItem( "Zero", static_cast<uint32_t>( Dataset::MissingValues::Fallback::eConstant ) );
    local_median_fallback->addItem( "Leave Missing", static_cast<uint32_t>( Dataset::MissingValues::Fallback::eNone ) );

    auto local_median_widget = new QWidget {};
    auto local_median_layout = new QFormLayout { local_median_widget };
    local_median_layout->setContentsMargins( 0, 0, 0, 0 );
    local_median_layout->addRow( "Radius", local_median_radius );
    local_median_layout->addRow( "Passes", local_median_passes );
    local_median_layout->addRow( "Fallback", local_median_fallback );
    local_median_layout->addRow( note( "Replaced by the median of the measured cells around them, repeatedly, so that wide gaps close from their edges inwards." ) );

    auto mode_properties = new QStackedWidget {};
    mode_properties->setContentsMargins( 20, 0, 0, 0 );
    mode_properties->addWidget( constant_widget );
    mode_properties->addWidget( channel_mean_widget );
    mode_properties->addWidget( channel_median_widget );
    mode_properties->addWidget( local_median_widget );

    auto button_replace = new QPushButton { "Replace" };

    auto layout = new QFormLayout { this };
    layout->addRow( "Missing Values", summary_label );
    layout->addRow( "Mode", mode );
    layout->addRow( mode_properties );
    layout->addRow( button_replace );

    // The item order matches the page order of the stacked widget.
    mode->setCurrentIndex( spatial_metadata ? 3 : 2 );
    mode_properties->setCurrentIndex( mode->currentIndex() );

    QObject::connect( mode, &QComboBox::currentIndexChanged, mode_properties, &QStackedWidget::setCurrentIndex );
    QObject::connect( button_replace, &QPushButton::clicked, [=, this] ()
    {
        auto settings = Dataset::MissingValues {};
        settings.mode = static_cast<Dataset::MissingValues::Mode>( mode->currentData().toUInt() );
        settings.constant = constant_value->value();
        settings.radius = static_cast<uint32_t>( local_median_radius->value() );
        settings.passes = static_cast<uint32_t>( local_median_passes->currentData().toUInt() );
        settings.fallback = static_cast<Dataset::MissingValues::Fallback>( local_median_fallback->currentData().toUInt() );

        if( settings.mode == Dataset::MissingValues::Mode::eLocalMedian )
        {
            const auto spatial_metadata = dataset->spatial_metadata();
            if( !spatial_metadata )
            {
                QMessageBox::warning( nullptr, "Replace Missing Values...", "The local median requires a dataset with spatial metadata." );
                return;
            }
            if( static_cast<uint64_t>( spatial_metadata->width ) * spatial_metadata->height != dataset->element_count() )
            {
                QMessageBox::warning( nullptr, "Replace Missing Values...", "The spatial metadata does not match the number of elements of this dataset." );
                return;
            }

            // The constant spin box belongs to another page, so the fallback
            // constant is fixed at the value its item advertises.
            if( settings.fallback == Dataset::MissingValues::Fallback::eConstant )
            {
                settings.constant = 0.0;
            }
        }

        dataset->replace_missing_values( settings );
        this->accept();
    } );
}

MissingValuesDialog::Summary MissingValuesDialog::summarize( const QSharedPointer<Dataset>& dataset )
{
    auto summary = Summary {};
    dataset->visit( [&summary] ( const auto& tensor_dataset )
    {
        using value_type = typename std::remove_cvref_t<decltype( tensor_dataset )>::value_type;
        if constexpr( std::is_floating_point_v<value_type> )
        {
            const auto channel_count = tensor_dataset.channel_count();
            for( uint32_t element_index = 0; element_index < tensor_dataset.element_count(); ++element_index )
            {
                const auto* const element_intensities = tensor_dataset.intensities().data()
                    + static_cast<size_t>( element_index ) * channel_count;

                auto count = uint32_t { 0 };
                for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
                {
                    if( std::isnan( element_intensities[channel_index] ) )
                    {
                        ++count;
                    }
                }

                summary.value_count += count;
                if( count > 0 ) ++summary.element_count;
                if( count == channel_count ) ++summary.complete_count;
            }
        }
    } );
    return summary;
}