cmake_minimum_required(VERSION 3.12...4.0)

project(pvz-portable LANGUAGES C CXX)

include(CMake/ProjectVersion.cmake)

option(BUILD_STATIC "Static link for a standalone exe (Windows, non-MSVC)" OFF)
if(BUILD_STATIC)
	if(WIN32 AND NOT MSVC)
		set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
		set(BUILD_SHARED_LIBS OFF)
		add_link_options(-static)
	elseif(MSVC)
		message(WARNING "BUILD_STATIC is not supported for MSVC. Use a vcpkg -static triplet instead.")
		set(BUILD_STATIC OFF)
	else()
		message(WARNING "BUILD_STATIC is only supported on Windows with MinGW-based toolchains.")
		set(BUILD_STATIC OFF)
	endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_GENERATOR MATCHES "Ninja")
	set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

if(MSVC)
	if(DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	elseif(DEFINED VCPKG_TARGET_TRIPLET)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS
	src/*.cpp
	src/SexyAppFramework/*.cpp
	src/SexyAppFramework/graphics/*.cpp
	src/SexyAppFramework/imagelib/*.cpp
	src/SexyAppFramework/misc/*.cpp
	src/SexyAppFramework/paklib/*.cpp
	src/SexyAppFramework/sound/*.cpp
	src/SexyAppFramework/widget/*.cpp
	src/SexyAppFramework/fcaseopen/*.c
	src/PvzpLib/*.cpp
	src/Lawn/*.cpp
	src/Lawn/Widget/*.cpp
	src/Lawn/System/*.cpp
)
# vx: SetupWizard.cpp/.rc are the standalone wizard (vatrix-setup target), not part of the game
list(FILTER SOURCES EXCLUDE REGEX "SetupWizard")

# vx: in-game code editor (Dear ImGui + ImGuiColorTextEdit); desktop Windows + MinGW only
if(VX_SCRIPT AND WIN32 AND NOT MSVC)
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_draw.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_tables.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_widgets.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_impl_sdl2.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_impl_opengl3.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuiColorTextEdit/TextEditor.cpp
	)
endif()

set(LOW_MEMORY OFF CACHE BOOL "Enable low memory mode")
if(NINTENDO_SWITCH)
	set(VORBIS_LIB vorbisfile vorbis)
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/switch
	)
elseif(EMSCRIPTEN)
	# libopenmpt has no Emscripten port; must be pre-built with emmake.
	find_library(OPENMPT_LIBRARY NAMES openmpt libopenmpt REQUIRED)
	set(OPENMPT_LIB ${OPENMPT_LIBRARY})

	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/emscripten/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/emscripten
	)
else()
	if(VCPKG_TOOLCHAIN)
		find_package(Vorbis CONFIG REQUIRED)
		find_package(Ogg CONFIG REQUIRED)
		find_package(libopenmpt CONFIG REQUIRED)
		find_package(mpg123 CONFIG REQUIRED)
		set(VORBIS_LIB Vorbis::vorbis Vorbis::vorbisfile)
		set(OGG_LIB Ogg::ogg)
		set(OPENMPT_LIB libopenmpt::libopenmpt)
		set(MPG123_LIB MPG123::libmpg123)
	else()
		find_library(OGG_LIBRARY NAMES ogg libogg REQUIRED)
		find_library(VORBIS_LIBRARY NAMES vorbis libvorbis REQUIRED)
		find_library(VORBISFILE_LIBRARY NAMES vorbisfile libvorbisfile REQUIRED)
		find_library(OPENMPT_LIBRARY NAMES openmpt libopenmpt REQUIRED)
		find_library(MPG123_LIBRARY NAMES mpg123 libmpg123 REQUIRED)

		set(VORBIS_LIB ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY})
		set(OGG_LIB ${OGG_LIBRARY})
		set(OPENMPT_LIB ${OPENMPT_LIBRARY})
		set(MPG123_LIB ${MPG123_LIBRARY})
	endif()
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default/Input.cpp
	)
	list(APPEND PLAT_INCLUDES
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/platform/default
	)
endif()

if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
	set(WIN_CONSOLE_DEFAULT ON)
	set(PVZ_DEBUG_DEFAULT ON)
else()
	set(WIN_CONSOLE_DEFAULT OFF)
	set(PVZ_DEBUG_DEFAULT OFF)
endif()

option(PVZ_DEBUG "Enable PVZ_DEBUG macro to enable cheats, hidden features, etc." ${PVZ_DEBUG_DEFAULT})
# vx: keep debug info in release builds so crash-log stack RVAs can be symbolized with addr2line
if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE MATCHES "^[Rr][Ee][Ll][Ee][Aa][Ss][Ee]$" AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
	add_compile_options(-g)
endif()
option(CONSOLE "Show console on Windows" ${WIN_CONSOLE_DEFAULT})
option(DO_FIX_BUGS "Define DO_FIX_BUGS macro (Community fixes for original game bugs of 1.2.0.1073 GOTY Edition)" OFF)

if(EMSCRIPTEN)
	list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_MPG123=1")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2 -sUSE_ZLIB=1 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_MPG123=1 -fexceptions")
else()
	find_package(Threads REQUIRED)
	find_package(ZLIB REQUIRED)
	find_package(JPEG REQUIRED)
	find_package(PNG REQUIRED)
	find_package(SDL2 REQUIRED)
endif()

# vx: optional Python scripting (_vx module). Preferred: self-contained NuGet python fetched by
# fetch_python.ps1 into third_party/python/; falls back to a system Python 3.12 dev install.
option(VX_SCRIPT "Enable Python scripting support (_vx module)" ON)
set(VX_PYTHON_TOOLS "${PROJECT_SOURCE_DIR}/third_party/python/tools" CACHE PATH "Fetched NuGet python tools dir (fetch_python.ps1)")
if(VX_SCRIPT)
	if(WIN32 AND EXISTS "${VX_PYTHON_TOOLS}/python312.dll" AND EXISTS "${VX_PYTHON_TOOLS}/libpython312.a")
		# vx: fetched NuGet python: import lib for MinGW + headers + full stdlib next to the exe
		set(VX_PYTHON_IMPLIB "${VX_PYTHON_TOOLS}/libpython312.a")
		set(VX_PYTHON_INCLUDE "${VX_PYTHON_TOOLS}/include")
		set(VX_PYTHON_EXECUTABLE "${VX_PYTHON_TOOLS}/python.exe")
		set(VX_PYTHON_RUNTIME_DLL "${VX_PYTHON_TOOLS}/python312.dll")
		message(STATUS "VX_SCRIPT: using fetched NuGet python (${VX_PYTHON_TOOLS})")
	else()
		if(WIN32)
			set(Python3_ROOT_DIR "D:/msys64/ucrt64" CACHE PATH "Python3 root (MSYS2 UCRT64)")
		endif()
		find_package(Python3 3.12 COMPONENTS Development)
		if(NOT Python3_FOUND)
			message(WARNING "VX_SCRIPT: no fetched python and Python3 development files not found; scripting disabled (run fetch_python.ps1 or install Python 3.12)")
			set(VX_SCRIPT OFF)
		endif()
	endif()
endif()

function(_pvz_patch_sdl2_iface_libs target)
	get_target_property(_libs ${target} INTERFACE_LINK_LIBRARIES)
	if(NOT _libs)
		return()
	endif()
	set(_patched)
	foreach(_lib IN LISTS _libs)
		if(_lib STREQUAL "SDL2::SDL2")
			list(APPEND _patched SDL2::SDL2-static)
		else()
			list(APPEND _patched "${_lib}")
		endif()
	endforeach()
	set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_patched}")
endfunction()

if(BUILD_STATIC AND TARGET SDL2::SDL2-static)
	if(TARGET SDL2::SDL2main)
		_pvz_patch_sdl2_iface_libs(SDL2::SDL2main)
	endif()
	set(SDL2_LINK_TARGET SDL2::SDL2-static)
else()
	set(SDL2_LINK_TARGET SDL2::SDL2)
endif()

set(SDL_MIXER_X_STATIC ON CACHE BOOL "Build static library of SDL Mixer X")
set(SDL_MIXER_X_SHARED OFF CACHE BOOL "Build shared library of SDL Mixer X")
set(USE_MIDI OFF CACHE BOOL "SDL Mixer X midi")
set(USE_MODPLUG OFF CACHE BOOL "SDL Mixer X modplug")
set(USE_XMP OFF CACHE BOOL "SDL Mixer X xmp")
set(USE_OPUS OFF CACHE BOOL "SDL Mixer X opus")
set(USE_DRFLAC OFF CACHE BOOL "SDL Mixer X flac")
set(USE_PXTONE OFF CACHE BOOL "SDL Mixer X pxtone")
add_subdirectory(src/SexyAppFramework/sound/SDL-Mixer-X)

# Patch SDL-Mixer-X's transitive SDL2 dep without touching vendored CMake files
if(BUILD_STATIC AND TARGET SDL2::SDL2-static)
	_pvz_patch_sdl2_iface_libs(SDL2_mixer_ext_Static)
endif()

# Android requires a shared library loaded by SDLActivity; other platforms use an executable.
if(ANDROID)
	add_library(pvz-portable SHARED ${SOURCES})
elseif(IOS)
	add_executable(pvz-portable MACOSX_BUNDLE ${SOURCES})
else()
	add_executable(pvz-portable ${SOURCES})
endif()

target_include_directories(pvz-portable PRIVATE
	${PLAT_INCLUDES}
	${PROJECT_BINARY_DIR}
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/src/SexyAppFramework
	${PROJECT_SOURCE_DIR}/src/SexyAppFramework/sound/SDL-Mixer-X/include
	${SDL2_INCLUDE_DIRS}
)
if(VX_SCRIPT)
	# vx: imgui + ImGuiColorTextEdit headers (GLES2/gl2.h shim resolves to the bundled glad)
	target_include_directories(pvz-portable PRIVATE
		${PROJECT_SOURCE_DIR}/src/ThirdParty/imgui
		${PROJECT_SOURCE_DIR}/src/ThirdParty/ImGuiColorTextEdit
	)
	if(WIN32 AND NOT MSVC)
		# vx: compile the GLES2 flavour of the opengl3 backend; glad supplies the function pointers
		set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_impl_opengl3.cpp PROPERTIES
			COMPILE_DEFINITIONS "IMGUI_IMPL_OPENGL_ES2"
			COMPILE_OPTIONS "-include;glad/gles2.h"
		)
	endif()
endif()
target_compile_definitions(pvz-portable PRIVATE
	$<$<BOOL:${PVZ_DEBUG}>:PVZ_DEBUG>
	$<$<BOOL:${DO_FIX_BUGS}>:DO_FIX_BUGS>
	$<$<BOOL:${VX_SCRIPT}>:VX_SCRIPT>
)
target_compile_features(pvz-portable PRIVATE cxx_std_20)
set_target_properties(pvz-portable PROPERTIES CXX_EXTENSIONS OFF)

if (LOW_MEMORY)
	target_compile_definitions(pvz-portable PRIVATE LOW_MEMORY)
endif()

if(EMSCRIPTEN)
	set_property(TARGET pvz-portable APPEND PROPERTY LINK_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/wasm/shell.html"
	)
	target_link_libraries(pvz-portable PRIVATE
		SDL2_mixer_ext_Static
		${OPENMPT_LIB}
	)
	target_link_options(pvz-portable PRIVATE
		"SHELL:-sUSE_SDL=2"
		"SHELL:-sUSE_ZLIB=1"
		"SHELL:-sUSE_LIBPNG=1"
		"SHELL:-sUSE_LIBJPEG=1"
		"SHELL:-sUSE_OGG=1"
		"SHELL:-sUSE_VORBIS=1"
		"SHELL:-sUSE_MPG123=1"
		"SHELL:-fexceptions"
		"SHELL:-sASYNCIFY"
		"SHELL:-sALLOW_MEMORY_GROWTH=1"
		"SHELL:-sMAXIMUM_MEMORY=1073741824"
		"SHELL:-sGROWABLE_ARRAYBUFFERS=0"
		"SHELL:-sFORCE_FILESYSTEM=1"
		"SHELL:-sEXPORTED_RUNTIME_METHODS=[\"FS\",\"callMain\"]"
		"SHELL:-sINVOKE_RUN=0"
		"SHELL:-lidbfs.js"
		"SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/wasm/shell.html"
	)
	set_target_properties(pvz-portable PROPERTIES SUFFIX ".html")
else()
	target_link_libraries(pvz-portable PRIVATE
		SDL2_mixer_ext_Static 
		${OPENMPT_LIB} 
		${MPG123_LIB} 
		${VORBIS_LIB} 
		${OGG_LIB} 
		PNG::PNG
		JPEG::JPEG
		ZLIB::ZLIB
		SDL2::SDL2main
		${SDL2_LINK_TARGET}
		Threads::Threads
	)
	if(VX_SCRIPT AND VX_PYTHON_IMPLIB)
		# vx: fetched NuGet python (MinGW import lib + headers)
		target_link_libraries(pvz-portable PRIVATE "${VX_PYTHON_IMPLIB}")
		target_include_directories(pvz-portable PRIVATE "${VX_PYTHON_INCLUDE}")
	elseif(VX_SCRIPT AND Python3_FOUND)
		target_link_libraries(pvz-portable PRIVATE Python3::Python)
	endif()
endif()

if (WIN32)
	if(MSVC)
		target_compile_options(pvz-portable PRIVATE /utf-8)
	endif()

	target_sources(pvz-portable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/LawnProject.rc)

	set_source_files_properties(
		${CMAKE_CURRENT_SOURCE_DIR}/src/SexyAppFramework/LawnProject.rc
		PROPERTIES
			COMPILE_DEFINITIONS "APP_ICON_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/icon.ico\""
			OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico"
	)

	target_link_libraries(pvz-portable PRIVATE ws2_32 user32 gdi32 winmm imm32 shlwapi bcrypt)
	target_compile_definitions(pvz-portable PRIVATE WINDOWS)
	# vx: packaged exe is named Vatrix.exe
	set_target_properties(pvz-portable PROPERTIES OUTPUT_NAME "Vatrix")
	# vx: ship the resource hash manifest next to the exe (startup verification)
	add_custom_command(TARGET pvz-portable POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${PROJECT_SOURCE_DIR}/resources.sha256" $<TARGET_FILE_DIR:pvz-portable>)

	if(VX_SCRIPT AND VX_PYTHON_RUNTIME_DLL)
		# vx: copy the fetched python runtime (python312.dll + vcruntime) next to the exe
		foreach(_VX_RT_DLL python312.dll vcruntime140.dll vcruntime140_1.dll)
			if(EXISTS "${VX_PYTHON_TOOLS}/${_VX_RT_DLL}")
				add_custom_command(TARGET pvz-portable POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VX_PYTHON_TOOLS}/${_VX_RT_DLL}" $<TARGET_FILE_DIR:pvz-portable>
				)
			endif()
		endforeach()
		if(EXISTS "${VX_PYTHON_EXECUTABLE}")
			# vx: sync the stdlib (Lib/ + DLLs/) into <exe>/python312/
			add_custom_command(TARGET pvz-portable POST_BUILD
				COMMAND "${VX_PYTHON_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/CMake/sync_python312.py" "$<TARGET_FILE_DIR:pvz-portable>" "${VX_PYTHON_TOOLS}"
				VERBATIM
			)
		endif()
	elseif(VX_SCRIPT AND Python3_RUNTIME_LIBRARY)
		# vx: fallback: keep libpython3.12.dll next to the exe for local runs
		add_custom_command(TARGET pvz-portable POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Python3_RUNTIME_LIBRARY}" $<TARGET_FILE_DIR:pvz-portable>
		)
		if(VX_SCRIPT AND Python3_LIBRARIES)
			# vx: sync the pure-python stdlib next to the exe (python312/)
			get_filename_component(_VX_PY_ROOT "${Python3_LIBRARIES}" DIRECTORY)
			get_filename_component(_VX_PY_ROOT "${_VX_PY_ROOT}" DIRECTORY)
			set(_VX_PY_STDLIB "${_VX_PY_ROOT}/lib/python3.12")
			if(EXISTS "${_VX_PY_STDLIB}/os.py")
				find_program(VX_PYTHON_FALLBACK_EXECUTABLE python3.12 PATHS "${_VX_PY_ROOT}/bin" NO_DEFAULT_PATH)
				if(VX_PYTHON_FALLBACK_EXECUTABLE)
					add_custom_command(TARGET pvz-portable POST_BUILD
						COMMAND "${VX_PYTHON_FALLBACK_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/CMake/sync_python312.py" "$<TARGET_FILE_DIR:pvz-portable>" "${_VX_PY_STDLIB}"
						VERBATIM
					)
				endif()
			endif()
		endif()
	endif()

	if(NOT CONSOLE)
		if(MSVC)
			set_target_properties(pvz-portable PROPERTIES WIN32_EXECUTABLE TRUE)
		else()
			target_link_options(pvz-portable PRIVATE -mwindows)
		endif()
	endif()
elseif (NINTENDO_SWITCH)
	target_link_libraries(pvz-portable PRIVATE GLESv2 EGL glapi drm_nouveau)

	nx_generate_nacp(pvz-portable.nacp
		NAME "PvZ Portable"
		AUTHOR "wszqkzqk, etc., a community-driven port"
		VERSION "${PVZP_VERSION_PLAIN}"
	)

	nx_create_nro(pvz-portable
		NACP pvz-portable.nacp
		ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon-switch.jpg
	)
elseif (ANDROID)
	target_link_libraries(pvz-portable PRIVATE GLESv2 EGL android log)
	set_target_properties(pvz-portable PROPERTIES OUTPUT_NAME "main")
elseif (IOS)
	target_link_libraries(pvz-portable PRIVATE
		"-framework Foundation"
		"-framework UIKit"
		"-framework OpenGLES"
		"-framework QuartzCore"
		"-framework CoreGraphics"
		"-framework CoreMotion"
		"-framework GameController"
		"-framework AVFoundation"
		"-framework AudioToolbox"
		"-framework Metal"
	)

	configure_file(ios/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
	set_target_properties(pvz-portable PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.wszqkzqk.pvz-portable"
		MACOSX_BUNDLE_BUNDLE_NAME "PvZ Portable"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
		XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
		XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "io.github.wszqkzqk.pvz-portable"
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
		XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
	)

	# Add launch storyboard to the bundle
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard")
		target_sources(pvz-portable PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard")
		set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard"
			PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	endif()

	# Add asset catalog (app icon)
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
		target_sources(pvz-portable PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets")
		set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets"
			PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	endif()
endif()


# vx: sync player scripts (scripts/) and game python code (src/python/) into build/scripts/
# on every build when any file changes (no relink needed); stale files are not deleted
file(GLOB_RECURSE VATRIX_SCRIPT_FILES CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/scripts/*"
	"${PROJECT_SOURCE_DIR}/src/python/*")
list(FILTER VATRIX_SCRIPT_FILES INCLUDE REGEX "\\.py$")
if(WIN32 AND VATRIX_SCRIPT_FILES)
	set(VATRIX_SCRIPT_SYNC_DEPS)
	foreach(VATRIX_SCRIPT_FILE ${VATRIX_SCRIPT_FILES})
		if(VATRIX_SCRIPT_FILE MATCHES "^${PROJECT_SOURCE_DIR}/scripts/")
			file(RELATIVE_PATH VATRIX_SCRIPT_REL "${PROJECT_SOURCE_DIR}/scripts" "${VATRIX_SCRIPT_FILE}")
		else()
			file(RELATIVE_PATH VATRIX_SCRIPT_REL "${PROJECT_SOURCE_DIR}/src/python" "${VATRIX_SCRIPT_FILE}")
		endif()
		get_filename_component(VATRIX_SCRIPT_REL_DIR "${VATRIX_SCRIPT_REL}" DIRECTORY)
		add_custom_command(
			OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/${VATRIX_SCRIPT_REL}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/scripts/${VATRIX_SCRIPT_REL_DIR}"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${VATRIX_SCRIPT_FILE}"
				"${CMAKE_CURRENT_BINARY_DIR}/scripts/${VATRIX_SCRIPT_REL}"
			DEPENDS "${VATRIX_SCRIPT_FILE}")
		list(APPEND VATRIX_SCRIPT_SYNC_DEPS "${CMAKE_CURRENT_BINARY_DIR}/scripts/${VATRIX_SCRIPT_REL}")
	endforeach()
	add_custom_target(vatrix_scripts ALL DEPENDS ${VATRIX_SCRIPT_SYNC_DEPS})
endif()

# vx: copy every file under Properties/ into the build's properties dir at build time
# (dialogue strings, custom images, etc.; add files freely, no CMake edits needed)
file(GLOB VATRIX_PROPERTY_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Properties/*")
if(WIN32 AND VATRIX_PROPERTY_FILES)
	set(VATRIX_SYNC_DEPS)
	foreach(VATRIX_PROPERTY_FILE ${VATRIX_PROPERTY_FILES})
		get_filename_component(VATRIX_PROPERTY_NAME "${VATRIX_PROPERTY_FILE}" NAME)
		add_custom_command(
			OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/properties/${VATRIX_PROPERTY_NAME}"
			COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/properties"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"${VATRIX_PROPERTY_FILE}"
				"${CMAKE_CURRENT_BINARY_DIR}/properties/${VATRIX_PROPERTY_NAME}"
			DEPENDS "${VATRIX_PROPERTY_FILE}")
		list(APPEND VATRIX_SYNC_DEPS "${CMAKE_CURRENT_BINARY_DIR}/properties/${VATRIX_PROPERTY_NAME}")
	endforeach()
	add_custom_target(vatrix_properties ALL DEPENDS ${VATRIX_SYNC_DEPS})
endif()

# vx: first-run setup wizard (VatrixSetup.exe, Windows only)
if(WIN32)
	add_executable(vatrix-setup src/SetupWizard.cpp src/SetupWizard.rc)
	set_source_files_properties(
		${CMAKE_CURRENT_SOURCE_DIR}/src/SetupWizard.rc
		PROPERTIES
			COMPILE_DEFINITIONS "APP_ICON_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/icon.ico\""
			OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icon.ico"
	)
	target_compile_definitions(vatrix-setup PRIVATE UNICODE _UNICODE)
	target_compile_features(vatrix-setup PRIVATE cxx_std_20)
	set_target_properties(vatrix-setup PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "VatrixSetup")
	if(MSVC)
		target_compile_options(vatrix-setup PRIVATE /utf-8)
	endif()
	target_link_libraries(vatrix-setup PRIVATE comctl32 comdlg32 shell32 ole32 user32 gdi32 bcrypt)
	target_link_options(vatrix-setup PRIVATE -mwindows -municode)
endif()

