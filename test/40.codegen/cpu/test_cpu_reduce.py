import re

import freetensor as ft
import pytest
import numpy as np

device = ft.CPU()
target = device.target()


def test_parallel_reduction():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4,), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i] = y[i] + x[i, j]

    with ft.VarDef([("x", (4, 64), "int32", "input", "cpu"),
                    ("y", (4,), "int32", "inout", "cpu")]) as (x, y):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 64, label="L2") as j:
                y[i] = y[i] + x[i, j]
    assert ft.pop_ast().match(test.body)

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "#pragma omp parallel for reduction(+:" in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4,), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    assert np.array_equal(y_np, y_std)


def test_parallel_reduction_on_2_vars():

    @ft.transform
    def test(x, y, z):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4,), "int32", "inout", "cpu"]
        z: ft.Var[(4,), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i] = y[i] + x[i, j]
                z[i] = z[i] + 2 * x[i, j]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "#pragma omp parallel for reduction(+:" in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4,), dtype="int32")
    z_np = np.zeros((4,), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    z_arr = ft.Array(z_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr, z=z_arr)
    y_np = y_arr.numpy()
    z_np = z_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    z_std = np.sum(x_np, axis=1) * 2
    assert np.array_equal(y_np, y_std)
    assert np.array_equal(z_np, z_std)


def test_parallel_reduction_on_array():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64, 64), "int32", "input", "cpu"]
        y: ft.Var[(4, 64), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(0, 64):
                    y[i, k] += x[i, j, k]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "#pragma omp parallel for reduction(+:" in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64, 64)).astype("int32")
    y_np = np.zeros((4, 64), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    assert np.array_equal(y_np, y_std)


def test_parallel_reduction_on_array_range():

    @ft.transform
    def test(x, y):
        x: ft.Var[(64, 64), "int32", "input", "cpu"]
        y: ft.Var[(64,), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(0, 16):
                    y[i * 16 + k] += x[i * 16 + k, j]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert re.search(r"pragma omp parallel for reduction.*16", str(code))
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (64, 64)).astype("int32")
    y_np = np.zeros((64,), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    assert np.array_equal(y_np, y_std)


def test_parallel_reduction_multiple_statements():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64, 64), "int32", "input", "cpu"]
        y: ft.Var[(4, 64), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(0, 64):
                    y[i, k] += x[i, j, k]
                y[i, 0] += x[i, j, 0]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "#pragma omp parallel for reduction(+:" in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64, 64)).astype("int32")
    y_np = np.zeros((4, 64), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    y_std[:, 0] += np.sum(x_np[:, :, 0], axis=1)
    assert np.array_equal(y_np, y_std)


def test_atomic_reduction():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4, 2), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i, j % 2] = y[i, j % 2] + x[i, j]

    with ft.VarDef([("x", (4, 64), "int32", "input", "cpu"),
                    ("y", (4, 2), "int32", "inout", "cpu")]) as (x, y):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 64, label="L2") as j:
                y[i, j % 2] = y[i, j % 2] + x[i, j]
    assert ft.pop_ast().match(test.body)

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "reduction" not in str(code)
    assert "#pragma omp atomic" in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4, 2), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np.reshape((4, 32, 2)), axis=1)
    assert np.array_equal(y_np, y_std)


def test_atomic_reduction_2_stmts_on_1_var():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4, 64), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i, j] += x[i, j]
                if j > 0:
                    y[i, j - 1] += x[i, j]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "reduction" not in str(code)
    assert str(code).count("#pragma omp atomic") == 2
    assert str(code).count("+=") == 2
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4, 64), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = x_np
    y_std[:, :-1] += x_np[:, 1:]
    assert np.array_equal(y_np, y_std)


def test_atomic_reduction_cache():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64, 10), "int32", "input", "cpu"]
        y: ft.Var[(4, 2), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(10):
                    y[i, j % 2] += x[i, j, k]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "reduction" not in str(code)
    assert str(code).count("#pragma omp atomic") == 1
    assert str(code).count("+=") == 2
    x_np = np.random.randint(0, 100, (4, 64, 10)).astype("int32")
    y_np = np.zeros((4, 2), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(np.sum(x_np, axis=-1).reshape((4, 32, 2)), axis=1)
    assert np.array_equal(y_np, y_std)


def test_atomic_reduction_cache_array():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64, 10, 3), "int32", "input", "cpu"]
        y: ft.Var[(4, 2, 3), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(10):
                    #! label: L4
                    for p in range(3):
                        y[i, j % 2, p] += x[i, j, k, p]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "reduction" not in str(code)
    assert str(code).count("#pragma omp atomic") == 1
    assert str(code).count("+=") == 2
    x_np = np.random.randint(0, 100, (4, 64, 10, 3)).astype("int32")
    y_np = np.zeros((4, 2, 3), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(np.sum(x_np, axis=-2).reshape((4, 32, 2, 3)), axis=1)
    assert np.array_equal(y_np, y_std)


def test_atomic_reduction_no_cache_array():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64, 10, 3), "int32", "input", "cpu"]
        y: ft.Var[(4, 2, 300), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                #! label: L3
                for k in range(10):
                    #! label: L4
                    for p in range(3):
                        y[i, j % 2, p * 100] += x[i, j, k, p]

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "reduction" not in str(code)
    assert str(code).count("#pragma omp atomic") == 1
    assert str(code).count("+=") == 1


def test_simultenous_parallel_and_atomic_reduction():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4, 2), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i, j % 2] += x[i, j]
                y[i, 0] += x[i, j]

    with ft.VarDef([("x", (4, 64), "int32", "input", "cpu"),
                    ("y", (4, 2), "int32", "inout", "cpu")]) as (x, y):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 64, label="L2") as j:
                y[i, j % 2] += x[i, j]
                y[i, 0] += x[i, j]
    assert ft.pop_ast().match(test.body)

    s = ft.Schedule(test)
    s.parallelize("L2", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target, verbose=True)
    assert "#pragma omp parallel for reduction(+:" in str(code)
    assert "#pragma omp atomic" in str(code)
    assert "+=" in str(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4, 2), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np.reshape((4, 32, 2)), axis=1)
    y_std[:, 0] += np.sum(x_np, axis=1)
    assert np.array_equal(y_np, y_std)


def test_serial_reduction_1():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4,), "int32", "inout", "cpu"]
        #! label: L1
        for i in range(0, 4):
            #! label: L2
            for j in range(0, 64):
                y[i] = y[i] + x[i, j]

    with ft.VarDef([("x", (4, 64), "int32", "input", "cpu"),
                    ("y", (4,), "int32", "inout", "cpu")]) as (x, y):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 64, label="L2") as j:
                y[i] = y[i] + x[i, j]
    assert ft.pop_ast().match(test.body)

    s = ft.Schedule(test)
    s.parallelize("L1", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target)
    assert "reduction" not in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    print(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4,), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1)
    assert np.array_equal(y_np, y_std)


def test_serial_reduction_2():

    @ft.transform
    def test(x, y):
        x: ft.Var[(4, 64), "int32", "input", "cpu"]
        y: ft.Var[(4,), "int32", "output", "cpu"]
        #! label: L1
        for i in range(0, 4):
            local_sum = ft.empty((), "int32", "cpu")
            local_sum[()] = 0.
            #! label: L2
            for j in range(0, 64):
                local_sum[()] += x[i, j]
            y[i] = local_sum[()] * 2

    s = ft.Schedule(test)
    s.parallelize("L1", "openmp")
    func = ft.lower(s.func(), target, verbose=1)

    code = ft.codegen(func, target)
    assert "reduction" not in str(code)
    assert "#pragma omp atomic" not in str(code)
    assert "+=" in str(code)
    print(code)
    x_np = np.random.randint(0, 100, (4, 64)).astype("int32")
    y_np = np.zeros((4,), dtype="int32")
    x_arr = ft.Array(x_np)
    y_arr = ft.Array(y_np)
    ft.build_binary(code, device)(x=x_arr, y=y_arr)
    y_np = y_arr.numpy()

    y_std = np.sum(x_np, axis=1) * 2
    assert np.array_equal(y_np, y_std)
