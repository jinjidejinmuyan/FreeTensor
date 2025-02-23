#include <sstream>

#include <analyze/deps.h>
#include <schedule.h>
#include <schedule/check_loop_order.h>
#include <schedule/reorder.h>

namespace freetensor {

std::vector<FindDepsDir> notLexLessAfterPermu(const std::vector<For> &outers,
                                              const std::vector<ID> &permu) {
    // Not lexicographically less <==> when all outer loops are the same, there
    // is no such dependence that the out-most non-'=' carrying loop is not a
    // '>'
    std::vector<FindDepsDir> direction;
    for (size_t i = 0, n = permu.size(); i < n; i++) {
        FindDepsDir dir;
        for (auto &&loop : outers) {
            dir.emplace_back(loop->id(), DepDirection::Same);
        }
        for (size_t j = 0; j < i; j++) {
            dir.emplace_back(permu[j], DepDirection::Same);
        }
        dir.emplace_back(permu[i], DepDirection::Inv);
        direction.emplace_back(std::move(dir));
    }
    return direction;
}

Expr RenameIter::visit(const Var &_op) {
    auto __op = Mutator::visit(_op);
    ASSERT(__op->nodeType() == ASTNodeType::Var);
    auto op = __op.as<VarNode>();
    if (op->name_ == oldName_) {
        ASSERT(!newName_.empty());
        op->name_ = newName_;
    }
    return op;
}

Stmt RenameIter::visit(const For &_op) {
    if (_op->iter_ == oldName_) {
        newName_ = oldName_ + "." + toString(_op->id());
        auto __op = Mutator::visit(_op);
        ASSERT(__op->nodeType() == ASTNodeType::For);
        auto op = __op.as<ForNode>();
        op->iter_ = newName_;
        newName_.clear();
        return op;
    } else {
        return Mutator::visit(_op);
    }
}

Stmt Reorder::visit(const For &_op) {
    if (_op->id() == oldOuter_->id()) {
        insideOuter_ = true;
        auto body = Mutator::visit(_op);
        insideOuter_ = false;
        return makeFor(oldInner_->iter_, oldInner_->begin_, oldInner_->end_,
                       oldInner_->step_, oldInner_->len_, oldInner_->property_,
                       body, oldInner_->metadata(), oldInner_->id());
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

Stmt Reorder::visit(const StmtSeq &_op) {
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
            before = beforeStmts.size() == 1 ? beforeStmts[0]
                                             : makeStmtSeq(beforeStmts);
        }
        if (!afterStmts.empty()) {
            after = afterStmts.size() == 1 ? afterStmts[0]
                                           : makeStmtSeq(afterStmts);
        }
        if (inner.isValid()) {
            // Otherwise, some outer visit(StmtSeq) will add the guards for us

            if (before.isValid()) {
                if (oldInner_->property_->parallel_ != serialScope) {
                    throw InvalidSchedule(
                        "Imperfect nesting is not allowed when "
                        "the inner loop is parallelized");
                }
                before =
                    makeIf(makeEQ(makeVar(oldInner_->iter_), oldInner_->begin_),
                           RenameIter{oldInner_->iter_}(before));
            }
            if (after.isValid()) {
                if (oldInner_->property_->parallel_ != serialScope) {
                    throw InvalidSchedule(
                        "Imperfect nesting is not allowed when "
                        "the inner loop is parallelized");
                }
                after =
                    makeIf(makeEQ(makeVar(oldInner_->iter_), oldInner_->begin_),
                           RenameIter{oldInner_->iter_}(after));
            }
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
        return stmts.size() == 1
                   ? stmts[0]
                   : makeStmtSeq(stmts, _op->metadata(), _op->id());
    } else {
        return Mutator::visit(_op);
    }
}

Stmt reorder(const Stmt &_ast, const std::vector<ID> &dstOrder) {
    auto ast = _ast;

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

    // A reorder is leagal if and only if:
    // 1. when all the outer loops are in the same iteration,
    // 2. after transformation, for each dependence pair, `earlier` is still
    // earlier (lexicographically less) than `later`.
    std::vector<ID> dstLoopAndStmtSeqOrder;
    for (auto &&seq : checker.stmtSeqInBetween()) {
        dstLoopAndStmtSeqOrder.emplace_back(seq->id());
    }
    dstLoopAndStmtSeqOrder.insert(dstLoopAndStmtSeqOrder.end(),
                                  dstOrder.begin(), dstOrder.end());
    FindDeps()
        .direction(
            notLexLessAfterPermu(checker.outerLoops(), dstLoopAndStmtSeqOrder))
        .filterSubAST(curOrder.front()->id())(ast, [&](const Dependency &d) {
            throw InvalidSchedule("Loops are not permutable: " + toString(d) +
                                  " cannot be resolved");
        });

    // Bubble Sort
    size_t n = index.size();
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j + 1 < n; j++) {
            if (index[j] > index[j + 1]) {
                Reorder mutator(curOrder[j], curOrder[j + 1]);
                ast = mutator(ast);
                std::swap(index[j], index[j + 1]);
                std::swap(curOrder[j], curOrder[j + 1]);
            }
        }
    }

    return ast;
}

void Schedule::reorder(const std::vector<ID> &order) {
    beginTransaction();
    auto log =
        appendLog(MAKE_SCHEDULE_LOG(Reorder, freetensor::reorder, order));
    try {
        applyLog(log);
        commitTransaction();
    } catch (const InvalidSchedule &e) {
        abortTransaction();
        throw InvalidSchedule(log, ast(), e.what());
    }
}

} // namespace freetensor
