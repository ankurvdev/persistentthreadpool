set(CMAKE_CXX_STANDARD 20)
project(TaskPool)

add_library(ptptestlib STATIC src/PersistentThreadPool.cpp)
target_compile_definitions(ptptestlib PUBLIC TESTING=1)

add_library(ptplib SHARED src/PersistentThreadPool.cpp)

add_executable(tests tests/TestTaskPool.cpp)
target_link_libraries(tests PRIVATE ptptestlib)
