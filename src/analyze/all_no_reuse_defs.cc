#include <analyze/all_defs.h>
#include <analyze/all_no_reuse_defs.h>
#include <analyze/deps.h>
#include <pass/make_reduction.h>

namespace ir {

std::vector<std::string>
allNoReuseDefs(const Stmt &_op, const std::unordered_set<AccessType> &atypes) {
    auto op = makeReduction(_op);
    std::unordered_set<std::string> reusing;
    auto found = [&](const Dependency &d) { reusing.insert(d.defId()); };
    findDeps(op, {{}}, found, FindDepsMode::Dep, DEP_WAR, nullptr, true, false);
    std::vector<std::string> ret;
    for (auto &&item : allDefs(op, atypes)) {
        if (!reusing.count(item)) {
            ret.emplace_back(item);
        }
    }
    return ret;
}

} // namespace ir

