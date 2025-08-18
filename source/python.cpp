#include "python.hpp"

#include "utility.hpp"

#include <filesystem>
#include <qmessagebox.h>

namespace pybind11
{
    PyConfig* configuration()
    {
        static PyConfig configuration {};
        PyConfig_InitIsolatedConfig( &configuration );
        PyConfig_SetString( &configuration, &configuration.home, interpreter::python_home.c_str() );

        PyConfig_SetWideStringList( &configuration, &configuration.module_search_paths, 0, nullptr );
        for( const auto& path : interpreter::module_search_paths )
            PyWideStringList_Append( &configuration.module_search_paths, path.c_str() );
        configuration.module_search_paths_set = 1;

        return &configuration;
    }

    void interpreter::initialize()
    {
        const auto libraries_directory = interpreter::python_home + L"/Lib";
        if( !std::filesystem::is_directory( libraries_directory ) )
        {
            Console::info( "Installing python package dependencies..." );
            QMessageBox::information(
                nullptr,
                config::application_display_name,
                "The application needs to install some Python dependencies.\nThis is a one-time setup and may take a few minutes.",
                QMessageBox::Ok
            );

            try
            {
                using namespace py::literals;
                auto interpreter = py::scoped_interpreter { py::configuration(), 0, nullptr, false };
                py::exec( R"(
                        import subprocess
                        subprocess.run( [f"{python_home}/python.exe", f"{python_home}/get-pip.py"] )
                        subprocess.run( [f"{python_home}/python.exe", "-m", "pip", "install", "--requirement", f"{python_home}/requirements.txt"] )
                    )", py::globals(), py::dict { "python_home"_a = interpreter::python_home } );
            }
            catch( const py::error_already_set& error )
            {
                Console::critical( std::format( "Python error during package installation: {}", error.what() ) );
            }

            Console::info( "Finished installing python package dependencies!" );
            QMessageBox::information(
                nullptr,
                config::application_display_name,
                "Python package dependencies have been installed successfully.\nPlease restart the application to apply the changes.",
                QMessageBox::Ok
            );
            std::exit( 0 );
        }

        _interpreter.reset( new py::scoped_interpreter { py::configuration(), 0, nullptr, false } );
        Console::info( "Finished initializing global python interpreter!" );
    }

    interpreter::~interpreter()
    {
        PyDict_Clear( PyModule_GetDict( PyImport_AddModule( "__main__" ) ) );
    }
}