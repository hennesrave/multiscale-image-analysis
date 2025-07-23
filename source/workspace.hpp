#pragma once
#include <qwidget.h>

class Database;

class BoxplotViewer;
class ColormapViewer;
class EmbeddingViewer;
class HistogramViewer;
class ImageViewer;
class SpectrumViewer;

class Workspace : public QWidget
{
    Q_OBJECT
public:
    Workspace( QSharedPointer<Database> database );

private:
    QSharedPointer<Database> _database;

    BoxplotViewer* _boxplot_viewer = nullptr;
    ColormapViewer* _colormap_viewer = nullptr;
    EmbeddingViewer* _embedding_viewer = nullptr;
    HistogramViewer* _histogram_viewer = nullptr;
    ImageViewer* _image_viewer = nullptr;
    SpectrumViewer* _spectrum_viewer = nullptr;
};