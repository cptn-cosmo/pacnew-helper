# SPDX-License-Identifier: GPL-3.0-or-later

# --- core: diff/merge engine, file knowledge base and the privileged file
#     operations (shared by the app, the helper and the tests)
add_library(pacnewcore STATIC
    core/merge.cpp core/merge.h
    core/fileinfo.cpp core/fileinfo.h
    core/operations.cpp core/operations.h
)
target_include_directories(pacnewcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(pacnewcore PUBLIC Qt6::Core KF6::I18n)
target_compile_definitions(pacnewcore PRIVATE TRANSLATION_DOMAIN="pacnew-helper")

# --- privileged KAuth helper
add_executable(pacnew-helper-authhelper helper/helper.cpp helper/helper.h)
target_link_libraries(pacnew-helper-authhelper PRIVATE pacnewcore KF6::AuthCore)
install(TARGETS pacnew-helper-authhelper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
kauth_install_helper_files(pacnew-helper-authhelper gay.geeks.pacnewhelper root)
kauth_install_actions(gay.geeks.pacnewhelper helper/gay.geeks.pacnewhelper.actions)

# --- application
add_executable(pacnew-helper)

qt_add_qml_module(pacnew-helper
    URI gay.geeks.pacnewhelper
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/FilesPage.qml
        qml/ReviewPage.qml
        qml/LeftoverPage.qml
        qml/ChangeCard.qml
        qml/DiffPane.qml
        qml/ResultPage.qml
        qml/SummaryPage.qml
        qml/HistoryPage.qml
        qml/StatTile.qml
    SOURCES
        main.cpp
        app/scanner.cpp app/scanner.h
        app/filelistmodel.cpp app/filelistmodel.h
        app/changemodel.cpp app/changemodel.h
        app/historymodel.cpp app/historymodel.h
        app/controller.cpp app/controller.h
)

target_include_directories(pacnew-helper PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_compile_definitions(pacnew-helper PRIVATE TRANSLATION_DOMAIN="pacnew-helper")

target_link_libraries(pacnew-helper PRIVATE
    pacnewcore
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::AuthCore
    KF6::CoreAddons
    KF6::Crash
    KF6::DBusAddons
    KF6::I18n
    KF6::I18nQml
    KF6::IconThemes
)

install(TARGETS pacnew-helper ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
