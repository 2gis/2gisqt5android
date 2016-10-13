GYP_ARGS += "-D qt_os=\"desktop_linux\""

include(linux.pri)

GYP_CONFIG += \
    desktop_linux=1 \
    enable_plugins=1 \

clang {
    GYP_CONFIG += clang=1 host_clang=1 clang_use_chrome_plugins=0 make_clang_dir=/usr
    linux-clang-libc++: GYP_CONFIG += use_system_libcxx=1
} else {
    GYP_CONFIG += clang=0 host_clang=0
}
