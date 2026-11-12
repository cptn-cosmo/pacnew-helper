# SPDX-License-Identifier: GPL-3.0-or-later

ecm_add_tests(
    mergetest.cpp
    operationstest.cpp
    LINK_LIBRARIES pacnewcore Qt6::Test
)

# End-to-end test on the demo system (autotests/demo-root). The app sources
# are compiled in directly, they are not a library.
ecm_add_test(
    pipelinetest.cpp
    ../src/app/scanner.cpp
    ../src/app/changemodel.cpp
    TEST_NAME pipelinetest
    LINK_LIBRARIES pacnewcore Qt6::Test Qt6::Qml
)
target_include_directories(pipelinetest PRIVATE ../src/app)
target_compile_definitions(pipelinetest PRIVATE DEMO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/demo-root" TRANSLATION_DOMAIN="pacnew-helper")

# The controller as the UI uses it, in demo mode (no administrator rights).
ecm_add_test(
    controllertest.cpp
    ../src/app/controller.cpp
    ../src/app/scanner.cpp
    ../src/app/changemodel.cpp
    ../src/app/filelistmodel.cpp
    ../src/app/historymodel.cpp
    TEST_NAME controllertest
    LINK_LIBRARIES pacnewcore Qt6::Test Qt6::Qml Qt6::Concurrent KF6::AuthCore KF6::CoreAddons KF6::I18n
)
target_include_directories(controllertest PRIVATE ../src/app)
target_compile_definitions(controllertest PRIVATE DEMO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/demo-root" TRANSLATION_DOMAIN="pacnew-helper")
