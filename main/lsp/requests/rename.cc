#include "main/lsp/requests/rename.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "core/lsp/QueryResponse.h"
#include "main/lsp/ShowOperation.h"
#include "main/lsp/json_types.h"
#include "main/lsp/lsp.h"
#include <stdio.h>
using namespace std;

namespace sorbet::realmain::lsp {

namespace {
bool isValidRenameLocation(const core::SymbolRef &symbol, const core::GlobalState &gs,
                           unique_ptr<ResponseMessage> &response) {
    auto locs = symbol.data(gs)->locs();
    string filetype;
    for (auto loc : locs) {
        if (loc.file().data(gs).isRBI()) {
            filetype = ".rbi";
        } else if (loc.file().data(gs).isPayload()) {
            filetype = "payload";
        }

        if (!filetype.empty()) {
            auto error =
                fmt::format("Renaming constants defined in {} files is not supported; symbol {} is defined at {}",
                            filetype, symbol.data(gs)->name.show(gs), loc.filePosToString(gs));
            response->error = make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest, error);
            return false;
        }
    }
    return true;
}

// Checks if s is a subclass of root, and updates isSubclass, visited, and subclasses vectors.
bool isSubclass(const core::GlobalState &gs, core::SymbolRef root, core::SymbolRef s, vector<bool> &memoized,
                vector<bool> &visited) {
    // don't visit the same class twice
    if (visited[s.classOrModuleIndex()] == true) {
        return memoized[s.classOrModuleIndex()];
    }
    visited[s.classOrModuleIndex()] = true;

    auto super = s.data(gs)->superClass();
    if (super.exists()) {
        memoized[s.classOrModuleIndex()] = isSubclass(gs, root, super, memoized, visited);
    } else {
        memoized[s.classOrModuleIndex()] = false;
    }
    return memoized[s.classOrModuleIndex()];
}

// Returns all subclasses of root (including root)
vector<core::SymbolRef> getSubclasses(LSPTypecheckerDelegate &typechecker, core::SymbolRef root) {
    const core::GlobalState &gs = typechecker.state();
    vector<bool> memoized(gs.classAndModulesUsed());
    vector<bool> visited(gs.classAndModulesUsed());
    memoized[root.classOrModuleIndex()] = true;
    visited[root.classOrModuleIndex()] = true;

    vector<core::SymbolRef> subclasses;
    for (u4 i = 1; i < gs.classAndModulesUsed(); ++i) {
        auto s = core::SymbolRef(&gs, core::SymbolRef::Kind::ClassOrModule, i);
        if (isSubclass(gs, root, s, memoized, visited)) {
            subclasses.emplace_back(s);
        }
    }
    return subclasses;
}

// Follow superClass links until we find the highest class that contains the given method. In other words we find the
// "root" of the tree of classes that define a method.
core::SymbolRef findRootClassWithMethod(const core::GlobalState &gs, core::SymbolRef klass, core::NameRef methodName) {
    auto root = klass;
    ENFORCE(klass.data(gs)->isClassOrModule());
    while (true) {
        auto tmp = root.data(gs)->superClass();
        ENFORCE(tmp.exists()); // everything derives from Kernel::Object so we can't ever reach the actual top type
        if (!tmp.exists() || !(tmp.data(gs)->findMember(gs, methodName).exists())) {
            break;
        }
        root = tmp;
    }
    return root;
}

class Renamer {
public:
    Renamer(const core::GlobalState &gs, const LSPConfiguration &config, const string oldName, const string newName)
        : gs(gs), config(config), oldName(oldName), newName(newName), invalid(false) {}
    virtual ~Renamer() = default;
    virtual void rename(unique_ptr<core::lsp::QueryResponse> &response) = 0;
    variant<JSONNullObject, unique_ptr<WorkspaceEdit>> buildEdit();

    bool getInvalid() {
        return invalid;
    }
    string getError() {
        return error;
    }

protected:
    const core::GlobalState &gs;
    const LSPConfiguration &config;
    string oldName;
    string newName;
    UnorderedMap<string, vector<unique_ptr<TextEdit>>> edits;
    bool invalid;
    string error;
};

variant<JSONNullObject, unique_ptr<WorkspaceEdit>> Renamer::buildEdit() {
    if (invalid) {
        return JSONNullObject();
    }

    auto we = make_unique<WorkspaceEdit>();
    vector<unique_ptr<TextDocumentEdit>> textDocEdits;
    for (auto &item : edits) {
        // TODO: Version.
        textDocEdits.push_back(make_unique<TextDocumentEdit>(
            make_unique<VersionedTextDocumentIdentifier>(item.first, JSONNullObject()), move(item.second)));
    }
    we->documentChanges = move(textDocEdits);
    return we;
}

class MethodRenamer : public Renamer {
public:
    MethodRenamer(const core::GlobalState &gs, const LSPConfiguration &config, const string oldName,
                  const string newName)
        : Renamer(gs, config, oldName, newName) {
        if (oldName == "initialize") {
            invalid = true;
        }
    }
    ~MethodRenamer() {}
    void rename(unique_ptr<core::lsp::QueryResponse> &response) override {
        if (invalid) {
            return;
        }
        auto loc = response->getLoc();
        auto source = loc.source(gs);
        auto location = config.loc2Location(gs, loc);
        if (location == nullptr) {
            return;
        }
        string newsrc;
        if (auto sendResp = response->isSend()) {
            auto methodNameLoc = sendResp->getMethodNameLoc(gs);
            if (!methodNameLoc) {
                ENFORCE(0, "Method name not found in send expression")
                return;
            }
            string::size_type methodNameOffset = methodNameLoc->beginPos() - sendResp->termLoc.beginPos();
            newsrc = replaceAt(source, methodNameOffset);
        } else if (auto defResp = response->isDefinition()) {
            newsrc = replaceMethodNameInDef(source);
        } else {
            ENFORCE(0, "Unexpected query response type while renaming method");
            return;
        }
        edits[location->uri].push_back(make_unique<TextEdit>(move(location->range), newsrc));
    }

private:
    string replaceMethodNameInDef(string def) {
        const vector<string> unsupportedDefPrefixes{"attr_reader", "attr_accessor", "attr_writer"};
        for (auto u : unsupportedDefPrefixes) {
            if (absl::StartsWith(def, u)) {
                invalid = true; // ensures the entire rename operation is blocked, not just this particular location
                error = fmt::format("Sorbet does not support renaming `{}`s", u);
                return def;
            }
        }

        // We can't do a simple string search because of cases like:
        //   `def foo(foobar)`
        //   `def de`
        //   `attr_reader :att`
        // Since we don't have more precise info about where the name is, we do this:
        //   1. if the definition starts with "def" (or another supported prefix) then skip over that
        //   2. and then change the first instance of oldName to newName
        string::size_type offset = 0;
        const vector<string> prefixes{"def"};
        for (auto prefix : prefixes) {
            if (absl::StartsWith(def, prefix)) {
                offset = prefix.length();
                break;
            }
        }
        auto pos = def.find(oldName, offset);
        if (pos == string::npos) {
            ENFORCE(0, "Method name should appear in definition");
            return def;
        }
        return replaceAt(def, pos);
    }
    string replaceAt(string_view input, string::size_type pos) {
        auto suffixOffset = pos + oldName.length();
        return absl::StrCat(input.substr(0, pos), newName, input.substr(suffixOffset, input.length() - suffixOffset));
    }
};
class ConstRenamer : public Renamer {
public:
    ConstRenamer(const core::GlobalState &gs, const LSPConfiguration &config, const string oldName,
                 const string newName)
        : Renamer(gs, config, oldName, newName) {}
    ~ConstRenamer() {}
    void rename(unique_ptr<core::lsp::QueryResponse> &response) override {
        auto loc = response->getLoc();
        auto source = loc.source(gs);
        auto location = config.loc2Location(gs, loc);
        if (location == nullptr) {
            return;
        }
        vector<string> strs = absl::StrSplit(source, "::");
        strs[strs.size() - 1] = string(newName);
        auto newsrc = absl::StrJoin(strs, "::");
        edits[location->uri].push_back(make_unique<TextEdit>(move(location->range), newsrc));
    }
};

} // namespace

void RenameTask::getRenameEdits(LSPTypecheckerDelegate &typechecker, core::SymbolRef symbol, string newName,
                                unique_ptr<ResponseMessage> &responseMsg) {
    const core::GlobalState &gs = typechecker.state();
    auto symbolData = symbol.data(gs);
    auto originalName = symbolData->name.show(gs);
    unique_ptr<Renamer> renamer;

    vector<core::SymbolRef> symbolsToRename;
    if (symbolData->isMethod()) {
        renamer = make_unique<MethodRenamer>(gs, config, originalName, newName);
        // We have to check for methods as part of a class hierarchy: Follow superClass() links till we find the root;
        // then find the full tree; then look for methods with the same name as ours; then find all references to all
        // those methods and rename them.
        auto symbolClass = symbolData->enclosingClass(gs);

        // We have to be careful to follow superclass links only as long as we find a method that `symbol` overrides.
        // Otherwise we will find unrelated methods and rename them even though they don't need to be (see the
        // method_class_hierarchy test case for an example).
        auto root = findRootClassWithMethod(gs, symbolClass, symbolData->name);

        auto subclasses = getSubclasses(typechecker, root);

        // find the target method definition in each subclass
        for (auto c : subclasses) {
            auto classSymbol = c.data(gs);
            auto member = classSymbol->findMember(gs, symbolData->name);
            if (member.exists()) {
                symbolsToRename.push_back(member);
            }
        }
    } else {
        renamer = make_unique<ConstRenamer>(gs, config, originalName, newName);
        symbolsToRename.push_back(symbol);
    }

    for (auto sym : symbolsToRename) {
        auto queryResult = queryBySymbol(typechecker, sym);
        if (queryResult.error) {
            responseMsg->result = JSONNullObject();
            return;
        }

        // Filter for untyped files, and deduplicate responses by location.  We don't use extractLocations here because
        // in some cases like sends, we need the SendResponse to be able to accurately find the method name in the
        // expression.
        for (auto &response : filterAndDedup(gs, queryResult.responses)) {
            auto loc = response->getLoc();
            if (loc.file().data(gs).isPayload()) {
                // We don't support renaming things in payload files.
                responseMsg->result = JSONNullObject();
                return;
            }
            renamer->rename(response);
        }
    }
    responseMsg->result = renamer->buildEdit();
    if (renamer->getInvalid()) {
        responseMsg->error = make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest, renamer->getError());
    }
}

RenameTask::RenameTask(const LSPConfiguration &config, MessageId id, unique_ptr<RenameParams> params)
    : LSPRequestTask(config, move(id), LSPMethod::TextDocumentRename), params(move(params)) {}

unique_ptr<ResponseMessage> RenameTask::runRequest(LSPTypecheckerDelegate &typechecker) {
    const core::GlobalState &gs = typechecker.state();

    auto response = make_unique<ResponseMessage>("2.0", id, LSPMethod::TextDocumentRename);
    if (!config.opts.lspRenameEnabled) {
        response->error = make_unique<ResponseError>(
            (int)LSPErrorCodes::InvalidRequest, "The `Rename` LSP feature is experimental and disabled by default.");
        return response;
    }

    prodCategoryCounterInc("lsp.messages.processed", "textDocument.rename");

    if (params->newName.empty()) {
        response->error =
            make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest, "No new name provided for rename request.");
        return response;
    }

    ShowOperation op(config, ShowOperation::Kind::Rename);

    auto result = queryByLoc(typechecker, params->textDocument->uri, *params->position, LSPMethod::TextDocumentRename);
    if (result.error) {
        // An error happened while setting up the query.
        response->error = move(result.error);
        return response;
    }

    // An explicit null indicates that we don't support this request (or that nothing was at the location).
    // Note: Need to correctly type variant here so it goes into right 'slot' of result variant.
    response->result = variant<JSONNullObject, unique_ptr<WorkspaceEdit>>(JSONNullObject());
    auto &queryResponses = result.responses;
    if (queryResponses.empty()) {
        return response;
    }

    auto resp = move(queryResponses[0]);
    if (auto constResp = resp->isConstant()) {
        // Sanity check the text.
        if (islower(params->newName[0])) {
            response->error = make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest,
                                                         "Constant names must begin with an uppercase letter.");
            return response;
        }
        if (isValidRenameLocation(constResp->symbol, gs, response)) {
            getRenameEdits(typechecker, constResp->symbol, params->newName, response);
        }
    } else if (auto defResp = resp->isDefinition()) {
        if (defResp->symbol.data(gs)->isClassOrModule() && islower(params->newName[0])) {
            response->error = make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest,
                                                         "Class and Module names must begin with an uppercase letter.");
            return response;
        }

        if (defResp->symbol.data(gs)->isMethod() && isupper(params->newName[0])) {
            response->error = make_unique<ResponseError>((int)LSPErrorCodes::InvalidRequest,
                                                         "Method names must begin with an lowercase letter.");
            return response;
        }

        if (defResp->symbol.data(gs)->isClassOrModule() || defResp->symbol.data(gs)->isMethod()) {
            if (isValidRenameLocation(defResp->symbol, gs, response)) {
                getRenameEdits(typechecker, defResp->symbol, params->newName, response);
            }
        }
    } else if (auto sendResp = resp->isSend()) {
        auto method = sendResp->dispatchResult->main.method;
        getRenameEdits(typechecker, method, params->newName, response);
    }

    return response;
}

} // namespace sorbet::realmain::lsp
