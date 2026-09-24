# ====================================================================
# Qt6 sideloading (reproducible, self-contained builds)
# ====================================================================
#
# Instead of depending on whatever Qt happens to be installed system-wide,
# this downloads a pinned copy of Qt6 straight from Qt's official archives
# (via aqtinstall: https://github.com/miurahr/aqtinstall - the same tool
# GitHub Action jurplel/install-qt-action wraps) into externals/qt/ inside
# the repository. Nothing is written outside the project directory.
#
# Set VELOCITY_SIDELOAD_QT=OFF to skip this and use a system/pre-existing
# Qt install instead (via CMAKE_PREFIX_PATH as usual).

option(VELOCITY_SIDELOAD_QT "Download a pinned, project-local copy of Qt6 instead of using a system Qt install" ON)

if(VELOCITY_SIDELOAD_QT)
    set(QT_SIDELOAD_VERSION "6.8.3" CACHE STRING "Qt version to sideload")
    set(QT_SIDELOAD_DIR "${CMAKE_SOURCE_DIR}/externals/qt" CACHE PATH "Directory Qt is sideloaded into")

    if(WIN32 AND MINGW)
        set(_qt_aqt_host "windows")
        set(_qt_aqt_arch "win64_mingw")
        set(_qt_subdir "mingw_64")
    elseif(WIN32)
        set(_qt_aqt_host "windows")
        set(_qt_aqt_arch "win64_msvc2022_64")
        set(_qt_subdir "msvc2022_64")
    elseif(APPLE)
        set(_qt_aqt_host "mac")
        set(_qt_aqt_arch "clang_64")
        set(_qt_subdir "macos")
    else()
        set(_qt_aqt_host "linux")
        set(_qt_aqt_arch "")
        set(_qt_subdir "gcc_64")
    endif()

    set(_qt_install_dir "${QT_SIDELOAD_DIR}/${QT_SIDELOAD_VERSION}/${_qt_subdir}")

    if(NOT EXISTS "${_qt_install_dir}/lib/cmake/Qt6/Qt6Config.cmake")
        find_package(Python3 COMPONENTS Interpreter REQUIRED)

        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m aqt version
            RESULT_VARIABLE _aqt_missing
            OUTPUT_QUIET ERROR_QUIET
        )
        if(_aqt_missing)
            message(STATUS "Installing aqtinstall (one-time)...")
            execute_process(
                COMMAND ${Python3_EXECUTABLE} -m pip install --quiet --upgrade aqtinstall
                RESULT_VARIABLE _pip_result
            )
            if(_pip_result)
                message(FATAL_ERROR "Failed to install aqtinstall. Install it manually: python -m pip install aqtinstall")
            endif()
        endif()

        message(STATUS "Sideloading Qt ${QT_SIDELOAD_VERSION} into ${QT_SIDELOAD_DIR} (first configure only, this downloads ~1-2 GB)...")

        set(_qt_install_args install-qt ${_qt_aqt_host} desktop ${QT_SIDELOAD_VERSION})
        if(_qt_aqt_arch)
            list(APPEND _qt_install_args ${_qt_aqt_arch})
        endif()
        list(APPEND _qt_install_args --outputdir ${QT_SIDELOAD_DIR})

        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m aqt ${_qt_install_args}
            RESULT_VARIABLE _qt_install_result
        )
        if(_qt_install_result)
            message(FATAL_ERROR "Failed to sideload Qt via aqtinstall (host=${_qt_aqt_host} arch=${_qt_aqt_arch} version=${QT_SIDELOAD_VERSION}).")
        endif()
    endif()

    list(PREPEND CMAKE_PREFIX_PATH "${_qt_install_dir}")
    message(STATUS "Using sideloaded Qt at: ${_qt_install_dir}")
endif()
