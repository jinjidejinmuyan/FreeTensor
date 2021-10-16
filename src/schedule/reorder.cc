#include <analyze/deps.h>
#include <analyze/hash.h>
#include <pass/make_reduction.h>
#include <schedule/check_loop_order.h>
#include <schedule/reorder.h>

namespace ir {

Stmt SwapFor::visit(const For &_op) {
    if (_op->id() == oldOuter_->id()) {
        insideOuter_ = true;
        auto body = Mutator::visit(_op);
        insideOuter_ = false;
        return makeFor(oldInner_->id(), oldInner_->iter_, oldInner_->begin_,
                       oldInner_->end_, oldInner_->len_, oldInner_->noDeps_,
                       oldInner_->property_, body);
    } else if (_op->id() == oldInner_->id()) {
        insideInner_ = true;
        auto __op = Mutator::visit(_op);
        insideInner_ = false;
        visitedInner_ = true;
        ASSERT(__op->nodeType() == ASTNodeType::For);
        auto op = __op.as<ForNode>();
        return op->body_;
    } else {
        return Mutator::visit(_op);
    }
}

Stmt SwapFor::visit(const StmtSeq &_op) {
    if (insideOuter_) {
        if (insideInner_) {
            return Mutator::visit(_op);
        }

        Stmt before, inner, after;
        std::vector<Stmt> beforeStmts, afterStmts;
        for (auto &&_stmt : _op->stmts_) {
            bool beforeInner = !visitedInner_;
            auto stmt = (*this)(_stmt);
            bool afterInner = visitedInner_;
            bool isInner = beforeInner && afterInner;
            if (isInner) {
                inner = stmt;
            } else if (beforeInner) {
                beforeStmts.emplace_back(stmt);
            } else {
                ASSERT(afterInner);
                afterStmts.emplace_back(stmt);
            }
        }

        if (!beforeStmts.empty()) {
            before =
                makeIf("", makeEQ(makeVar(oldInner_->iter_), oldInner_->begin_),
                       beforeStmts.size() == 1 ? beforeStmts[0]
                                               : makeStmtSeq("", beforeStmts));
        }
        if (!afterStmts.empty()) {
            after =
                makeIf("", makeEQ(makeVar(oldInner_->iter_), oldInner_->begin_),
                       afterStmts.size() == 1 ? afterStmts[0]
                                              : makeStmtSeq("", afterStmts));
        }

        std::vector<Stmt> stmts;
        if (before.isValid()) {
            stmts.emplace_back(before);
        }
        if (inner.isValid()) {
            stmts.emplace_back(inner);
        }
        if (after.isValid()) {
            stmts.emplace_back(after);
        }
        return stmts.size() == 1 ? stmts[0] : makeStmtSeq(_op->id(), stmts);
    } else {
        return Mutator::visit(_op);
    }
}

Stmt reorder(const Stmt &_ast, const std::vector<std::string> &dstOrder) {
    auto ast = makeReduction(_ast);

    CheckLoopOrder checker(dstOrder);
    checker(ast);
    auto curOrder = checker.order();

    std::vector<int> index;
    index.reserve(curOrder.size());
    for (auto &&loop : curOrder) {
        index.emplace_back(
            std::find(dstOrder.begin(), dstOrder.end(), loop->id()) -
            dstOrder.begin());
    }

    // Bubble Sort
    size_t n = index.size();
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j + 1 < n; j++) {
            if (index[j] > index[j + 1]) {
                auto filter = [&](const AccessPoint &later,
                                  const AccessPoint &earlier) {
                    return earlier.cursor_.getParentById(curOrder[j + 1]->id())
                               .isValid() &&
                           later.cursor_.getParentById(curOrder[j + 1]->id())
                               .isValid();
                };
                auto found = [&](const Dependency &d) {
                    ASSERT(d.cond_.size() == 1);
                    std::ostringstream os;
                    os << "Loop " << curOrder[j]->id() << " and "
                       << curOrder[j + 1]->id() << " are not permutable: "
                       << dep2Str(d.cond_[0].first, d.var_, d.later(),
                                  d.earlier());
                    throw InvalidSchedule(os.str());
                };
                findDeps(ast,
                         {{{curOrder[j]->id(), DepDirection::Inv}},
                          {{curOrder[j + 1]->id(), DepDirection::Inv}}},
                         found, FindDepsMode::Dep, DEP_ALL, filter);

                SwapFor swapper(curOrder[j], curOrder[j + 1]);
                ast = swapper(ast);
                std::swap(index[j], index[j + 1]);
                std::swap(curOrder[j], curOrder[j + 1]);
            }
        }
    }

    return ast;
}

} // namespace ir
