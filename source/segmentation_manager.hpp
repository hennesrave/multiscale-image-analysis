#pragma once
#include <qdialog.h>

class Database;

class SegmentationManager : public QDialog
{
    Q_OBJECT
public:
    SegmentationManager( Database& database );
    static void execute( Database& database );

private:
    Database& _database;
};