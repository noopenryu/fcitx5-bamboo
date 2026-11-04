find_package(Fcitx5Module REQUIRED COMPONENTS DBus)

add_fcitx5_addon(bambooindicator indicator.cpp)
target_link_libraries(bambooindicator PRIVATE Fcitx5::Core Fcitx5::Module::DBus)
configure_file(bambooindicator.conf.in bambooindicator.conf @ONLY)
install(TARGETS bambooindicator DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bambooindicator.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

find_program(GNOME_EXTENSIONS_EXECUTABLE gnome-extensions)
if(GNOME_EXTENSIONS_EXECUTABLE)
    add_custom_target(gnome-extension
        COMMAND "${GNOME_EXTENSIONS_EXECUTABLE}" pack --force
                --extra-source=client.js --extra-source=state.js
                "--out-dir=${CMAKE_CURRENT_BINARY_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/extension"
        COMMENT "Package the Bamboo GNOME Shell extension"
        VERBATIM)
endif()

if(BUILD_TESTING)
    find_program(GJS_EXECUTABLE gjs)
    if(GJS_EXECUTABLE)
        add_test(NAME bamboo-gnome-client
                 COMMAND "${GJS_EXECUTABLE}" -m "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-client.js")
        set_tests_properties(bamboo-gnome-client PROPERTIES TIMEOUT 30)
        find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
        if(DBUS_RUN_SESSION_EXECUTABLE)
            add_test(NAME bamboo-gnome-dbus
                COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}" -- "${GJS_EXECUTABLE}"
                        -m "${CMAKE_CURRENT_SOURCE_DIR}/tests/test-dbus.js")
            set_tests_properties(bamboo-gnome-dbus PROPERTIES TIMEOUT 30)
        endif()
    endif()
endif()
