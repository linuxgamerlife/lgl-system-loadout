#include "core/protocol.h"

#include <json-glib/json-glib.h>

#include <cstdio>

namespace loadout::protocol {
namespace {

std::string finish(JsonBuilder *builder)
{
    JsonNode *root = json_builder_get_root(builder);
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, root);
    gchar *data = json_generator_to_data(generator, nullptr);
    std::string result = data ? data : "";
    g_free(data);
    json_node_unref(root);
    g_object_unref(generator);
    g_object_unref(builder);
    return result;
}

} // namespace

std::string authRequest(const std::string &nonce)
{
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, "auth");
    json_builder_set_member_name(builder, "nonce");
    json_builder_add_string_value(builder, nonce.c_str());
    json_builder_end_object(builder);
    return finish(builder);
}

std::string executeRequest(const std::string &requestId,
                           const std::vector<std::string> &argv)
{
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, "execute");
    json_builder_set_member_name(builder, "requestId");
    json_builder_add_string_value(builder, requestId.c_str());
    json_builder_set_member_name(builder, "program");
    json_builder_add_string_value(builder, argv.empty() ? "" : argv.front().c_str());
    json_builder_set_member_name(builder, "args");
    json_builder_begin_array(builder);
    for (std::size_t index = 1; index < argv.size(); ++index)
        json_builder_add_string_value(builder, argv[index].c_str());
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    return finish(builder);
}

std::string simpleRequest(const std::string &type)
{
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, type.c_str());
    json_builder_end_object(builder);
    return finish(builder);
}

std::vector<std::string> takeLines(std::string &buffer)
{
    std::vector<std::string> lines;
    std::size_t newline = 0;
    while ((newline = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, newline);
        buffer.erase(0, newline + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty())
            lines.push_back(std::move(line));
    }
    return lines;
}

Response parseResponse(const std::string &line)
{
    Response response;
    JsonParser *parser = json_parser_new();
    GError *error = nullptr;
    if (!json_parser_load_from_data(parser, line.c_str(), static_cast<gssize>(line.size()),
                                    &error)) {
        g_clear_error(&error);
        g_object_unref(parser);
        return response;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        return response;
    }

    JsonObject *object = json_node_get_object(root);
    const std::string type =
        json_object_has_member(object, "type")
            ? json_object_get_string_member_with_default(object, "type", "")
            : "";
    response.requestId = json_object_get_string_member_with_default(object, "requestId", "");
    response.line = json_object_get_string_member_with_default(object, "line", "");
    response.error = json_object_get_string_member_with_default(object, "error", "");
    response.exitCode = static_cast<int>(json_object_get_int_member_with_default(
        object, "exitCode", -1));

    if (type == "started")
        response.kind = Response::Kind::Started;
    else if (type == "output")
        response.kind = Response::Kind::Output;
    else if (type == "finished")
        response.kind = Response::Kind::Finished;
    else if (type == "rejected")
        response.kind = Response::Kind::Rejected;
    else if (type == "error")
        response.kind = Response::Kind::Error;
    else if (type == "cancel_ack")
        response.kind = Response::Kind::CancelAck;
    else if (type == "auth_ok")
        response.kind = Response::Kind::AuthOk;
    else if (type.empty())
        response.kind = Response::Kind::Malformed;
    else
        response.kind = Response::Kind::Other;

    g_object_unref(parser);
    return response;
}

Request parseRequest(const std::string &line)
{
    Request request;
    bool argsOk = true;
    JsonParser *parser = json_parser_new();
    GError *error = nullptr;
    if (!json_parser_load_from_data(parser, line.c_str(), static_cast<gssize>(line.size()),
                                    &error)) {
        g_clear_error(&error);
        g_object_unref(parser);
        return request;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) {
        g_object_unref(parser);
        return request;
    }

    JsonObject *object = json_node_get_object(root);
    request.type = json_object_get_string_member_with_default(object, "type", "");
    request.requestId = json_object_get_string_member_with_default(object, "requestId", "");
    request.nonce = json_object_get_string_member_with_default(object, "nonce", "");
    request.program = json_object_get_string_member_with_default(object, "program", "");

    if (json_object_has_member(object, "args")) {
        JsonNode *argsNode = json_object_get_member(object, "args");
        if (argsNode && JSON_NODE_HOLDS_ARRAY(argsNode)) {
            JsonArray *array = json_node_get_array(argsNode);
            const guint length = json_array_get_length(array);
            for (guint index = 0; index < length; ++index) {
                JsonNode *element = json_array_get_element(array, index);
                if (element && JSON_NODE_HOLDS_VALUE(element) &&
                    json_node_get_value_type(element) == G_TYPE_STRING)
                    request.args.emplace_back(json_node_get_string(element));
                else
                    argsOk = false;
            }
        } else {
            argsOk = false;
        }
    }

    request.valid = argsOk &&
                    (request.type == "execute"   ? !request.program.empty()
                     : request.type == "auth"    ? !request.nonce.empty()
                                                 : !request.type.empty());
    g_object_unref(parser);
    return request;
}

std::string escape(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                char buffer[7];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                              static_cast<unsigned char>(character));
                escaped += buffer;
            } else {
                escaped += character;
            }
            break;
        }
    }
    return escaped;
}

} // namespace loadout::protocol
