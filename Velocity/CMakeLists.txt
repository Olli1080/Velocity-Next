set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# TextEncoding module sources
set(TEXT_ENCODING_SOURCES
  TextEncoding/TextDecoder.cpp
  TextEncoding/EncodingDetector.cpp
  TextEncoding/ShiftJIS/CP932Decoder.cpp
  TextEncoding/ShiftJIS/CP932Mapping.cpp
  TextEncoding/GBK/CP936Decoder.cpp
  TextEncoding/GBK/CP936Mapping.cpp
  TextEncoding/Big5/CP950Decoder.cpp
  TextEncoding/Big5/CP950Mapping.cpp
  TextEncoding/EUCKR/CP949Decoder.cpp
  TextEncoding/EUCKR/CP949Mapping.cpp
)

set(SOURCES
  about.cpp
  achievementcreationwizard.cpp
  addressconverterdialog.cpp
  AvatarAssetDownloader.cpp
  boxartretriever.cpp
  certificatedialog.cpp
  clustertooldialog.cpp
  creationwizard.cpp
  deviceviewer.cpp
  dragdroptreewidget.cpp
  fatxfiledialog.cpp
  fatxpathgendialog.cpp
  flashdriveconfigdatadialog.cpp
  gameadderdialog.cpp
  gamerpicturepackdialog.cpp
  githubcommitsdialog.cpp
  gpddownloader.cpp
  gpduploader.cpp
  imagedialog.cpp
  licensingdatadialog.cpp
  main.cpp
  mainwindow.cpp
  metadata.cpp
  multiprogressdialog.cpp
  nightcharts.cpp
  packageviewer.cpp
  partitiondialog.cpp
  preferencesdialog.cpp
  profilecleanerwizard.cpp
  profilecreatorwizard.cpp
  profileeditor.cpp
  propertiesdialog.cpp
  qthelpers.cpp
  renamedialog.cpp
  securitysectordialog.cpp
  singleprogressdialog.cpp
  stfstoolsdialog.cpp
  stfsvolumedescriptordialog.cpp
  strbdialog.cpp
  svoddialog.cpp
  svodfileinfodialog.cpp
  svodtooldialog.cpp
  isodialog.cpp
  isosectordialog.cpp
  xexdialog.cpp
  svodvolumedescriptordialog.cpp
  stfsvolumedescriptordialog.cpp
  themecreationwizard.cpp
  textdialog.cpp
  thememanager.cpp
  titleidfinder.cpp
  titleidfinderdialog.cpp
  transferflagsdialog.cpp
  xdbfdialog.cpp
  xeniasaveexportwizard.cpp
  xmldialog.cpp
  zipviewer.cpp
  ytgrdialog.cpp
  ${TEXT_ENCODING_SOURCES}
)

set(UI_FORMS
  mainwindow.ui
  packageviewer.ui
  profileeditor.ui
  about.ui
  metadata.ui
  certificatedialog.ui
  licensingdatadialog.ui
  transferflagsdialog.ui
  xdbfdialog.ui
  strbdialog.ui
  imagedialog.ui
  renamedialog.ui
  creationwizard.ui
  achievementcreationwizard.ui
  propertiesdialog.ui
  addressconverterdialog.ui
  stfstoolsdialog.ui
  singleprogressdialog.ui
  profilecreatorwizard.ui
  themecreationwizard.ui
  gameadderdialog.ui
  titleidfinderdialog.ui
  gamerpicturepackdialog.ui
  preferencesdialog.ui
  githubcommitsdialog.ui
  fatxpathgendialog.ui
  profilecleanerwizard.ui
  svoddialog.ui
  svodfileinfodialog.ui
  svodtooldialog.ui
  svodvolumedescriptordialog.ui
  isosectordialog.ui
  xexdialog.ui
  stfsvolumedescriptordialog.ui
  multiprogressdialog.ui
  ytgrdialog.ui
  deviceviewer.ui
  clustertooldialog.ui
  fatxfiledialog.ui
  partitiondialog.ui
  securitysectordialog.ui
  flashdriveconfigdatadialog.ui
  xmldialog.ui
  textdialog.ui
  zipviewer.ui
  xeniasaveexportwizard.ui
)

# Platform-specific executable properties
if(APPLE)
    # macOS bundle configuration
    set(MACOSX_BUNDLE_GUI_IDENTIFIER com.velocitynext.app)
    set(MACOSX_BUNDLE_BUNDLE_NAME VelocityNext)
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_ICON_FILE velocity.icns)
    
    add_executable(VelocityNext MACOSX_BUNDLE ${SOURCES} ${UI_FORMS} Resources.qrc)
    
else()
    # Windows/Linux executable
    add_executable(VelocityNext ${SOURCES} ${UI_FORMS} Resources.qrc)
    
    if(WIN32)
        set_target_properties(VelocityNext PROPERTIES WIN32_EXECUTABLE TRUE)
        target_sources(VelocityNext PRIVATE velocity.rc)
    endif()
endif()

# Common include directories for all platforms
target_include_directories(VelocityNext
  PRIVATE
    # Current directory for header files co-located with source
    ${CMAKE_CURRENT_LIST_DIR}
    # Qt private headers for advanced features
    ${Qt6Core_PRIVATE_INCLUDE_DIRS}
)

# Common linking for all platforms
target_link_libraries(VelocityNext
  PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    XboxInternals::XboxInternals  # Use namespaced target (modern CMake); pulls in Botan::Botan transitively
    velocity_compiler_flags  # Inherit common compile options
)

# Define VERSION and platform build name
target_compile_definitions(VelocityNext 
  PRIVATE 
    VERSION="\\\"${PROJECT_VERSION}\\\""
    PLATFORM_BUILD_NAME="${PLATFORM_BUILD_NAME}"
)

# App requires C++20
target_compile_features(VelocityNext PUBLIC cxx_std_20)

# Organize in IDE (Visual Studio solution explorer)
set_target_properties(VelocityNext PROPERTIES FOLDER "Applications")

# Post-build: Copy XboxInternals DLL to Velocity output directory (only when building shared library)
if(BUILD_XBOXINTERNALS_SHARED)
    add_custom_command(TARGET VelocityNext POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:XboxInternals::XboxInternals>
            $<TARGET_FILE_DIR:VelocityNext>
        COMMENT "Copying libXboxInternals.dll to Velocity output directory"
    )
endif()

# Post-build: Copy Qt's runtime DLLs (Qt6Core.dll, Qt6Widgets.dll, etc.) next to the
# executable so it runs without needing Qt's bin/ on PATH. $<TARGET_RUNTIME_DLLS:tgt>
# (CMake 3.21+) resolves the DLLs of every SHARED/IMPORTED SHARED target VelocityNext
# links against, transitively - this covers direct Qt DLL dependencies but NOT Qt's
# plugins (platforms/qwindows.dll etc.), which are loaded dynamically at runtime and
# can't be discovered via link-time generator expressions; windeployqt (used in CI
# packaging) is still the right tool for a fully self-contained distributable build.
# Windows only: the expression is empty on Linux/macOS, which would leave copy_if_different
# with a single argument and fail the build.
if(WIN32)
    add_custom_command(TARGET VelocityNext POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:VelocityNext>
            $<TARGET_FILE_DIR:VelocityNext>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Qt runtime DLLs to Velocity output directory"
    )
endif()

# Post-build (Windows only): deploy Qt's plugins via windeployqt. Without this, the exe
# links fine and its direct DLLs are present (via the copy step above), but it fails at
# startup with "This application failed to start because no Qt platform plugin could be
# initialized" - platforms/qwindows.dll (and friends) are loaded dynamically, not linked,
# so no generator expression can discover them. windeployqt inspects the built exe and
# copies exactly the plugins it actually needs.
if(WIN32)
    get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS "${_qt_bin_dir}"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET VelocityNext POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-compiler-runtime $<TARGET_FILE:VelocityNext>
            COMMENT "Deploying Qt plugins (windeployqt)"
        )
    else()
        message(WARNING "windeployqt6/windeployqt not found near Qt6_DIR (${Qt6_DIR}) - "
            "VelocityNext.exe may fail to start with a 'Qt platform plugin' error.")
    endif()
endif()