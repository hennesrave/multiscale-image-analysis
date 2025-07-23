#include "dataset_importer.hpp"

#include "dataset.hpp"
#include "logger.hpp"
#include "number_input.hpp"
#include "python.hpp"

#include "json.hpp"

#include <fstream>

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qspinbox.h>

QSharedPointer<Dataset> DatasetImporter::execute()
{
    const auto filepath = QFileDialog::getOpenFileName( nullptr, "Import dataset...", "", "", nullptr, QFileDialog::DontUseNativeDialog );
    const auto fileinfo = QFileInfo { filepath };

    auto dataset = QSharedPointer<Dataset> {};
    auto interpreter = QScopedPointer<py::interpreter> {};

    if( fileinfo.exists() && fileinfo.isFile() )
    {
        const auto filename = fileinfo.fileName();
        const auto suffix = fileinfo.suffix();

        // Initialize dialog
        auto dialog = QDialog {};
        dialog.setWindowTitle( "Import dataset..." );
        dialog.setMinimumSize( 300, 400 );

        auto button_import = new QPushButton { "Import" };

        auto layout = new QFormLayout { &dialog };
        layout->setContentsMargins( 20, 10, 20, 10 );
        layout->setHorizontalSpacing( 30 );

        auto execute_dialog = false;

        // Import dataset
        if( filename == "laser.info" )
        {
            [&dataset, filepath = std::filesystem::path { filepath.toStdWString() }]
            {
                using json = nlohmann::json;

                // Read laser line info
                struct LaserLineInfo
                {
                    uint32_t line = 0;
                    double start_x = 0.0;
                    double start_y = 0.0;
                    double spot_spacing = 0.0;
                    double spot_size = 0.0;
                    uint32_t line_type = 0;
                    uint32_t number_of_shots = 0;
                };
                auto laser_lines = std::vector<LaserLineInfo> {};

                const auto laser_info = json::parse( std::ifstream { filepath } );
                const auto line_group_size = laser_info["AcquisitionLineGroupSize"].get<uint32_t>();
                for( const auto& laser_line : laser_info["LaserLineInfo"] )
                {
                    laser_lines.push_back(
                        LaserLineInfo {
                            .line = laser_line["ln"].get<uint32_t>(),
                            .start_x = laser_line["sx"].get<double>(),
                            .start_y = laser_line["sy"].get<double>(),
                            .spot_spacing = laser_line["sp"].get<double>(),
                            .spot_size = laser_line["ss"].get<double>(),
                            .line_type = laser_line["lt"].get<uint32_t>(),
                            .number_of_shots = laser_line["ns"].get<uint32_t>()
                        }
                    );
                }

                // Validate data
                if( laser_lines[0].line != 1 )
                {
                    QMessageBox::critical( nullptr, "", "Invalid line number encountered" );
                    return;
                }

                for( const auto& laser_line : laser_lines )
                {
                    if( laser_line.line_type != 0 )
                    {
                        QMessageBox::critical( nullptr, "", "Unsupported line type encountered" );
                        return;
                    }
                }
                for( size_t i = 1; i < laser_lines.size(); ++i )
                {
                    const auto& previous = laser_lines[i - 1];
                    const auto& current = laser_lines[i];

                    if( current.line != previous.line + 1 )
                    {
                        QMessageBox::critical( nullptr, "", "Invalid line number encountered" );
                        return;
                    }

                    if( current.spot_spacing != previous.spot_spacing )
                    {
                        QMessageBox::critical( nullptr, "", "Varying spot spacing not supported" );
                        return;
                    }

                    if( current.spot_size != previous.spot_size )
                    {
                        QMessageBox::critical( nullptr, "", "Varying spot size not supported" );
                        return;
                    }

                    if( std::abs( current.start_y - ( previous.start_y + current.spot_size ) ) > 1.0e-6 )
                    {
                        QMessageBox::critical( nullptr, "", "Vertical spacing must be equal to spot size" );
                        return;
                    }

                    if( current.number_of_shots != previous.number_of_shots )
                    {
                        QMessageBox::critical( nullptr, "", "Varying number of shots not supported" );
                        return;
                    }
                }

                // Transit time is an offset added to laserspot timings
                const auto transit_time = [&filepath]
                {
                    const auto trigger_corrections_filepath = filepath.parent_path() / "TriggerCorrections.dat";
                    const auto trigger_corrections = nlohmann::json::parse( std::ifstream { trigger_corrections_filepath } );
                    return trigger_corrections["Transit1Time"].get<uint64_t>();
                }( );

                const auto spots_per_pixel = static_cast<uint32_t>( std::round( laser_lines[0].spot_size / laser_lines[0].spot_spacing ) );

                auto dimensions = vec3<uint32_t> {};
                auto spatial_metadata = Dataset::SpatialMetadata {};
                auto channels = Array<double> {};
                auto intensities = Matrix<float> {};
                auto baseline = Array<double> {};
                auto baseline_counter = uint32_t { 0 };

                auto current_line_index = uint32_t { 0 };

                auto channels_permutation = Array<size_t> {};

                for( const auto& entry : std::filesystem::directory_iterator( filepath.parent_path() ) )
                {
                    if( !entry.is_directory() )
                        continue;

                    const auto subdirectory = entry.path();
                    const auto run_info = json::parse( std::ifstream { subdirectory / "run.info" } );
                    const auto average_single_ion_area = run_info["AverageSingleIonArea"].get<float>();
                    const auto acquisition_period = run_info["SegmentInfo"][0]["AcquisitionPeriodNs"].get<uint64_t>();

                    Logger::info() << "Importing subdirectory: " << subdirectory;

                    auto current_laserspot_linebreak_index = uint32_t { 0 };
                    auto current_coordinates = vec2<double> { laser_lines[current_line_index].start_x, laser_lines[current_line_index].start_y };
                    auto current_pixel = vec2<uint32_t> { 0, laser_lines[current_line_index].line - 1 };
                    auto current_spot_counter = 0;

                    // Read pulse data
                    struct LaserspotMetadata
                    {
                        uint64_t elapsed_time = 0;
                        vec2<double> coordinates = { 0.0, 0.0 };
                        vec2<uint32_t> pixel = { 0, 0 };
                        bool linebreak = false;
                    };
                    auto laserspots = std::vector<LaserspotMetadata> {};

                    const auto pulse_index = json::parse( std::ifstream { subdirectory / "pulse.index" } );
                    for( const auto& entry : pulse_index )
                    {
#pragma pack(push, 1)
                        struct PulseEntry
                        {
                            uint32_t cycle_number = 0;
                            uint32_t segment_number = 0;
                            uint32_t acquisition_number = 0;
                            bool overflow = false;
                        };
#pragma pack(pop)

                        const auto filepath = subdirectory / ( std::to_string( entry["FileNum"].get<int32_t>() ) + ".pulse" );
                        auto stream = std::ifstream { filepath, std::ios::in | std::ios::binary | std::ios::ate };

                        const auto filesize = stream.tellg();
                        stream.seekg( 0 );

                        auto pulse_entries = std::vector<PulseEntry>( filesize / sizeof( PulseEntry ) );
                        stream.read( reinterpret_cast<char*>( pulse_entries.data() ), filesize );

                        for( const auto& pulse_entry : pulse_entries )
                        {
                            auto metadata = LaserspotMetadata {
                                .elapsed_time = static_cast<uint64_t>( pulse_entry.acquisition_number ) * acquisition_period + transit_time,
                                .coordinates = vec2<double> { 0.0, 0.0 },
                                .pixel = vec2<uint32_t> { 0, 0 },
                                .linebreak = false
                            };
                            laserspots.push_back( metadata );
                        }
                    }

                    const auto line_count = std::min( line_group_size, static_cast<uint32_t>( laser_lines.size() ) - current_pixel.y );

                    if( line_count > 1 )
                    {
                        // There should be line_count lines, so we search for the (line_count - 1) laser spots
                        // with the highest delays to the previous spot to identify where linebreaks occur
                        auto laserspot_indices = std::vector<uint32_t>( laserspots.size() - 1 );
                        std::iota( laserspot_indices.begin(), laserspot_indices.end(), 1 );

                        std::partial_sort( laserspot_indices.begin(), laserspot_indices.begin() + line_count - 1, laserspot_indices.end(),
                            [&laserspots] ( const auto a, const auto b )
                        {
                            return ( laserspots[b].elapsed_time - laserspots[b - 1].elapsed_time ) <
                                ( laserspots[a].elapsed_time - laserspots[a - 1].elapsed_time );
                        } );

                        for( uint32_t i = 0; i < line_count - 1; ++i )
                            laserspots[laserspot_indices[i]].linebreak = true;
                    }

                    // Compute coordinates and pixels for each laserspot
                    for( uint32_t laserspot_index = 0; laserspot_index < laserspots.size(); ++laserspot_index )
                    {
                        auto& laserspot = laserspots[laserspot_index];
                        if( laserspot.linebreak )
                        {
                            current_laserspot_linebreak_index += 1;
                            current_line_index += 1;
                            current_coordinates = vec2<double> {
                                laser_lines[current_line_index].start_x,
                                laser_lines[current_line_index].start_y
                            };
                            current_pixel = vec2<uint32_t> {
                                0,
                                current_pixel.y + 1
                            };
                            current_spot_counter = 0;
                        }

                        laserspot.coordinates = current_coordinates;
                        laserspot.pixel = current_pixel;

                        current_coordinates.x += laser_lines[current_line_index].spot_spacing;

                        if( current_pixel.x == 0 || ++current_spot_counter == spots_per_pixel )
                        {
                            current_spot_counter = 0;
                            current_pixel.x += 1;
                        }
                    }

                    // Read integrated data
                    auto current_laserspot_index = uint32_t { 0 };

                    const auto integrated_index = json::parse( std::ifstream { subdirectory / "integrated.index" } );
                    for( const auto& entry : integrated_index )
                    {
                        const auto filepath = subdirectory / ( std::to_string( entry["FileNum"].get<int32_t>() ) + ".integ" );
                        Logger::info() << filepath;

                        auto stream = std::ifstream { filepath, std::ios::in | std::ios::binary | std::ios::ate };

                        const auto streamsize = stream.tellg();
                        stream.seekg( 0 );

                        struct Header
                        {
                            int32_t cycle_number = 0;
                            int32_t segment_number = 0;
                            int32_t acquisition_number = 0;
                            int32_t number_of_results = 0;
                        };

#pragma pack(push, 1)
                        struct Result
                        {
                            float peak_center = 0.0f;
                            float signal = 0.0f;
                            float unused1;
                            bool unused2;
                        };
#pragma pack(pop)

                        auto header = Header {};
                        stream.read( reinterpret_cast<char*>( &header ), sizeof( Header ) );
                        stream.seekg( 0 );

                        const auto spectrum_size = sizeof( Header ) + header.number_of_results * sizeof( Result );
                        const auto spectrum_count = streamsize / spectrum_size;
                        auto values = std::vector<char>( streamsize );
                        stream.read( values.data(), values.size() );

                        if( channels.empty() )
                        {
                            channels = Array<double>::allocate( header.number_of_results );
                            baseline = Array<double> { static_cast<size_t>( header.number_of_results ), 0.0 };

                            const auto acquisition_trigger_delay = run_info["SegmentInfo"][0]["AcquisitionTriggerDelayNs"].get<double>();
                            const auto coefficients = run_info["MassCalCoefficients"];

                            const auto spectrum = reinterpret_cast<const Result*>( values.data() + sizeof( Header ) );
                            for( int32_t i = 0; i < header.number_of_results; ++i )
                            {
                                channels[i] = std::pow(
                                    coefficients[0].get<double>() + coefficients[1].get<double>() *
                                    ( 0.5 * spectrum[i].peak_center + acquisition_trigger_delay ),
                                    2.0
                                );
                            }

                            // Check if channels are ascending
                            for( size_t i = 1; i < channels.size(); ++i )
                            {
                                if( channels[i] <= channels[i - 1] )
                                {
                                    channels_permutation = Array<size_t>::allocate( channels.size() );
                                    std::iota( channels_permutation.begin(), channels_permutation.end(), 0 );
                                    std::sort( channels_permutation.begin(), channels_permutation.end(), [&channels] ( const auto a, const auto b )
                                    {
                                        return channels[a] < channels[b];
                                    } );
                                }
                            }

                            dimensions = vec3<uint32_t> {
                                static_cast<uint32_t>( std::ceil( static_cast<double>( laser_lines[0].number_of_shots + spots_per_pixel - 1 ) / spots_per_pixel ) ),
                                static_cast<uint32_t>( laser_lines.size() ),
                                static_cast<uint32_t>( channels.size() )
                            };
                            spatial_metadata = Dataset::SpatialMetadata { dimensions[0], dimensions[1] };
                            intensities = Matrix<float> { { dimensions[0] * dimensions[1], dimensions[2] }, 0.0f };
                        }

                        for( auto spectrum_iterator = values.begin(); spectrum_iterator != values.end(); spectrum_iterator += spectrum_size )
                        {
                            const auto header = reinterpret_cast<const Header*>( spectrum_iterator._Ptr );
                            const auto elapsed_time = static_cast<uint64_t>( header->acquisition_number ) * acquisition_period;
                            const auto input_spectrum = reinterpret_cast<const Result*>( spectrum_iterator._Ptr + sizeof( Header ) );

                            if( elapsed_time < laserspots[current_laserspot_index].elapsed_time )
                            {
                                for( int32_t i = 0; i < header->number_of_results; ++i )
                                {
                                    baseline[i] += input_spectrum[i].signal / average_single_ion_area;
                                }
                                baseline_counter += 1;
                                continue;
                            }

                            if( current_laserspot_index < laserspots.size() - 1 && elapsed_time > laserspots[current_laserspot_index + 1].elapsed_time )
                                current_laserspot_index += 1;

                            const auto& current_laserspot = laserspots[current_laserspot_index];
                            if( current_laserspot.pixel.x == dimensions.x - 1 )
                                continue;

                            const auto pixel_index = spatial_metadata.element_index( current_laserspot.pixel );
                            auto output_spectrum = intensities.data() + pixel_index * header->number_of_results;
                            for( int32_t i = 0; i < header->number_of_results; ++i )
                            {
                                output_spectrum[i] += input_spectrum[i].signal / average_single_ion_area;
                            }
                        }
                    }

                    current_line_index += 1;
                }

                if( baseline_counter )
                {
                    const auto result = QMessageBox::question( nullptr, "Import Dataset...", "Subtract average background spectrum for baseline correction?" );
                    if( result == QMessageBox::Yes )
                    {
                        for( auto& value : baseline )
                            value /= baseline_counter;

                        auto current = intensities.data();
                        for( size_t i = 0; i < intensities.size(); ++i, ++current )
                            *current = std::max( 0.0, *current - baseline[i % channels.size()] );
                    }
                }

                if( channels_permutation.size() )
                {
                    const auto result = QMessageBox::question( nullptr, "Import Dataset...", "Invalid channel order detected. Fix the problem via permutation?" );
                    if( result == QMessageBox::Yes )
                    {
                        auto permuted_channels = Array<double>::allocate( channels.size() );
                        for( size_t i = 0; i < channels.size(); ++i )
                            permuted_channels[i] = channels[channels_permutation[i]];
                        channels = std::move( permuted_channels );

                        auto permuted_intensities = Array<float>::allocate( channels.size() );
                        for( auto current = intensities.data(); current != intensities.end(); current += channels.size() )
                        {
                            for( size_t i = 0; i < channels.size(); ++i )
                                permuted_intensities[i] = current[channels_permutation[i]];
                            std::copy( permuted_intensities.begin(), permuted_intensities.end(), current );
                        }
                    }
                    else
                    {
                        return;
                    }
                }

                dataset.reset( new TensorDataset<float> { spatial_metadata, std::move( intensities ), std::move( channels ) } );
            }( );
        }
        else if( filename == ".zgroup" )
        {
            interpreter.reset( new py::interpreter {} );
            try
            {
                using namespace py::literals;
                const auto zarr_dataset = py::module::import( "zarr" ).attr( "open" )( fileinfo.absolutePath().toStdString(), "mode"_a = "r" );
                auto hypercube = static_cast<py::array>( zarr_dataset["hypercube"][py::make_tuple()] );
                auto wavenumbers = static_cast<py::array>( zarr_dataset["wvnm"][py::make_tuple()] );

                if( !hypercube.dtype().is( py::dtype::of<float>() ) )
                {
                    QMessageBox::critical( nullptr, "", "Unexpected hypercube data type: " + QString::fromStdString( py::str { hypercube.dtype() } ), QMessageBox::Ok );
                    return nullptr;
                }

                if( !wavenumbers.dtype().is( py::dtype::of<double>() ) )
                {
                    QMessageBox::critical( nullptr, "", "Unexpected wavenumbers data type: " + QString::fromStdString( py::str { wavenumbers.dtype() } ), QMessageBox::Ok );
                    return nullptr;
                }

                const auto shape = py::tuple { hypercube.attr( "shape" ) };
                const auto dimensions = vec3<uint32_t> {
                    shape[0].cast<uint32_t>(),
                    shape[1].cast<uint32_t>(),
                    shape[2].cast<uint32_t>()
                };

                auto intensities_pointer = static_cast<float*>( hypercube.mutable_data() );
                auto wavenumbers_pointer = static_cast<double*>( wavenumbers.mutable_data() );

                dataset.reset(
                    new TensorDataset<float> { Dataset::SpatialMetadata { dimensions.x, dimensions.y },
                    Matrix<float>::from_pointer( { static_cast<size_t>( dimensions[0] ) * dimensions[1], dimensions[2] }, intensities_pointer ),
                    Array<double>::from_pointer( { dimensions[2] }, wavenumbers_pointer )
                    } );

                hypercube.release();
                wavenumbers.release();
            }
            catch( const py::error_already_set& error )
            {
                Logger::error() << "Python error during dataset import: " << error.what();
                QMessageBox::critical( nullptr, "", "Failed to import dataset", QMessageBox::Ok );
                return nullptr;
            }
        }
        else if( suffix == "h5" )
        {
            interpreter.reset( new py::interpreter {} );
            try
            {
                const auto file = py::module::import( "h5py" ).attr( "File" )( filepath.toStdString(), "r" );
                auto tensor = static_cast<py::array>( file["AB-Transpose-Multiple"]["Spec"][py::make_tuple()] );

                if( !tensor.dtype().is( py::dtype::of<float>() ) )
                {
                    QMessageBox::critical( nullptr, "", "Unexpected data type: " + QString::fromStdString( py::str { tensor.dtype() } ), QMessageBox::Ok );
                    return nullptr;
                }

                auto shape = py::tuple { tensor.attr( "shape" ) };
                auto dimensions = new QComboBox {};
                for( uint32_t i = 1; i <= shape[0].cast<uint32_t>(); ++i )
                {
                    if( shape[0].cast<double>() / i == shape[0].cast<uint32_t>() / i )
                    {
                        dimensions->addItem( QString::number( i ) + " x " + QString::number( shape[0].cast<uint32_t>() / i ), i );
                    }
                }
                dimensions->setCurrentIndex( dimensions->count() / 2 );

                auto channels_lower = new Override<double> { 1.0, std::nullopt };
                auto channels_upper = new Override<double> { shape[1].cast<double>(), std::nullopt };
                auto channels_range = new RangeInput { *channels_lower, *channels_upper };
                channels_lower->setParent( channels_range );
                channels_upper->setParent( channels_range );

                layout->addRow( "Dimensions", dimensions );
                layout->addRow( "Channels", channels_range );
                execute_dialog = true;

                QObject::connect( button_import, &QPushButton::clicked, [=, &dataset] () mutable
                {
                    const auto width = static_cast<uint32_t>( dimensions->currentData().toInt() );
                    const auto dimensions = vec3<uint32_t> {
                        width,
                        shape[0].cast<uint32_t>() / width,
                        shape[1].cast<uint32_t>()
                    };

                    auto intensities = Matrix<float>::from_pointer(
                        { shape[0].cast<uint32_t>(), shape[1].cast<uint32_t>() },
                        static_cast<float*>( tensor.mutable_data() )
                    );

                    auto channels = Array<double> { shape[1].cast<uint32_t>(), 0.0 };
                    const auto stepsize = ( channels_upper->value() - channels_lower->value() ) / ( channels.size() - 1 );
                    for( uint32_t i = 0; i < channels.size(); ++i )
                    {
                        channels.value( i ) = std::clamp( channels_lower->value() + i * stepsize, channels_lower->value(), channels_upper->value() );
                    }

                    const auto spatial_metadata = Dataset::SpatialMetadata { dimensions.x, dimensions.y };
                    dataset.reset( new TensorDataset { spatial_metadata, std::move( intensities ), std::move( channels ) } );
                    tensor.release();
                } );
            }
            catch( const py::error_already_set& error )
            {
                Logger::error() << "Python error during dataset import: " << error.what();
                QMessageBox::critical( nullptr, "", "Failed to import dataset", QMessageBox::Ok );
                return nullptr;
            }
        }
        else if( suffix == "mia" )
        {
            auto file = QFile { filepath };
            if( !file.open( QFile::ReadOnly ) )
            {
                QMessageBox::critical( nullptr, "", "Failed to open file.", QMessageBox::Ok );
                return nullptr;
            }

            auto stream = QDataStream { &file };

            uint64_t version;
            stream >> version;

            uint32_t element_count, channel_count;
            stream >> element_count >> channel_count;

            Dataset::SpatialMetadata spatial_metadata;
            stream.readRawData( reinterpret_cast<char*>( &spatial_metadata ), sizeof( Dataset::SpatialMetadata ) );

            Dataset::BaseType base_type;
            stream >> base_type;

            auto channel_positions = Array<double>::allocate( channel_count );
            stream.readRawData( reinterpret_cast<char*>( channel_positions.data() ), channel_positions.bytes() );

            if( base_type == Dataset::BaseType::eInt8 )
            {
                auto intensities = Matrix<int8_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<int8_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eInt16 )
            {
                auto intensities = Matrix<int16_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<int16_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eInt32 )
            {
                auto intensities = Matrix<int32_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<int32_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eUint8 )
            {
                auto intensities = Matrix<uint8_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<uint8_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eUint16 )
            {
                auto intensities = Matrix<uint16_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<uint16_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eUint32 )
            {
                auto intensities = Matrix<uint32_t>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<uint32_t> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eFloat )
            {
                auto intensities = Matrix<float>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<float> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else if( base_type == Dataset::BaseType::eDouble )
            {
                auto intensities = Matrix<double>::allocate( { element_count, channel_count } );
                stream.readRawData( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
                dataset.reset( new TensorDataset<double> { spatial_metadata, std::move( intensities ), std::move( channel_positions ) } );
            }
            else
            {
                QMessageBox::critical( nullptr, "", "Unsupported dataset value type.", QMessageBox::Ok );
                return nullptr;
            }
        }
        else if( suffix == "rpl" )
        {
            auto metadata_file = QFile { filepath };
            if( !metadata_file.open( QFile::ReadOnly | QFile::Text ) )
            {
                QMessageBox::critical( nullptr, "", "Failed to open metadata file: " + filepath, QMessageBox::Ok );
                return nullptr;
            }

            auto metadata_stream = QTextStream { &metadata_file };

            auto dimensions = vec3<uint32_t> {};
            auto datalength = QString {};
            auto datatype = QString {};
            auto byteorder = QString {};

            while( !metadata_stream.atEnd() )
            {
                auto line = metadata_stream.readLine();
                auto linestream = QTextStream { &line, QTextStream::ReadOnly };

                auto key = QString {};
                auto value = QString {};
                linestream >> key >> value;

                if( key == "width" ) dimensions.x = static_cast<uint32_t>( value.toULongLong() );
                else if( key == "height" ) dimensions.y = static_cast<uint32_t>( value.toULongLong() );
                else if( key == "depth" ) dimensions.z = static_cast<uint32_t>( value.toULongLong() );
                else if( key == "data-length" ) datalength = value;
                else if( key == "data-type" ) datatype = value;
                else if( key == "byte-order" ) byteorder = value;
            }

            const auto pixelcount = dimensions.x * dimensions.y;
            if( dimensions.x == 0 || dimensions.y == 0 || dimensions.z == 0 )
            {
                QMessageBox::critical( nullptr, "", "Invalid dimensions in metadata file", QMessageBox::Ok );
                return nullptr;
            }

            if( datalength != "1" && byteorder != "little-endian" )
            {
                QMessageBox::critical( nullptr, "", "Unsupported byte order: " + byteorder, QMessageBox::Ok );
                return nullptr;
            }

            const auto spatial_metadata = Dataset::SpatialMetadata { dimensions.x, dimensions.y };

            auto channels = Array<double> { dimensions.z, 0.0 };
            std::iota( channels.begin(), channels.end(), 0.0 );

            const auto intensities_filepath = fileinfo.absolutePath() + "/" + fileinfo.completeBaseName() + ".raw";
            auto intensities_file = QFile { intensities_filepath };
            if( !intensities_file.open( QFile::ReadOnly ) )
            {
                QMessageBox::critical( nullptr, "", "Failed to open file: " + intensities_filepath, QMessageBox::Ok );
                return nullptr;
            }

            const auto read_intensities = [&] ( auto&& intensities )
            {
                auto intensities_stream = QDataStream { &intensities_file };
                intensities_stream.readRawData( reinterpret_cast<char*>( intensities.data() ), static_cast<qint64>( intensities.bytes() ) );
                dataset.reset( new TensorDataset { spatial_metadata, std::move( intensities ), std::move( channels ) } );
            };

            if( datatype == "unsigned" )
            {
                if( datalength == "1" ) read_intensities( Matrix<uint8_t>::allocate( { pixelcount, dimensions.z } ) );
                else if( datalength == "2" ) read_intensities( Matrix<uint16_t>::allocate( { pixelcount, dimensions.z } ) );
                else if( datalength == "4" ) read_intensities( Matrix<uint32_t>::allocate( { pixelcount, dimensions.z } ) );
            }
            else if( datatype == "signed" )
            {
                if( datalength == "1" ) read_intensities( Matrix<int8_t>::allocate( { pixelcount, dimensions.z } ) );
                else if( datalength == "2" ) read_intensities( Matrix<int16_t>::allocate( { pixelcount, dimensions.z } ) );
                else if( datalength == "4" ) read_intensities( Matrix<int32_t>::allocate( { pixelcount, dimensions.z } ) );
            }
            else
            {
                QMessageBox::critical( nullptr, "", "Unsupported data type: " + datatype, QMessageBox::Ok );
                return nullptr;
            }
        }
        else
        {
            QMessageBox::critical( nullptr, "", "Unsupported file format: " + filename, QMessageBox::Ok );
            return nullptr;
        }

        if( execute_dialog )
        {
            layout->addItem( new QSpacerItem { 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding } );
            layout->addRow( button_import );

            QObject::connect( button_import, &QPushButton::clicked, &dialog, &QDialog::accept );
            dialog.exec();
        }
    }

    return dataset;
}