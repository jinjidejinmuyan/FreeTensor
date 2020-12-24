#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <string>

// Include minimal headers

#include <ast.h>

namespace ir {

std::string toString(const AST &op);

bool match(const AST &pattern, const AST &instance);

inline std::ostream &operator<<(std::ostream &os, const AST &op) {
    os << toString(op);
    return os;
}

} // namespace ir

#endif // DEBUG_H
