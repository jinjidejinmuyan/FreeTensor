#ifndef FREE_TENSOR_PRESBURGER_H
#define FREE_TENSOR_PRESBURGER_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include <isl/aff.h>
#include <isl/ctx.h>
#include <isl/ilp.h>
#include <isl/map.h>
#include <isl/options.h>
#include <isl/set.h>
#include <isl/space.h>

#include <debug/profile.h>
#include <except.h>
#include <serialize/to_string.h>

namespace freetensor {

// Presburger arithmetic, currently implemented with ISL

template <class T> T *GET_ISL_PTR(T *ptr) {
    ASSERT(ptr != nullptr);
    return ptr;
}

#define COPY_ISL_PTR(ptr, type) _COPY_ISL_PTR(ptr, isl_##type##_copy)
template <class T> T *_COPY_ISL_PTR(const T *ptr, T *(copy)(T *)) {
    ASSERT(ptr != nullptr);
    return copy(const_cast<T *>(ptr));
}

template <class T> T *MOVE_ISL_PTR(T *&ptr) {
    ASSERT(ptr != nullptr);
    auto ret = ptr;
    ptr = nullptr;
    return ret;
}

class PBCtx {
    isl_ctx *ctx_ = nullptr;

  public:
    PBCtx() : ctx_(isl_ctx_alloc()) {
        isl_options_set_on_error(ctx_, ISL_ON_ERROR_ABORT);
    }
    ~PBCtx() { isl_ctx_free(ctx_); }

    PBCtx(const PBCtx &other) = delete;
    PBCtx &operator=(const PBCtx &other) = delete;

    isl_ctx *get() const { return GET_ISL_PTR(ctx_); }
};

class PBMap {
    isl_map *map_ = nullptr;

  public:
    PBMap() {}
    PBMap(isl_map *map) : map_(map) {}
    PBMap(const PBCtx &ctx, const std::string &str)
        : map_(isl_map_read_from_str(ctx.get(), str.c_str())) {
        if (map_ == nullptr) {
            ERROR("Unable to construct an PBMap from " + str);
        }
    }
    ~PBMap() {
        if (map_ != nullptr) {
            isl_map_free(map_);
        }
    }

    PBMap(const PBMap &other) : map_(other.copy()) {}
    PBMap &operator=(const PBMap &other) {
        if (map_ != nullptr) {
            isl_map_free(map_);
        }
        map_ = other.copy();
        return *this;
    }

    PBMap(PBMap &&other) : map_(other.move()) {}
    PBMap &operator=(PBMap &&other) {
        if (map_ != nullptr) {
            isl_map_free(map_);
        }
        map_ = other.move();
        return *this;
    }

    bool isValid() const { return map_ != nullptr; }

    isl_map *get() const { return GET_ISL_PTR(map_); }
    isl_map *copy() const { return COPY_ISL_PTR(map_, map); }
    isl_map *move() { return MOVE_ISL_PTR(map_); }

    bool empty() const {
        DEBUG_PROFILE("empty");
        return isl_map_is_empty(get());
    }
    bool isSingleValued() const { return isl_map_is_single_valued(get()); }
    bool isBijective() const { return isl_map_is_bijective(get()); }

    isl_size nBasic() const { return isl_map_n_basic_map(map_); }

    isl_size nInDims() const { return isl_map_dim(map_, isl_dim_in); }
    isl_size nOutDims() const { return isl_map_dim(map_, isl_dim_out); }
    isl_size nParamDims() const { return isl_map_dim(map_, isl_dim_param); }

    void projectOutInputDims(unsigned first, unsigned n) {
        map_ = isl_map_project_out(map_, isl_dim_in, first, n);
    }
    void projectOutOutputDims(unsigned first, unsigned n) {
        map_ = isl_map_project_out(map_, isl_dim_out, first, n);
    }

    void moveDimsInputToOutput(unsigned first, unsigned n, unsigned target) {
        map_ =
            isl_map_move_dims(map_, isl_dim_out, target, isl_dim_in, first, n);
    }
    void moveDimsOutputToInput(unsigned first, unsigned n, unsigned target) {
        map_ =
            isl_map_move_dims(map_, isl_dim_in, target, isl_dim_out, first, n);
    }

    friend std::ostream &operator<<(std::ostream &os, const PBMap &map) {
        return os << isl_map_to_str(map.map_);
    }
};

class PBVal {
    isl_val *val_ = nullptr;

  public:
    PBVal() {}
    PBVal(isl_val *val) : val_(val) {}
    ~PBVal() {
        if (val_ != nullptr) {
            isl_val_free(val_);
        }
    }

    PBVal(const PBVal &other) : val_(other.copy()) {}
    PBVal &operator=(const PBVal &other) {
        if (val_ != nullptr) {
            isl_val_free(val_);
        }
        val_ = other.copy();
        return *this;
    }

    PBVal(PBVal &&other) : val_(other.move()) {}
    PBVal &operator=(PBVal &&other) {
        if (val_ != nullptr) {
            isl_val_free(val_);
        }
        val_ = other.move();
        return *this;
    }

    bool isValid() const { return val_ != nullptr; }

    isl_val *get() const { return GET_ISL_PTR(val_); }
    isl_val *copy() const { return COPY_ISL_PTR(val_, val); }
    isl_val *move() { return MOVE_ISL_PTR(val_); }

    bool isRat() const { return isl_val_is_rat(get()); }
    int numSi() const { return isl_val_get_num_si(get()); }
    int denSi() const { return isl_val_get_den_si(get()); }

    friend std::ostream &operator<<(std::ostream &os, const PBVal &val) {
        return os << isl_val_to_str(val.val_);
    }
};

class PBSet {
    isl_set *set_ = nullptr;

  public:
    PBSet() {}
    PBSet(isl_set *set) : set_(set) {}
    PBSet(const PBCtx &ctx, const std::string &str)
        : set_(isl_set_read_from_str(ctx.get(), str.c_str())) {
        if (set_ == nullptr) {
            ERROR("Unable to construct an PBSet from " + str);
        }
    }
    ~PBSet() {
        if (set_ != nullptr) {
            isl_set_free(set_);
        }
    }

    PBSet(const PBSet &other) : set_(other.copy()) {}
    PBSet &operator=(const PBSet &other) {
        if (set_ != nullptr) {
            isl_set_free(set_);
        }
        set_ = other.copy();
        return *this;
    }

    PBSet(PBSet &&other) : set_(other.move()) {}
    PBSet &operator=(PBSet &&other) {
        if (set_ != nullptr) {
            isl_set_free(set_);
        }
        set_ = other.move();
        return *this;
    }

    bool isValid() const { return set_ != nullptr; }

    isl_set *get() const { return GET_ISL_PTR(set_); }
    isl_set *copy() const { return COPY_ISL_PTR(set_, set); }
    isl_set *move() { return MOVE_ISL_PTR(set_); }

    bool empty() const {
        DEBUG_PROFILE("empty");
        return isl_set_is_empty(get());
    }

    isl_size nBasic() const { return isl_set_n_basic_set(set_); }

    isl_size nDims() const { return isl_set_dim(set_, isl_dim_set); }

    void projectOutDims(unsigned first, unsigned n) {
        set_ = isl_set_project_out(set_, isl_dim_set, first, n);
    }

    friend std::ostream &operator<<(std::ostream &os, const PBSet &set) {
        return os << isl_set_to_str(set.set_);
    }
};

class PBSpace {
    isl_space *space_ = nullptr;

  public:
    PBSpace() {}
    PBSpace(isl_space *space) : space_(space) {}
    PBSpace(const PBSet &set) : space_(isl_set_get_space(set.get())) {}
    ~PBSpace() {
        if (space_ != nullptr) {
            isl_space_free(space_);
        }
    }

    PBSpace(const PBSpace &other) : space_(other.copy()) {}
    PBSpace &operator=(const PBSpace &other) {
        if (space_ != nullptr) {
            isl_space_free(space_);
        }
        space_ = other.copy();
        return *this;
    }

    PBSpace(PBSpace &&other) : space_(other.move()) {}
    PBSpace &operator=(PBSpace &&other) {
        if (space_ != nullptr) {
            isl_space_free(space_);
        }
        space_ = other.move();
        return *this;
    }

    bool isValid() const { return space_ != nullptr; }

    isl_space *get() const { return GET_ISL_PTR(space_); }
    isl_space *copy() const { return COPY_ISL_PTR(space_, space); }
    isl_space *move() { return MOVE_ISL_PTR(space_); }

    bool operator==(const PBSpace &other) const {
        if (space_ == nullptr || other.space_ == nullptr)
            return space_ == other.space_;
        return isl_space_is_equal(get(), other.get());
    }

    friend std::ostream &operator<<(std::ostream &os, const PBSpace &space) {
        return os << isl_space_to_str(space.space_);
    }
};

class PBFunc {
    isl_pw_multi_aff *func_ = nullptr;

  public:
    PBFunc() {}
    PBFunc(isl_pw_multi_aff *func) : func_(func) {}
    PBFunc(const PBMap &map) : func_(isl_pw_multi_aff_from_map(map.copy())) {}
    PBFunc(PBMap &&map) : func_(isl_pw_multi_aff_from_map(map.move())) {}
    ~PBFunc() {
        if (func_ != nullptr) {
            isl_pw_multi_aff_free(func_);
        }
    }

    PBFunc(const PBFunc &other) : func_(other.copy()) {}
    PBFunc &operator=(const PBFunc &other) {
        if (func_ != nullptr) {
            isl_pw_multi_aff_free(func_);
        }
        func_ = other.copy();
        return *this;
    }

    PBFunc(PBFunc &&other) : func_(other.move()) {}
    PBFunc &operator=(PBFunc &&other) {
        if (func_ != nullptr) {
            isl_pw_multi_aff_free(func_);
        }
        func_ = other.move();
        return *this;
    }

    bool isValid() const { return func_ != nullptr; }

    isl_pw_multi_aff *get() const { return GET_ISL_PTR(func_); }
    isl_pw_multi_aff *copy() const { return COPY_ISL_PTR(func_, pw_multi_aff); }
    isl_pw_multi_aff *move() { return MOVE_ISL_PTR(func_); }

    friend std::ostream &operator<<(std::ostream &os, const PBFunc &func) {
        return os << isl_pw_multi_aff_to_str(func.func_);
    }
};

class PBPoint {
    isl_point *point_ = nullptr;

  public:
    PBPoint() {}
    PBPoint(isl_point *point) : point_(point) {}

    ~PBPoint() {
        if (point_ != nullptr) {
            isl_point_free(point_);
        }
    }

    PBPoint(const PBPoint &other) : point_(other.copy()) {}
    PBPoint &operator=(const PBPoint &other) {
        if (point_ != nullptr) {
            isl_point_free(point_);
        }
        point_ = other.copy();
        return *this;
    }

    PBPoint(PBPoint &&other) : point_(other.move()) {}
    PBPoint &operator=(PBPoint &&other) {
        if (point_ != nullptr) {
            isl_point_free(point_);
        }
        point_ = other.move();
        return *this;
    }

    bool isValid() const { return point_ != nullptr; }

    isl_point *get() const { return GET_ISL_PTR(point_); }
    isl_point *copy() const { return COPY_ISL_PTR(point_, point); }
    isl_point *move() { return MOVE_ISL_PTR(point_); }

    bool isVoid() const { return isl_point_is_void(point_); }

    std::vector<PBVal> coordinates() const {
        ASSERT(!isVoid());
        std::vector<PBVal> result;
        isl_size nCoord = isl_space_dim(
            PBSpace(isl_point_get_space(point_)).get(), isl_dim_set);
        for (isl_size i = 0; i < nCoord; ++i)
            result.emplace_back(
                isl_point_get_coordinate_val(point_, isl_dim_set, i));
        return result;
    }
};

template <typename T>
concept PBMapRef = std::same_as<PBMap, std::decay_t<T>>;
template <typename T>
concept PBValRef = std::same_as<PBVal, std::decay_t<T>>;
template <typename T>
concept PBSetRef = std::same_as<PBSet, std::decay_t<T>>;
template <typename T>
concept PBSpaceRef = std::same_as<PBSpace, std::decay_t<T>>;
template <typename T>
concept PBFuncRef = std::same_as<PBFunc, std::decay_t<T>>;

template <typename T> auto PBRefTake(std::remove_reference_t<T> &t) {
    return t.copy();
}
template <typename T> auto PBRefTake(std::remove_reference_t<T> &&t) {
    static_assert(!std::is_lvalue_reference_v<T>); // similar to std::forward
    return t.move();
}

template <PBSetRef T> PBSet complement(T &&set) {
    DEBUG_PROFILE("complement");
    return isl_set_complement(PBRefTake<T>(set));
}
template <PBMapRef T> PBMap complement(T &&map) {
    DEBUG_PROFILE("complement");
    return isl_map_complement(PBRefTake<T>(map));
}

template <PBMapRef T> PBMap reverse(T &&map) {
    DEBUG_PROFILE("reverse");
    return isl_map_reverse(PBRefTake<T>(map));
}

template <PBMapRef T, PBMapRef U> PBMap subtract(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("subtract", "nBasic=" + std::to_string(lhs.nBasic()) +
                                          "," + std::to_string(rhs.nBasic()));
    return isl_map_subtract(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}
template <PBSetRef T, PBSetRef U> PBSet subtract(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("subtract", "nBasic=" + std::to_string(lhs.nBasic()) +
                                          "," + std::to_string(rhs.nBasic()));
    return isl_set_subtract(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T, PBMapRef U> PBMap intersect(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("intersect",
                          "nBasic=" + std::to_string(lhs.nBasic()) + "," +
                              std::to_string(rhs.nBasic()));
    return isl_map_intersect(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}
template <PBSetRef T, PBSetRef U> PBSet intersect(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("intersect",
                          "nBasic=" + std::to_string(lhs.nBasic()) + "," +
                              std::to_string(rhs.nBasic()));
    return isl_set_intersect(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T, PBSetRef U> PBMap intersectDomain(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("intersectDomain",
                          "nBasic=" + std::to_string(lhs.nBasic()) + "," +
                              std::to_string(rhs.nBasic()));
    return isl_map_intersect_domain(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}
template <PBMapRef T, PBSetRef U> PBMap intersectRange(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("intersectRange",
                          "nBasic=" + std::to_string(lhs.nBasic()) + "," +
                              std::to_string(rhs.nBasic()));
    return isl_map_intersect_range(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T, PBMapRef U> PBMap uni(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("uni", "nBasic=" + std::to_string(lhs.nBasic()) +
                                     "," + std::to_string(rhs.nBasic()));
    return isl_map_union(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBSetRef T, PBSetRef U> PBSet uni(T &&lhs, U &&rhs) {
    DEBUG_PROFILE_VERBOSE("uni", "nBasic=" + std::to_string(lhs.nBasic()) +
                                     "," + std::to_string(rhs.nBasic()));
    return isl_set_union(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBSetRef T, PBMapRef U> PBSet apply(T &&lhs, U &&rhs) {
    DEBUG_PROFILE("apply");
    return isl_set_apply(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T, PBMapRef U> PBMap applyDomain(T &&lhs, U &&rhs) {
    DEBUG_PROFILE("applyDomain");
    return isl_map_apply_domain(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T, PBMapRef U> PBMap applyRange(T &&lhs, U &&rhs) {
    DEBUG_PROFILE("applyRange");
    return isl_map_apply_range(PBRefTake<T>(lhs), PBRefTake<U>(rhs));
}

template <PBMapRef T> PBMap lexmax(T &&map) {
    DEBUG_PROFILE_VERBOSE("lexmax", "nBasic=" + std::to_string(map.nBasic()));
    return isl_map_lexmax(PBRefTake<T>(map));
}

template <PBMapRef T> PBMap lexmin(T &&map) {
    DEBUG_PROFILE_VERBOSE("lexmin", "nBasic=" + std::to_string(map.nBasic()));
    return isl_map_lexmin(PBRefTake<T>(map));
}

template <PBSetRef T> PBSet lexmax(T &&set) {
    DEBUG_PROFILE_VERBOSE("lexmax", "nBasic=" + std::to_string(set.nBasic()));
    return isl_set_lexmax(PBRefTake<T>(set));
}

template <PBSetRef T> PBSet lexmin(T &&set) {
    DEBUG_PROFILE_VERBOSE("lexmin", "nBasic=" + std::to_string(set.nBasic()));
    return isl_set_lexmin(PBRefTake<T>(set));
}

template <PBSpaceRef T> PBMap identity(T &&space) {
    DEBUG_PROFILE("identity");
    return isl_map_identity(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap lexGE(T &&space) {
    DEBUG_PROFILE("lexGE");
    return isl_map_lex_ge(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap lexGT(T &&space) {
    DEBUG_PROFILE("lexGT");
    return isl_map_lex_gt(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap lexLE(T &&space) {
    DEBUG_PROFILE("lexLE");
    return isl_map_lex_le(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap lexLT(T &&space) {
    DEBUG_PROFILE("lexLT");
    return isl_map_lex_lt(PBRefTake<T>(space));
}

inline PBSpace spaceAlloc(const PBCtx &ctx, unsigned nparam, unsigned nIn,
                          unsigned nOut) {
    return isl_space_alloc(ctx.get(), nparam, nIn, nOut);
}

inline PBSpace spaceSetAlloc(const PBCtx &ctx, unsigned nparam, unsigned dim) {
    return isl_space_set_alloc(ctx.get(), nparam, dim);
}

template <PBSpaceRef T> PBSet emptySet(T &&space) {
    return isl_set_empty(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap emptyMap(T &&space) {
    return isl_map_empty(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBSet universeSet(T &&space) {
    return isl_set_universe(PBRefTake<T>(space));
}

template <PBSpaceRef T> PBMap universeMap(T &&space) {
    return isl_map_universe(PBRefTake<T>(space));
}

template <PBMapRef T> PBSet domain(T &&map) {
    return isl_map_domain(PBRefTake<T>(map));
}

template <PBMapRef T> PBSet range(T &&map) {
    return isl_map_range(PBRefTake<T>(map));
}

template <PBSetRef T> PBSet coalesce(T &&set) {
    DEBUG_PROFILE("coalesce");
    return isl_set_coalesce(PBRefTake<T>(set));
}

template <PBMapRef T> PBMap coalesce(T &&map) {
    DEBUG_PROFILE("coalesce");
    return isl_map_coalesce(PBRefTake<T>(map));
}

template <PBSetRef T> PBVal dimMaxVal(T &&set, int pos) {
    return isl_set_dim_max_val(PBRefTake<T>(set), pos);
}

template <PBSetRef T> PBVal dimMinVal(T &&set, int pos) {
    return isl_set_dim_min_val(PBRefTake<T>(set), pos);
}

template <PBSpaceRef T> PBSpace spaceMapFromSet(T &&space) {
    return isl_space_map_from_set(PBRefTake<T>(space));
}

template <PBMapRef T> PBSet flattenMapToSet(T &&map) {
    return isl_set_flatten(isl_map_wrap(PBRefTake<T>(map)));
}

template <PBSetRef T> PBPoint sample(T &&set) {
    return isl_set_sample_point(PBRefTake<T>(set));
}

/**
 * @brief Compute the set of coefficients corresponded to the given set
 *
 * The output coefficient set will be of the same dimension as the given set
 *
 * Let the variables of the input set $X$ be $x_i$, and that of the output
 * coefficient set $C$ be $c_i$, then this procedure guarantees:
 * $\{c_i\} \in C \Leftrightarrow \forall \{x_i\} \in X, \Sigma c_i x_i \geq c$
 *
 * @param set the input set
 * @param c the RHS constant
 * @return PBSet set of valid coefficients for the input set
 */
template <PBSetRef T> PBSet coefficients(T &&set, int64_t c = 0) {
    auto coefficientsMap = isl_map_from_basic_map(
        isl_basic_set_unwrap(isl_set_coefficients(PBRefTake<T>(set))));
    auto ctx = isl_map_get_ctx(coefficientsMap);
    auto paramsSpace = isl_space_domain(isl_map_get_space(coefficientsMap));
    auto nParams = isl_space_dim(paramsSpace, isl_dim_set);
    auto cPoint = isl_point_zero(paramsSpace);
    isl_point_set_coordinate_val(cPoint, isl_dim_set, nParams - 1,
                                 isl_val_int_from_si(ctx, -c));
    return apply(PBSet(isl_set_from_point(cPoint)), PBMap(coefficientsMap));
}

inline bool operator==(const PBSet &lhs, const PBSet &rhs) {
    DEBUG_PROFILE_VERBOSE("equal", "nBasic=" + std::to_string(lhs.nBasic()) +
                                       "," + std::to_string(rhs.nBasic()));
    return isl_set_is_equal(lhs.get(), rhs.get());
}

inline bool operator==(const PBMap &lhs, const PBMap &rhs) {
    DEBUG_PROFILE_VERBOSE("equal", "nBasic=" + std::to_string(lhs.nBasic()) +
                                       "," + std::to_string(rhs.nBasic()));
    return isl_map_is_equal(lhs.get(), rhs.get());
}

class PBBuildExpr {
    std::string expr_;
    explicit PBBuildExpr(const std::string &expr) : expr_(expr) {}
    friend class PBBuilder;

  public:
    PBBuildExpr(const PBBuildExpr &) = default;
    PBBuildExpr(PBBuildExpr &&) = default;
    PBBuildExpr &operator=(const PBBuildExpr &) = default;
    PBBuildExpr &operator=(PBBuildExpr &&) = default;

    PBBuildExpr(bool b) : expr_(b ? "true" : "false") {}
    PBBuildExpr(std::integral auto i) : expr_(toString(i)) {}

    friend PBBuildExpr operator+(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " + " + b.expr_ + ")");
    }
    PBBuildExpr &operator+=(const PBBuildExpr &other) {
        return *this = *this + other;
    }

    PBBuildExpr operator-() const { return PBBuildExpr("(-" + expr_ + ")"); }
    friend PBBuildExpr operator-(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " - " + b.expr_ + ")");
    }
    PBBuildExpr &operator-=(const PBBuildExpr &other) {
        return *this = *this - other;
    }

    friend PBBuildExpr operator*(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " * " + b.expr_ + ")");
    }
    PBBuildExpr &operator*=(const PBBuildExpr &other) {
        return *this = *this * other;
    }

    friend PBBuildExpr operator/(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " / " + b.expr_ + ")");
    }
    PBBuildExpr &operator/=(const PBBuildExpr &other) {
        return *this = *this / other;
    }

    friend PBBuildExpr ceilDiv(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("ceil(" + a.expr_ + " / " + b.expr_ + ")");
    }

    friend PBBuildExpr operator%(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " % " + b.expr_ + ")");
    }
    PBBuildExpr &operator%=(const PBBuildExpr &other) {
        return *this = *this % other;
    }

    friend PBBuildExpr operator<(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " < " + b.expr_ + ")");
    }

    friend PBBuildExpr operator<=(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " <= " + b.expr_ + ")");
    }

    friend PBBuildExpr operator>(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " > " + b.expr_ + ")");
    }

    friend PBBuildExpr operator>=(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " >= " + b.expr_ + ")");
    }

    friend PBBuildExpr operator==(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " = " + b.expr_ + ")");
    }

    friend PBBuildExpr operator!=(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " != " + b.expr_ + ")");
    }

    friend PBBuildExpr operator&&(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " and " + b.expr_ + ")");
    }

    friend PBBuildExpr operator||(const PBBuildExpr &a, const PBBuildExpr &b) {
        return PBBuildExpr("(" + a.expr_ + " or " + b.expr_ + ")");
    }

    friend std::ostream &operator<<(std::ostream &os, const PBBuildExpr &e);
};

class PBBuilder {
    int anonVarNum_ = 0;
    std::unordered_set<std::string> namedVars;

    std::vector<PBBuildExpr> constraints_;

  protected:
    PBBuildExpr newVar(const std::string &name = "");
    std::vector<PBBuildExpr> newVars(int n, const std::string &prefix = "");

    std::string getConstraintsStr() const;

  public:
    PBBuilder() = default;
    PBBuilder(const PBBuilder &) = default;
    PBBuilder(PBBuilder &&) = default;
    PBBuilder &operator=(const PBBuilder &) = default;
    PBBuilder &operator=(PBBuilder &&) = default;

    void addConstraint(const PBBuildExpr &constraint);
    void addConstraint(PBBuildExpr &&constraint);
    template <typename T = std::initializer_list<PBBuildExpr>>
    void addConstraints(T &&constraints) {
        for (auto &&c : constraints)
            addConstraint(c);
    }
    const std::vector<PBBuildExpr> &constraints() const { return constraints_; }
    void clearConstraints() { constraints_.clear(); };
};

class PBMapBuilder : public PBBuilder {
    std::vector<PBBuildExpr> inputs_;
    std::vector<PBBuildExpr> outputs_;

  public:
    PBMapBuilder() = default;
    PBMapBuilder(const PBMapBuilder &) = default;
    PBMapBuilder(PBMapBuilder &&) = default;
    PBMapBuilder &operator=(const PBMapBuilder &) = default;
    PBMapBuilder &operator=(PBMapBuilder &&) = default;

    void addInput(const PBBuildExpr &expr);
    void addInputs(auto exprs) {
        for (auto &&e : exprs)
            addInput(e);
    }
    PBBuildExpr newInput(const std::string &name);
    std::vector<PBBuildExpr> newInputs(int n, const std::string &prefix = "");
    const std::vector<PBBuildExpr> &inputs() const { return inputs_; }
    void clearInputs() { inputs_.clear(); }

    void addOutput(const PBBuildExpr &expr);
    void addOutputs(auto exprs) {
        for (auto &&e : exprs)
            addOutput(e);
    }
    PBBuildExpr newOutput(const std::string &name);
    std::vector<PBBuildExpr> newOutputs(int n, const std::string &prefix = "");
    const std::vector<PBBuildExpr> &outputs() const { return outputs_; }
    void clearOutputs() { outputs_.clear(); }

    PBMap build(const PBCtx &ctx) const;
};

class PBSetBuilder : public PBBuilder {
    std::vector<PBBuildExpr> vars_;

  public:
    PBSetBuilder() = default;
    PBSetBuilder(const PBSetBuilder &) = default;
    PBSetBuilder(PBSetBuilder &&) = default;
    PBSetBuilder &operator=(const PBSetBuilder &) = default;
    PBSetBuilder &operator=(PBSetBuilder &&) = default;

    void addVar(const PBBuildExpr &expr);
    void addVars(auto exprs) {
        for (auto &&e : exprs)
            addVar(e);
    }
    PBBuildExpr newVar(const std::string &name);
    std::vector<PBBuildExpr> newVars(int n, const std::string &prefix = "");
    const std::vector<PBBuildExpr> &vars() const { return vars_; }
    void clearVars() { vars_.clear(); }

    PBSet build(const PBCtx &ctx) const;
};

} // namespace freetensor

#endif // FREE_TENSOR_PRESBURGER_H
