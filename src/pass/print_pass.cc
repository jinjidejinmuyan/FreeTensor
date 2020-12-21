#include <pass/print_pass.h>

namespace ir {

void PrintPass::visit(const VarDef &op) {
    makeIndent();
    os << ::ir::toString(op->buffer_->atype()) << " " << op->name_ << ": ";
    auto &&tensor = op->buffer_->tensor();
    os << ::ir::toString(tensor.dtype()) << "[";
    auto &&shape = tensor.shape();
    for (size_t i = 0, iEnd = shape.size(); i < iEnd; i++) {
        (*this)(shape[i]);
        os << (i < iEnd - 1 ? ", " : "");
    }
    os << "]";
    beginBlock();
    (*this)(op->body_);
    endBlock();
}

void PrintPass::visit(const Var &op) {
    os << op->name_;
    Visitor::visit(op);
}

void PrintPass::visit(const Store &op) {
    makeIndent();
    (*this)(op->var_);
    os << "[";
    for (size_t i = 0, iEnd = op->indices_.size(); i < iEnd; i++) {
        (*this)(op->indices_[i]);
        if (i < iEnd - 1) {
            os << ", ";
        }
    }
    os << "] = ";
    (*this)(op->expr_);
    os << std::endl;
}

void PrintPass::visit(const Load &op) {
    (*this)(op->var_);
    os << "[";
    for (size_t i = 0, iEnd = op->indices_.size(); i < iEnd; i++) {
        (*this)(op->indices_[i]);
        if (i < iEnd - 1) {
            os << ", ";
        }
    }
    os << "]";
}

void PrintPass::visit(const IntConst &op) { os << std::to_string(op->val_); }

void PrintPass::visit(const FloatConst &op) { os << std::to_string(op->val_); }

void PrintPass::beginBlock() {
    os << " {" << std::endl;
    nIndent++;
}

void PrintPass::endBlock() {
    nIndent--;
    makeIndent();
    os << "}" << std::endl;
}

void PrintPass::makeIndent() {
    for (int i = 0; i < nIndent; i++) {
        os << "  ";
    }
}

std::string PrintPass::toString() { return os.str(); }

std::string printPass(const AST &op) {
    PrintPass pass;
    pass(op);
    return pass.toString();
}

} // namespace ir

