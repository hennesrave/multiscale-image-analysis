#include "database.hpp"
#include "dataset_importer.hpp"
#include "python.hpp"
#include "workspace.hpp"
#include "utility.hpp"

#include <qapplication.h>
#include <qscreen.h>
#include <qsurfaceformat.h>
#include <qwidget.h>

int main( int argc, char** argv )
{
    // Initialize application
    auto surface_format = QSurfaceFormat {};
    surface_format.setProfile( QSurfaceFormat::CoreProfile );
    surface_format.setVersion( 4, 5 );
    QSurfaceFormat::setDefaultFormat( surface_format );

    QApplication::setApplicationName( config::application_display_name );
    QApplication::setAttribute( Qt::AA_ShareOpenGLContexts );
    QApplication::setFont( config::font );
    QLocale::setDefault( QLocale::c() );

    auto application = QApplication { argc, argv };
    application.setStyleSheet( config::stylesheet );
    application.setWindowIcon( QIcon { ":/multiscale-image-analysis-icon.svg" } );

    // Initialize configuration
    config::executable_directory = QDir { application.applicationDirPath() };

    Console::initialize();
    Console::info( std::format( R"(
 _____ ______   ___  ________     
|\   _ \  _   \|\  \|\   __  \      
\ \  \\\__\ \  \ \  \ \  \|\  \         Multiscale Image Analysis - Version {}
 \ \  \\|__| \  \ \  \ \   __  \        Copyright Hennes Rave (2025)
  \ \  \    \ \  \ \  \ \  \ \  \       
   \ \__\    \ \__\ \__\ \__\ \__\      (づ｡◕‿‿◕｡)づ Thank you for using this software~!
    \|__|     \|__|\|__|\|__|\|__|      
    )", config::application_version_string ) );
    Console::info( std::format( "Executable directory: {}", config::executable_directory.absolutePath().toStdString() ) );

    // Initialize python
    py::interpreter::python_home = config::executable_directory.absoluteFilePath( "python" ).toStdWString();
    py::interpreter::module_search_paths = {
        py::interpreter::python_home,
        py::interpreter::python_home + L"\\python313.zip",
        py::interpreter::python_home + L"\\Lib\\site-packages"
    };
    py::interpreter::initialize();

    // Initialize workspace
    if( const auto dataset = DatasetImporter::execute_dialog() )
    {
        auto database = Database { dataset };
        auto workspace = Workspace { database };
        workspace.setWindowTitle( QString { config::application_display_name } + " - v" + config::application_version_string );
        workspace.resize( 1600, 900 );
        workspace.showMaximized();

        return application.exec();
    }

    return 0;
}