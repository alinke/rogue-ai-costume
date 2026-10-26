# Copies the costume's LED code into src/gen/ before each build, so it compiles
# against this project's config.h and gfx.h (a quoted #include looks in the
# including file's own folder first -- compiled in place it would pick up the
# costume's).
import os
import shutil

Import("env")  # noqa: F821

here = env.subst("$PROJECT_DIR")  # noqa: F821
src = os.path.join(here, "..", "costume", "src")
gen = os.path.join(here, "src", "gen")
os.makedirs(gen, exist_ok=True)
for name in ("leds.cpp", "leds.h"):
    a, b = os.path.join(src, name), os.path.join(gen, name)
    if not os.path.exists(b) or open(a, "rb").read() != open(b, "rb").read():
        shutil.copyfile(a, b)
        print("[ledlink] copied", name)
