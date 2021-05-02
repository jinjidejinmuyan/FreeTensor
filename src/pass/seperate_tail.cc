#include <algorithm>

#include <analyze/check_all_defined.h>
#include <analyze/hash.h>
#include <pass/seperate_tail.h>
#include <pass/z3_simplify.h>

namespace ir {

static ASTNodeType reverseCmp(ASTNodeType type) {
    switch (type) {
    case ASTNodeType::LT:
        return ASTNodeType::GT;
    case ASTNodeType::LE:
        return ASTNodeType::GE;
    case ASTNodeType::GT:
        return ASTNodeType::LT;
    case ASTNodeType::GE:
        return ASTNodeType::LE;
    default:
        ASSERT(false);
    }
}

void FindAllIfs::visit(const If &op) {
    Visitor::visit(op);
    results_.insert(op->id());
}

Stmt AppendIDs::visitStmt(const Stmt &op,
                          const std::function<Stmt(const Stmt &)> &visitNode) {
    auto ret = Mutator::visitStmt(op, visitNode);
    ret->setId(op->id() + suffix_);
    return ret;
}

void SeperateTail::genSeperation(
    uint64_t iterHash, const Expr &cond,
    const std::function<void(const Expr &)> &callback) {
    auto type = cond->nodeType();

    Expr norm;
    switch (type) {
    case ASTNodeType::LAnd:
        genSeperation(iterHash, cond.as<LAndNode>()->lhs_, callback);
        genSeperation(iterHash, cond.as<LAndNode>()->rhs_, callback);
        return;
    case ASTNodeType::LOr:
        genSeperation(iterHash, cond.as<LOrNode>()->lhs_, callback);
        genSeperation(iterHash, cond.as<LOrNode>()->rhs_, callback);
        return;
    case ASTNodeType::LNot:
        genSeperation(iterHash, cond.as<LNotNode>()->expr_, callback);
        return;
    case ASTNodeType::LT:
        norm = makeSub(cond.as<LTNode>()->lhs_, cond.as<LTNode>()->rhs_);
        break;
    case ASTNodeType::LE:
        norm = makeSub(cond.as<LENode>()->lhs_, cond.as<LENode>()->rhs_);
        break;
    case ASTNodeType::GT:
        norm = makeSub(cond.as<GTNode>()->lhs_, cond.as<GTNode>()->rhs_);
        break;
    case ASTNodeType::GE:
        norm = makeSub(cond.as<GENode>()->lhs_, cond.as<GENode>()->rhs_);
        break;
    case ASTNodeType::EQ:
        norm = makeSub(cond.as<EQNode>()->lhs_, cond.as<EQNode>()->rhs_);
        break;
    case ASTNodeType::NE:
        norm = makeSub(cond.as<NENode>()->lhs_, cond.as<NENode>()->rhs_);
        break;
    default:
        return;
    }
    ASSERT(norm.isValid());
    analyzeLinear_(norm);
    if (!analyzeLinear_.result().count(norm)) {
        return;
    }
    LinearExpr lin = analyzeLinear_.result().at(norm);

    auto it =
        std::find_if(lin.coeff_.begin(), lin.coeff_.end(),
                     [iterHash](const decltype(lin.coeff_)::value_type &kx) {
                         return kx.first == iterHash;
                     });
    if (it == lin.coeff_.end()) {
        return;
    }
    auto selfK = it->second.k_;
    if (selfK < 0) {
        type = reverseCmp(type);
        selfK *= -1;
        lin.bias_ *= -1;
        for (auto &item : lin.coeff_) {
            item.second.k_ *= -1;
        }
    }

    Expr seperation = makeIntConst(-lin.bias_);
    for (auto &&item : lin.coeff_) {
        if (item.first != iterHash) {
            seperation =
                makeAdd(seperation,
                        makeMul(makeIntConst(-item.second.k_), item.second.a_));
        }
    }
    switch (type) {
    case ASTNodeType::LT:
    case ASTNodeType::GE:
        callback(makeCeilDiv(seperation, makeIntConst(selfK)));
        break;
    case ASTNodeType::LE:
    case ASTNodeType::GT:
        callback(makeAdd(makeFloorDiv(seperation, makeIntConst(selfK)),
                         makeIntConst(1)));
        break;
    case ASTNodeType::EQ:
    case ASTNodeType::NE:
        callback(makeCeilDiv(seperation, makeIntConst(selfK)));
        callback(makeAdd(makeFloorDiv(seperation, makeIntConst(selfK)),
                         makeIntConst(1)));
        break;
    default:
        ASSERT(false);
    }
}

Stmt SeperateTail::visit(const If &op) {
    if (candidates_.count(op->id())) {
        for (auto &item : ifStack_) {
            item.emplace_back(op); // Use the old one
        }
    }
    auto ret = Mutator::visit(op);
    if (hasVarDef_.count(op->thenCase_) ||
        (op->elseCase_.isValid() && hasVarDef_.count(op->elseCase_))) {
        hasVarDef_.insert(ret);
    }
    return ret;
}

Stmt SeperateTail::visit(const For &_op) {
    def_.insert(_op->iter_);
    ifStack_.emplace_back();

    auto __op = Mutator::visit(_op);
    ASSERT(__op->nodeType() == ASTNodeType::For);
    auto op = __op.as<ForNode>();

    if (hasVarDef_.count(op->body_)) {
        hasVarDef_.insert(op);
        return op;
    }

    std::vector<If> ifList;
    for (auto &&branch : ifStack_.back()) {
        if (checkAllDefined(def_, branch->cond_)) {
            ifList.emplace_back(branch);
        }
    }

    def_.erase(_op->iter_);
    ifStack_.pop_back();

    if (!op->parallel_.empty()) {
        return op;
    }

    auto iterHash = getHash(makeVar(op->iter_));
    std::unordered_map<uint64_t, Expr> sepSet;
    for (auto &&branch : ifList) {
        genSeperation(iterHash, branch->cond_,
                      [&](const Expr &sep) { sepSet[getHash(sep)] = sep; });
    }

    std::vector<Expr> seperations;
    seperations.reserve(sepSet.size());
    for (auto &&item : sepSet) {
        seperations.emplace_back(item.second);
    }
    std::function<Stmt(size_t, const For &)> dfs =
        [&seperations, &dfs, this](size_t i, const For &old) -> Stmt {
        if (i == seperations.size()) {
            return old;
        }
        auto &&sep = seperations[i];
        auto front = makeFor(old->id(), old->iter_, old->begin_, sep,
                             makeSub(sep, old->begin_), old->parallel_,
                             old->unroll_, old->body_);
        auto back = makeFor(old->id(), old->iter_, sep, old->end_,
                            makeSub(old->end_, sep), old->parallel_,
                            old->unroll_, old->body_);
        front = dfs(i + 1, AppendIDs(".front")(front).as<ForNode>());
        back = dfs(i + 1, AppendIDs(".back")(back).as<ForNode>());
        auto seperated = makeStmtSeq("", {front, back});
        auto ret = makeIf(
            "", makeLAnd(makeGE(sep, old->begin_), makeLE(sep, old->end_)),
            seperated, dfs(i + 1, old));
        nextCandidates_.insert(ret->id());
        return ret;
    };
    return dfs(0, op);
}

Stmt SeperateTail::visit(const VarDef &op) {
    def_.insert(op->name_);
    auto ret = Mutator::visit(op);
    def_.erase(op->name_);
    hasVarDef_.insert(ret);
    return ret;
}

Stmt SeperateTail::visit(const StmtSeq &_op) {
    auto __op = Mutator::visit(_op);
    ASSERT(__op->nodeType() == ASTNodeType::StmtSeq);
    auto op = __op.as<StmtSeqNode>();
    for (auto &&stmt : op->stmts_) {
        if (hasVarDef_.count(stmt)) {
            hasVarDef_.insert(op);
            break;
        }
    }
    return op;
}

Stmt SeperateTail::visit(const Assert &_op) {
    auto __op = Mutator::visit(_op);
    ASSERT(__op->nodeType() == ASTNodeType::Assert);
    auto op = __op.as<AssertNode>();
    if (hasVarDef_.count(op->body_)) {
        hasVarDef_.insert(op);
    }
    return op;
}

Stmt seperateTail(const Stmt &_op) {
    auto op = _op;

    FindAllIfs finder;
    finder(op);
    auto candidates = finder.results();

    while (!candidates.empty()) {
        SeperateTail mutator(candidates);
        op = mutator(op);
        op = z3Simplify(op); // Although Z3 may be slow, if we don't use Z3
                             // here, there will be too many redundant branches,
                             // which will make each pass even slower
        candidates = mutator.nextCandidates();
    }

    return op;
}

} // namespace ir
