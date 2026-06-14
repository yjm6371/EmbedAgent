#include <ea_session_codec.h>

#include <ev_json.h>

namespace embedagent {

namespace {

std::vector<cppev::ext::EvLlmMessage> nonSystemMessages(
    const EaSession& session) {
    std::vector<cppev::ext::EvLlmMessage> out;
    const std::vector<cppev::ext::EvLlmMessage>& msgs = session.messages();
    const std::size_t start =
        (!msgs.empty() && msgs.front().role == "system") ? 1 : 0;
    for (std::size_t i = start; i < msgs.size(); ++i) {
        out.push_back(msgs[i]);
    }
    return out;
}

}  // namespace

bool serializeSession(const EaSession& session, std::string* jsonOut) {
    if (!jsonOut) {
        return false;
    }

    std::string messagesJson;
    if (!cppev::ext::serializeMessagesJson(nonSystemMessages(session),
                                           &messagesJson)) {
        return false;
    }

    cppev::ext::EvJsonBuilder builder;
    builder.beginObject();
    builder.addInt("version", 1);
    builder.addString("systemPrompt", session.systemPrompt());

    cppev::ext::EvJsonBuilder optsBuilder;
    optsBuilder.beginObject();
    optsBuilder.addInt("maxMessages",
                       static_cast<int>(session.options().maxMessages));
    optsBuilder.addInt("maxApproxTokens",
                       static_cast<int>(session.options().maxApproxTokens));
    optsBuilder.endObject();
    std::string optsJson;
    if (!optsBuilder.finish(&optsJson)) {
        return false;
    }
    builder.addRawJson("sessionOptions", optsJson);

    builder.addRawJson("messages", messagesJson);
    builder.endObject();
    return builder.finish(jsonOut);
}

bool loadSessionSnapshot(const std::string& json, EaSessionSnapshot* out) {
    if (!out || json.empty()) {
        return false;
    }

    cppev::ext::EvJsonDocument doc;
    if (!cppev::ext::EvJsonDocument::parse(json, &doc)) {
        return false;
    }

    int version = 0;
    if (!doc.getInt("version", &version) || version != 1) {
        return false;
    }

    if (!doc.getString("systemPrompt", &out->systemPrompt)) {
        out->systemPrompt.clear();
    }

    cppev::ext::EvJsonDocument optsDoc;
    if (doc.getObject("sessionOptions", &optsDoc)) {
        int maxMessages = static_cast<int>(out->opts.maxMessages);
        int maxApproxTokens = static_cast<int>(out->opts.maxApproxTokens);
        optsDoc.getInt("maxMessages", &maxMessages);
        optsDoc.getInt("maxApproxTokens", &maxApproxTokens);
        if (maxMessages > 0) {
            out->opts.maxMessages = static_cast<std::size_t>(maxMessages);
        }
        if (maxApproxTokens > 0) {
            out->opts.maxApproxTokens =
                static_cast<std::size_t>(maxApproxTokens);
        }
    }

    cppev::ext::EvJsonDocument messagesDoc;
    if (!doc.getArray("messages", &messagesDoc)) {
        out->messages.clear();
        return true;
    }

    return cppev::ext::parseMessagesJson(messagesDoc.toString(), &out->messages);
}

bool applySessionSnapshot(const EaSessionSnapshot& snap, EaSession* session) {
    if (!session) {
        return false;
    }

    session->clear();
    session->setOptions(snap.opts);
    session->setSystemPrompt(snap.systemPrompt);
    session->restoreMessages(snap.messages);
    return true;
}

}  // namespace embedagent
