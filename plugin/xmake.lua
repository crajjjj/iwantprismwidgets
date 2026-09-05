-- set minimum xmake version
set_xmakever("2.8.2")

includes("lib/commonlibsse-ng")

set_project("iWantWidgetsPrisma")
set_version("0.1.0")
set_license("MIT")

set_languages("c++23")
set_warnings("allextra")

set_policy("package.requires_lock", true)

add_rules("mode.release")
add_rules("plugin.vsxmake.autoupdate")

target("iWantWidgetsPrisma")
    add_deps("commonlibsse-ng")

    add_rules("commonlibsse-ng.plugin", {
        name = "iWantWidgetsPrisma",
        author = "crajjjj",
        description = "iWant Widgets reimplemented on PrismaUI (drop-in for the Flash/Scaleform original)"
    })

    -- Image decode/encode uses Windows' built-in WIC (DDS + PNG codecs),
    -- so no third-party image library is required.
    add_syslinks("ole32", "shlwapi", "windowscodecs")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
