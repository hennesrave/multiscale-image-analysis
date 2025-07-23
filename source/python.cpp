#include "python.hpp"

#include "logger.hpp"

#include <filesystem>

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

    interpreter::interpreter()
    {
        if( !_interpreter )
        {
            Logger::info() << "Initializing global python interpreter...";

            if( !std::filesystem::is_directory( interpreter::python_home + L"/Lib" ) )
            {
                Logger::info() << "Installing python package requirements...";

                try
                {
                    using namespace py::literals;
                    auto interpreter = py::scoped_interpreter { py::configuration(), 0, nullptr, false};
                    py::exec( R"(
                        import subprocess
                        subprocess.run( [f"{python_home}/python.exe", f"{python_home}/get-pip.py"] )
                        subprocess.run( [f"{python_home}/python.exe", "-m", "pip", "install", "--requirement", f"{python_home}/requirements.txt"] )
                    )", py::globals(), py::dict { "python_home"_a = interpreter::python_home } );
                }
                catch( const py::error_already_set& error )
                {
                    Logger::error() << "Python error during package installation: " << error.what();
                }

                Logger::info() << "Finished installing python package requirements!";
            }

            _interpreter.reset( new py::scoped_interpreter { py::configuration(), 0, nullptr, false } );
            Logger::info() << "Finished initializing global python interpreter!";
        }
    }

    interpreter::~interpreter()
    {
        PyDict_Clear( PyModule_GetDict( PyImport_AddModule( "__main__" ) ) );
    }
}