# The production engine is entirely C++; legacy Go files are reference only.
add_subdirectory(native)
add_library(Bamboo::Core ALIAS bamboo-native)
