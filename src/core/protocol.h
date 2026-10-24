#pragma once

#include <string>
#include <vector>

namespace loadout::protocol {

// One newline-delimited message from the helper.
struct Response final {
    enum class Kind {
        Started,
        Output,
        Finished,
        Rejected,
        Error,
        CancelAck,
        AuthOk,
        Malformed,
        Other
    };
    Kind kind{Kind::Malformed};
    std::string requestId;
    std::string line;
    std::string error;
    int exitCode{-1};
};

// {"type":"auth","nonce":...}
std::string authRequest(const std::string &nonce);

// {"type":"execute","requestId":...,"program":argv[0],"args":argv[1..]}
std::string executeRequest(const std::string &requestId, const std::vector<std::string> &argv);

// {"type":"shutdown"} or {"type":"cancel"}
std::string simpleRequest(const std::string &type);

// A client-to-helper message.
struct Request final {
    std::string type;
    std::string requestId;
    std::string nonce;
    std::string program;
    std::vector<std::string> args;
    bool valid{false};
};

Request parseRequest(const std::string &line);

// Escapes quotes, backslashes, and control characters for a JSON string body.
std::string escape(const std::string &value);

// Complete lines, with the trailing newline removed and empty lines dropped.
// The remainder stays in the buffer.
std::vector<std::string> takeLines(std::string &buffer);

Response parseResponse(const std::string &line);

} // namespace loadout::protocol
