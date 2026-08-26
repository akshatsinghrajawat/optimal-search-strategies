# optimal-search-strategies

Empirically verifying information-theoretic lower bounds on search strategies
-- and checking each claim against real data instead of just asserting it.

## What this does

Starts from a simple question: is binary search's `ceil(log2 N)` worst case
actually optimal? The answer turns out to be "only under specific
assumptions" -- and this repo makes each of those assumptions explicit,
then tests where they break.

```
./guess play                                        # you guess, computer thinks of a number
./guess demo                                         # computer guesses your number
./guess simulate --min 1 --max 100000 --trials 100000 --seed 1
```

`simulate` runs binary search, linear scan, and interpolation search against
random secrets, checks the measured worst case against the theoretical
bound (failing loudly with a non-zero exit code if they ever disagree), and
reports a 95% confidence interval, a chi-square check on the RNG's
uniformity, and an entropy-optimal comparison under a skewed (Zipf) prior.
Add `--json` for machine-readable output or `--svg chart.svg` for a bar
chart (native C++, no external plotting library).

## A real bug this project found

Parameterizing the range (instead of hardcoding N=100) and adding exhaustive
tests at edge cases surfaced a genuine bug: the original bound formula,
`ceil(log2 N)`, is wrong at every exact power of two. The correct formula is
the smallest `k` such that `2^k >= N+1`. The two only disagree when N is a
power of two -- which is exactly why this was invisible with the old
hardcoded default of N=100. Confirmed exhaustively for N in
{1,2,4,8,16,32,64,128}: the old formula was wrong at every single one, by
exactly 1, always in the optimistic direction. See the commit
`fix: correct information-theoretic bound formula...` for the full
before/after.

## What's actually verified, not just claimed

- **Binary search's worst case matches the (corrected) bound exactly**,
  asserted with a real pass/fail exit code, not just printed for inspection.
- **Interpolation search's O(log log N) win on uniform data** and its
  **adversarial collapse toward O(N)** on a constructed Fibonacci-spaced
  array -- both measured, not asserted.
- **Entropy-optimal search beats plain binary search under a skewed prior**,
  and its expected query count is checked against the Shannon entropy lower
  bound `H(p)` using a matched yes/no query model (an earlier version of
  this comparison used the game's normal 3-way higher/lower/exact feedback,
  which let it appear to beat `H(p)` -- impossible for a valid decision
  procedure. Caught in testing; see that commit for the full story).
- **The RNG's uniformity** via chi-square test, and **reproducibility** via
  `--seed`, verified with byte-identical output across separate process
  launches.

## Build

```
g++ -std=c++17 -Wall -Wextra number_guessing_game.cpp -o guess
g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined tests.cpp -o tests
./tests
```

Or with CMake: `cmake -B build && cmake --build build`.

## Reading the history

Each commit is a single, atomic change -- compiled and tested before being
committed, not written after the fact. The commit messages explain the
*why* behind each change, including the two things that were caught by the
testing infrastructure itself (the power-of-two bound bug, and the
entropy-model bug above) rather than planned in advance. `git log -p` tells
that story in order.
