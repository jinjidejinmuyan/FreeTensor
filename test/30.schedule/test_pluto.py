import freetensor as ft
import pytest


def test_pluto_fuse():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"]):
        #! label: L0
        for i in range(256):
            for j in range(255):
                x[i, j + 1] += x[i, j]
        #! label: L1
        for i in range(256):
            for j in range(255):
                x[i, 254 - j] += x[i, 255 - j]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_fuse("L0", "L1")
    assert parallelism == 1
    kernel = s.func()
    print(kernel)


def test_pluto_fuse_2():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"]):
        #! label: L0
        for i in range(256):
            for j in range(255):
                x[i, j + 1] += x[i, j]
        #! label: L1
        for i in range(256):
            for j in range(255):
                x[255 - i, 254 - j] += x[255 - i, 255 - j]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_fuse("L0", "L1")
    assert parallelism == 1
    kernel = s.func()
    print(kernel)


def test_pluto_fuse_3():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"]):
        #! label: L0
        for i in range(256):
            for j in range(255):
                x[i, j + 1] += x[i, j]
        #! label: L1
        for i in range(255, -1, -1):
            for j in range(254, -1, -1):
                x[i, j] += x[i, j + 1]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_fuse("L0", "L1")
    assert parallelism == 1
    kernel = s.func()
    print(kernel)


def test_pluto_fuse_imbalanced_nest():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"],
               c: ft.Var[(256,), "float32", "inout"]):
        #! label: L0
        for j in range(1, 256):
            c[j] = j
        #! label: L1
        for i in range(256):
            for j in range(1, 256):
                x[i, j] *= c[j - 1] + c[j]

    @ft.transform
    def kernel_expected(x: ft.Var[(256, 256), "float32", "inout"],
                        c: ft.Var[(256,), "float32", "inout"]):
        for j in range(1, 256):
            c[j] = j
            for i in range(256):
                # the schedule produces "j + (-1)", just reproduce it
                x[i, j] *= c[j + -1] + c[j]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_fuse("L0", "L1")
    kernel = s.func()
    print(kernel)
    assert parallelism == 0
    assert kernel.body.match(kernel_expected.body)


def test_pluto_permute_reorder():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"]):
        #! label: L0
        for i in range(1, 256):
            for j in range(256):
                x[i, j] += x[i - 1, j]

    @ft.transform
    def kernel_expected(x: ft.Var[(256, 256), "float32", "inout"]):
        for j in range(256):
            for i in range(1, 256):
                x[i, j] += x[i + -1, j]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_permute("L0")
    kernel = s.func()
    print(kernel)
    assert parallelism == 1
    assert kernel.body.match(kernel_expected.body)


def test_pluto_permute_fully_parallelizable():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "output"],
               y: ft.Var[(258, 258), "float32", "input"]):
        #! label: L0
        for i in range(256):
            for j in range(256):
                x[i, j] = y[i - 1, j - 1] + y[i + 1, j + 1]

    @ft.transform
    def kernel_expected(x: ft.Var[(256, 256), "float32", "output"],
                        y: ft.Var[(258, 258), "float32", "input"]):
        #! label: L0
        for i in range(256):
            for j in range(256):
                x[i, j] = y[i + -1, j + -1] + y[i + 1, j + 1]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_permute("L0")
    kernel = s.func()
    print(kernel)
    assert parallelism == 2
    assert kernel.body.match(kernel_expected.body)


def test_pluto_permute_inner_loop():

    @ft.transform
    def kernel(x: ft.Var[(256, 256, 5), "float32", "inout"]):
        #! label: L0
        for i in range(1, 256):
            for j in range(256):
                x[i, j] = x[i - 1, j] * 5

    @ft.simplify
    @ft.transform
    def kernel_expected(x: ft.Var[(256, 256, 5), "float32", "inout"]):
        #! label: L0
        for j in range(256):
            for i in range(1, 256):
                x[i, j] = x[i + -1, j] * 5

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_permute("L0")
    kernel = s.func()
    print(kernel)
    assert parallelism == 1
    assert kernel.body.match(kernel_expected.body)


def test_pluto_permute_outer_loop():

    @ft.transform
    def kernel(x: ft.Var[(256, 256), "float32", "inout"]):
        for t in range(100):
            #! label: L0
            for i in range(1, 256):
                for j in range(256):
                    x[i, j] += x[i - 1, j]

    @ft.transform
    def kernel_expected(x: ft.Var[(256, 256), "float32", "inout"]):
        for t in range(100):
            #! label: L0
            for j in range(256):
                for i in range(1, 256):
                    x[i, j] += x[i + -1, j]

    print(kernel)
    s = ft.Schedule(kernel)
    _, parallelism = s.pluto_permute("L0")
    kernel = s.func()
    print(kernel)
    assert parallelism == 1
    assert kernel.body.match(kernel_expected.body)
