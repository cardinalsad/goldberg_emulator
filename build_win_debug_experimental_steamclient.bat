cd /d "%~dp0"
call build_set_protobuf_directories.bat
"%PROTOC_X86_EXE%" -I.\dll\ --cpp_out=.\dll\ .\dll\net.proto
call build_env_x86.bat
cl dll/rtlgenrandom.c dll/rtlgenrandom.def
cl /LD /IImGui /Iglew\include /I%PROTOBUF_X86_DIRECTORY%\include\ /DSTEAMCLIENT_DLL /DCONTROLLER_SUPPORT /DEMU_EXPERIMENTAL_BUILD /DGLEW_STATIC /DEMU_OVERLAY dll/*.cpp dll/*.cc detours/*.cpp controller/gamepad.c ImGui/*.cpp ImGui/impls/*.cpp ImGui/impls/windows/*.cpp overlay_experimental/*.cpp overlay_experimental/windows/*.cpp "%PROTOBUF_X86_LIBRARY%" glew\glew.c opengl32.lib Iphlpapi.lib Ws2_32.lib rtlgenrandom.lib Shell32.lib Winmm.lib /EHsc /MP12 /link /OUT:steamclient.dll
call build_env_x64.bat
cl dll/rtlgenrandom.c dll/rtlgenrandom.def
cl /LD /IImGui /Iglew\include /I%PROTOBUF_X64_DIRECTORY%\include\ /DSTEAMCLIENT_DLL /DCONTROLLER_SUPPORT /DEMU_EXPERIMENTAL_BUILD /DGLEW_STATIC /DEMU_OVERLAY dll/*.cpp dll/*.cc detours/*.cpp controller/gamepad.c ImGui/*.cpp ImGui/impls/*.cpp ImGui/impls/windows/*.cpp overlay_experimental/*.cpp overlay_experimental/windows/*.cpp "%PROTOBUF_X64_LIBRARY%" glew\glew.c opengl32.lib Iphlpapi.lib Ws2_32.lib rtlgenrandom.lib Shell32.lib Winmm.lib /EHsc /MP12 /link /OUT:steamclient64.dll
