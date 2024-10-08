#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <random>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <queue>

void generateFile(const std::string &binFileName, size_t fileSize) {
    std::cout << "Starting file generation: " << binFileName << " of size " << fileSize / (1024 * 1024) << " MB" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    std::ofstream binFile(binFileName, std::ios::binary);
    if (!binFile) {
        std::cerr << "Cannot open file to write." << std::endl;
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

    size_t totalNumbers = fileSize / sizeof(int);
    std::vector<int> buffer(1000000);

    for (size_t i = 0; i < totalNumbers; i += buffer.size()) {
        size_t count = std::min(buffer.size(), totalNumbers - i);
        for (size_t j = 0; j < count; ++j) {
            buffer[j] = dis(gen);
        }
        binFile.write(reinterpret_cast<const char*>(buffer.data()), count * sizeof(int));
        std::cout << "Progress: " << i + count << '/' << totalNumbers << "\r" << std::flush;
    }

    binFile.close();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "\nFile generation completed in " << duration << " ms." << std::endl;
}

std::vector<std::string> splitAndSortFile(const std::string &inputFile, size_t chunkSize) {
    std::ifstream inFile(inputFile, std::ios::binary);
    if (!inFile) {
        std::cerr << "Cannot open input file." << std::endl;
        return {};
    }

    std::vector<std::string> partFiles;
    std::vector<int> buffer;

    size_t partNumber = 0;
    size_t totalParts = 0;

    inFile.seekg(0, std::ios::end);
    size_t totalSize = inFile.tellg();
    inFile.seekg(0, std::ios::beg);
    totalParts = totalSize / (chunkSize * sizeof(int));

    while (inFile) {
        buffer.clear();
        int value;

        while (buffer.size() < chunkSize && inFile.read(reinterpret_cast<char*>(&value), sizeof(value))) {
            buffer.push_back(value);
        }

        if (buffer.empty()) {
            break;
        }

        std::sort(buffer.begin(), buffer.end());

        std::string partFileName = "part_" + std::to_string(partNumber++) + ".bin";
        std::ofstream partFile(partFileName, std::ios::binary);
        for (const auto &val : buffer) {
            partFile.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
        partFile.close();
        partFiles.push_back(partFileName);

        std::cout << "Sorting part " << partNumber << " of " << totalParts << "\r" << std::flush;
    }
    inFile.close();
    std::cout << std::endl;
    return partFiles;
}

void fibonacciDistribute(std::vector<std::string> &partFiles, std::vector<int> &seriesCount) {
    size_t fib1 = 0, fib2 = 1, fib3 = 1;
    size_t index = 0;
    for (const auto &file : partFiles) {
        if (index < seriesCount.size()) {
            seriesCount[index] = fib2;
            fib3 = fib1 + fib2;
            fib1 = fib2;
            fib2 = fib3;
            index++;
        }
    }
}

void mergeFilesFibonacci(std::vector<std::string> &partFiles, const std::string &outputFile) {
    std::vector<std::ifstream> inputs;
    for (const auto &part : partFiles) {
        inputs.emplace_back(part, std::ios::binary);
    }

    std::ofstream outFile(outputFile, std::ios::binary);
    using MinHeapEntry = std::pair<int, size_t>;

    auto compare = [](const MinHeapEntry &a, const MinHeapEntry &b) { return a.first > b.first; };
    std::priority_queue<MinHeapEntry, std::vector<MinHeapEntry>, decltype(compare)> minHeap(compare);

    for (size_t i = 0; i < inputs.size(); ++i) {
        int value;
        if (inputs[i].read(reinterpret_cast<char*>(&value), sizeof(value))) {
            minHeap.emplace(value, i);
        }
    }

    while (!minHeap.empty()) {
        auto [minValue, index] = minHeap.top();
        minHeap.pop();

        outFile.write(reinterpret_cast<const char*>(&minValue), sizeof(minValue));

        int nextValue;
        if (inputs[index].read(reinterpret_cast<char*>(&nextValue), sizeof(nextValue))) {
            minHeap.emplace(nextValue, index);
        }
    }

    for (auto &input : inputs) {
        input.close();
    }
    outFile.close();
}

void sortFileFibonacci(const std::string &inputFile, const std::string &outputFile, size_t chunkSize) {
    std::cout << "Starting Fibonacci-based sorting..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::string> partFiles = splitAndSortFile(inputFile, chunkSize);

    std::vector<int> seriesCount(partFiles.size(), 0);
    fibonacciDistribute(partFiles, seriesCount);

    mergeFilesFibonacci(partFiles, outputFile);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Fibonacci-based sorting completed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms." << std::endl;

    std::cout << "Sorting completed. Output saved to " << outputFile << std::endl;

    char choice;
    std::cout << "Do you want to create a TXT file to verify the sorting? (y/n): ";
    std::cin >> choice;

    if (choice == 'y' || choice == 'Y') {
        int n;
        std::cout << "Enter the number of elements to write to the TXT file (or 0 for all): ";
        std::cin >> n;

        std::ifstream sortedFile(outputFile, std::ios::binary);
        std::ofstream txtOutputFile("sorted_output.txt");
        if (!sortedFile || !txtOutputFile) {
            std::cerr << "Error opening files for verification." << std::endl;
            return;
        }

        int value;
        int count = 0;
        while (sortedFile.read(reinterpret_cast<char*>(&value), sizeof(value))) {
            if (n > 0 && count >= n) {
                break;
            }
            txtOutputFile << value << std::endl;
            count++;
        }

        sortedFile.close();
        txtOutputFile.close();
        std::cout << "TXT file created as 'sorted_output.txt'." << std::endl;
    }
}

void validateInput(size_t &fileSize, size_t &memoryLimit, std::string &outputFile) {
    if (fileSize <= 0) {
        throw std::invalid_argument("File size must be greater than 0.");
    }
    if (memoryLimit <= 0) {
        throw std::invalid_argument("Memory limit must be greater than 0.");
    }
    if (outputFile.empty()) {
        throw std::invalid_argument("Output file name cannot be empty.");
    }
}

int main(int argc, char *argv[]) {
    try {
        size_t fileSize = 0;
        size_t memoryLimit = 0;
        std::string outputFile;

        if (argc == 7 && std::strcmp(argv[1], "-s") == 0 && std::strcmp(argv[3], "-m") == 0 && std::strcmp(argv[5], "-o") == 0) {
            fileSize = std::stoul(argv[2]) * 1024 * 1024;
            memoryLimit = std::stoul(argv[4]) * 1024 * 1024;
            outputFile = argv[6];
        } else {
            std::cout << "Enter file size (MB): ";
            size_t fileSizeMB;
            std::cin >> fileSizeMB;
            fileSize = fileSizeMB * 1024 * 1024;

            std::cout << "Enter memory limit (MB): ";
            size_t memoryLimitMB;
            std::cin >> memoryLimitMB;
            memoryLimit = memoryLimitMB * 1024 * 1024;

            std::cout << "Enter output file name: ";
            std::cin >> outputFile;
        }

        validateInput(fileSize, memoryLimit, outputFile);

        std::string binFileName = "input_file.bin";
        std::string txtFileName = "input_file.txt";
        size_t chunkSize = memoryLimit / sizeof(int);
        generateFile(binFileName, fileSize);
        sortFileFibonacci(binFileName, outputFile, chunkSize);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
