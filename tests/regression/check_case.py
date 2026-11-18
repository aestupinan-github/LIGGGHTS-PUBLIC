#!/usr/bin/env python3
"""Regression check for surrogate test cases (Spec 05 RF-19).

Usage: check_case.py <liggghts> <input.in> <reference_output.log> [rel_tol]

Runs <input.in> in a scratch directory, parses the thermo table of the new run and of the frozen
reference log, and compares:
  * the particle count at the end (particle not lost)                      -- exact
  * every thermo column at every common step (z_c, KE, E_rot, E_pot, E_tot) -- rel. tolerance
  * physics invariant: E_tot never exceeds E_0 (if the case prints etot)    -- 1e-9 relative
Exit code 0 = pass, 1 = fail (diagnostics printed: column, step, expected, actual).
The default tolerance 1e-6 relative is meant for the same build on the same machine; looser
values may be passed for other compilers/platforms (Spec 05 NR-1).
"""
import os, shutil, subprocess, sys, tempfile


def thermo(path):
    """Return (header, {step: [values...]}) of the last thermo block in a LIGGGHTS log."""
    header, rows, cur = None, {}, None
    for line in open(path, errors="replace"):
        parts = line.split()
        if parts and parts[0] == "Step":
            header, cur = parts, {}
            continue
        if cur is not None:
            if len(parts) == len(header) and parts[0].isdigit():
                try:
                    cur[int(parts[0])] = [float(x) for x in parts[1:]]
                except ValueError:
                    pass
            elif line.startswith("Loop time"):
                rows, cur = cur, None
    if cur:
        rows = cur
    return header, rows


def close(a, e, rtol, atol=1e-30):
    return abs(a - e) <= max(rtol * abs(e), atol)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    exe, inp, ref = (os.path.abspath(p) for p in sys.argv[1:4])
    rtol = float(sys.argv[4]) if len(sys.argv) > 4 else 1e-6
    work = tempfile.mkdtemp(prefix="surr_reg_")
    try:
        r = subprocess.run([exe, "-in", inp], cwd=work, capture_output=True, text=True)
        out = os.path.join(work, "output.log")
        open(out, "w").write(r.stdout + r.stderr)
        if r.returncode != 0:
            print(f"FAIL run exited with {r.returncode}; last lines:\n" + "\n".join(r.stdout.splitlines()[-15:]))
            return 1
        h_new, new = thermo(out)
        h_ref, old = thermo(ref)
        if not new or not old or h_new != h_ref:
            print(f"FAIL thermo table missing or header differs:\n  new {h_new}\n  ref {h_ref}")
            return 1
        fails = 0
        steps = sorted(set(new) & set(old))
        if len(steps) < 2:
            print("FAIL fewer than 2 common thermo steps")
            return 1
        last = steps[-1]
        if new[last][0] != old[last][0]:
            print(f"FAIL particle count at step {last}: expected {old[last][0]}, actual {new[last][0]}")
            fails += 1
        cols = h_ref[1:]
        escale = max(abs(x[cols.index("etot")]) for x in old.values()) if "etot" in cols else 0.0
        worst = {}
        for s in steps:
            for j, name in enumerate(cols):
                a, e = new[s][j], old[s][j]
                if a != a:  # NaN
                    print(f"FAIL NaN in {name} at step {s}")
                    return 1
                # scale: largest reference magnitude of this column; energy-like columns whose
                # reference is round-off (e.g. erot ~1e-36 for a non-spinning case) are compared on
                # the scale of the total energy instead
                scale = max(abs(x[j]) for x in old.values()) or 1.0
                if escale and name in ("ke", "erot", "epot", "etot") and scale < 1e-12 * escale:
                    scale = escale
                if not close(a, e, rtol, atol=rtol * scale):
                    d = abs(a - e) / scale
                    if d > worst.get(name, (0,))[0]:
                        worst[name] = (d, s, e, a)
        for name, (d, s, e, a) in worst.items():
            print(f"FAIL {name}: worst at step {s}: expected {e!r}, actual {a!r} (|diff|/max|ref| = {d:.3g} > {rtol:g})")
            fails += 1
        if "etot" in cols:
            j = cols.index("etot")
            e0 = new[steps[0]][j]
            emax = max(new[s][j] for s in steps)
            if emax > e0 * (1 + 1e-9):
                print(f"FAIL energy gain: max E_tot = {emax!r} > E_0 = {e0!r}")
                fails += 1
        print(f"{len(steps)} thermo rows x {len(cols)} columns compared, rtol {rtol:g}: "
              + ("PASS" if fails == 0 else f"{fails} failure(s)"))
        return 1 if fails else 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
