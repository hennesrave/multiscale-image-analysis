#pragma once
#include <unordered_map>
#include <qdir.h>
#include <qfont.h>

namespace config
{
    constexpr inline auto application_version = "1.0.0";
    constexpr inline auto application_identifier = "multiscale-image-analysis";
    constexpr inline auto application_display_name = "MIA: Multiscale Image Analysis";

    constexpr inline auto developer_version = true;
    constexpr inline auto logger_console_enabled = true;

    static inline auto font = QFont { "sans-serif", 10, -1 };
    static inline auto palette = std::unordered_map<int, const char*> {
        { 50, "#FAFAFA" },
        { 100, "#F5F5F5" },
        { 200, "#EEEEEE" },
        { 300, "#E0E0E0" },
        { 400, "#BDBDBD" },
        { 500, "#9E9E9E" },
        { 600, "#757575" },
        { 700, "#616161" },
        { 800, "#424242" },
        { 900, "#212121" }
    };
    constexpr inline auto stylesheet = R"(
        * { background: white; }

        QMenu { border: 1px solid #9E9E9E; }
        QMenu::icon { padding: 2px 5px 2px 0px; }
        QMenu::item { padding: 2px 20px 2px 20px; }
        QMenu::item:selected { background: #E0E0E0; }

        QScrollBar:vertical             { width: 0px; }
        QScrollBar:horizontal           { height: 0px; }

        QSplitter::handle               { background: #E0E0E0; }
        QSplitter::handle:vertical      { height: 1px; }
        QSplitter::handle:horizontal    { width: 1px; }

        QTabWidget::pane {
            border-top: 1px solid #E0E0E0;
        }
        QTabBar::tab {
            background: #EEEEEE;
            border: 1px solid #E0E0E0;
            border-bottom: none;
            border-top-left-radius: 2px;
            border-top-right-radius: 2px;
            height: 10px;
            width: 30px;
            margin-left: 5px;
            margin-top: 2px;
        }
        QTabBar::tab:selected, QTabBar::tab:hover {
            background: #BDBDBD;
        }

        QToolButton                             { border: none; border-radius: 5px; background: #FAFAFA; padding: 0px; }
        QToolButton:hover, QToolButton:pressed  { background: #E0E0E0; }
    )";

    // ----- Dynamic ----- //

    inline QDir executable_directory {};
}