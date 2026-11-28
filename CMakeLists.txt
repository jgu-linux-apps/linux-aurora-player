cmake_minimum_required(VERSION 3.18)
project(linux-aurora-player VERSION 0.6.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
find_package(Qt6 REQUIRED COMPONENTS Widgets DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-pbutils-1.0)

add_executable(linux-aurora-player
  src/main.cpp
  src/MainWindow.cpp       src/MainWindow.h
  src/AuroraTheme.cpp      src/AuroraTheme.h
  src/LevelMeter.cpp       src/LevelMeter.h
  src/SpectrumAnalyzer.cpp src/SpectrumAnalyzer.h
  src/Player.cpp           src/Player.h
  src/PlaylistModel.cpp    src/PlaylistModel.h
  src/PlaylistWidget.cpp   src/PlaylistWidget.h
  src/MetadataLoader.cpp   src/MetadataLoader.h
  src/AudioOutput.cpp      src/AudioOutput.h
  src/PreferencesDialog.cpp src/PreferencesDialog.h
  src/Config.cpp           src/Config.h
  src/ExportDialog.cpp     src/ExportDialog.h
  src/PlaylistExporter.cpp src/PlaylistExporter.h
  src/CueSheet.cpp         src/CueSheet.h
  src/Mpris.cpp            src/Mpris.h
  src/VizDelay.cpp         src/VizDelay.h
  src/TrayIcon.cpp         src/TrayIcon.h
  src/Translator.cpp       src/Translator.h
  src/CoverArt.cpp         src/CoverArt.h
  src/PolishTexts.cpp
  src/TrackInfo.h          src/TransportIcons.h
  data/icons.qrc)
target_compile_definitions(linux-aurora-player PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(linux-aurora-player PRIVATE Qt6::Widgets Qt6::DBus PkgConfig::GST)

install(TARGETS linux-aurora-player RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/linux-aurora-player.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/icons/linux-aurora-player.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/icons/linux-aurora-player-dark.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/linux-aurora-player RENAME icon-dark.svg)
install(PROGRAMS data/icon-watch.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/linux-aurora-player)
install(FILES data/linux-aurora-player-icon.desktop DESTINATION /etc/xdg/autostart)
