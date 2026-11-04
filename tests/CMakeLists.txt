file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data/addon"
                    "${CMAKE_CURRENT_BINARY_DIR}/data/inputmethod"
                    "${CMAKE_CURRENT_BINARY_DIR}/data/bamboo")
configure_file(../src/bamboo-addon.conf.in.in data/addon/bamboo.conf @ONLY)
configure_file(../src/bamboo.conf.in data/inputmethod/bamboo.conf COPYONLY)
configure_file(../data/vietnamese.cm.dict data/bamboo/vietnamese.cm.dict COPYONLY)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/data/addon/testfrontend.conf"
    "[Addon]\nName=Test Frontend\nCategory=Frontend\nType=SharedLibrary\nLibrary=libtestfrontend\nEnabled=True\n")
add_executable(test-bamboo-fcitx test-fcitx.cpp)
target_link_libraries(test-bamboo-fcitx PRIVATE Fcitx5::Core Fcitx5::Module::TestFrontend)
target_compile_definitions(test-bamboo-fcitx PRIVATE
    BAMBOO_BINARY_DIR="${PROJECT_BINARY_DIR}"
    FCITX_ADDON_DIR="${FCITX_INSTALL_ADDONDIR}")
add_dependencies(test-bamboo-fcitx bamboo)
add_test(NAME bamboo-fcitx COMMAND test-bamboo-fcitx)
set_tests_properties(bamboo-fcitx PROPERTIES TIMEOUT 30)

if(ENABLE_GNOME_INTEGRATION)
    configure_file(../integrations/gnome/bambooindicator.conf.in
                   data/addon/bambooindicator.conf @ONLY)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/data/addon/keyboard.conf"
        "[Addon]\nName=Keyboard\nCategory=InputMethod\nType=StaticLibrary\nLibrary=keyboard\nEnabled=True\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/data/addon/dbus.conf"
        "[Addon]\nName=DBus\nCategory=Module\nType=SharedLibrary\nLibrary=libdbus\nEnabled=True\n[Addon/Dependencies]\n0=keyboard\n")
    find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session)
    if(NOT DBUS_RUN_SESSION_EXECUTABLE)
        message(FATAL_ERROR "GNOME integration tests require dbus-run-session")
    endif()
    add_executable(test-bamboo-indicator test-indicator.cpp)
    target_link_libraries(test-bamboo-indicator PRIVATE Fcitx5::Core
        Fcitx5::Module::DBus Fcitx5::Module::TestFrontend)
    target_compile_definitions(test-bamboo-indicator PRIVATE
        BAMBOO_BINARY_DIR="${PROJECT_BINARY_DIR}"
        FCITX_ADDON_DIR="${FCITX_INSTALL_ADDONDIR}")
    add_dependencies(test-bamboo-indicator bamboo bambooindicator)
    add_test(NAME bamboo-indicator COMMAND "${DBUS_RUN_SESSION_EXECUTABLE}"
        -- $<TARGET_FILE:test-bamboo-indicator>)
    set_tests_properties(bamboo-indicator PROPERTIES TIMEOUT 30)
endif()
