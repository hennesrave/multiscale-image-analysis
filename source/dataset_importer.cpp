#include "dataset_importer.hpp"

#include "dataset.hpp"
#include "number_input.hpp"
#include "python.hpp"

#include "json.hpp"

#include <fstream>
#include <regex>
#include <sstream>

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qmessagebox.h>
#include <qpushbutton.h>

namespace from_laser_lines_helper
{
    // Value written into grid cells that no laser shot ever covered, including
      // whole rows belonging to missing laser line files. If the downstream
      // pipeline cannot tolerate NaN, change this one constant -- but do not use
      // 0.0 without thinking, it biases every statistic and colour scale
      // computed over the map.
    constexpr auto unsampled_value = std::numeric_limits<double>::quiet_NaN();

    constexpr auto maximum_header_lines = uint32_t { 64 };

    // How many consecutive missing files end the sequence rather than count as
    // a gap within it.
    constexpr auto maximum_line_gap = uint32_t { 8 };

    // How many missing line numbers to spell out before abbreviating.
    constexpr auto maximum_listed_gaps = std::size_t { 12 };

    // Two consecutive x positions within a line may differ from the line's
    // nominal pitch by this fraction before the line is rejected as irregular.
    constexpr auto relative_pitch_tolerance = double { 0.02 };

    // A line's y position must sit this close to the global row lattice,
    // expressed as a fraction of the row pitch.
    constexpr auto row_lattice_tolerance = double { 0.25 };

    // Maximum permitted spread of y within a single line, in micrometres.
    // Non-zero spread means the raster is rotated, which this importer does
    // not support (rows would no longer be iso-y).
    constexpr auto line_flatness_tolerance = double { 0.5 };

    void report_error( const std::string& message )
    {
        Console::error( message );
        QMessageBox::critical( nullptr, "", QString::fromStdString( message ), QMessageBox::Ok );
    }

    void report_warning( const std::string& message )
    {
        // Swap for Console::warning if the project has one.
        Console::info( "Warning: " + message );
    }

    [[nodiscard]] std::string trimmed( std::string_view text )
    {
        constexpr auto whitespace = std::string_view { " \t\r\n" };
        const auto begin = text.find_first_not_of( whitespace );
        if( begin == std::string_view::npos )
        {
            return {};
        }
        const auto end = text.find_last_not_of( whitespace );
        return std::string { text.substr( begin, end - begin + 1 ) };
    }

    [[nodiscard]] std::string lowercased( std::string_view text )
    {
        auto result = std::string { text };
        std::transform( result.begin(), result.end(), result.begin(), [] ( unsigned char character )
        {
            return static_cast<char>( std::tolower( character ) );
        } );
        return result;
    }

    [[nodiscard]] std::vector<std::string> split_trimmed( std::string_view text, char delimiter )
    {
        auto tokens = std::vector<std::string> {};
        auto begin = std::size_t { 0 };
        while( true )
        {
            const auto end = text.find( delimiter, begin );
            const auto count = ( end == std::string_view::npos ) ? std::string_view::npos : end - begin;
            tokens.push_back( trimmed( text.substr( begin, count ) ) );
            if( end == std::string_view::npos )
            {
                break;
            }
            begin = end + 1;
        }
        while( !tokens.empty() && tokens.back().empty() )
        {
            tokens.pop_back();
        }
        return tokens;
    }

    [[nodiscard]] std::optional<double> parse_number( const std::string& token )
    {
        if( token.empty() )
        {
            return std::nullopt;
        }
        auto value = double {};
        const auto* const first = token.data();
        const auto* const last = token.data() + token.size();
        const auto result = std::from_chars( first, last, value );
        if( result.ec != std::errc {} || result.ptr != last || !std::isfinite( value ) )
        {
            return std::nullopt;
        }
        return value;
    }

    // Intensities may legitimately be NaN; coordinates and metadata may not.
    [[nodiscard]] std::optional<double> parse_intensity( const std::string& token )
    {
        if( token.empty() )
        {
            return unsampled_value;         // empty cell == not measured
        }
        auto value = double {};
        const auto* const last = token.data() + token.size();
        const auto result = std::from_chars( token.data(), last, value );
        if( result.ec != std::errc {} || result.ptr != last )
        {
            return std::nullopt;            // genuinely unparseable
        }
        return std::isnan( value ) ? unsampled_value : value;
    }

    [[nodiscard]] std::string joined( const std::vector<uint32_t>& numbers, std::string_view format )
    {
        auto text = std::string {};
        const auto listed = std::min( numbers.size(), maximum_listed_gaps );
        for( std::size_t index = 0; index < listed; ++index )
        {
            if( index > 0 )
            {
                text += ", ";
            }
            text += std::vformat( format, std::make_format_args( numbers[index] ) );
        }
        if( listed < numbers.size() )
        {
            text += std::format( " and {} more", numbers.size() - listed );
        }
        return text;
    }
}

QSharedPointer<Dataset> DatasetImporter::from_csv( const std::filesystem::path& filepath )
{
    const auto filename = filepath.filename();

    auto filestream = std::ifstream { filepath };
    auto linestring = std::string {};
    std::getline( filestream, linestring, '\n' );
    filestream.close();

    if( linestring.find( "# SPCal Export" ) != std::string::npos )
    {
        return DatasetImporter::from_single_particle_csv( filepath );
    }
    else if( linestring.find( "SpTool version" ) == 0 )
    {
        return DatasetImporter::from_single_particle_composition( filepath );
    }
    else if( linestring.find( ";;;;Sample 1" ) != std::string::npos )
    {
        return DatasetImporter::from_la_icp_ms( filepath );
    }
    else if( std::regex_search( filename.string(), std::regex { R"(line_\d+\.csv)" } ) && linestring.find( "Timestamp" ) == 0 )
    {
        return DatasetImporter::from_laser_lines( filepath );
    }
    else if( std::regex_search( filename.string(), std::regex { R"([A-z]+?\d+?\.csv)" } ) )
    {
        if( linestring.find( "X,Y,I" ) != std::string::npos )
        {
            //return DatasetImporter::from_xyz( filepath );
        }
        else
        {
            return DatasetImporter::from_matrix( filepath );
        }
    }

    QMessageBox::critical( nullptr, "", "Unsupported file format" );
    return nullptr;
}
QSharedPointer<Dataset> DatasetImporter::from_data_dims_freq( const std::filesystem::path& input_filepath )
{
    enum class ValueType : uint32_t
    {
        eInvalid,
        eInt8,
        eInt16,
        eInt32,
        eInt64,

        eUint8,
        eUint16,
        eUint32,
        eUint64,

        eFloat,
        eDouble
    };

    auto filepath = input_filepath;
    filepath.replace_extension( "dims" );

    auto dimensions = vec3<uint32_t> {};
    auto value_type = ValueType::eInvalid;

    std::ifstream stream { filepath, std::ios::in | std::ios::binary };
    stream.read( reinterpret_cast<char*>( &dimensions ), sizeof( vec3<uint32_t> ) );
    stream.read( reinterpret_cast<char*>( &value_type ), sizeof( ValueType ) );

    filepath.replace_extension( "freq" );

    stream = std::ifstream { filepath, std::ios::in | std::ios::binary | std::ios::ate };
    auto channels = Array<double>::allocate( dimensions.z );

    const auto channels_element_size = stream.tellg() / dimensions.z;
    stream.seekg( 0 );

    if( channels_element_size == sizeof( double ) )
    {
        stream.read( reinterpret_cast<char*>( channels.data() ), channels.size() * sizeof( double ) );
    }
    else if( channels_element_size == sizeof( float ) )
    {
        std::vector<float> channels_float( dimensions.z );
        stream.read( reinterpret_cast<char*>( channels_float.data() ), channels.size() * sizeof( float ) );
        for( uint32_t i = 0; i < dimensions.z; ++i )
            channels[i] = static_cast<double>( channels_float[i] );
    }

    filepath.replace_extension( "data" );
    if( value_type == ValueType::eInvalid )
    {
        stream = std::ifstream { filepath, std::ios::in | std::ios::binary | std::ios::ate };
        const auto values_element_size = stream.tellg() / dimensions.cast<uint64_t>().product();
        value_type = values_element_size == 4 ? ValueType::eFloat : ValueType::eDouble;
    }
    stream = std::ifstream { filepath, std::ios::in | std::ios::binary };

    Dataset* dataset = nullptr;
    const auto import_dataset = [&] ( auto&& intensities )
    {
        stream.read( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );

        // transpose spatial dimensions
        auto intensities_transposed = Matrix<typename std::decay_t<decltype( intensities )>::value_type>::allocate( { dimensions.x * dimensions.y, dimensions.z } );

        for( size_t z = 0; z < dimensions.z; ++z )
        {
            for( size_t y = 0; y < dimensions.y; ++y )
            {
                for( size_t x = 0; x < dimensions.x; ++x )
                {
                    intensities_transposed.value( { x + y * dimensions.x, z } ) = intensities.value( { y + x * dimensions.y, z } );
                }
            }
        }

        dataset = new TensorDataset { std::move( intensities_transposed ), std::move( channels ) };
        dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( dimensions.x, dimensions.y ) );
    };

    switch( value_type )
    {
    case ValueType::eInt8: import_dataset( Matrix<int8_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eInt16: import_dataset( Matrix<int16_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eInt32: import_dataset( Matrix<int32_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eUint8: import_dataset( Matrix<uint8_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eUint16: import_dataset( Matrix<uint16_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eUint32: import_dataset( Matrix<uint32_t>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eFloat: import_dataset( Matrix<float>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    case ValueType::eDouble: import_dataset( Matrix<double>::allocate( { dimensions.x * dimensions.y, dimensions.z } ) ); break;
    default:
    {
        Console::error( "Invalid value type" );
        QMessageBox::critical( nullptr, "", "Invalid value type", QMessageBox::Ok );
        return nullptr;
    }
    }

    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_hdf5( const std::filesystem::path& filepath )
{
    auto interpreter = py::interpreter {};
    try
    {
        const auto file = py::module::import( "h5py" ).attr( "File" )( filepath.string(), "r" );
        auto tensor = static_cast<py::array>( file["AB-Transpose-Multiple"]["Spec"][py::make_tuple()] );

        if( !tensor.dtype().is( py::dtype::of<float>() ) )
        {
            Console::error( "Unsupported data type: " + std::string { py::str { tensor.dtype() } } );
            QMessageBox::critical( nullptr, "", "Unsupported data type", QMessageBox::Ok );
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

        auto button_import = new QPushButton { "Import" };

        auto dialog = QDialog {};
        dialog.setWindowTitle( "Import Dataset..." );
        dialog.setMinimumSize( 300, 400 );

        auto layout = new QFormLayout { &dialog };
        layout->setContentsMargins( 20, 10, 20, 10 );
        layout->setHorizontalSpacing( 30 );

        layout->addRow( "Dimensions", dimensions );
        layout->addRow( "Channels", channels_range );
        layout->addItem( new QSpacerItem { 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding } );
        layout->addRow( button_import );

        auto dataset = QSharedPointer<Dataset> {};

        QObject::connect( button_import, &QPushButton::clicked, [&]
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
            tensor.release();

            auto channels = Array<double> { shape[1].cast<uint32_t>(), 0.0 };
            const auto stepsize = ( channels_upper->value() - channels_lower->value() ) / ( channels.size() - 1 );
            for( uint32_t i = 0; i < channels.size(); ++i )
            {
                channels.value( i ) = std::clamp( channels_lower->value() + i * stepsize, channels_lower->value(), channels_upper->value() );
            }

            dataset.reset( new TensorDataset { std::move( intensities ), std::move( channels ) } );
            dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( dimensions.x, dimensions.y ) );
            dialog.accept();
        } );

        if( dialog.exec() == QDialog::Accepted )
        {
            return dataset;
        }
    }
    catch( const py::error_already_set& error )
    {
        Console::error( std::format( "Python error during dataset import: {}", error.what() ) );
        QMessageBox::critical( nullptr, "", "Failed to import dataset", QMessageBox::Ok );
    }

    return nullptr;
}
QSharedPointer<Dataset> DatasetImporter::from_matrix( const std::filesystem::path& base_filepath )
{
    auto filepaths = std::vector<std::filesystem::path> {};

    const auto filename_regex = std::regex { R"(^(.+?(\d+?))\.csv$)" };
    const auto directory = base_filepath.parent_path();
    for( const auto& entry : std::filesystem::directory_iterator { directory } )
    {
        if( !entry.is_regular_file() )
        {
            continue;
        }

        const auto filename = entry.path().filename().string();
        auto matches = std::smatch {};
        if( !std::regex_match( filename, matches, filename_regex ) )
        {
            continue;
        }

        filepaths.push_back( entry.path() );
    }

    auto channel_count          = static_cast<uint32_t>( filepaths.size() );
    auto channel_positions      = Array<double>::allocate( channel_count );
    auto channel_identifiers    = Array<QString> { channel_count, QString {} };

    for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
    {
        const auto& filepath    = filepaths[channel_index];
        const auto filename     = filepath.filename().string();
        auto matches            = std::smatch {};
        std::regex_match( filename, matches, filename_regex );

        channel_positions[channel_index]    = std::stod( matches[2].str() );
        channel_identifiers[channel_index]  = QString::fromStdString( matches[1].str() );
    }

    auto permutation = Array<uint32_t>::allocate( channel_count );
    std::iota( permutation.begin(), permutation.end(), 0 );
    std::sort( permutation.begin(), permutation.end(), [&] ( uint32_t a, uint32_t b )
    {
        return channel_positions[a] < channel_positions[b];
    } );

    utility::apply_permutation( filepaths.data(), filepaths.data() + channel_count, permutation.begin() );
    utility::apply_permutation( channel_positions.data(), channel_positions.data() + channel_count, permutation.begin() );
    utility::apply_permutation( channel_identifiers.data(), channel_identifiers.data() + channel_count, permutation.begin() );

    auto dimensions         = vec2<uint32_t> { 0, 0 };
    auto spatial_metadata   = std::unique_ptr<Dataset::SpatialMetadata> {};
    auto intensities        = Matrix<double> {};

    for( uint32_t channel_index = 0; channel_index < filepaths.size(); ++channel_index )
    {
        const auto& filepath = filepaths[channel_index];

        auto filestream = std::ifstream { filepath, std::ios::in };
        auto lines      = std::vector<std::string> {};
        auto linestring = std::string {};
        while( std::getline( filestream, linestring, '\n' ) )
        {
            if( !linestring.empty() )
            {
                lines.push_back( linestring );
            }
        }

        if( lines.empty() )
        {
            QMessageBox::critical( nullptr, "", "Failed to import dataset: empty file" );
            return nullptr;
        }

        auto current_dimensions = vec2<uint32_t> {
            static_cast<uint32_t>( std::count( lines[0].begin(), lines[0].end(), ',' ) + 1 ),
            static_cast<uint32_t>( lines.size() )
        };

        if( intensities.empty() )
        {
            dimensions = current_dimensions;
            spatial_metadata.reset( new Dataset::SpatialMetadata { dimensions.x, dimensions.y } );
            intensities = Matrix<double>::allocate( { dimensions.x * dimensions.y, channel_count } );

            Console::info( std::format( "Importing dataset with dimensions {} x {} and {} channels", dimensions.x, dimensions.y, channel_count ) );
        }
        else
        {
            if( false && current_dimensions != dimensions )
            {
                Console::error( std::format( "File {} has incompatible dimensions ({} x {})", filepath.filename().string(), current_dimensions.x, current_dimensions.y ) );
                QMessageBox::critical( nullptr, "", "Failed to import dataset: incompatible dimensions" );
                return nullptr;
            }
        }

        for( uint32_t y = 0; y < lines.size(); ++y )
        {
            auto linestream = std::stringstream { lines[y] };
            auto tokenstring = std::string {};

            for( uint32_t x = 0; x < dimensions.x; ++x )
            {
                //if( !std::getline( linestream, tokenstring, ',' ) )
                //{
                //    Console::error( std::format( "Failed to parse value at ({}, {}) in file {}", x, y, filepath.string() ) );
                //    QMessageBox::critical( nullptr, "", "Failed to import dataset: invalid format" );
                //    return nullptr;
                //}
                if( !std::getline( linestream, tokenstring, ',' ) )
                {
                    tokenstring.clear();
                }

                const auto element_index = spatial_metadata->element_index( vec2<uint32_t> { x, y } );
                const auto value = tokenstring.empty() ? 0.0 : std::stod( tokenstring );

                intensities.update_value( { element_index, channel_index }, value );
            }
        }
    }

    auto dataset = new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    dataset->update_spatial_metadata( std::move( spatial_metadata ) );

    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_mia( const std::filesystem::path& filepath )
{
    auto stream = MIAFileStream { filepath, std::ios::in };
    if( !stream )
    {
        Console::error( "Failed to open file: " + filepath.string() );
        QMessageBox::critical( nullptr, "", "Failed to open file" );
        return nullptr;
    }
    return stream.read<QSharedPointer<Dataset>>();
}
QSharedPointer<Dataset> DatasetImporter::from_la_icp_ms( const std::filesystem::path& filepath )
{
    auto filestream     = std::ifstream { filepath };
    auto linestring     = std::string {};

    // Ignore the first two lines
    std::getline( filestream, linestring, '\n' );
    std::getline( filestream, linestring, '\n' );

    auto dimensions = vec2<uint32_t> { 0, 0 };
    for( const auto character : linestring )
    {
        if( character == ';' )
        {
            dimensions.x++;
        }
    }
    dimensions.x -= 4;

    auto identifiers    = std::vector<std::string> {};
    auto positions      = std::vector<double> {};

    auto intensities = std::vector<double> {};
    while( std::getline( filestream, linestring, '\n' ) )
    {
        if( linestring.empty() )
        {
            continue;
        }

        auto linestream     = std::stringstream { linestring };
        auto tokenstring    = std::string {};

        std::getline( linestream, tokenstring, ';' ); // Ignore "MainRuns"

        // Read second coordinate
        std::getline( linestream, tokenstring, ';' );
        dimensions.y = std::max( dimensions.y, std::stoi( tokenstring ) + 1u );

        // Read identifier
        std::getline( linestream, tokenstring, ';' );

        auto identifier = tokenstring;
        if( const auto space_position = identifier.find( ' ' ); space_position != std::string::npos )
        {
            identifier = std::string { identifier.begin(), identifier.begin() + space_position };
        }

        if( identifiers.empty() || identifiers.back() != identifier )
        {
            identifiers.push_back( identifier );
            positions.push_back( std::stod( identifier ) );
        }

        // Read "Y" or "Time"
        std::getline( linestream, tokenstring, ';' );
        if( tokenstring != "Y" )
        {
            continue;
        }

        // Read intensities
        while( std::getline( linestream, tokenstring, ';' ) )
        {
            if( tokenstring.empty() )
            {
                intensities.push_back( 0.0 );
            }
            else
            {
                intensities.push_back( std::stod( tokenstring ) );
            }
        }
    }

    auto spatial_metadata = std::unique_ptr<Dataset::SpatialMetadata> { new Dataset::SpatialMetadata {
        dimensions.x, dimensions.y
    } };

    auto matrix = Matrix<double>::allocate( {
        dimensions.x * dimensions.y,
        positions.size()
    } );

    for( size_t value_index = 0; value_index < intensities.size(); ++value_index )
    {
        const auto x = static_cast<uint32_t>( value_index % dimensions.x );
        const auto y = static_cast<uint32_t>( ( value_index / dimensions.x ) % dimensions.y );
        const auto channel_index = static_cast<uint32_t>( value_index / ( dimensions.x * dimensions.y ) );

        const auto element_index = spatial_metadata->element_index( vec2<uint32_t> { x, y } );
        matrix.update_value( { element_index, channel_index }, intensities[value_index] );
    }

    auto channel_identifiers = Array<QString> { identifiers.size(), QString {} };
    auto channel_positions = Array<double>::allocate( positions.size() );
    for( uint32_t channel_index = 0; channel_index < positions.size(); ++channel_index )
    {
        channel_identifiers[channel_index] = QString::fromStdString( identifiers[channel_index] );
        channel_positions[channel_index] = positions[channel_index];
    }

    auto dataset = new TensorDataset<double> { std::move( matrix ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    dataset->update_spatial_metadata( std::move( spatial_metadata ) );

    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_laser_info( const std::filesystem::path& filepath )
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
        Console::error( "Invalid line number encountered: " + std::to_string( laser_lines[0].line ) );
        QMessageBox::critical( nullptr, "", "Invalid line number encountered" );
        return nullptr;
    }

    for( const auto& laser_line : laser_lines )
    {
        if( laser_line.line_type != 0 )
        {
            Console::error( "Unsupported line type encountered: " + std::to_string( laser_line.line_type ) );
            QMessageBox::critical( nullptr, "", "Unsupported line type encountered" );
            return nullptr;
        }
    }

    auto ignore_invalid_vertical_spacing = false;

    for( size_t i = 1; i < laser_lines.size(); ++i )
    {
        const auto& previous = laser_lines[i - 1];
        const auto& current = laser_lines[i];

        if( current.line != previous.line + 1 )
        {
            QMessageBox::critical( nullptr, "", "Invalid line number encountered" );
            return nullptr;
        }

        if( current.spot_spacing != previous.spot_spacing )
        {
            QMessageBox::critical( nullptr, "", "Varying spot spacing not supported" );
            return nullptr;
        }

        if( current.spot_size != previous.spot_size )
        {
            QMessageBox::critical( nullptr, "", "Varying spot size not supported" );
            return nullptr;
        }

        if( !ignore_invalid_vertical_spacing && std::abs( current.start_y - ( previous.start_y + current.spot_size ) ) > 1.0e-6 )
        {
            const auto result = QMessageBox::critical( nullptr, "", "Vertical spacing must be equal to spot size", QMessageBox::Abort | QMessageBox::Ignore, QMessageBox::Abort );
            if( result == QMessageBox::Ignore )
            {
                ignore_invalid_vertical_spacing = true;
            }
            else
            {
                return nullptr;
            }
        }

        if( current.number_of_shots != previous.number_of_shots )
        {
            QMessageBox::critical( nullptr, "", "Varying number of shots not supported" );
            return nullptr;
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

        Console::info( std::format( "Importing subdirectory: {}", subdirectory.string() ) );

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
            Console::info( filepath.string() );

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
                spatial_metadata = Dataset::SpatialMetadata { dimensions.x, dimensions.y };
                intensities = Matrix<float> { { dimensions[0] * dimensions[1], dimensions[2] }, 0.0f };
            }

            for( auto spectrum_iterator = values.begin(); spectrum_iterator != values.end(); spectrum_iterator += spectrum_size )
            {
                const auto header = reinterpret_cast<const Header*>( &( *spectrum_iterator ) );
                const auto elapsed_time = static_cast<uint64_t>( header->acquisition_number ) * acquisition_period;
                const auto input_spectrum = reinterpret_cast<const Result*>( &( *spectrum_iterator ) + sizeof( Header ) );

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
        const auto result = QMessageBox::question( nullptr, "", "Subtract average background spectrum for baseline correction?" );
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
        const auto result = QMessageBox::question( nullptr, "", "Invalid channel order detected. Fix the problem via permutation?" );
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
    }

    auto dataset = new TensorDataset<float> { std::move( intensities ), std::move( channels ) };
    dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( dimensions[0], dimensions[1] ) );
    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_laser_lines( const std::filesystem::path& filepath )
{
    using namespace from_laser_lines_helper;

    const auto directory = filepath.parent_path();

    const auto line_filepath = [&directory] ( uint32_t file_number )
    {
        return directory / std::format( "line_{}.csv", file_number );
    };

    // A pixel keeps its absolute stage position. Grid indices are derived only
    // once every line has been read, because the position of a line along x is
    // not implied by its position in the file: the ablated region's edges need
    // not be parallel to the stage axes, so successive lines can start at
    // different x and can contain different numbers of pixels.
    struct Pixel
    {
        vec2<double> position { 0.0, 0.0 };     // [um]
        std::vector<double> intensities {};
    };

    struct LaserLine
    {
        uint32_t file_number { 0 };
        double y { 0.0 };                       // [um]
        double pitch { 0.0 };                    // unsigned along-line step [um]
        uint32_t pixel_count { 0 };
        bool left_to_right { true };
    };

    // --- discover the laser line files --------------------------------------
    // Done up front so that a gap can be reported before the user waits for a
    // full import. A run of more than maximum_line_gap missing numbers ends
    // the sequence; anything shorter is a hole to be asked about.
    auto present_numbers = std::vector<uint32_t> {};
    auto missing_numbers = std::vector<uint32_t> {};

    for( uint32_t file_number = 1; true; ++file_number )
    {
        if( std::filesystem::exists( line_filepath( file_number ) ) )
        {
            present_numbers.push_back( file_number );
            continue;
        }

        auto continues = false;
        for( uint32_t probe = 1; probe <= maximum_line_gap; ++probe )
        {
            if( std::filesystem::exists( line_filepath( file_number + probe ) ) )
            {
                continues = true;
                break;
            }
        }
        if( !continues )
        {
            break;
        }
        missing_numbers.push_back( file_number );
    }

    if( present_numbers.empty() )
    {
        report_error( "No laser line files found next to " + filepath.string() );
        return nullptr;
    }

    if( !missing_numbers.empty() )
    {
        const auto summary = std::format( "{} laser line file(s) missing: {}",
            missing_numbers.size(), joined( missing_numbers, "line_{}.csv" ) );
        report_warning( summary );

        const auto question = QString::fromStdString( summary
            + ".\n\nThe corresponding image row(s) will be left unsampled and filled "
              "with the default fill value. The remaining lines are placed by their "
              "recorded stage coordinates, so the geometry of the map is preserved."
              "\n\nContinue with the import?" );

        const auto answer = QMessageBox::question( nullptr, "", question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
        if( answer != QMessageBox::Yes )
        {
            Console::info( "Laser line import cancelled because of missing files" );
            return nullptr;
        }
        Console::info( "Continuing the import with missing laser lines left unsampled" );
    }

    auto columns = std::vector<std::string> {};
    auto pixels = std::vector<Pixel> {};
    auto lines = std::vector<LaserLine> {};

    for( const auto file_number : present_numbers )
    {
        const auto laser_line_filepath = line_filepath( file_number );

        auto filestream = std::ifstream { laser_line_filepath };
        if( !filestream )
        {
            report_error( "Failed to open laser line file: " + laser_line_filepath.string() );
            return nullptr;
        }

        Console::info( std::format( "Importing {}...", laser_line_filepath.string() ) );

        // --- metadata block -------------------------------------------------
        // Read labelled rows until the column header appears, rather than
        // assuming a fixed count, so an extra metadata row does not corrupt
        // the import.
        auto metadata = std::unordered_map<std::string, std::vector<std::string>> {};
        auto header_tokens = std::vector<std::string> {};
        auto linestring = std::string {};

        for( uint32_t header_line = 0; header_line < maximum_header_lines; ++header_line )
        {
            if( !std::getline( filestream, linestring, '\n' ) )
            {
                break;
            }

            auto tokens = split_trimmed( linestring, ',' );
            if( tokens.empty() )
            {
                continue;
            }

            if( lowercased( tokens.front() ).starts_with( "cycle time" ) )
            {
                header_tokens = std::move( tokens );
                break;
            }

            auto key = tokens.front();
            if( key.ends_with( ':' ) )
            {
                key.pop_back();
            }
            metadata.insert_or_assign(
                lowercased( trimmed( key ) ),
                std::vector<std::string> { tokens.begin() + 1, tokens.end() } );
        }

        if( header_tokens.size() < 4
            || !lowercased( header_tokens[1] ).starts_with( "x" )
            || !lowercased( header_tokens[2] ).starts_with( "y" ) )
        {
            report_error( "Invalid or missing column header in laser line file: "
                + laser_line_filepath.string() );
            return nullptr;
        }

        auto file_columns = std::vector<std::string> { header_tokens.begin() + 3, header_tokens.end() };
        if( columns.empty() )
        {
            columns = std::move( file_columns );
            if( columns.empty() )
            {
                report_error( "No intensity columns in laser line file: "
                    + laser_line_filepath.string() );
                return nullptr;
            }
        }
        else if( file_columns != columns )
        {
            report_error( "Column layout differs from the first laser line file: "
                + laser_line_filepath.string() );
            return nullptr;
        }

        const auto metadata_number = [&metadata] ( std::string_view key ) -> std::optional<double>
        {
            const auto iterator = metadata.find( std::string { key } );
            if( iterator == metadata.end() || iterator->second.empty() )
            {
                return std::nullopt;
            }
            return parse_number( iterator->second.front() );
        };

        const auto spot_size = metadata_number( "spot size" );
        const auto spot_spacing = metadata_number( "spot spacing" );
        const auto number_of_shots = metadata_number( "number of shots" );

        // The row lattice is reconstructed from line numbers, so a filename
        // that disagrees with the recorded line number matters.
        const auto recorded_number = metadata_number( "laser line number" );
        if( recorded_number.has_value()
            && static_cast<uint32_t>( *recorded_number ) != file_number )
        {
            report_warning( std::format( "{} records laser line number {}",
                            laser_line_filepath.filename().string(),
                            static_cast<int64_t>( *recorded_number ) ) );
        }

        // --- direction of ablation ------------------------------------------
        // Both directions are accepted. Because pixels are placed by absolute
        // x, a right-to-left line needs no reversal: its rows simply arrive in
        // descending x. The declared direction is used only to validate that
        // the coordinates actually run the way the file claims.
        const auto direction_entry = metadata.find( "direction of ablation" );
        if( direction_entry == metadata.end() || direction_entry->second.empty() )
        {
            report_error( "Missing direction of ablation in laser line file: "
                + laser_line_filepath.string() );
            return nullptr;
        }

        auto direction_text = std::string {};
        for( const auto& token : direction_entry->second )
        {
            direction_text += lowercased( token ) + " ";
        }

        auto left_to_right = std::optional<bool> {};
        if( direction_text.find( "left to right" ) != std::string::npos )
        {
            left_to_right = true;
        }
        else if( direction_text.find( "right to left" ) != std::string::npos )
        {
            left_to_right = false;
        }
        else if( const auto code = parse_number( direction_entry->second.front() ) )
        {
            if( *code == 0.0 )
            {
                left_to_right = true;
            }
            else if( *code == 1.0 )
            {
                left_to_right = false;
            }
        }

        if( !left_to_right.has_value() )
        {
            report_error( "Unsupported direction of ablation: " + trimmed( direction_text ) );
            return nullptr;
        }

        // --- data rows ------------------------------------------------------
        const auto first_pixel_index = pixels.size();
        auto line_y = std::optional<double> {};
        auto previous_x = double { 0.0 };
        auto pitch = double { 0.0 };
        auto row_number = uint32_t { 0 };

        while( std::getline( filestream, linestring, '\n' ) )
        {
            auto tokens = split_trimmed( linestring, ',' );
            if( tokens.empty() )
            {
                continue;   // tolerate blank or trailing lines
            }
            ++row_number;

            if( tokens.size() != columns.size() + 3 )
            {
                report_error( std::format(
                    "Inconsistent number of columns in {} (row {}): expected {}, found {}",
                    laser_line_filepath.filename().string(), row_number,
                    columns.size() + 3, tokens.size() ) );
                return nullptr;
            }

            const auto x = parse_number( tokens[1] );
            const auto y = parse_number( tokens[2] );
            if( !x.has_value() || !y.has_value() )
            {
                report_error( std::format( "Invalid coordinates in {} (row {})",
                              laser_line_filepath.filename().string(), row_number ) );
                return nullptr;
            }

            // Rows of one line must share a y, otherwise the raster is rotated
            // and cannot be mapped onto an axis-aligned grid.
            if( !line_y.has_value() )
            {
                line_y = *y;
            }
            else if( std::abs( *y - *line_y ) > line_flatness_tolerance )
            {
                report_error( std::format(
                    "Laser line {} is not axis-aligned: y changes from {} to {} within the line",
                    file_number, *line_y, *y ) );
                return nullptr;
            }

            if( pixels.size() > first_pixel_index )
            {
                const auto step = *x - previous_x;
                if( ( *left_to_right && step <= 0.0 ) || ( !*left_to_right && step >= 0.0 ) )
                {
                    report_error( std::format(
                        "x coordinates in {} (row {}) contradict the declared direction of ablation",
                        laser_line_filepath.filename().string(), row_number ) );
                    return nullptr;
                }
                if( pitch == 0.0 )
                {
                    pitch = std::abs( step );
                }
                else if( std::abs( std::abs( step ) - pitch ) > relative_pitch_tolerance * pitch )
                {
                    report_error( std::format(
                        "Irregular pixel pitch in {} (row {}): expected {}, found {}",
                        laser_line_filepath.filename().string(), row_number, pitch, std::abs( step ) ) );
                    return nullptr;
                }
            }
            previous_x = *x;

            auto pixel = Pixel {
                .position = vec2<double> { *x, *y },
                .intensities = {}
            };
            pixel.intensities.reserve( columns.size() );

            for( std::size_t token_index = 3; token_index < tokens.size(); ++token_index )
            {
                const auto intensity = parse_intensity( tokens[token_index] );
                if( !intensity.has_value() )
                {
                    report_error( std::format( "Invalid intensity '{}' in {} (row {}, column {})",
                                  tokens[token_index], laser_line_filepath.filename().string(),
                                  row_number, columns[token_index - 3] ) );
                    return nullptr;
                }
                pixel.intensities.push_back( *intensity );
            }

            pixels.push_back( std::move( pixel ) );
        }

        const auto pixel_count = static_cast<uint32_t>( pixels.size() - first_pixel_index );
        if( pixel_count == 0 )
        {
            report_error( "No data rows in laser line file: " + laser_line_filepath.string() );
            return nullptr;
        }

        // --- consistency of the export against the acquisition settings -----
        // Spot spacing is the pulse-to-pulse increment, not the export pitch:
        // several shots are aggregated into one exported row. Verify that the
        // aggregation factor is integral and that the row count matches the
        // declared shot count, so a changed export setting is noticed here
        // instead of quietly distorting the map.
        if( pixel_count > 1 && spot_spacing.has_value() && *spot_spacing > 0.0 )
        {
            const auto shots_per_pixel = pitch / *spot_spacing;
            const auto rounded = std::round( shots_per_pixel );
            if( rounded < 1.0 || std::abs( shots_per_pixel - rounded ) > 0.01 )
            {
                report_warning( std::format(
                    "Laser line {}: pixel pitch {} um is not an integer multiple of the {} um spot spacing",
                    file_number, pitch, *spot_spacing ) );
            }
            else if( number_of_shots.has_value() && *number_of_shots > 0.0 )
            {
                const auto expected = static_cast<uint32_t>( std::ceil( *number_of_shots / rounded ) );
                if( expected != pixel_count )
                {
                    report_warning( std::format(
                        "Laser line {}: {} rows exported but {} shots at {} shots/pixel imply {}",
                        file_number, pixel_count, static_cast<uint64_t>( *number_of_shots ),
                        static_cast<uint32_t>( rounded ), expected ) );
                }
            }
        }

        if( pixel_count > 1 && spot_size.has_value() && *spot_size > 0.0
            && std::abs( pitch - *spot_size ) > relative_pitch_tolerance * *spot_size )
        {
            report_warning( std::format(
                "Laser line {}: pixel pitch {} um differs from the {} um spot size",
                file_number, pitch, *spot_size ) );
        }

        lines.push_back( LaserLine {
            .file_number = file_number,
            .y = *line_y,
            .pitch = pitch,
            .pixel_count = pixel_count,
            .left_to_right = *left_to_right
        } );
    }

    if( pixels.empty() || lines.empty() )
    {
        report_error( "Invalid laser line data: no pixels were read" );
        return nullptr;
    }

    // --- derive the global lattice ------------------------------------------
    auto column_pitch = double { 0.0 };
    for( const auto& line : lines )
    {
        if( line.pitch <= 0.0 )
        {
            continue;
        }
        if( column_pitch == 0.0 )
        {
            column_pitch = line.pitch;
        }
        else if( std::abs( line.pitch - column_pitch ) > relative_pitch_tolerance * column_pitch )
        {
            report_error( std::format(
                "Laser line {} uses a pixel pitch of {} um, incompatible with {} um used by earlier lines",
                line.file_number, line.pitch, column_pitch ) );
            return nullptr;
        }
    }
    if( column_pitch == 0.0 )
    {
        report_error( "Unable to determine the pixel pitch: every laser line holds a single pixel" );
        return nullptr;
    }

    // The line spacing is measured per unit of line number, not per present
    // file, so a hole in the sequence cannot inflate it: with line_64 absent,
    // lines 63 and 65 sit 320 um apart across two line numbers and still yield
    // a 160 um spacing.
    auto row_pitch = double { 0.0 };
    auto descending = false;
    for( std::size_t index = 1; index < lines.size(); ++index )
    {
        const auto& previous = lines[index - 1];
        const auto& current = lines[index];

        const auto position_gap = current.y - previous.y;
        if( std::abs( position_gap ) <= line_flatness_tolerance )
        {
            report_error( std::format( "Laser lines {} and {} share the y position {} um",
                          previous.file_number, current.file_number, current.y ) );
            return nullptr;
        }
        if( index == 1 )
        {
            descending = position_gap < 0.0;
        }
        else if( ( position_gap < 0.0 ) != descending )
        {
            report_error( std::format(
                "Laser line {} reverses the direction in which y advances", current.file_number ) );
            return nullptr;
        }

        const auto number_gap = static_cast<double>( current.file_number - previous.file_number );
        const auto candidate = std::abs( position_gap ) / number_gap;
        if( row_pitch == 0.0 )
        {
            row_pitch = candidate;
        }
        else if( std::abs( candidate - row_pitch ) > relative_pitch_tolerance * row_pitch )
        {
            report_error( std::format(
                "Laser lines {} and {} are {} um apart over {} line number(s), "
                "incompatible with a line spacing of {} um",
                previous.file_number, current.file_number, std::abs( position_gap ),
                static_cast<uint32_t>( number_gap ), row_pitch ) );
            return nullptr;
        }
    }
    if( row_pitch == 0.0 )
    {
        row_pitch = column_pitch;   // single line: the row pitch is arbitrary
    }
    if( descending )
    {
        report_warning( "y decreases with the laser line number: grid row 0 is the last laser line" );
    }

    auto minimum = vec2<double> { pixels.front().position.x, pixels.front().position.y };
    auto maximum = minimum;
    for( const auto& pixel : pixels )
    {
        minimum.x = std::min( minimum.x, pixel.position.x );
        minimum.y = std::min( minimum.y, pixel.position.y );
        maximum.x = std::max( maximum.x, pixel.position.x );
        maximum.y = std::max( maximum.y, pixel.position.y );
    }

    // Rows must land on the lattice; an off-lattice line means the spacing is
    // uneven and the map would be silently sheared vertically.
    for( const auto& line : lines )
    {
        const auto position = ( line.y - minimum.y ) / row_pitch;
        if( std::abs( position - std::round( position ) ) > row_lattice_tolerance )
        {
            report_error( std::format(
                "Laser line {} at y = {} um does not fit the {} um line spacing",
                line.file_number, line.y, row_pitch ) );
            return nullptr;
        }
    }

    const auto width = static_cast<uint32_t>( std::llround( ( maximum.x - minimum.x ) / column_pitch ) + 1 );
    const auto height = static_cast<uint32_t>( std::llround( ( maximum.y - minimum.y ) / row_pitch ) + 1 );
    const auto element_count = static_cast<std::size_t>( width ) * static_cast<std::size_t>( height );

    if( element_count < pixels.size() )
    {
        report_error( std::format( "Grid of {} x {} cells cannot hold {} pixels",
                      width, height, pixels.size() ) );
        return nullptr;
    }

    // The grid must span every line number, present or not.
    const auto number_span = present_numbers.back() - present_numbers.front() + 1;
    if( height != number_span )
    {
        report_warning( std::format(
            "Laser line numbers {}..{} span {} lines but the coordinates imply {} grid rows",
            present_numbers.front(), present_numbers.back(), number_span, height ) );
    }

    Console::info( std::format(
        "{} laser lines ({} missing), {} pixels -> grid {} x {} at {} x {} um, origin ({}, {}) um",
        lines.size(), missing_numbers.size(), pixels.size(), width, height,
        column_pitch, row_pitch, minimum.x, minimum.y ) );

    // --- channels -----------------------------------------------------------
    auto channel_identifiers = Array<QString> { columns.size(), QString {} };
    auto channel_positions = Array<double>::allocate( columns.size() );
    for( uint32_t channel_index = 0; channel_index < columns.size(); ++channel_index )
    {
        auto identifier = QString::fromStdString( columns[channel_index] );
        if( identifier.endsWith( " cps", Qt::CaseInsensitive ) )
        {
            identifier.chop( 4 );
        }
        channel_identifiers[channel_index] = identifier;

        const auto regex = QRegularExpression { R"(\d+)" };
        const auto match = regex.match( identifier );
        if( !match.hasMatch() )
        {
            report_error( "Unable to determine the mass of channel: " + columns[channel_index] );
            return nullptr;
        }
        channel_positions[channel_index] = match.captured( 0 ).toDouble();
    }

    // --- scatter the pixels onto the grid -----------------------------------
    auto spatial_metadata = std::make_unique<Dataset::SpatialMetadata>( width, height );
    auto intensities = Matrix<double>::allocate( { element_count, channel_positions.size() } );

    for( std::size_t element_index = 0; element_index < element_count; ++element_index )
    {
        for( uint32_t channel_index = 0; channel_index < columns.size(); ++channel_index )
        {
            intensities.update_value( { element_index, channel_index }, unsampled_value );
        }
    }

    auto coverage = std::vector<uint32_t>( element_count, 0 );
    auto maximum_snap_distance = double { 0.0 };

    for( const auto& pixel : pixels )
    {
        const auto exact_column = ( pixel.position.x - minimum.x ) / column_pitch;
        const auto exact_row = ( pixel.position.y - minimum.y ) / row_pitch;
        const auto column = static_cast<uint32_t>( std::llround( exact_column ) );
        const auto row = static_cast<uint32_t>( std::llround( exact_row ) );

        if( column >= width || row >= height )
        {
            report_error( std::format( "Pixel at ({}, {}) um falls outside the {} x {} grid",
                          pixel.position.x, pixel.position.y, width, height ) );
            return nullptr;
        }

        // Lines are offset from one another by a fraction of a pixel, so each
        // pixel is snapped to the nearest lattice cell. The displacement is
        // bounded by half a pitch and is reported for inspection.
        maximum_snap_distance = std::max( maximum_snap_distance,
            std::abs( exact_column - std::round( exact_column ) ) * column_pitch );

        const auto coordinates = vec2<uint32_t> { column, row };
        const auto element_index = spatial_metadata->element_index( coordinates );

        if( coverage[element_index] > 0 )
        {
            report_error( std::format(
                "Two pixels map onto grid cell ({}, {}); the last one is at ({}, {}) um",
                column, row, pixel.position.x, pixel.position.y ) );
            return nullptr;
        }
        ++coverage[element_index];

        for( uint32_t channel_index = 0; channel_index < pixel.intensities.size(); ++channel_index )
        {
            intensities.update_value( { element_index, channel_index }, pixel.intensities[channel_index] );
        }
    }

    // Report the rows left entirely unsampled, so that the consequence of the
    // missing files is visible in the log next to the grid geometry.
    auto empty_rows = std::vector<uint32_t> {};
    for( uint32_t row = 0; row < height; ++row )
    {
        auto sampled = false;
        for( uint32_t column = 0; column < width && !sampled; ++column )
        {
            sampled = coverage[spatial_metadata->element_index( vec2<uint32_t> { column, row } )] > 0;
        }
        if( !sampled )
        {
            empty_rows.push_back( row );
        }
    }
    if( !empty_rows.empty() )
    {
        report_warning( std::format( "{} grid row(s) are entirely unsampled: {}",
                        empty_rows.size(), joined( empty_rows, "{}" ) ) );
    }

    const auto unsampled_count = element_count - pixels.size();
    Console::info( std::format(
        "Snapped pixels by at most {:.1f} um ({:.1f} % of the {} um pitch); "
        "{} of {} cells unsampled ({:.2f} %)",
        maximum_snap_distance, 100.0 * maximum_snap_distance / column_pitch, column_pitch,
        unsampled_count, element_count, 100.0 * static_cast<double>( unsampled_count )
        / static_cast<double>( element_count ) ) );

    auto dataset = new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    dataset->update_spatial_metadata( std::move( spatial_metadata ) );
    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_rpl( const std::filesystem::path& filepath )
{
    auto metadata_stream = std::ifstream { filepath };
    if( !metadata_stream )
    {
        Console::error( "Failed to open metadata file: " + filepath.string() );
        QMessageBox::critical( nullptr, "", "Failed to open metadata file", QMessageBox::Ok );
        return nullptr;
    }

    auto dimensions = vec3<uint32_t> {};
    auto datalength = std::string {};
    auto datatype = std::string {};
    auto byteorder = std::string {};

    auto line = std::string {};
    while( std::getline( metadata_stream, line ) )
    {
        auto linestream = std::stringstream { line };

        auto key = std::string {};
        auto value = std::string {};
        linestream >> key >> value;

        if( key == "width" ) dimensions.x = static_cast<uint32_t>( std::stoul( value ) );
        else if( key == "height" ) dimensions.y = static_cast<uint32_t>( std::stoul( value ) );
        else if( key == "depth" ) dimensions.z = static_cast<uint32_t>( std::stoul( value ) );
        else if( key == "data-length" ) datalength = value;
        else if( key == "data-type" ) datatype = value;
        else if( key == "byte-order" ) byteorder = value;
    }

    const auto pixelcount = dimensions.x * dimensions.y;
    if( dimensions.x == 0 || dimensions.y == 0 || dimensions.z == 0 )
    {
        Console::error( std::format( "Invalid dimensions: {}x{}x{}", dimensions.x, dimensions.y, dimensions.z ) );
        QMessageBox::critical( nullptr, "", "Invalid dimensions", QMessageBox::Ok );
        return nullptr;
    }

    if( datalength != "1" && byteorder != "little-endian" )
    {
        Console::error( std::format( "Invalid data length or byte order: {}, {}", datalength, byteorder ) );
        QMessageBox::critical( nullptr, "", "Invalid data length or byte order", QMessageBox::Ok );
        return nullptr;
    }

    auto channels = Array<double> { dimensions.z, 0.0 };
    std::iota( channels.begin(), channels.end(), 0.0 );

    const auto intensities_filepath = filepath.parent_path() / filepath.filename().replace_extension( ".raw" );
    auto intensities_stream = std::ifstream { intensities_filepath, std::ios::in | std::ios::binary };
    if( !intensities_stream )
    {
        Console::error( "Failed to open intensities file: " + intensities_filepath.string() );
        QMessageBox::critical( nullptr, "", "Failed to open intensities file", QMessageBox::Ok );
        return nullptr;
    }

    Dataset* dataset = nullptr;
    const auto read_intensities = [&] ( auto&& intensities )
    {
        intensities_stream.read( reinterpret_cast<char*>( intensities.data() ), intensities.bytes() );
        dataset = new TensorDataset { std::move( intensities ), std::move( channels ) };
        dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( dimensions.x, dimensions.y ) );
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
        Console::error( "Invalid data type: " + datatype );
        QMessageBox::critical( nullptr, "", "Invalid data type", QMessageBox::Ok );
        return nullptr;
    }

    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_single_particle_composition( const std::filesystem::path& filepath )
{
    auto filestream = std::stringstream {};
    auto linestring = std::string {};

    filestream << std::fstream { filepath, std::ios::in }.rdbuf();
    if( !filestream )
    {
        return nullptr;
    }

    std::getline( filestream, linestring, '\n' );
    if( linestring.find( "SpTool version" ) != 0 )
    {
        Console::error( "Expected 'SpTool version' in first line" );
        return nullptr;
    }

    std::getline( filestream, linestring, '\n' );
    if( linestring != "SP_COMPOSITION_MATRIX_SPTOOL" )
    {
        Console::error( "Expected 'SP_COMPOSITION_MATRIX_SPTOOL' in second line" );
        return nullptr;
    }

    // Read feature identifier
    std::getline( filestream, linestring, '=' );
    std::getline( filestream, linestring, '\n' );
    const auto feature_identifier = linestring;
    Console::info( "Feature identifier: " + feature_identifier );

    // Read header
    std::getline( filestream, linestring, '\n' );
    const auto particle_count = static_cast<uint32_t>( std::count( linestring.begin(), linestring.end(), ',' ) - 2 );
    Console::info( "Particle count: " + std::to_string( particle_count ) );

    // Read rows
    auto rows = std::vector<std::string> {};
    while( std::getline( filestream, linestring, '\n' ) )
    {
        if( linestring.empty() )
            break;

        rows.push_back( linestring );
    }
    const auto channel_count = static_cast<uint32_t>( rows.size() );

    // Read intensities
    auto intensities = Matrix<double>::allocate( { particle_count, channel_count } );
    auto channel_positions = Array<double>::allocate( channel_count );
    auto channel_identifiers = Array<QString> { channel_count, QString {} };

    for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
    {
        auto linestream     = std::stringstream { rows[channel_index] };
        auto tokenstring    = std::string {};

        std::getline( linestream, tokenstring, ',' );
        channel_positions[channel_index] = std::stod( tokenstring );

        std::getline( linestream, tokenstring, ',' );
        channel_identifiers[channel_index] = QString::fromStdString( tokenstring );

        std::getline( linestream, tokenstring, ',' ); // Skip 'Element' column

        for( uint32_t particle_index = 0; particle_index < particle_count; ++particle_index )
        {
            std::getline( linestream, tokenstring, ',' );
            intensities.value( { particle_index, channel_index } ) = tokenstring.empty() ? 0.0 : std::stod( tokenstring );
        }
    }

    // Fix duplicate channel positions
    for( uint32_t channel_index = 1; channel_index < channel_count; ++channel_index )
    {
        if( channel_positions[channel_index] == channel_positions[channel_index - 1] )
        {
            Console::warning( std::format( "Duplicate channel position detected: {}. Adding small offset to fix the problem.", std::to_string( channel_positions[channel_index] ) ) );
            channel_positions[channel_index] += 1e-6;
        }
    }

    // Create dataset
    auto dataset = new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    dataset->update_identifier( QString::fromStdString( feature_identifier ) );
    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_single_particle_csv( const std::filesystem::path& filepath )
{
    auto filestream = std::stringstream {};
    auto linestring = std::string {};
    auto linestream = std::stringstream {};
    auto tokenstring = std::string {};

    filestream << std::fstream { filepath, std::ios::in }.rdbuf();
    if( !filestream )
    {
        return nullptr;
    }

    // Skip comments
    while( std::getline( filestream, linestring, '\n' ) )
        if( linestring.front() != '#' ) break;

    // Read dimensions
    linestream = std::stringstream { linestring };
    tokenstring = std::string {};

    auto dimensions = std::vector<std::string> {};
    while( std::getline( linestream, tokenstring, ',' ) )
        dimensions.push_back( tokenstring );

    // Read units
    std::getline( filestream, linestring, '\n' );
    linestream = std::stringstream { linestring };
    tokenstring = std::string {};

    auto units = std::vector<std::string> {};
    while( std::getline( linestream, tokenstring, ',' ) )
        units.push_back( tokenstring );

    auto channel_indices = std::vector<uint32_t> {};
    auto channel_strings = std::vector<std::string> {};
    for( uint32_t dimension_index = 0; dimension_index < dimensions.size(); ++dimension_index )
    {
        if( units[dimension_index] == "counts" )
        {
            channel_indices.push_back( dimension_index );
            channel_strings.push_back( dimensions[dimension_index] );
        }
    }

    auto lines = std::vector<std::string> {};
    while( std::getline( filestream, linestring, '\n' ) )
    {
        if( linestring.front() == '#' ) break;
        lines.push_back( linestring );
    }

    auto channel_positions = Array<double>::allocate( channel_indices.size() );
    auto channel_identifiers = Array<QString> { channel_indices.size(), QString {} };
    for( uint32_t channel_index = 0; channel_index < channel_indices.size(); ++channel_index )
    {
        channel_positions[channel_index] = static_cast<double>( channel_index );
        channel_identifiers[channel_index] = QString::fromStdString( channel_strings[channel_index] );
    }

    auto intensities = Matrix<double>::allocate( { lines.size(), channel_indices.size() } );
    for( uint32_t element_index = 0; element_index < lines.size(); ++element_index )
    {
        linestream = std::stringstream { lines[element_index] };

        auto channel_index = uint32_t { 0 };
        for( uint32_t dimension_index = 0; dimension_index < dimensions.size(); ++dimension_index )
        {
            std::getline( linestream, tokenstring, ',' );
            if( dimension_index == channel_indices[channel_index] )
            {
                intensities.value( { element_index, channel_index } ) = tokenstring.empty() ? 0.0 : std::stod( tokenstring );
                ++channel_index;
            }
        }
    }

    auto dataset = new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_xyz( const std::filesystem::path& base_filepath )
{
    auto filepaths = std::vector<std::filesystem::path> {};

    const auto filename_regex = std::regex { R"(^(.+?(\d+?))\.csv$)" };
    const auto directory = base_filepath.parent_path();
    for( const auto& entry : std::filesystem::directory_iterator { directory } )
    {
        if( !entry.is_regular_file() )
        {
            continue;
        }

        const auto filename = entry.path().filename().string();
        auto matches = std::smatch {};
        if( !std::regex_match( filename, matches, filename_regex ) )
        {
            continue;
        }

        filepaths.push_back( entry.path() );
    }

    auto channel_count          = static_cast<uint32_t>( filepaths.size() );
    auto channel_positions      = Array<double>::allocate( channel_count );
    auto channel_identifiers    = Array<QString> { channel_count, QString {} };

    for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
    {
        const auto& filepath    = filepaths[channel_index];
        const auto filename     = filepath.filename().string();
        auto matches            = std::smatch {};
        std::regex_match( filename, matches, filename_regex );

        channel_positions[channel_index]    = std::stod( matches[2].str() );
        channel_identifiers[channel_index]  = QString::fromStdString( matches[1].str() );
    }

    auto permutation = Array<uint32_t>::allocate( channel_count );
    std::iota( permutation.begin(), permutation.end(), 0 );
    std::sort( permutation.begin(), permutation.end(), [&] ( uint32_t a, uint32_t b )
    {
        return channel_positions[a] < channel_positions[b];
    } );

    utility::apply_permutation( filepaths.data(), filepaths.data() + channel_count, permutation.begin() );
    utility::apply_permutation( channel_positions.data(), channel_positions.data() + channel_count, permutation.begin() );
    utility::apply_permutation( channel_identifiers.data(), channel_identifiers.data() + channel_count, permutation.begin() );

    auto coordinates_and_intensities = std::vector<std::vector<std::array<double, 3>>>( channel_count );
    auto coordinates_unique_x = std::set<double> {};
    auto coordinates_unique_y = std::set<double> {};

    for( uint32_t channel_index = 0; channel_index < filepaths.size(); ++channel_index )
    {
        const auto& filepath    = filepaths[channel_index];
        auto filestream         = std::ifstream { filepath, std::ios::in };
        auto linestring         = std::string {};

        std::getline( filestream, linestring, '\n' ); // skip header

        while( std::getline( filestream, linestring, '\n' ) )
        {
            if( linestring.empty() )
            {
                continue;
            }

            auto linestream = std::stringstream { linestring };
            auto tokenstring = std::string {};

            std::getline( linestream, tokenstring, ',' );
            const auto x = std::stod( tokenstring );

            std::getline( linestream, tokenstring, ',' );
            const auto y = std::stod( tokenstring );

            std::getline( linestream, tokenstring, '\n' );
            const auto intensity = std::stod( tokenstring );

            coordinates_and_intensities[channel_index].push_back( std::array<double, 3> { x, y, intensity } );
            coordinates_unique_x.insert( x );
            coordinates_unique_y.insert( y );
        }
    }

    auto dimensions = vec2<uint32_t> {
        static_cast<uint32_t>( coordinates_unique_x.size() ),
        static_cast<uint32_t>( coordinates_unique_y.size() )
    };
    auto spatial_metadata   = std::make_unique<Dataset::SpatialMetadata>( dimensions.x, dimensions.y );
    auto intensities        = Matrix<double> { { dimensions.x * dimensions.y, channel_count }, 0.0 };

    Console::info( std::format( "Dataset dimensions: {}x{}x{}", dimensions.x, dimensions.y, channel_count ) );

    auto coordinates_sorted_x = std::vector<double>( coordinates_unique_x.begin(), coordinates_unique_x.end() );
    auto coordinates_sorted_y = std::vector<double>( coordinates_unique_y.begin(), coordinates_unique_y.end() );

    for( uint32_t channel_index = 0; channel_index < channel_count; ++channel_index )
    {
        for( const auto& [x, y, intensity] : coordinates_and_intensities[channel_index] )
        {
            const auto iterator_x = std::lower_bound( coordinates_sorted_x.begin(), coordinates_sorted_x.end(), x );
            const auto iterator_y = std::lower_bound( coordinates_sorted_y.begin(), coordinates_sorted_y.end(), y );

            const auto coordinates = vec2<uint32_t> {
                static_cast<uint32_t>( std::distance( coordinates_sorted_x.begin(), iterator_x ) ),
                static_cast<uint32_t>( std::distance( coordinates_sorted_y.begin(), iterator_y ) )
            };

            const auto element_index = spatial_metadata->element_index( coordinates );
            intensities.update_value( { element_index, channel_index }, intensity );
        }
    }

    auto dataset = new TensorDataset<double> { std::move( intensities ), std::move( channel_positions ) };
    dataset->update_channel_identifiers( std::move( channel_identifiers ) );
    dataset->update_spatial_metadata( std::move( spatial_metadata ) );

    return QSharedPointer<Dataset> { dataset };
}
QSharedPointer<Dataset> DatasetImporter::from_zarr( const std::filesystem::path& filepath )
{
    auto interpreter = py::interpreter {};
    try
    {
        using namespace py::literals;
        const auto zarr_dataset = py::module::import( "zarr" ).attr( "open" )( filepath.parent_path().string(), "mode"_a = "r" );
        auto hypercube = static_cast<py::array>( zarr_dataset["hypercube"][py::make_tuple()] );
        auto wavenumbers = static_cast<py::array>( zarr_dataset["wvnm"][py::make_tuple()].attr( "astype" )( py::dtype::of<double>() ) );

        if( !( hypercube.dtype().kind() == 'f' && hypercube.dtype().itemsize() == sizeof( float ) ) )
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

        hypercube.release();
        wavenumbers.release();

        auto dataset = new TensorDataset<float> {
            Matrix<float>::from_pointer( { static_cast<size_t>( dimensions[0] ) * dimensions[1], dimensions[2] }, intensities_pointer ),
            Array<double>::from_pointer( { dimensions[2] }, wavenumbers_pointer )
        };
        dataset->update_spatial_metadata( std::make_unique<Dataset::SpatialMetadata>( dimensions.y, dimensions.x ) );
        return QSharedPointer<Dataset> { dataset };
    }
    catch( const py::error_already_set& error )
    {
        Console::error( std::format( "Python error during dataset import: {}", error.what() ) );
        QMessageBox::critical( nullptr, "", "Failed to import dataset", QMessageBox::Ok );
    }

    return nullptr;
}

QSharedPointer<Dataset> DatasetImporter::execute_dialog()
{
    const auto filepath = std::filesystem::path { QFileDialog::getOpenFileName( nullptr, "Import Dataset...", "", "", nullptr ).toStdWString() };
    if( filepath.empty() )
    {
        return nullptr;
    }

    Console::info( "Importing dataset: " + filepath.string() );
    const auto filename = filepath.filename();
    const auto extension = filepath.extension();

    if( filename == "laser.info" )
    {
        return DatasetImporter::from_laser_info( filepath );
    }
    else if( filename == ".zgroup" )
    {
        return DatasetImporter::from_zarr( filepath );
    }
    else if( extension == ".csv" || extension == ".txt" )
    {
        return DatasetImporter::from_csv( filepath );
    }
    else if( extension == ".data" || extension == ".dims" || extension == ".freq" )
    {
        return DatasetImporter::from_data_dims_freq( filepath );
    }
    else if( extension == ".h5" )
    {
        return DatasetImporter::from_hdf5( filepath );
    }
    else if( extension == ".mia" )
    {
        return DatasetImporter::from_mia( filepath );
    }
    else if( extension == ".rpl" )
    {
        return DatasetImporter::from_rpl( filepath );
    }

    QMessageBox::critical( nullptr, "", "Unsupported file format" );
    return nullptr;
}
