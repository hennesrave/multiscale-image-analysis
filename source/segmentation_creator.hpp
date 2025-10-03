#pragma once
#include <qdialog.h>
#include <qsharedpointer.h>

class Database;
class Segmentation;

class SegmentationCreator : public QDialog
{
    Q_OBJECT
public:
    static QSharedPointer<Segmentation> execute( const Database& database );

    SegmentationCreator( const Database& database );
    QSharedPointer<Segmentation> segmentation() const noexcept;

private:
    const Database& _database;
    QSharedPointer<Segmentation> _segmentation;
};