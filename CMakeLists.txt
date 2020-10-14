# Based on: https://github.com/ttroy50/cmake-examples/blob/master/03-code-generation/protobuf/CMakeLists.txt
cmake_minimum_required(VERSION 3.6)

# Set the project name
project(goldberg_emulator)

# Workaround because cross-compiling with CMake + vcpkg fucks up this variable
# making the 'find_*' functions/macros somehow fail to find the right arch.
# There are conditions like
#  CMAKE_SIZEOF_VOID_P = "4" then look for i386 libraries...
# or
#  CMAKE_SIZEOF_VOID_P = "8" then look for x86_64 libraries...
if(X86 AND NOT X64)
  set(CMAKE_SIZEOF_VOID_P "4")
elseif(X64 AND NOT X86)
  set(CMAKE_SIZEOF_VOID_P "8")
else()
  message(FATAL_ERROR "Please define either -DX86=ON or -DX64=ON")
endif()

if(WIN32) # Setup some variables for Windows build
  if(MSVC) # If building with MSVC
    add_definitions(-D_CRT_SECURE_NO_WARNINGS) # Disable warning about strncpy_s and his friends
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:\"msvcrtd.lib\"")       # Disable this linkage
    #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:\"msvcrtd.lib\"") # Disable this linkage
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE) # Force to only build Debug & Release projects

    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        )
    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    
  endif()

  if(X64)
    set(LIB_STEAM_API steam_api64)
	set(LIB_STEAMCLIENT steamclient64)
    set(OUT_DIR win64)
  elseif(X86)
    set(LIB_STEAM_API steam_api)
    set(LIB_STEAMCLIENT steamclient)
    set(OUT_DIR win32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()

  file(
    GLOB
    OVERLAY_EXPERIMENTAL_SRC_SHARED
	overlay_experimental/*.cpp
    overlay_experimental/windows/*.cpp
	ImGui/*.cpp
    ImGui/impls/*.cpp
    ImGui/impls/windows/*.cpp
  )
  
  set(overlay_includes
    overlay/
    overlay/windows/
  )

elseif(APPLE)
  if(X64)
    # Global flags for building steamapi (64bits)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64")
    #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -m64")
    set(OUT_DIR macosx64)
  elseif(X86)
    # Global flags for building steamapi (32bits)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
    #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -m32")
    set(OUT_DIR macosx32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

  set(LIB_STEAM_API steam_api)
  set(LIB_STEAMCLIENT steamclient)

  file(
    GLOB
    OVERLAY_EXPERIMENTAL_SRC_SHARED
    overlay/*.cpp
  )
  
  set(overlay_includes
    overlay/
  )

elseif(UNIX)
  if(X64)
    # Global flags for building steamapi (64bits)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m64")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m64")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64")
    #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -m64")
    set(OUT_DIR linux64)
  elseif(X86)
    # Global flags for building steamapi (32bits)
    set(CMAKE_C_FLAGS             "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS           "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
    #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -m32")
    set(OUT_DIR linux32)
  else()
    message(FATAL_ERROR "Arch unknown")
  endif()
  set(LIB_STEAM_API steam_api)
  set(LIB_STEAMCLIENT steamclient)

  file(
    GLOB
    OVERLAY_EXPERIMENTAL_SRC_SHARED
	overlay_experimental/*.cpp
	overlay_experimental/linux/*.cpp
	ImGui/*.cpp
    ImGui/impls/*.cpp
    ImGui/impls/linux/*.cpp
  )

  set(overlay_includes
    overlay/
    overlay/linux/
  )

else()
  message(FATAL_ERROR "No CMake for other platforms")

endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add option to enable experimental build
option(EMU_EXPERIMENTAL_BUILD "Enable experimental build" OFF)
if(EMU_EXPERIMENTAL_BUILD)
	if(APPLE)
		set(EMU_OVERLAY OFF)
	else()
		option(EMU_OVERLAY "Enable the emulator overlay" ON)
	endif()
else()
	set(EMU_OVERLAY OFF)
endif()

set(Protobuf_USE_STATIC_LIBS ON)
include(FindProtobuf)
find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(nlohmann-fifo-map CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb.h")

if(NOT WIN32)
	find_package(OpenSSL REQUIRED)
endif()

if(NOT APPLE)
	find_package(GLEW REQUIRED)
endif()

# Generate the .h and .cxx files for dll/net.proto
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS dll/net.proto)

# Print path to generated files
message(STATUS "PROTO_SRCS = ${PROTO_SRCS}")
message(STATUS "PROTO_HDRS = ${PROTO_HDRS}")

# Gather the files that are shared between multiple targets
file(GLOB DLL_SRC_SHARED
	dll/*.h
	dll/*.cpp
)

file(GLOB DETOURS_SRC_SHARED
	detours/*.cpp
)

###################################################
# Setup for the steam_api(64).dll / libsteam_api.so
###################################################

# Setup the target
add_library(${LIB_STEAM_API}
	SHARED
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:${DETOURS_SRC_SHARED}>
	$<$<AND:$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:${OVERLAY_EXPERIMENTAL_SRC_SHARED}>
	${DLL_SRC_SHARED}
	${PROTO_SRCS}
	${PROTO_HDRS}
)

# Include the required directories
target_include_directories(
	${LIB_STEAM_API}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
	${STB_INCLUDE_DIRS}
	
	overlay_experimental
)

# Link the required libraries
target_link_libraries(${LIB_STEAM_API}
	PRIVATE
	Threads::Threads
	protobuf::libprotobuf-lite
	#nlohmann_json::nlohmann_json doesn't work
	nlohmann-fifo-map::nlohmann-fifo-map
	
	$<$<NOT:$<BOOL:${APPLE}>>:GLEW::GLEW>
	
	$<$<BOOL:${WIN32}>:comdlg32>
	$<$<BOOL:${WIN32}>:advapi32>
	$<$<BOOL:${WIN32}>:shell32>
	$<$<BOOL:${WIN32}>:user32>
	$<$<BOOL:${WIN32}>:ws2_32>
	$<$<BOOL:${WIN32}>:iphlpapi>
	$<$<AND:$<BOOL:${WIN32}>,$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:opengl32.lib>
	$<$<AND:$<BOOL:${WIN32}>,$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:winmm.lib>
	
	# For library UNIX loading
	$<$<NOT:$<BOOL:${WIN32}>>:dl>
	# For overlay opengl
	$<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:GL>
)

# Add target compile definitions
target_compile_definitions(${LIB_STEAM_API}
	PRIVATE
	GLEW_NO_GLU
	STBI_NO_SIMD
	
	$<$<CONFIG:>:EMU_RELEASE_BUILD>
	$<$<CONFIG:Release>:EMU_RELEASE_BUILD>
	$<$<CONFIG:RelWithDebInfo>:EMU_RELEASE_BUILD>
	$<$<CONFIG:MinSizeRel>:EMU_RELEASE_BUILD>
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:EMU_EXPERIMENTAL_BUILD>
	$<$<BOOL:${EMU_OVERLAY}>:EMU_OVERLAY>
)

########################################################
# Setup for the steamclient(64).dll / libsteamclient.so?
########################################################

if(UNIX)
  SET_TARGET_PROPERTIES(${LIB_STEAMCLIENT} PROPERTIES PREFIX "")
endif()

# Setup the target
add_library(${LIB_STEAMCLIENT}
	SHARED
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:${DETOURS_SRC_SHARED}>
	$<$<AND:$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:${OVERLAY_EXPERIMENTAL_SRC_SHARED}>
	${DLL_SRC_SHARED}
	${PROTO_SRCS}
	${PROTO_HDRS}
)

# Include the required directories
target_include_directories(
	${LIB_STEAMCLIENT}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
	${STB_INCLUDE_DIRS}
	
	overlay_experimental
)

# Link the required libraries
target_link_libraries(${LIB_STEAMCLIENT}
	PRIVATE
	Threads::Threads
	protobuf::libprotobuf-lite
	#nlohmann_json::nlohmann_json doesn't work
	nlohmann-fifo-map::nlohmann-fifo-map
	
	$<$<NOT:$<BOOL:${APPLE}>>:GLEW::GLEW>
	
	$<$<BOOL:${WIN32}>:comdlg32>
	$<$<BOOL:${WIN32}>:advapi32>
	$<$<BOOL:${WIN32}>:shell32>
	$<$<BOOL:${WIN32}>:user32>
	$<$<BOOL:${WIN32}>:ws2_32>
	$<$<BOOL:${WIN32}>:iphlpapi>
	$<$<AND:$<BOOL:${WIN32}>,$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:opengl32.lib>
	$<$<AND:$<BOOL:${WIN32}>,$<BOOL:${EMU_EXPERIMENTAL_BUILD}>,$<BOOL:${EMU_OVERLAY}>>:winmm.lib>
	
	# For library UNIX loading
	$<$<NOT:$<BOOL:${WIN32}>>:dl>
	# For overlay opengl
	$<$<AND:$<BOOL:${UNIX}>,$<NOT:$<BOOL:${APPLE}>>>:GL>
)

# Add target compile definitions
target_compile_definitions(${LIB_STEAMCLIENT}
	PRIVATE
	GLEW_NO_GLU
	STBI_NO_SIMD
	STEAMCLIENT_DLL
	
	$<$<CONFIG:>:EMU_RELEASE_BUILD>
	$<$<CONFIG:Release>:EMU_RELEASE_BUILD>
	$<$<CONFIG:RelWithDebInfo>:EMU_RELEASE_BUILD>
	$<$<CONFIG:MinSizeRel>:EMU_RELEASE_BUILD>
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:EMU_EXPERIMENTAL_BUILD>
	$<$<BOOL:${EMU_OVERLAY}>:EMU_OVERLAY>
)

##############################################################################
# Setup for the steamnetworkingsockets(64).dll / libsteamnetworkingsockets.so?
##############################################################################

# Setup the target
add_library(steamnetworkingsockets
	SHARED
	steamnetworkingsockets.cpp
)

# Add target compile definitions
target_compile_definitions(steamnetworkingsockets
	PRIVATE
	$<$<CONFIG:>:EMU_RELEASE_BUILD>
	$<$<CONFIG:Release>:EMU_RELEASE_BUILD>
	$<$<CONFIG:RelWithDebInfo>:EMU_RELEASE_BUILD>
	$<$<CONFIG:MinSizeRel>:EMU_RELEASE_BUILD>
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:EMU_EXPERIMENTAL_BUILD>
)

set_target_properties(steamnetworkingsockets PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

###########################################################################
# Setup for the lobby_connect(64).exe / lobby_connect
###########################################################################

# Setup the target
add_executable(lobby_connect
	lobby_connect.cpp
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:${DETOURS_SRC_SHARED}>
	${DLL_SRC_SHARED}
	${PROTO_SRCS}
	${PROTO_HDRS}
)

target_include_directories(lobby_connect
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
)

# Link the required libraries
target_link_libraries(lobby_connect
	PRIVATE
	Threads::Threads
	protobuf::libprotobuf-lite
	#nlohmann_json::nlohmann_json doesn't work
	nlohmann-fifo-map::nlohmann-fifo-map
	
	$<$<BOOL:${WIN32}>:comdlg32>
	$<$<BOOL:${WIN32}>:advapi32>
	$<$<BOOL:${WIN32}>:shell32>
	$<$<BOOL:${WIN32}>:user32>
	$<$<BOOL:${WIN32}>:ws2_32>
	$<$<BOOL:${WIN32}>:iphlpapi>
	
	# For library UNIX loading
	$<$<NOT:$<BOOL:${WIN32}>>:dl>
	
	-debug:none
)

# Add target compile definitions
target_compile_definitions(lobby_connect
	PRIVATE
	NO_DISK_WRITES
	LOBBY_CONNECT
	$<$<CONFIG:>:EMU_RELEASE_BUILD>
	$<$<CONFIG:Release>:EMU_RELEASE_BUILD>
	$<$<CONFIG:RelWithDebInfo>:EMU_RELEASE_BUILD>
	$<$<CONFIG:MinSizeRel>:EMU_RELEASE_BUILD>
	$<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:EMU_EXPERIMENTAL_BUILD>
)

###########################################################################
# Setup for the generate_interfaces_file(64).exe / generate_interfaces_file
###########################################################################

# Setup the target
add_executable(
	generate_interfaces
	generate_interfaces_file.cpp
)

# Link the required libraries
target_link_libraries(
	generate_interfaces
	PRIVATE
	-debug:none
)


###########################################################################
# Installation setup
###########################################################################

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set(OUT_DIR debug/${OUT_DIR})
else()
	set(OUT_DIR release/${OUT_DIR})
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

# emulators
install(
	TARGETS ${LIB_STEAM_API} ${LIB_STEAMCLIENT}
	RUNTIME DESTINATION ${OUT_DIR}
	LIBRARY DESTINATION ${OUT_DIR}
)

# generate_interfaces
install(TARGETS
	generate_interfaces
	RUNTIME DESTINATION ${OUT_DIR}/tools/generate_interfaces
)
install(FILES 
  scripts/find_interfaces.sh
  scripts/find_interfaces.ps1
  Readme_generate_interfaces.txt
  DESTINATION ${OUT_DIR}/tools/generate_interfaces
)

# lobby_connect
install(TARGETS
	lobby_connect
	RUNTIME DESTINATION ${OUT_DIR}/tools/lobby_connect
)
install(FILES 
  Readme_lobby_connect.txt
  DESTINATION ${OUT_DIR}/tools/lobby_connect
)

install(FILES 
  Readme_release.txt
  files_example/steam_appid.EDIT_AND_RENAME.txt
  files_example/steam_interfaces.EXAMPLE.txt
  $<$<BOOL:${EMU_EXPERIMENTAL_BUILD}>:${PROJECT_SOURCE_DIR}/Readme_experimental.txt>
  $<$<CONFIG:Debug>:${PROJECT_SOURCE_DIR}/Readme_debug.txt>
  DESTINATION ${OUT_DIR}
)

install(DIRECTORY 
  files_example/steam_settings.EXAMPLE
  DESTINATION ${OUT_DIR}
)
