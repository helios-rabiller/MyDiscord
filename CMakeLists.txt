cmake_minimum_required(VERSION 3.10)
project(GtkChatApp C)

set(CMAKE_C_STANDARD 11)

# Recherches GTK4
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

# Ajout des fichiers source
add_executable(app
    src/main.c
    src/auth.c
    src/chat.c
)

# Lier les bibliothèques
target_link_libraries(app
    ${GTK_LIBRARIES}
    pq
    dotenv
    pthread
)
