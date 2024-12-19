#include <fstream>
#include <iostream>
#include <filesystem>

int main() {
    const std::string fileName = "../saved-position.txt";

    if (!std::filesystem::exists(fileName)) {
        std::cerr << "File " << fileName << " not found.\n";
        return 1;
    }

    std::ofstream file(fileName, std::ios::trunc);
    if (file.is_open()) {
        std::cout << "File " << fileName << " has been successfully cleared.\n";
    } else {
        std::cerr << "Can't open file " << fileName << std::endl;
        return 1;
    }

    return 0;
}
