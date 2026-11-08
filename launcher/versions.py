"""Release versions: "0.3.11", optionally with build metadata ("0.3.11+abc1234") that never orders releases."""
import re

_NUM = re.compile(r'^\s*v?(\d+(?:\.\d+)*)(?:-([0-9A-Za-z.-]+))?(?:\+([0-9A-Za-z.-]+))?\s*$')


def parse(text):
    """(numbers, pre-release) for comparing; a pre-release sorts before its release. ValueError when malformed."""
    m = _NUM.match(text or '')
    if not m:
        raise ValueError(f'not a version: {text!r}')
    nums = tuple(int(x) for x in m.group(1).split('.'))
    nums = nums + (0,) * (4 - len(nums)) if len(nums) < 4 else nums
    pre = m.group(2)
    return nums, (1,) if pre is None else (0, pre)


def newer(a, b):
    """True when version a is newer than version b."""
    return parse(a) > parse(b)


def display(text):
    """The version without build metadata."""
    return (text or '').split('+', 1)[0].strip()
