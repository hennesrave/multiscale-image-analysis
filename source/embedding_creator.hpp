#pragma once
#include <filesystem>
#include <qdialog.h>
#include <qsharedpointer.h>

class Dataset;
class Segmentation;

class EmbeddingCreator : public QDialog
{
    Q_OBJECT
public:
    static std::filesystem::path execute( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation );

    EmbeddingCreator( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation );
    const std::filesystem::path& filepath() const noexcept;

private:
    QSharedPointer<const Dataset> _dataset;
    QSharedPointer<const Segmentation> _segmentation;
    std::filesystem::path _filepath;
};