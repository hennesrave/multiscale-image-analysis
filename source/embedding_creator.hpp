#pragma once
#include <qdialog.h>
#include <qsharedpointer.h>

class Dataset;
class Segmentation;

class EmbeddingCreator : public QDialog
{
    Q_OBJECT
public:
    static QString execute( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation );

    EmbeddingCreator( QSharedPointer<const Dataset> dataset, QSharedPointer<const Segmentation> segmentation );
    const QString& filepath() const noexcept;

private:
    QSharedPointer<const Dataset> _dataset;
    QSharedPointer<const Segmentation> _segmentation;
    QString _filepath;
};