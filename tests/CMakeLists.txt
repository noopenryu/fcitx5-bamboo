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
