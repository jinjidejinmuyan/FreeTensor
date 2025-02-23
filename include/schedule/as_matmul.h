#ifndef FREE_TENSOR_MAKE_MATMUL_H
#define FREE_TENSOR_MAKE_MATMUL_H

#include <unordered_map>
#include <unordered_set>

#include <analyze/analyze_linear.h>
#include <analyze/check_all_defined.h>
#include <analyze/symbol_table.h>
#include <container_utils.h>
#include <hash.h>
#include <mutator.h>

namespace freetensor {

class AsMatMul : public SymbolTable<Mutator> {
    typedef SymbolTable<Mutator> BaseClass;

    ID loop_;

    int nestCnt_ = 0;
    std::unordered_map<std::string, int> iterMap_; // iter var -> nest cnt
    std::unordered_set<std::string> outerDefs_;
    std::vector<VarDef> innerDefs_;
    std::vector<int> orderInit_;

    bool foundInit_ = false, foundLeaf_ = false, inside_ = false;
    Expr a_, b_, c_, initC_, m_, k_, n_, lda_, stridea_, ldb_, strideb_, ldc_,
        stridec_, batchSize_;
    bool aIsRowMajor_, bIsRowMajor_, cIsRowMajor_;

    AnalyzeLinear analyzeLinear_;

  public:
    AsMatMul(const ID &loop) : loop_(loop) {}

  private:
    const LinearExpr<int64_t> &analyzeLinear(const Expr &expr);

    template <class T>
    std::tuple<std::vector<bool>, std::vector<int>, Expr>
    findIterUsedAndBaseAddr(const T &acc) {
        std::vector<bool> usedBy(nestCnt_, false);
        std::vector<int> order;
        Expr baseAddr = makeLoad(acc->var_, acc->indices_,
                                 buffer(acc->var_)->tensor()->dtype());
        for (auto &&[idx, dimLen, baseIdx] :
             views::zip(acc->indices_, buffer(acc->var_)->tensor()->shape(),
                        baseAddr.as<LoadNode>()->indices_)) {
            auto &&lin = analyzeLinear(idx);
            if (lin.coeff_.size() != 1 ||
                std::abs(lin.coeff_.front().k_) != 1 ||
                lin.coeff_.front().a_->nodeType() != ASTNodeType::Var) {
                if (!checkAllDefined(outerDefs_, idx)) {
                    throw InvalidSchedule("Indices of " + acc->var_ +
                                          " should be plain loop iterators");
                }
                continue; // not a dim in matmul
            }
            Var var = lin.coeff_.front().a_.template as<VarNode>();
            if (!iterMap_.count(var->name_)) {
                continue; // not a dim in matmul
            } else {
                baseIdx = makeIntConst(0);
            }
            int loopLevel = iterMap_.at(var->name_);
            if (!HashComparator()(loop(var->name_)->len_, dimLen)) {
                throw InvalidSchedule(
                    "Iterator " + var->name_ + " of " + acc->var_ +
                    " should loop over the entire range (" + toString(dimLen) +
                    "), instead of " + toString(loop(var->name_)->len_));
            }
            usedBy[loopLevel] = true;
            order.emplace_back(loopLevel);
        }
        return std::make_tuple(usedBy, order, baseAddr);
    }

    template <class T>
    std::pair<Expr, Expr> findLenAndStride(const T &acc,
                                           const std::vector<bool> &flag) {
        Expr len, stride;
        bool thisDimIn = false, lastDimIn = false;
        Expr lastInDim;
        for (auto &&[idx, dimLen] :
             views::zip(acc->indices_, buffer(acc->var_)->tensor()->shape())) {
            auto &&lin = analyzeLinear(idx);
            lastDimIn = thisDimIn;
            thisDimIn = true;
            if (lin.coeff_.size() != 1 ||
                std::abs(lin.coeff_.front().k_) != 1 ||
                lin.coeff_.front().a_->nodeType() != ASTNodeType::Var) {
                thisDimIn = false;
            } else {
                Var var = lin.coeff_.front().a_.template as<VarNode>();
                if (!iterMap_.count(var->name_) ||
                    !flag[iterMap_.at(var->name_)]) {
                    thisDimIn = false;
                }
            }
            if (thisDimIn) {
                if (lastInDim.isValid()) {
                    if (!lastDimIn) {
                        throw InvalidSchedule(
                            "Dimensions " + toString(lastInDim) + " and " +
                            toString(idx) + " should be contiguous");
                    }
                }
                len = len.isValid() ? makeMul(len, dimLen) : (Expr)dimLen;
                lastInDim = idx;
            } else {
                if (len.isValid()) {
                    stride = stride.isValid() ? makeMul(stride, dimLen)
                                              : (Expr)dimLen;
                }
            }
        }
        len = len.isValid() ? len : makeIntConst(1);
        stride = stride.isValid() ? stride : makeIntConst(1);
        return std::make_pair(len, stride);
    }

  protected:
    Stmt visitStmt(const Stmt &op) override;
    Stmt visit(const For &op) override;
    Stmt visit(const ReduceTo &op) override;
    Stmt visit(const Store &op) override;
    Stmt visit(const VarDef &op) override;
};

Stmt asMatMul(const Stmt &ast, const ID &loop);

} // namespace freetensor

#endif // FREE_TENSOR_MAKE_MATMUL_H
