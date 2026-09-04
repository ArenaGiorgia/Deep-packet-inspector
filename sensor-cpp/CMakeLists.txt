cmake_minimum_required(VERSION 3.10)

# Diamo un nome al nostro microservizio
project(DeepPacketSensor)

# Scegliamo C++17: ci permette di usare i puntatori intelligenti (smart pointers) moderni
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 1. CERCHIAMO LE LIBRERIE NECESSARIE
# Cerchiamo i Protocol Buffers (per il contratto) e i Thread del sistema operativo
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

# 2. CARTELLE DI INCLUSIONE
# Diciamo al compilatore dove trovare i nostri file .h e quelli di Protobuf
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})

# 3. CREAZIONE DELL'ESEGUIBILE
# Elenchiamo tutti i file .cpp che devono essere presi e "fusi" nel programma finale chiamato "sensor"
add_executable(sensor
    main.cpp
    capture_thread.cpp
    packet_handler.cpp
    forwarder.cpp
    packet_data.pb.cc
)

# 4. COLLEGAMENTO DELLE LIBRERIE (Linking)
# Attacchiamo al nostro eseguibile le librerie di rete (pcap), Protobuf e la gestione dei Thread
target_link_libraries(sensor
    ${Protobuf_LIBRARIES}
    pcap
    Threads::Threads
)