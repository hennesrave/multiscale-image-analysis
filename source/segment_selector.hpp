#pragma once
#include <qwidget.h>

class Segment;
class Segmentation;

class QComboBox;

// ----- SegmentSelector ----- //

class SegmentSelector : public QWidget
{
    Q_OBJECT
public:
    SegmentSelector( QSharedPointer<const Segmentation> features );

    QSharedPointer<Segment> selected_segment() const;

signals:
    void selected_segment_changed( QSharedPointer<Segment> segment );

private:
    void segment_appended( QSharedPointer<Segment> segment );
    void segment_removed( QSharedPointer<Segment> segment );

    QSharedPointer<const Segmentation> _segmentation;
    QWeakPointer<Segment> _selected_segment;
    QComboBox* _combobox = nullptr;
};