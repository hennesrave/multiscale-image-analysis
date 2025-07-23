#pragma once
#include <qdialog.h>

class Dataset;

class DatasetImporter
{
public:
    static QSharedPointer<Dataset> execute();

private:
    DatasetImporter() = delete;
};