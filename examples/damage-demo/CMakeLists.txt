find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Qml)
qt_policy(SET QTP0001 NEW)

qt_add_executable(damage-demo
    main.cpp
)

qt_add_qml_module(damage-demo
    URI DamageDemo
    VERSION 1.0
    QML_FILES
        Main.qml
    SOURCES
        demoscene.h
        demoscene.cpp
        damageoverlay.h
        damageoverlay.cpp
)

target_link_libraries(damage-demo PRIVATE
    gdt
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::Gui
)
