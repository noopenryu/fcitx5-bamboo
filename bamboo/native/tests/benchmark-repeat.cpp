/* SPDX-License-Identifier: MIT */
#include "adapter.h"
#include <chrono>
#include <iostream>

int main() {
    using namespace bamboo;
    std::cout << "method,auto_restore,key,count,total_ms\n";
    for (const auto &method : {"Telex", "Telex 2", "VNI"}) {
        for (bool restore : {false, true}) {
            for (auto key : {'k', 'a'}) {
                for (int count : {32, 64, 128, 256, 512, 1024}) {
                    Adapter engine(
                        ParseInputMethod(inputMethodDefinitions(), method), {},
                        {});
                    Options options;
                    options.autoNonVnRestore = restore;
                    engine.setOptions(options);
                    auto start = std::chrono::steady_clock::now();
                    for (int i = 0; i < count; ++i)
                        engine.processKey(key, 0);
                    auto elapsed = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
                    std::cout << method << ',' << restore << ',' << key << ','
                              << count << ',' << elapsed << std::endl;
                }
            }
        }
    }
}
