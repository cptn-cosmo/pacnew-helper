# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.22)

project(pacnew-helper VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.8.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(ECMInstallIcons)
include(ECMAddTests)
include(FeatureSummary)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX PACNEWHELPER
    VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/src/pacnewhelper-version.h"
)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Auth
    CoreAddons
    Crash
    DBusAddons
    I18n
    IconThemes
    Kirigami
)

find_package(KF6KirigamiAddons 1.4.0 REQUIRED)
set_package_properties(KF6KirigamiAddons PROPERTIES
    TYPE RUNTIME
    PURPOSE "Provides FormCard components used for the About page"
)

qt_policy(SET QTP0001 NEW)

add_subdirectory(src)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(autotests)
endif()

install(PROGRAMS gay.geeks.pacnewhelper.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES gay.geeks.pacnewhelper.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
ecm_install_icons(ICONS icons/sc-apps-gay.geeks.pacnewhelper.svg DESTINATION ${KDE_INSTALL_ICONDIR} THEME hicolor)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
