cmake_minimum_required(VERSION 3.20)
project(kde-lyricmon VERSION 0.2.0 LANGUAGES CXX)

# 小程序「关于」对话框读的是 package/metadata.json 里的 Version，而标签和构建走
# 的是 CMake 的 project() 版本。两处版本漂移过一次就会一直对不上，所以在这里直接
# 比对，发版时漏改一个文件会立刻构建失败。
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package/metadata.json" LYRICS_METADATA)
string(REGEX MATCH "\"Version\"[ \t]*:[ \t]*\"([0-9]+[.][0-9]+[.][0-9]+)\"" _ "${LYRICS_METADATA}")
set(LYRICS_METADATA_VERSION "${CMAKE_MATCH_1}")
if(NOT LYRICS_METADATA_VERSION STREQUAL PROJECT_VERSION)
    message(FATAL_ERROR
        "Version mismatch: package/metadata.json says ${LYRICS_METADATA_VERSION}, "
        "CMakeLists.txt project() says ${PROJECT_VERSION}. Bump both in the same commit.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick DBus Network Test)

# 后端核心：MPRIS 代理、播放器注册表、歌词引擎、LRC 解析。
# 单独做成静态库，测试和 QML 插件共用同一份实现。
add_library(lyricmoncore STATIC
    src/lrcparser.cpp
    src/mprisplayer.cpp
    src/mprisregistry.cpp
    src/lyricsengine.cpp
)
set_target_properties(lyricmoncore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(lyricmoncore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(lyricmoncore PUBLIC Qt6::Core Qt6::Gui Qt6::DBus Qt6::Network)

add_library(lyricmonplugin MODULE src/lyricmonplugin.cpp)
target_link_libraries(lyricmonplugin PRIVATE lyricmoncore Qt6::Qml)
# 插件直接落进源码树里的 package/，这样 plasmawindowed、离屏预览、软链接安装
# 都能立刻用上刚编译出来的 .so。
add_custom_command(TARGET lyricmonplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:lyricmonplugin>
        ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/lib/Hao/LyricMon/liblyricmonplugin.so
)

# 开发工具：一个可以被任意摆布的 MPRIS 播放器，用来在不动真实播放器的前提下
# 联调进度、切歌、Seek 与歌词滚动，截图也靠它出确定性数据。
add_executable(lyricmon-fake-player tools/fakeplayer.cpp)
target_link_libraries(lyricmon-fake-player PRIVATE Qt6::Core Qt6::DBus)

enable_testing()
add_executable(lyricmon-tests tests/tst_lyrics.cpp)
target_link_libraries(lyricmon-tests PRIVATE lyricmoncore Qt6::Test)
add_test(NAME lyricmon-tests COMMAND lyricmon-tests)

# 离屏渲染工具：把真实数据画成 PNG，README 的截图和布局回归都靠它。
add_executable(lyricmon-preview tools/preview.cpp)
target_link_libraries(lyricmon-preview PRIVATE Qt6::Gui Qt6::Quick Qt6::Qml)
