#pragma once
#include <filesystem>
#include <qdialog.h>
#include <qsharedpointer.h>

class Database;

class EmbeddingCreator : public QDialog
{
    Q_OBJECT
public:
    static std::filesystem::path execute( const Database& database );

    EmbeddingCreator( const Database& database );
    const std::filesystem::path& filepath() const noexcept;

private:
    const Database& _database;
    std::filesystem::path _filepath;
};