cmake_minimum_required(VERSION 3.10)
project(MyDiscordApp C)

set(CMAKE_C_STANDARD 17)

# Trouve GTK4
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Ajoute les includes manuellement
include_directories(
    ${GTK4_INCLUDE_DIRS}
    /usr/include/postgresql
    /usr/include # pour dotenv.h
)

link_directories(
    ${GTK4_LIBRARY_DIRS}
    /usr/lib # pour libpq et libdotenv
)

# Supprime les warnings de pkg-config
add_definitions(${GTK4_CFLAGS_OTHER})

# Crée l'exécutable
add_executable(app auth-client.c)

# Lien avec les bibliothèques
target_link_libraries(app
    ${GTK4_LIBRARIES}
    pq
    dotenv
)
