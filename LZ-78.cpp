#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <iterator>
#include <cstdlib>

using namespace std;

// ============================================================
// TOKEN
// ============================================================

struct Token {
    uint32_t index;
    unsigned char character;
    bool hasCharacter;
    bool reset;
};

// ============================================================
// COMPRESSION RESULT
// ============================================================

struct CompressionResult {
    vector<Token> tokens;
    size_t peakDictionarySize;
    double compressionTime;
};

// ============================================================
// BIT WRITER
// ============================================================

class BitWriter {

private:
    ofstream& file;

    uint8_t buffer = 0;
    int bitsInBuffer = 0;

public:

    BitWriter(ofstream& output)
        : file(output) {}

    void writeBit(bool bit) {

        buffer <<= 1;

        if (bit)
            buffer |= 1;

        bitsInBuffer++;

        if (bitsInBuffer == 8) {

            file.put(
                static_cast<char>(buffer)
            );

            buffer = 0;
            bitsInBuffer = 0;
        }
    }

    void writeBits(
        uint32_t value,
        int numberOfBits
    ) {

        for (
            int i = numberOfBits - 1;
            i >= 0;
            --i
        ) {

            bool bit =
                ((value >> i) & 1) != 0;

            writeBit(bit);
        }
    }

    void flush() {

        if (bitsInBuffer > 0) {

            buffer <<=
                (8 - bitsInBuffer);

            file.put(
                static_cast<char>(buffer)
            );

            buffer = 0;
            bitsInBuffer = 0;
        }
    }
};

// ============================================================
// BIT READER
// ============================================================

class BitReader {

private:
    ifstream& file;

    uint8_t buffer = 0;
    int bitsRemaining = 0;

public:

    BitReader(ifstream& input)
        : file(input) {}

    bool readBit(bool& bit) {

        if (bitsRemaining == 0) {

            int value = file.get();

            if (value == EOF)
                return false;

            buffer =
                static_cast<uint8_t>(value);

            bitsRemaining = 8;
        }

        bit =
            ((buffer >>
              (bitsRemaining - 1)) & 1) != 0;

        bitsRemaining--;

        return true;
    }

    bool readBits(
        int numberOfBits,
        uint32_t& value
    ) {

        value = 0;

        for (
            int i = 0;
            i < numberOfBits;
            ++i
        ) {

            bool bit;

            if (!readBit(bit))
                return false;

            value <<= 1;

            if (bit)
                value |= 1;
        }

        return true;
    }
};

// ============================================================
// BITS NEEDED
// ============================================================

int bitsNeeded(uint32_t value) {

    if (value <= 1)
        return 1;

    int bits = 0;

    while (value > 0) {

        bits++;

        value >>= 1;
    }

    return bits;
}

// ============================================================
// LZ-78 COMPRESSION
// ============================================================

CompressionResult compressLZ78(
    const string& input,
    size_t maxDictionarySize
) {

    unordered_map<string, uint32_t> dictionary;

    vector<Token> tokens;

    string current;

    uint32_t nextIndex = 1;

    size_t peakDictionarySize = 0;

    auto start =
        chrono::high_resolution_clock::now();

    for (unsigned char c : input) {

        string candidate =
            current + static_cast<char>(c);

        auto found =
            dictionary.find(candidate);

        // ----------------------------------------------------
        // PATTERN EXISTS
        // ----------------------------------------------------

        if (found != dictionary.end()) {

            current = candidate;

            continue;
        }

        // ----------------------------------------------------
        // NEW PATTERN
        // ----------------------------------------------------

        uint32_t prefixIndex = 0;

        if (!current.empty()) {

            auto prefix =
                dictionary.find(current);

            if (prefix != dictionary.end()) {

                prefixIndex =
                    prefix->second;
            }
        }

        tokens.push_back({
            prefixIndex,
            c,
            true,
            false
        });

        // ----------------------------------------------------
        // ADD TO DICTIONARY
        // ----------------------------------------------------

        if (
            maxDictionarySize == 0 ||
            dictionary.size() < maxDictionarySize
        ) {

            dictionary[candidate] =
                nextIndex++;

            peakDictionarySize =
                max(
                    peakDictionarySize,
                    dictionary.size()
                );
        }

        else {

            // ------------------------------------------------
            // RESET DICTIONARY
            // ------------------------------------------------

            tokens.push_back({
                0,
                0,
                false,
                true
            });

            dictionary.clear();

            nextIndex = 1;
        }

        current.clear();
    }

    // ========================================================
    // REMAINING PATTERN
    // ========================================================

    if (!current.empty()) {

        auto found =
            dictionary.find(current);

        if (found != dictionary.end()) {

            tokens.push_back({
                found->second,
                0,
                false,
                false
            });
        }
    }

    auto end =
        chrono::high_resolution_clock::now();

    double elapsed =
        chrono::duration<double>(
            end - start
        ).count();

    return {
        tokens,
        peakDictionarySize,
        elapsed
    };
}

// ============================================================
// WRITE COMPRESSED FILE
// ============================================================

bool writeCompressedFile(
    const string& filename,
    const vector<Token>& tokens
) {

    ofstream file(
        filename,
        ios::binary
    );

    if (!file)
        return false;

    uint64_t tokenCount =
        static_cast<uint64_t>(
            tokens.size()
        );

    file.write(
        reinterpret_cast<const char*>(
            &tokenCount
        ),
        sizeof(tokenCount)
    );

    BitWriter writer(file);

    size_t dictionarySize = 0;

    for (const Token& token : tokens) {

        // RESET

        if (token.reset) {

            writer.writeBit(true);

            dictionarySize = 0;

            continue;
        }

        // NORMAL TOKEN

        writer.writeBit(false);

        int indexBits =
            bitsNeeded(
                static_cast<uint32_t>(
                    dictionarySize + 1
                )
            );

        writer.writeBits(
            token.index,
            indexBits
        );

        writer.writeBit(
            token.hasCharacter
        );

        if (token.hasCharacter) {

            writer.writeBits(
                token.character,
                8
            );

            dictionarySize++;
        }
    }

    writer.flush();

    return file.good();
}

// ============================================================
// DECOMPRESSION
// ============================================================

string decompressLZ78(
    const string& filename
) {

    ifstream file(
        filename,
        ios::binary
    );

    if (!file)
        return "";

    uint64_t tokenCount = 0;

    file.read(
        reinterpret_cast<char*>(
            &tokenCount
        ),
        sizeof(tokenCount)
    );

    if (!file)
        return "";

    BitReader reader(file);

    vector<string> dictionary;

    dictionary.push_back("");

    string output;

    for (
        uint64_t i = 0;
        i < tokenCount;
        ++i
    ) {

        bool reset;

        if (!reader.readBit(reset))
            return "";

        // ----------------------------------------------------
        // RESET
        // ----------------------------------------------------

        if (reset) {

            dictionary.clear();

            dictionary.push_back("");

            continue;
        }

        // ----------------------------------------------------
        // INDEX
        // ----------------------------------------------------

        size_t dictionarySize =
            dictionary.size() - 1;

        int indexBits =
            bitsNeeded(
                static_cast<uint32_t>(
                    dictionarySize + 1
                )
            );

        uint32_t index;

        if (
            !reader.readBits(
                indexBits,
                index
            )
        )
            return "";

        // ----------------------------------------------------
        // CHARACTER FLAG
        // ----------------------------------------------------

        bool hasCharacter;

        if (
            !reader.readBit(
                hasCharacter
            )
        )
            return "";

        unsigned char character = 0;

        if (hasCharacter) {

            uint32_t value;

            if (
                !reader.readBits(
                    8,
                    value
                )
            )
                return "";

            character =
                static_cast<unsigned char>(
                    value
                );
        }

        // ----------------------------------------------------
        // VALIDATE INDEX
        // ----------------------------------------------------

        if (
            index >= dictionary.size()
        )
            return "";

        // ----------------------------------------------------
        // RECONSTRUCT
        // ----------------------------------------------------

        string entry =
            dictionary[index];

        if (hasCharacter) {

            entry +=
                static_cast<char>(
                    character
                );
        }

        output += entry;

        // ----------------------------------------------------
        // ADD DICTIONARY ENTRY
        // ----------------------------------------------------

        if (hasCharacter) {

            dictionary.push_back(entry);
        }
    }

    return output;
}

// ============================================================
// FILE SIZE
// ============================================================

long long getFileSize(
    const string& filename
) {

    ifstream file(
        filename,
        ios::binary
    );

    if (!file)
        return -1;

    file.seekg(
        0,
        ios::end
    );

    return static_cast<long long>(
        file.tellg()
    );
}

// ============================================================
// COPY FILE
// ============================================================

bool copyFile(
    const string& source,
    const string& destination
) {

    ifstream input(
        source,
        ios::binary
    );

    ofstream output(
        destination,
        ios::binary
    );

    if (!input || !output)
        return false;

    output << input.rdbuf();

    return output.good();
}

// ============================================================
// COMPRESSION RATIO
// ============================================================

double compressionRatio(
    size_t original,
    long long compressed
) {

    if (
        original == 0 ||
        compressed <= 0
    )
        return 0.0;

    return static_cast<double>(original)
           / compressed;
}

// ============================================================
// SPACE SAVING
// ============================================================

double spaceSaving(
    size_t original,
    long long compressed
) {

    if (original == 0)
        return 0.0;

    return (
        1.0 -
        static_cast<double>(compressed)
        / original
    ) * 100.0;
}

// ============================================================
// CREATE PYTHON GRAPH PROGRAM
// ============================================================

bool createGraphScript() {

    ofstream pythonFile(
        "generate_graphs.py"
    );

    if (!pythonFile)
        return false;

    pythonFile << R"PY(
import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv("results.csv")


# ============================================================
# GRAPH 1
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["CompressedSize"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Compressed File Size (Bytes)")
plt.title("Compressed File Size vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph1_Compressed_File_Size.png",
    dpi=300
)

plt.close()


# ============================================================
# GRAPH 2
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["SpaceSaving"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Space Saving (%)")
plt.title("Space Saving vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph2_Space_Saving.png",
    dpi=300
)

plt.close()


# ============================================================
# GRAPH 3
# ============================================================

plt.figure(figsize=(8, 5))

plt.plot(
    data["DictionaryLimit"],
    data["CompressionTime"],
    marker="o"
)

plt.xlabel("Dictionary Limit (Entries)")
plt.ylabel("Compression Time (seconds)")
plt.title("Compression Time vs Dictionary Limit")

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "Graph3_Compression_Time.png",
    dpi=300
)

plt.close()


print()
print("===============================================")
print("             GRAPHS GENERATED")
print("===============================================")
print()
print("1. Graph1_Compressed_File_Size.png")
print("2. Graph2_Space_Saving.png")
print("3. Graph3_Compression_Time.png")
print()
)PY";

    pythonFile.close();

    return true;
}

// ============================================================
// MAIN
// ============================================================

int main() {

    // ========================================================
    // INPUT FILE
    // ========================================================

    const string inputFilename =
        "input.txt";

    // ========================================================
    // READ INPUT
    // ========================================================

    ifstream inputFile(
        inputFilename,
        ios::binary
    );

    if (!inputFile) {

        cerr
            << "ERROR: Cannot open input.txt\n";

        return 1;
    }

    string input(
        (
            istreambuf_iterator<char>(
                inputFile
            )
        ),
        istreambuf_iterator<char>()
    );

    inputFile.close();

    if (input.empty()) {

        cerr
            << "ERROR: input.txt is empty.\n";

        return 1;
    }

    // ========================================================
    // HEADER
    // ========================================================

    cout
        << "\n===============================================\n";

    cout
        << "             LZ-78 COMPRESSION\n";

    cout
        << "===============================================\n\n";

    cout
        << "Input file       : "
        << inputFilename
        << "\n";

    cout
        << "Original size    : "
        << input.size()
        << " bytes\n";

    // ========================================================
    // DICTIONARY LIMITS
    // ========================================================

    vector<size_t> limits = {
        100,
        500,
        1000,
        5000,
        10000
    };

    // ========================================================
    // EXPERIMENT STRUCTURE
    // ========================================================

    struct Experiment {

        size_t limit;

        long long compressedSize;

        size_t dictionarySize;

        double time;

        double ratio;

        double saving;

        bool correct;

        string filename;
    };

    vector<Experiment> results;

    // ========================================================
    // RUN EXPERIMENTS
    // ========================================================

    cout
        << "\n===============================================\n";

    cout
        << "             DICTIONARY EXPERIMENT\n";

    cout
        << "===============================================\n\n";

    cout
        << left
        << setw(10) << "Limit"
        << setw(16) << "Compressed"
        << setw(16) << "Dictionary"
        << setw(14) << "Time"
        << setw(12) << "Ratio"
        << setw(14) << "Saving"
        << "Status\n";

    cout
        << string(96, '-')
        << "\n";

    for (size_t limit : limits) {

        // ----------------------------------------------------
        // COMPRESS
        // ----------------------------------------------------

        CompressionResult result =
            compressLZ78(
                input,
                limit
            );

        string filename =
            "lz78_limit_" +
            to_string(limit) +
            ".bin";

        // ----------------------------------------------------
        // WRITE
        // ----------------------------------------------------

        if (
            !writeCompressedFile(
                filename,
                result.tokens
            )
        ) {

            cerr
                << "ERROR writing "
                << filename
                << "\n";

            continue;
        }

        // ----------------------------------------------------
        // SIZE
        // ----------------------------------------------------

        long long compressedSize =
            getFileSize(filename);

        // ----------------------------------------------------
        // DECOMPRESS
        // ----------------------------------------------------

        string decompressed =
            decompressLZ78(filename);

        bool correct =
            (decompressed == input);

        // ----------------------------------------------------
        // STATISTICS
        // ----------------------------------------------------

        double ratio =
            compressionRatio(
                input.size(),
                compressedSize
            );

        double saving =
            spaceSaving(
                input.size(),
                compressedSize
            );

        // ----------------------------------------------------
        // SAVE RESULT
        // ----------------------------------------------------

        results.push_back({

            limit,

            compressedSize,

            result.peakDictionarySize,

            result.compressionTime,

            ratio,

            saving,

            correct,

            filename
        });

        // ----------------------------------------------------
        // PRINT
        // ----------------------------------------------------

        cout
            << left
            << setw(10)
            << limit

            << setw(16)
            << compressedSize

            << setw(16)
            << result.peakDictionarySize

            << setw(14)
            << fixed
            << setprecision(6)
            << result.compressionTime

            << setw(12)
            << fixed
            << setprecision(3)
            << ratio

            << setw(14)
            << fixed
            << setprecision(2)
            << saving;

        if (correct)
            cout << "OK";
        else
            cout << "ERROR";

        cout << "\n";
    }

    // ========================================================
    // SAVE CSV
    // ========================================================

    ofstream csv(
        "results.csv"
    );

    if (!csv) {

        cerr
            << "ERROR: Cannot create results.csv\n";

        return 1;
    }

    csv
        << "DictionaryLimit,"
        << "OriginalSize,"
        << "CompressedSize,"
        << "DictionarySize,"
        << "CompressionTime,"
        << "CompressionRatio,"
        << "SpaceSaving,"
        << "Decompression\n";

    for (
        const Experiment& result :
        results
    ) {

        csv
            << result.limit << ","
            << input.size() << ","
            << result.compressedSize << ","
            << result.dictionarySize << ","
            << fixed
            << setprecision(9)
            << result.time << ","
            << fixed
            << setprecision(6)
            << result.ratio << ","
            << fixed
            << setprecision(6)
            << result.saving << ","
            << (
                result.correct
                ? "SUCCESS"
                : "FAILED"
            )
            << "\n";
    }

    csv.close();

    cout
        << "\nResults saved to results.csv\n";

    // ========================================================
    // FIND BEST RESULT
    // ========================================================

    long long bestSize =
        input.size();

    size_t bestLimit = 0;

    string bestFile;

    bool foundCompression = false;

    for (
        const Experiment& result :
        results
    ) {

        if (
            result.correct &&
            result.compressedSize <
            bestSize
        ) {

            bestSize =
                result.compressedSize;

            bestLimit =
                result.limit;

            bestFile =
                result.filename;

            foundCompression = true;
        }
    }

    // ========================================================
    // FINAL OUTPUT
    // ========================================================

    const string finalFile =
        "final_lz78_output.bin";

    cout
        << "\n===============================================\n";

    cout
        << "                 FINAL RESULT\n";

    cout
        << "===============================================\n\n";

    if (foundCompression) {

    if (!copyFile(bestFile, finalFile)) {
    cerr << "ERROR: Could not create final output file.\n";
    return 1;
}

string finalDecompressed = decompressLZ78(finalFile);

if (finalDecompressed != input) {
    cerr << "ERROR: Final decompression verification failed.\n";
    return 1;
}

cout << "Final decompression verification: PASS\n";

        cout
            << "Compression successful!\n\n";

        cout
            << "Best dictionary limit : "
            << bestLimit
            << "\n";

        cout
            << "Original size         : "
            << input.size()
            << " bytes\n";

        cout
            << "Final size            : "
            << bestSize
            << " bytes\n";

        cout
            << "Space saving          : "
            << fixed
            << setprecision(2)
            << spaceSaving(
                input.size(),
                bestSize
            )
            << "%\n";

        cout
            << "Compression ratio     : "
            << fixed
            << setprecision(3)
            << compressionRatio(
                input.size(),
                bestSize
            )
            << "\n";

    } else {

        copyFile(
            inputFilename,
            finalFile
        );

        cout
            << "LZ-78 did not reduce the file size.\n";

        cout
            << "Original file stored instead.\n";
    }

    cout
        << "\nFinal output : "
        << finalFile
        << "\n";

    // ========================================================
    // CREATE GRAPH SCRIPT
    // ========================================================

    cout
        << "\nCreating graph generator...\n";

    if (!createGraphScript()) {

        cerr
            << "ERROR: Could not create graph script.\n";

        return 1;
    }

    // ========================================================
    // RUN PYTHON
    // ========================================================

    cout
        << "Generating graphs...\n\n";

    int status =
        system(
            "python3 generate_graphs.py"
        );

    // If python3 doesn't work, try python.
    if (status != 0) {

        status =
            system(
                "python generate_graphs.py"
            );
    }

    // ========================================================
    // RESULT
    // ========================================================

    if (status == 0) {

        cout
            << "\n===============================================\n";

        cout
            << "             ALL TASKS COMPLETE\n";

        cout
            << "===============================================\n\n";

        cout
            << "Created files:\n\n";

        cout
            << "results.csv\n";

        cout
            << "Graph1_Compressed_File_Size.png\n";

        cout
            << "Graph2_Space_Saving.png\n";

        cout
            << "Graph3_Compression_Time.png\n";

        cout
            << "final_lz78_output.bin\n";

    } else {

        cout
            << "\nGraph generation failed.\n";

        cout
            << "Make sure Python, pandas and matplotlib are installed.\n";

        cout
            << "\nInstall them using:\n";

        cout
            << "pip3 install pandas matplotlib\n";
    }

    cout
        << "\n===============================================\n";

    cout
        << "             EXPERIMENT COMPLETED\n";

    cout
        << "===============================================\n";

    return 0;
}
