#include <except.h>
#include <mutator.h>

namespace freetensor {

Expr Mutator::visitExpr(const Expr &op) {
    switch (op->nodeType()) {

#define DISPATCH_EXPR_CASE(name)                                               \
    case ASTNodeType::name:                                                    \
        return visit(op.as<name##Node>());

        DISPATCH_EXPR_CASE(Var);
        DISPATCH_EXPR_CASE(Load);
        DISPATCH_EXPR_CASE(IntConst);
        DISPATCH_EXPR_CASE(FloatConst);
        DISPATCH_EXPR_CASE(BoolConst);
        DISPATCH_EXPR_CASE(Add);
        DISPATCH_EXPR_CASE(Sub);
        DISPATCH_EXPR_CASE(Mul);
        DISPATCH_EXPR_CASE(RealDiv);
        DISPATCH_EXPR_CASE(FloorDiv);
        DISPATCH_EXPR_CASE(CeilDiv);
        DISPATCH_EXPR_CASE(RoundTowards0Div);
        DISPATCH_EXPR_CASE(Mod);
        DISPATCH_EXPR_CASE(Remainder);
        DISPATCH_EXPR_CASE(Min);
        DISPATCH_EXPR_CASE(Max);
        DISPATCH_EXPR_CASE(LT);
        DISPATCH_EXPR_CASE(LE);
        DISPATCH_EXPR_CASE(GT);
        DISPATCH_EXPR_CASE(GE);
        DISPATCH_EXPR_CASE(EQ);
        DISPATCH_EXPR_CASE(NE);
        DISPATCH_EXPR_CASE(LAnd);
        DISPATCH_EXPR_CASE(LOr);
        DISPATCH_EXPR_CASE(LNot);
        DISPATCH_EXPR_CASE(Sqrt);
        DISPATCH_EXPR_CASE(Exp);
        DISPATCH_EXPR_CASE(Square);
        DISPATCH_EXPR_CASE(Sigmoid);
        DISPATCH_EXPR_CASE(Tanh);
        DISPATCH_EXPR_CASE(Abs);
        DISPATCH_EXPR_CASE(Floor);
        DISPATCH_EXPR_CASE(Ceil);
        DISPATCH_EXPR_CASE(IfExpr);
        DISPATCH_EXPR_CASE(Cast);
        DISPATCH_EXPR_CASE(Intrinsic);
        DISPATCH_EXPR_CASE(AnyExpr);

    default:
        ERROR("Unexpected Expr node type");
    }
}

Expr Mutator::operator()(const Expr &op) { return visitExpr(op); }

Stmt Mutator::visitStmt(const Stmt &op) {
    switch (op->nodeType()) {

#define DISPATCH_STMT_CASE(name)                                               \
    case ASTNodeType::name:                                                    \
        return visit(op.as<name##Node>());

        DISPATCH_STMT_CASE(StmtSeq);
        DISPATCH_STMT_CASE(VarDef);
        DISPATCH_STMT_CASE(Store);
        DISPATCH_STMT_CASE(Alloc);
        DISPATCH_STMT_CASE(Free);
        DISPATCH_STMT_CASE(ReduceTo);
        DISPATCH_STMT_CASE(For);
        DISPATCH_STMT_CASE(If);
        DISPATCH_STMT_CASE(Assert);
        DISPATCH_STMT_CASE(Assume);
        DISPATCH_STMT_CASE(Eval);
        DISPATCH_STMT_CASE(MatMul);
        DISPATCH_STMT_CASE(Any);

    default:
        ERROR("Unexpected Stmt node type");
    }
}

Stmt Mutator::operator()(const Stmt &op) { return visitStmt(op); }

} // namespace freetensor
