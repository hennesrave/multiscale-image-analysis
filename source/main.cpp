#include "database.hpp"
#include "dataset_alignment_dialog.hpp"
#include "dataset_importer.hpp"
#include "embedding_creator.hpp"
#include "python.hpp"
#include "workspace.hpp"
#include "utility.hpp"

#include <qapplication.h>
#include <qmessagebox.h>
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
    if( py::interpreter::initialize() )
    {
        return 0;
    }

    class Project
    {
    public:
        void import_dataset( QSharedPointer<Dataset> dataset )
        {
            auto segmentation = QSharedPointer<Segmentation> {};

            if( _databases.size() )
            {
                const auto current_spatial_metadata = _databases.front()->dataset()->spatial_metadata();
                const auto dataset_spatial_metadata = dataset->spatial_metadata();

                auto buttons = QMessageBox::StandardButtons { QMessageBox::Yes };
                auto message = QString {};

                if( current_spatial_metadata->dimensions != dataset_spatial_metadata->dimensions )
                {
                    buttons |= QMessageBox::Cancel;
                    message = QString { "The dataset you are trying to import has " } +
                        QString::number( dataset_spatial_metadata->dimensions.x ) +
                        "x" +
                        QString::number( dataset_spatial_metadata->dimensions.y ) +
                        " pixels, while the currently open dataset has " +
                        QString::number( current_spatial_metadata->dimensions.x ) +
                        "x" +
                        QString::number( current_spatial_metadata->dimensions.y ) +
                        " pixels.\n\n";
                }
                else
                {
                    buttons |= QMessageBox::No;
                }

                message += QString { "Do you want to align the new dataset to the currently open dataset?" };

                const auto response = QMessageBox::question(
                    nullptr,
                    "Dataset Alignment...",
                    message,
                    buttons
                );

                if( response == QMessageBox::Cancel )
                {
                    return;
                }
                else if( response == QMessageBox::Yes )
                {
                    auto dataset_alignment_dialog = DatasetAlignmentDialog { dataset, _databases.front()->dataset() };
                    dataset_alignment_dialog.resize( 1600, 900 );

                    if( dataset_alignment_dialog.exec() == QDialog::Rejected )
                    {
                        return;
                    }

                    if( dataset = dataset_alignment_dialog.aligned(); !dataset )
                    {
                        QMessageBox::critical( nullptr, "Dataset Alignment...", "Failed to align dataset.", QMessageBox::Ok );
                        return;
                    }
                }

                segmentation = _databases.front()->segmentation();
            }

            _databases.emplace_back( new Database { dataset, segmentation } );
            auto& database = _databases.back();

            _workspaces.emplace_back( new Workspace { *database } );
            auto& workspace = _workspaces.back();

            QObject::connect( database.get(), &Database::request_additional_dataset_import, [&]
            {
                if( const auto dataset = DatasetImporter::execute_dialog() )
                {
                    this->import_dataset( dataset );
                }
            } );
            QObject::connect( workspace.get(), &Workspace::closed, [&, workspace = workspace.get()]
            {
                for( size_t i = 0; i < _workspaces.size(); ++i )
                {
                    if( _workspaces[i].get() == workspace )
                    {
                        _workspaces.erase( _workspaces.begin() + i );
                        _databases.erase( _databases.begin() + i );
                        break;
                    }
                }
            } );

            workspace->setWindowTitle( QString { config::application_display_name } + " - v" + config::application_version_string );
            workspace->resize( 1600, 900 );
            workspace->showMaximized();
        }

        std::vector<std::unique_ptr<Database>> _databases;
        std::vector<std::unique_ptr<Workspace>> _workspaces;
    };

    // Initialize workspace
    if( const auto dataset = DatasetImporter::execute_dialog() )
    {
        auto project = Project {};

        EmbeddingCreator::database_registry = &project._databases;

        project.import_dataset( dataset );
        return application.exec();
    }

    return 0;
}