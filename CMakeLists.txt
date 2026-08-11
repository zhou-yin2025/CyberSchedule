cmake_minimum_required(VERSION 3.19)
project(CyberScheduleV2 LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql Multimedia)

qt_standard_project_setup()

qt_add_executable(CyberScheduleV2 WIN32
    main.cpp
    # core
    core/BaseFloatingWidget.h core/BaseFloatingWidget.cpp
    core/DatabaseManager.h core/DatabaseManager.cpp
    core/PomodoroTimer.h core/PomodoroTimer.cpp
    # animations
    animations/SplashScreen.h animations/SplashScreen.cpp
    # modules
    modules/StatusWidget.h modules/StatusWidget.cpp
    modules/HUDWidget.h modules/HUDWidget.cpp
    modules/CalendarWidget.h modules/CalendarWidget.cpp
    modules/StickyNoteWidget.h modules/StickyNoteWidget.cpp
    modules/TaskWidgetV3.h modules/TaskWidgetV3.cpp
    modules/settingswidget.h modules/settingswidget.cpp
    modules/PomodoroConfigDialog.h modules/PomodoroConfigDialog.cpp modules/PomodoroConfigDialog.ui
    animations/gridbackground.h animations/gridbackground.cpp
    modules/mottowidget.h modules/mottowidget.cpp
)

target_include_directories(CyberScheduleV2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/animations
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

target_link_libraries(CyberScheduleV2 PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::Multimedia
)

install(TARGETS CyberScheduleV2
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET CyberScheduleV2
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})