#pragma once

#include "Common/OperationResult.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Loads malware signature strings from a text file.
// Changing the file does not require recompiling the program.
class SignatureManager {
public:
    explicit SignatureManager(std::string file_path);

    OperationResult load();

    const std::vector<std::string>& getSignatures() const;
    std::size_t count() const;
    std::size_t maxSignatureLength() const;
    std::int64_t lastModified() const;

private:
    static std::string trim(const std::string& text);
    static bool isValidSignature(const std::string& line);

    std::string file_path_;
    std::vector<std::string> signatures_;
    std::size_t max_signature_length_ = 0;
    std::int64_t last_modified_ = 0;
};
