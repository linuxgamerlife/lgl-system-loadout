#include "core/protocol.h"

#include <glib.h>

#include <string>

using namespace loadout;

namespace {

void buildsExecuteRequests()
{
    const auto json = protocol::executeRequest("7", {"/usr/bin/dnf", "-y", "install", "steam"});
    g_assert_nonnull(strstr(json.c_str(), "\"type\":\"execute\""));
    g_assert_nonnull(strstr(json.c_str(), "\"requestId\":\"7\""));
    g_assert_nonnull(strstr(json.c_str(), "\"program\":\"/usr/bin/dnf\""));
    g_assert_nonnull(strstr(json.c_str(), "\"args\":[\"-y\",\"install\",\"steam\"]"));

    const auto request = protocol::parseRequest(json);
    g_assert_true(request.valid);
    g_assert_true(request.type == "execute");
    g_assert_true(request.requestId == "7");
    g_assert_true(request.program == "/usr/bin/dnf");
    g_assert_cmpuint(request.args.size(), ==, 3);
    g_assert_true(request.args[2] == "steam");
}

void parsesAuth()
{
    const auto json = protocol::authRequest("0123456789abcdef0123456789abcdef");
    g_assert_nonnull(strstr(json.c_str(), "\"type\":\"auth\""));
    g_assert_nonnull(strstr(json.c_str(), "\"nonce\":\"0123456789abcdef0123456789abcdef\""));

    const auto request = protocol::parseRequest(json);
    g_assert_true(request.valid);
    g_assert_true(request.type == "auth");
    g_assert_true(request.nonce == "0123456789abcdef0123456789abcdef");

    const auto response = protocol::parseResponse("{\"type\":\"auth_ok\"}");
    g_assert_true(response.kind == protocol::Response::Kind::AuthOk);

    g_assert_false(protocol::parseRequest("{\"type\":\"auth\"}").valid);
}

void parsesResponses()
{
    auto response = protocol::parseResponse(
        "{\"type\":\"output\",\"requestId\":\"1\",\"line\":\"Installing\"}");
    g_assert_true(response.kind == protocol::Response::Kind::Output);
    g_assert_true(response.line == "Installing");

    response = protocol::parseResponse("{\"type\":\"finished\",\"requestId\":\"1\",\"exitCode\":0}");
    g_assert_true(response.kind == protocol::Response::Kind::Finished);
    g_assert_cmpint(response.exitCode, ==, 0);

    response = protocol::parseResponse("{\"type\":\"rejected\",\"error\":\"nope\"}");
    g_assert_true(response.kind == protocol::Response::Kind::Rejected);
    g_assert_true(response.error == "nope");

    response = protocol::parseResponse("not json");
    g_assert_true(response.kind == protocol::Response::Kind::Malformed);

    response = protocol::parseResponse("{\"type\":\"finished\"}");
    g_assert_cmpint(response.exitCode, ==, -1);
}

void framesLines()
{
    std::string buffer = "one\ntwo\r\n\nthree";
    const auto lines = protocol::takeLines(buffer);
    g_assert_cmpuint(lines.size(), ==, 2);
    g_assert_true(lines[0] == "one");
    g_assert_true(lines[1] == "two");
    g_assert_true(buffer == "three");

    buffer += "\n";
    const auto rest = protocol::takeLines(buffer);
    g_assert_cmpuint(rest.size(), ==, 1);
    g_assert_true(rest[0] == "three");
}

void escapesOutputLines()
{
    const auto escaped = protocol::escape("quote \" and \\ and \n and \t");
    g_assert_nonnull(strstr(escaped.c_str(), "\\\""));
    g_assert_nonnull(strstr(escaped.c_str(), "\\\\"));
    g_assert_nonnull(strstr(escaped.c_str(), "\\n"));
    g_assert_nonnull(strstr(escaped.c_str(), "\\t"));
}

void parsesShutdownAndCancel()
{
    for (const char *type : {"shutdown", "cancel"}) {
        const auto request = protocol::parseRequest(protocol::simpleRequest(type));
        g_assert_true(request.valid);
        g_assert_true(request.type == type);
    }
}

void rejectsMalformedRequests()
{
    g_assert_false(protocol::parseRequest("{").valid);
    g_assert_false(protocol::parseRequest("{\"type\":\"execute\"}").valid);
    g_assert_false(protocol::parseRequest("{\"type\":\"execute\",\"program\":\"/bin/sh\","
                                          "\"args\":[1]}").valid);
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/protocol/execute", buildsExecuteRequests);
    g_test_add_func("/protocol/auth", parsesAuth);
    g_test_add_func("/protocol/responses", parsesResponses);
    g_test_add_func("/protocol/framing", framesLines);
    g_test_add_func("/protocol/escape", escapesOutputLines);
    g_test_add_func("/protocol/simple", parsesShutdownAndCancel);
    g_test_add_func("/protocol/malformed", rejectsMalformedRequests);
    return g_test_run();
}
