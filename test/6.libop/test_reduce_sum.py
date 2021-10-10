import torch
import numpy as np

import ir
import ir.libop


def test_static():
    device = ir.Device(ir.CPU())

    @ir.transform
    def f(x, y):
        ir.declare_var(x, (3, 4, 5), "float32", "input", "cpu")
        ir.declare_var(y, (3, 5), "float32", "output", "cpu")
        "nid: reduce_sum"
        ir.libop.reduce_sum_(axes=[1], keepdims=False)(x, y)

    print(f)
    f = ir.lower(f, ir.CPU())
    print(f)

    code = ir.codegen(f, ir.CPU())

    x_torch = torch.rand(3, 4, 5, dtype=torch.float32)
    x_arr = ir.Array(x_torch.numpy(), device)
    y_torch = torch.zeros(3, 5, dtype=torch.float32)
    y_arr = ir.Array(y_torch.numpy(), device)
    ir.Driver(f, code, device)(x_arr, y_arr)
    y_torch = torch.Tensor(y_arr.numpy().reshape(3, 5))

    assert torch.all(torch.isclose(y_torch, x_torch.sum(axis=1)))


def test_keepdims():
    device = ir.Device(ir.CPU())

    @ir.transform
    def f(x, y):
        ir.declare_var(x, (3, 4, 5), "float32", "input", "cpu")
        ir.declare_var(y, (3, 1, 5), "float32", "output", "cpu")
        "nid: reduce_sum"
        ir.libop.reduce_sum_(axes=[1], keepdims=True)(x, y)

    print(f)
    f = ir.lower(f, ir.CPU())
    print(f)

    code = ir.codegen(f, ir.CPU())

    x_torch = torch.rand(3, 4, 5, dtype=torch.float32)
    x_arr = ir.Array(x_torch.numpy(), device)
    y_torch = torch.zeros(3, 1, 5, dtype=torch.float32)
    y_arr = ir.Array(y_torch.numpy(), device)
    ir.Driver(f, code, device)(x_arr, y_arr)
    y_torch = torch.Tensor(y_arr.numpy().reshape(3, 1, 5))

    assert torch.all(torch.isclose(y_torch, x_torch.sum(axis=1, keepdim=True)))


def test_out_of_place():
    device = ir.Device(ir.CPU())

    @ir.transform
    def f(x, y_shape, y):
        ir.declare_var(x, (3, 4, 5), "float32", "input", "cpu")
        ir.declare_var(y_shape, (2,), "int32", "output", "cpu")
        ir.declare_var(y, (3, 5), "float32", "output", "cpu")
        "nid: reduce_sum"
        _y = ir.libop.reduce_sum(axes=[1], keepdims=False)(x)
        for i in range(2):
            y_shape[i] = _y.shape(i)
        for i in range(3):
            for j in range(5):
                y[i, j] = _y[i, j]

    print(f)
    f = ir.lower(f, ir.CPU())
    print(f)

    code = ir.codegen(f, ir.CPU())

    x_torch = torch.rand(3, 4, 5, dtype=torch.float32)
    x_arr = ir.Array(x_torch.numpy(), device)
    y_shape_torch = torch.zeros(2, dtype=torch.int32)
    y_shape_arr = ir.Array(y_shape_torch.numpy(), device)
    y_torch = torch.zeros(3, 5, dtype=torch.float32)
    y_arr = ir.Array(y_torch.numpy(), device)
    ir.Driver(f, code, device)(x_arr, y_shape_arr, y_arr)
    y_shape_np = y_shape_arr.numpy()
    y_torch = torch.Tensor(y_arr.numpy().reshape(3, 5))

    assert np.array_equal(y_shape_np, [3, 5])
    assert torch.all(torch.isclose(y_torch, x_torch.sum(axis=1)))


def test_out_of_place_keepdims():
    device = ir.Device(ir.CPU())

    @ir.transform
    def f(x, y_shape, y):
        ir.declare_var(x, (3, 4, 5), "float32", "input", "cpu")
        ir.declare_var(y_shape, (3,), "int32", "output", "cpu")
        ir.declare_var(y, (3, 1, 5), "float32", "output", "cpu")
        "nid: reduce_sum"
        _y = ir.libop.reduce_sum(axes=[1], keepdims=True)(x)
        for i in range(3):
            y_shape[i] = _y.shape(i)
        for i in range(3):
            for j in range(5):
                y[i, 0, j] = _y[i, 0, j]

    print(f)
    f = ir.lower(f, ir.CPU())
    print(f)

    code = ir.codegen(f, ir.CPU())

    x_torch = torch.rand(3, 4, 5, dtype=torch.float32)
    x_arr = ir.Array(x_torch.numpy(), device)
    y_shape_torch = torch.zeros(3, dtype=torch.int32)
    y_shape_arr = ir.Array(y_shape_torch.numpy(), device)
    y_torch = torch.zeros(3, 1, 5, dtype=torch.float32)
    y_arr = ir.Array(y_torch.numpy(), device)
    ir.Driver(f, code, device)(x_arr, y_shape_arr, y_arr)
    y_shape_np = y_shape_arr.numpy()
    y_torch = torch.Tensor(y_arr.numpy().reshape(3, 1, 5))

    assert np.array_equal(y_shape_np, [3, 1, 5])
    assert torch.all(torch.isclose(y_torch, x_torch.sum(axis=1, keepdim=True)))
