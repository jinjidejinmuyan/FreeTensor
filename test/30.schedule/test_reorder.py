import freetensor as ft
import pytest


def test_basic():
    with ft.VarDef("y", (4, 8), "int32", "output", "cpu") as y:
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                y[i, j] = i + j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef("y", (4, 8), "int32", "output", "cpu") as y:
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                y[i, j] = i + j
    std = ft.pop_ast()

    assert std.match(ast)


def test_multiple_loops():
    with ft.VarDef("y", (4, 8, 16), "int32", "output", "cpu") as y:
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                with ft.For("k", 0, 16, label="L3") as k:
                    y[i, j, k] = (i + j) * k
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L3", "L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef("y", (4, 8, 16), "int32", "output", "cpu") as y:
        with ft.For("k", 0, 16) as k:
            with ft.For("j", 0, 8) as j:
                with ft.For("i", 0, 4) as i:
                    y[i, j, k] = (i + j) * k
    std = ft.pop_ast()

    assert std.match(ast)


def test_if_in_between():
    with ft.VarDef([("x", (4,), "int32", "input", "cpu"),
                    ("y", (4, 8), "int32", "output", "cpu")]) as (x, y):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.If(x[i] > 0):
                with ft.For("j", 0, 8, label="L2") as j:
                    y[i, j] = i + j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([("x", (4,), "int32", "input", "cpu"),
                    ("y", (4, 8), "int32", "output", "cpu")]) as (x, y):
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                with ft.If(x[i] > 0):
                    y[i, j] = i + j
    std = ft.pop_ast()

    assert std.match(ast)


def test_stmt_in_between():
    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4,), "int32", "output", "cpu")]) as (y, z):
        with ft.For("i", 0, 4, label="L1") as i:
            z[i] = i
            with ft.For("j", 0, 8, label="L2") as j:
                y[i, j] = i + j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4,), "int32", "output", "cpu")]) as (y, z):
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                with ft.If(j == 0):
                    z[i] = i
                y[i, j] = i + j
    std = ft.pop_ast()

    assert std.match(ast)


def test_loop_in_between():
    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4, 8), "int32", "output", "cpu")]) as (y, z):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                z[i, j] = i * j
            with ft.For("j", 0, 8, label="L3") as j:
                y[i, j] = i + j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L3", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4, 8), "int32", "output", "cpu")]) as (y, z):
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                with ft.If(j == 0):
                    with ft.For("j1", 0, 8) as j1:
                        z[i, j1] = i * j1
                y[i, j] = i + j
    std = ft.pop_ast()

    assert std.match(ast)


def test_multiple_loops_in_between_separated_by_vardef():
    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4, 8), "int32", "output", "cpu"),
                    ("w", (4, 8), "int32", "output", "cpu"),
                    ("n", (), "int32", "input", "cpu")]) as (y, z, w, n):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                z[i, j] = i * j
            with ft.VarDef("t", (), "int32", "cache", "cpu") as t:
                t[...] = n[...] * i
                with ft.For("j", 0, 8, label="L2") as j:
                    w[i, j] = t[...]
            with ft.For("j", 0, 8, label="L3") as j:
                y[i, j] = i + j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L3", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (4, 8), "int32", "output", "cpu"),
                    ("w", (4, 8), "int32", "output", "cpu"),
                    ("n", (), "int32", "input", "cpu")]) as (y, z, w, n):
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                with ft.If(j == 0):
                    with ft.For("j1", 0, 8) as j1:
                        z[i, j1] = i * j1
                    with ft.VarDef("t", (), "int32", "cache", "cpu") as t:
                        t[...] = n[...] * i
                        with ft.For("j2", 0, 8) as j2:
                            w[i, j2] = t[...]
                y[i, j] = i + j
    std = ft.pop_ast()

    assert std.match(ast)


def test_legal_dependence():
    with ft.VarDef("y", (8,), "int32", "inout", "cpu") as y:
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                y[j] = (y[j] + 1) * j
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef("y", (8,), "int32", "inout", "cpu") as y:
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                y[j] = (y[j] + 1) * j
    std = ft.pop_ast()

    assert std.match(ast)


def test_legal_dependence_only_inner_loops():
    with ft.VarDef("y", (16,), "int32", "inout", "cpu") as y:
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                with ft.For("k", 0, 16, label="L3") as k:
                    y[k] = (y[k] + 1) * j * k
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L3", "L2"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef("y", (16,), "int32", "inout", "cpu") as y:
        with ft.For("i", 0, 4) as i:
            with ft.For("k", 0, 16) as k:
                with ft.For("j", 0, 8) as j:
                    y[k] = (y[k] + 1) * j * k
    std = ft.pop_ast()

    assert std.match(ast)


def test_illegal_dependence():
    with ft.VarDef("y", (1,), "int32", "output", "cpu") as y:
        y[0] = 0
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                y[0] = y[0] * i + j
    ast = ft.pop_ast(verbose=True)
    s = ft.Schedule(ast)
    with pytest.raises(ft.InvalidSchedule):
        s.reorder(["L2", "L1"])
    ast_ = s.ast()  # Should not changed
    assert ast_.match(ast)


def test_illegal_dependence_of_stmt_in_between():
    with ft.VarDef([("y", (4, 8), "int32", "output", "cpu"),
                    ("z", (), "int32", "cache", "cpu")]) as (y, z):
        with ft.For("i", 0, 4, label="L1") as i:
            z[()] = i * i
            with ft.For("j", 0, 8, label="L2") as j:
                y[i, j] = z[()] + j
    ast = ft.pop_ast(verbose=True)
    s = ft.Schedule(ast)
    with pytest.raises(ft.InvalidSchedule):
        s.reorder(["L2", "L1"])
    ast_ = s.ast()  # Should not changed
    assert ast_.match(ast)


def test_reduction():
    with ft.VarDef([("x", (4, 8), "int32", "output", "cpu"),
                    ("y", (1,), "int32", "output", "cpu")]) as (x, y):
        y[0] = 0
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                y[0] = y[0] + x[i, j]
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([("x", (4, 8), "int32", "output", "cpu"),
                    ("y", (1,), "int32", "output", "cpu")]) as (x, y):
        y[0] = 0
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                ft.Any()
    std = ft.pop_ast()

    assert std.match(ast)


def test_local_var():
    with ft.VarDef([
        ("x0", (4, 8), "int32", "input", "cpu"),
        ("x1", (4, 8), "int32", "input", "cpu"),
        ("y1", (4, 8), "int32", "output", "cpu"),
        ("y2", (4, 8), "int32", "output", "cpu"),
    ]) as (x0, x1, y1, y2):
        with ft.For("i", 0, 4, label="L1") as i:
            with ft.For("j", 0, 8, label="L2") as j:
                with ft.VarDef("buf", (1,), "int32", "cache", "cpu") as buf:
                    buf[0] = x0[i, j] + x1[i, j]
                    y1[i, j] = buf[0] * 2
                    y2[i, j] = buf[0] * 3
    ast = ft.pop_ast(verbose=True)
    ast = ft.schedule(ast, lambda s: s.reorder(["L2", "L1"]), verbose=1)
    ast = ft.lower(ast, verbose=1)

    with ft.VarDef([
        ("x0", (4, 8), "int32", "input", "cpu"),
        ("x1", (4, 8), "int32", "input", "cpu"),
        ("y1", (4, 8), "int32", "output", "cpu"),
        ("y2", (4, 8), "int32", "output", "cpu"),
    ]) as (x0, x1, y1, y2):
        with ft.For("j", 0, 8) as j:
            with ft.For("i", 0, 4) as i:
                with ft.VarDef("buf", (1,), "int32", "cache", "cpu") as buf:
                    buf[0] = x0[i, j] + x1[i, j]
                    y1[i, j] = buf[0] * 2
                    y2[i, j] = buf[0] * 3
    std = ft.pop_ast()

    assert std.match(ast)
