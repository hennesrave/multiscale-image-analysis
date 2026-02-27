#pragma once
#include <filesystem>
#include <qdialog.h>

#include "python.hpp"

class Database;

class EmbeddingCreator : public QDialog
{
    Q_OBJECT
public:
    static std::vector<std::unique_ptr<Database>>* database_registry;

    static std::filesystem::path execute();

    EmbeddingCreator();

    const std::filesystem::path& filepath() const noexcept;
    py::object model() const noexcept;

private:
    std::filesystem::path _filepath;
    py::object _model;
};