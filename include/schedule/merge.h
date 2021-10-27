#ifndef MERGE_H
#define MERGE_H

#include <mutator.h>

namespace ir {

/**
 * Merge two directly nested loops
 */
class MergeFor : public Mutator {
    For oldOuter_, oldInner_;
    Expr outerLen_, innerLen_;

    std::string newIter_, newId_;

    bool insideOuter_ = false, insideInner_ = false;
    bool visitedInner_ = false;
    bool innerNoDeps_ = false;

    std::vector<VarDef> intermediateDefs_; // from inner to outer

  public:
    MergeFor(const For oldOuter, const For &oldInner)
        : oldOuter_(oldOuter), oldInner_(oldInner), outerLen_(oldOuter_->len_),
          innerLen_(oldInner_->len_),
          newIter_("m." + oldOuter_->iter_ + "." + oldInner_->iter_),
          newId_("merged." + oldOuter_->id() + "." + oldInner_->id()) {}

    const std::string &newIter() const { return newIter_; }
    const std::string &newId() const { return newId_; }

  protected:
    Stmt visit(const For &op) override;
    Stmt visit(const StmtSeq &op) override;
    Expr visit(const Var &op) override;
    Stmt visit(const VarDef &op) override;
};

std::pair<Stmt, std::string> merge(const Stmt &ast, const std::string &loop1,
                                   const std::string &loop2);

} // namespace ir

#endif // MERGE_H
