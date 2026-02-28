#pragma once
#include "database.hpp"

#include <qwidget.h>

class ChannelGlyphsViewer : public QWidget
{
    Q_OBJECT
public:
    ChannelGlyphsViewer( Database& database );

private:
    Database& _database;
};