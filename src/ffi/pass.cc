#include <ffi.h>
#include <pass/code_gen_c.h>
#include <pass/simplify.h>

namespace ir {

void init_ffi_pass(py::module_ &m) {
    m.def("codeGenC", &codeGenC);
    m.def("simplifyPass", &simplifyPass);
}

} // namespace ir

