cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(tomografo)

# ROOT paths
execute_process(
  COMMAND /home/daniel/Downloads/root/bin/root-config --cflags
  OUTPUT_VARIABLE ROOT_CFLAGS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
  COMMAND /home/daniel/Downloads/root/bin/root-config --libs
  OUTPUT_VARIABLE ROOT_LIBS
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Separar flags do ROOT
separate_arguments(ROOT_CFLAGS_LIST UNIX_COMMAND "${ROOT_CFLAGS}")
separate_arguments(ROOT_LIBS_LIST UNIX_COMMAND "${ROOT_LIBS}")

# Incluir Garfield++ via CMAKE_PREFIX_PATH
set(CMAKE_PREFIX_PATH "/home/daniel/garfield/install")

# Encontrar Garfield++
find_package(Garfield REQUIRED)

# Encontrar OpenMP
find_package(OpenMP REQUIRED)

# Executável gain
add_executable(gain gain.cpp)
target_compile_options(gain PRIVATE ${ROOT_CFLAGS_LIST})

if(OpenMP_CXX_FOUND)
  target_compile_options(gain PRIVATE ${OpenMP_CXX_FLAGS})
  target_link_libraries(gain PRIVATE ${OpenMP_CXX_LIBRARIES})
endif()

target_link_libraries(gain PRIVATE Garfield::Garfield ${ROOT_LIBS_LIST})

# Executável checkfield
add_executable(checkfield checkfield.cpp)
target_link_libraries(checkfield Garfield::Garfield)

# Executável photon (com ROOT também)
add_executable(photon photon.cpp)
target_compile_options(photon PRIVATE ${ROOT_CFLAGS_LIST})
target_link_libraries(photon PRIVATE Garfield::Garfield ${ROOT_LIBS_LIST})

# Executável photon (com ROOT também)
add_executable(photon_gain photon_gain.cpp)
target_compile_options(photon_gain PRIVATE ${ROOT_CFLAGS_LIST})
target_link_libraries(photon_gain PRIVATE Garfield::Garfield ${ROOT_LIBS_LIST})

# Executável gain
add_executable(full_gain full_gain.cpp)
target_compile_options(full_gain PRIVATE ${ROOT_CFLAGS_LIST})

if(OpenMP_CXX_FOUND)
  target_compile_options(full_gain PRIVATE ${OpenMP_CXX_FLAGS})
  target_link_libraries(full_gain PRIVATE ${OpenMP_CXX_LIBRARIES})
endif()

target_link_libraries(full_gain PRIVATE Garfield::Garfield ${ROOT_LIBS_LIST})
