cmake_minimum_required(VERSION 3.10)
project(MyDiscord C)

set(CMAKE_C_STANDARD 11)

# Rechercher GTK4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk4)

# Inclure les headers
include_directories(
    ${GTK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    /usr/include/postgresql
    /usr/local/include
)

# Ajouter les options de compilation
add_definitions(${GTK_CFLAGS_OTHER})

# Client GTK (application graphique)
add_executable(app
    src/app_main.c
    src/app_auth.c
    src/app_chat.c
)

target_link_libraries(app
    ${GTK_LIBRARIES}
    pq
    dotenv
    pthread
)

# Serveur (application console)
add_executable(server
    src/server.c
    src/server_auth.c
    src/server_chat.c
)

target_link_libraries(server
    pq
    dotenv
    pthread
)
