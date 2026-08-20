find_package(Qt6 REQUIRED COMPONENTS Test Qml Quick)

add_executable(tst_gdt tst_gdt.cpp)
target_link_libraries(tst_gdt PRIVATE gdt Qt6::Test)
add_test(NAME tst_gdt COMMAND tst_gdt)
set_tests_properties(tst_gdt PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_demo
    tst_demo.cpp
    ../examples/damage-demo/demoscene.cpp
    ../examples/damage-demo/demoscene.h
    ../examples/damage-demo/damageoverlay.cpp
    ../examples/damage-demo/damageoverlay.h
)
target_include_directories(tst_demo PRIVATE ../examples/damage-demo)
target_link_libraries(tst_demo PRIVATE gdt Qt6::Test Qt6::Qml Qt6::Quick)
add_test(NAME tst_demo COMMAND tst_demo)
set_tests_properties(tst_demo PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(bench_gdt bench_gdt.cpp)
target_link_libraries(bench_gdt PRIVATE gdt Qt6::Test)
add_test(NAME bench_gdt COMMAND bench_gdt)
set_tests_properties(bench_gdt PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
