#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string to_upper_ident(const std::string& stem) {
    std::string out;
    for (unsigned char c : stem) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::toupper(c)));
        else out.push_back('_');
    }
    return out;
}

int main(int argc, char** argv) {
    fs::path payloads_dir = (argc > 1) ? argv[1] : "src/payloads";
    fs::path out_file = (argc > 2) ? argv[2] : "src/cpp/payloads_gen.hpp";

    if (!fs::exists(payloads_dir)) {
        std::cerr << "Missing payloads dir: " << payloads_dir << "\n";
        return 1;
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(payloads_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".luau") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    fs::create_directories(out_file.parent_path());
    std::ofstream out(out_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "Cannot write: " << out_file << "\n";
        return 1;
    }

    out << "#pragma once\n\nnamespace realplus::payloads {\n\n";
    for (const auto& p : files) {
        std::ifstream in(p, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string ident = "LUAU_" + to_upper_ident(p.stem().string());

        out << "inline constexpr char " << ident << "_BYTES[] = {";
        for (size_t i = 0; i < data.size(); ++i) {
            if (i % 16 == 0) out << "\n    ";
            out << "static_cast<char>(0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned int>(static_cast<unsigned char>(data[i])) << "), ";
        }
        out << "\n    0x00\n};\n";
        out << "inline constexpr const char* " << ident << " = " << ident << "_BYTES;\n\n";
    }
    out << "} // namespace realplus::payloads\n";
    std::cout << std::dec << "Embedded " << files.size() << " payloads into " << out_file.string() << "\n";
    return 0;
}
