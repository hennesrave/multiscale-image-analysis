#pragma once
#include <filesystem>

#include <qdialog.h>
#include <qsharedpointer.h>

class Database;
class Dataset;

class DatasetExporter
{
public:
    static void execute_dialog( const Database& database, const QSharedPointer<Dataset>& dataset );

private:
    DatasetExporter() = delete;
};

class DatasetExporterCSV : public QDialog
{
    Q_OBJECT
public:
    DatasetExporterCSV( const Database& database, const QSharedPointer<Dataset>& dataset, const std::filesystem::path& filepath );

private:
    const Database& _database;
    QSharedPointer<Dataset> _dataset;
    std::filesystem::path _filepath;
};