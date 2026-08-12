#pragma once
#include <qdialog.h>

class Dataset;

class MissingValuesDialog : public QDialog
{
    Q_OBJECT
public:
    MissingValuesDialog( QSharedPointer<Dataset> dataset );

    // Returns whether values were replaced. Reports and returns false without
    // showing the dialog if there is nothing to replace.
    static bool execute( QSharedPointer<Dataset> dataset );

private:
    struct Summary
    {
        uint64_t value_count = 0;       // missing values over all elements and channels
        uint32_t element_count = 0;     // elements missing at least one channel
        uint32_t complete_count = 0;    // elements missing every channel
    };

    static Summary summarize( const QSharedPointer<Dataset>& dataset );
};