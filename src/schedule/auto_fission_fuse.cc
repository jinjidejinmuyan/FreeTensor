#include <analyze/deps.h>
#include <schedule.h>

namespace freetensor {

void Schedule::autoFissionFuse(const Target &target,
                               const Ref<RandTrace> &trace) {
    RandCondStack conds;

    // Random decision on whether to fission or fuse:
    //
    // - Decision = 0: not to fuse, to fission
    // - Decision = 1: to fuse, not to fission
    //
    // Fusing (or not fissioning) may reduce parallelizing opportunities, which
    // is related to dependences on the two loops being fused (or the two
    // fissioned loops):
    //
    // - If neither loop has dependence: It doesn't matter
    // - If both loops have dependence: It doesn't matter, too
    // - If exactly one of the loop: It may have negative influence on
    // parallelizing
    //
    // Therefore, we add "the two loops having different
    // dependences" as a condition of our decision
    auto decisionId = PROGRAM_POSITION;
    auto decisionName = "fuse";
    auto depDiffCondName = "defDiff";

    // Record which loop is fission from which loop, so we don't fuse them back
    // to one
    //
    // We only care about the last fission. E.g., if we fission `L1 L2` to `L1
    // {L3 L4}` and then to `L1 {{L5 L6}, L4}`, we won't fuse L5 and L6 back,
    // but we can fuse L6 and L4
    std::unordered_map<ID, ID> fissionFrom;

    // Try to fission a loop into consecutive loops. Only fission at
    // beginnings of each sub-loops, and at each leaf nodes (Store, ReduceTo,
    // Eval)
    std::function<void(For nest)> tryFission = [&, this](For nest) {
        // Recurse first
        for (auto &&subNest :
             findAll("<For><-(!<For><-)*#" + toString(nest->id()))) {
            tryFission(subNest.as<ForNode>());
        }

        // Try fission
        auto thisId = nest->id();
        int partCnt = 0;
        auto splitters =
            findAll("(<For>|<Store>|<ReduceTo>|<Eval>)<-(!<For><-)*#" +
                    toString(nest->id()));
        for (auto &&[i, splitter] : views::enumerate(splitters)) {
            if (i == 0) {
                continue;
            }
            bool frontHasDep =
                FindDeps()
                    .direction({{{thisId, DepDirection::Different}}})
                    .filterSubAST(thisId)
                    .filterAccess([&](const AccessPoint &ap) {
                        return ap.stmt_->isBefore(splitter);
                    })
                    .exists(ast());
            bool backHasDep =
                FindDeps()
                    .direction({{{thisId, DepDirection::Different}}})
                    .filterSubAST(thisId)
                    .filterAccess([&](const AccessPoint &ap) {
                        return !ap.stmt_->isBefore(splitter);
                    })
                    .exists(ast());
            bool depDiff = frontHasDep != backHasDep;
            {
                RandCondGuard _(conds, depDiffCondName, depDiff);
                if (!randCtx_->decide(decisionId, decisionName, conds,
                                      {depDiff ? 1 : 0, depDiff ? 0 : 1}, trace,
                                      "not fission " + toString(thisId) +
                                          " before " +
                                          toString(splitter->id()) + "?")) {
                    beginTransaction();
                    try {
                        auto newId =
                            fission(thisId, FissionSide::Before, splitter->id(),
                                    "." + toString(partCnt), "")
                                .first.at(thisId);
                        fissionFrom[newId] = thisId;
                        partCnt++;
                        commitTransaction();
                    } catch (const InvalidSchedule &e) {
                        abortTransaction();
                    }
                }
            }
        }
        fissionFrom[thisId] = thisId;
    };
    for (auto &&loop : findAll("<For><-(!<For><-)*-|")) {
        // Suppose the root node is not <For>. It should be <VarDef>
        tryFission(loop.as<ForNode>());
    }

    // Try to fuse each pair of consecutive loops, unless they are just
    // fissioned from the same loop
    std::function<void(Stmt)> tryFuse = [&, this](Stmt root) {
        For last;
        ID lastId;
        for (auto &&_loop :
             findAll("<For><-(!<For><-)*#" + toString(root->id()))) {
            auto loop = _loop.as<ForNode>();
            auto loopId = loop->id();
            if (!last.isValid()) {
                goto skip;
            }
            if (fissionFrom.count(loopId) && fissionFrom.count(lastId) &&
                fissionFrom.at(loopId) == fissionFrom.at(lastId)) {
                goto skip;
            }
            {
                bool thisHasDep =
                    FindDeps()
                        .direction({{{loopId, DepDirection::Different}}})
                        .filterSubAST(loopId)
                        .exists(ast());
                bool lastHasDep =
                    FindDeps()
                        .direction({{{lastId, DepDirection::Different}}})
                        .filterSubAST(lastId)
                        .exists(ast());
                bool depDiff = thisHasDep != lastHasDep;
                {
                    RandCondGuard _(conds, depDiffCondName, depDiff);
                    if (randCtx_->decide(decisionId, decisionName, conds,
                                         {depDiff ? 1 : 0, depDiff ? 0 : 1},
                                         trace,
                                         "fuse " + toString(lastId) + " and " +
                                             toString(loopId) + "?")) {
                        beginTransaction();
                        try {
                            try {
                                lastId =
                                    moveTo(lastId, MoveToSide::Before, loopId)
                                        .first;
                            } catch (const InvalidSchedule &e) {
                                loopId =
                                    moveTo(loopId, MoveToSide::After, lastId)
                                        .first;
                            }
                            loopId = fuse(lastId, loopId, true);
                            loop = find(loopId).as<ForNode>();
                            commitTransaction();
                        } catch (const InvalidSchedule &e) {
                            abortTransaction();
                            tryFuse(last);
                        }
                    }
                }
            }
        skip:
            lastId = loopId, last = loop;
        }
        if (last.isValid()) {
            tryFuse(last);
        }
    };
    tryFuse(ast());
}

} // namespace freetensor
