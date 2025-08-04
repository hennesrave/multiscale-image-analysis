#pragma once
#include <filesystem>
#include <qsharedpointer.h>

class Dataset;

class DatasetImporter
{
public:
    static QSharedPointer<Dataset> from_csv( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_hdf5( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_mia( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_laser_info( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_laser_lines( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_rpl( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_single_particle_csv( const std::filesystem::path& filepath );
    static QSharedPointer<Dataset> from_zarr( const std::filesystem::path& filepath );

    static QSharedPointer<Dataset> execute_dialog();

private:
    DatasetImporter() = delete;
};