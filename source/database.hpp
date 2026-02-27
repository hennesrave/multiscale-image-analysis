#pragma once
#include "collection.hpp"
#include "colormap.hpp"
#include "dataset.hpp"
#include "embedding.hpp"
#include "feature.hpp"
#include "segmentation.hpp"

#include <qmenu.h>
#include <qobject.h>
#include <qsharedpointer.h>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database( QSharedPointer<Dataset> dataset );

    QSharedPointer<Dataset> dataset() const noexcept;
    QSharedPointer<Segmentation> segmentation() const noexcept;
    QSharedPointer<Storage<Feature>> features() const noexcept;
    QSharedPointer<Storage<Colormap>> colormaps() const noexcept;

    QSharedPointer<Embedding> embedding() const noexcept;
    void update_embedding( QSharedPointer<Embedding> embedding );

    QSharedPointer<ColormapEmbedding> colormap_embedding() const noexcept;

    QSharedPointer<Segment> active_segment() const;
    void update_active_segment( QSharedPointer<Segment> segment );

    std::optional<uint32_t> highlighted_element_index() const noexcept;
    void update_highlighted_element_index( std::optional<uint32_t> index );

    std::optional<uint32_t> highlighted_channel_index() const noexcept;
    void update_highlighted_channel_index( std::optional<uint32_t> index );

    void populate_segmentation_menu( QMenu& context_menu, bool enable_propogate = false );

signals:
    void embedding_changed( QSharedPointer<Embedding> embedding ) const;
    void active_segment_changed( QSharedPointer<Segment> segment ) const;
    void highlighted_element_index_changed( std::optional<uint32_t> index ) const;
    void highlighted_channel_index_changed( std::optional<uint32_t> index ) const;

private:
    QSharedPointer<Dataset> _dataset;
    QSharedPointer<Segmentation> _segmentation;
    QSharedPointer<Storage<Feature>> _features;
    QSharedPointer<Storage<Colormap>> _colormaps;
    QSharedPointer<Embedding> _embedding;
    QSharedPointer<ColormapEmbedding> _colormap_embedding;

    mutable QWeakPointer<Segment> _active_segment;

    std::optional<uint32_t> _highlighted_element_index;
    std::optional<uint32_t> _highlighted_channel_index;
};