from pathlib import Path
from subprocess import PIPE, STDOUT, run

TEST = Path(__file__).resolve().parent
ROOT = TEST.parent

class CompilationError(Exception):
    pass

def compile(source: Path, cflags=[], ldadd=[]):
    binary = source.with_suffix(".so")

    result = run(
        ["gcc", "-shared", *cflags, "-o", str(binary), str(source), *ldadd],
        stdout=PIPE,
        stderr=STDOUT,
        cwd=ROOT,
    )

    if (result.returncode == 0):
        return

    raise CompilationError(result.stdout.decode())
