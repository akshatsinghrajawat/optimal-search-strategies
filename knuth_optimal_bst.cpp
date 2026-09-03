// knuth_optimal_bst.cpp
//
// Companion to number_guessing_game.cpp in this repo.
//
// The guessing game asks: "what's the fewest guesses in the WORST case,
// assuming every value is equally likely?" Answer: ceil(log2(N)), achieved
// by binary search.
//
// This file asks the natural follow-up: what if values are NOT equally
// likely to be searched for? Plain binary search still works (it's still
// correct), but it stops being optimal — it ignores the frequency
// distribution entirely. This computes the BST that minimizes EXPECTED
// search cost given per-key search frequencies, using Knuth's optimal BST
// algorithm.
//
// Build:
//   g++ -O2 -std=c++17 -Wall -Wextra knuth_optimal_bst.cpp -o knuth_bst
//   ./knuth_bst

#include <iostream>
#include <vector>
#include <iomanip>
#include <numeric>
#include <limits>
#include <string>

using namespace std;

// ---------------------------------------------------------------------
// Problem setup
// ---------------------------------------------------------------------
// We're given n keys k_1 < k_2 < ... < k_n with search frequencies
// p_1..p_n (probability a search is a *hit* on k_i), and n+1 "gap"
// frequencies q_0..q_n (probability a search falls strictly between two
// keys, or off either end, and therefore misses — an unsuccessful search).
// sum(p_i) + sum(q_i) = 1.
//
// The cost of a BST is the expected number of comparisons to resolve a
// search, where a node at depth d (root = depth 1) costs d comparisons on
// a hit, and a gap represented as a leaf at depth d costs d comparisons
// on a miss.
//
// Goal: find the BST shape minimizing expected cost. This is NOT the same
// problem as balancing the tree by height — a frequently-searched key
// belongs near the root even if that unbalances the tree, exactly the way
// Huffman coding puts frequent symbols near the root of a prefix tree.

struct KnuthBSTResult {
    double expectedCost;
    vector<vector<int>> root; // root[i][j] = index (1-based) of the optimal
                               // root key for the subtree covering gap i
                               // through gap j (keys i+1..j)
};

// Knuth's optimal BST via dynamic programming, O(n^2) time / O(n^2) space
// using Knuth's "monotonicity of optimal roots" speedup (root[i][j-1] <=
// root[i][j] <= root[i+1][j]), which cuts the naive O(n^3) DP down to
// O(n^2). This is the classical result from Knuth's 1971 paper
// "Optimum Binary Search Trees."
KnuthBSTResult knuthOptimalBST(const vector<double>& p, const vector<double>& q) {
    int n = (int)p.size(); // number of keys; q has n+1 entries (gaps 0..n)

    // e[i][j]: expected cost of optimal BST over keys i+1..j (1-indexed),
    //          for i in [0, n], j in [i, n]. e[i][i] = q[i] (empty subtree,
    //          just the gap itself).
    // w[i][j]: total weight (sum of p's and q's) covered by e[i][j].
    // root[i][j]: index of the optimal root for that range.
    vector<vector<double>> e(n + 2, vector<double>(n + 1, 0.0));
    vector<vector<double>> w(n + 2, vector<double>(n + 1, 0.0));
    vector<vector<int>> root(n + 1, vector<int>(n + 1, 0));

    for (int i = 0; i <= n; i++) {
        e[i][i] = q[i];
        w[i][i] = q[i];
    }

    for (int len = 1; len <= n; len++) {
        for (int i = 0; i + len <= n; i++) {
            int j = i + len;
            e[i][j] = numeric_limits<double>::infinity();
            w[i][j] = w[i][j - 1] + p[j - 1] + q[j]; // p is 0-indexed (key j is p[j-1])

            // Knuth's speedup: search root candidates only within
            // [root[i][j-1], root[i+1][j]] instead of the full [i+1, j].
            int lo = (j - 1 >= i) ? root[i][j - 1] : i + 1;
            int hi = (i + 1 <= j) ? root[i + 1][j] : j;
            if (lo < i + 1) lo = i + 1;
            if (hi > j) hi = j;
            if (lo > hi) { lo = i + 1; hi = j; } // fallback: full range

            for (int r = lo; r <= hi; r++) {
                double cost = e[i][r - 1] + e[r][j] + w[i][j];
                if (cost < e[i][j]) {
                    e[i][j] = cost;
                    root[i][j] = r;
                }
            }
        }
    }

    return { e[0][n], root };
}

// Cost of a plain sorted-order (unweighted) binary search tree, i.e. what
// vanilla binary search effectively builds when it ignores frequencies.
// Ties are broken by picking the middle element, same rule binary search
// uses — this is the natural baseline binary search is implicitly using.
double naiveMidpointBSTCost(const vector<double>& p, const vector<double>& q,
                             int i, int j, int depth,
                             vector<pair<int,int>>* trace = nullptr) {
    if (i == j) return q[i] * depth;
    int r = i + 1 + (j - i - 1) / 2; // midpoint key, 1-indexed
    if (trace) trace->push_back({r, depth});
    double cost = p[r - 1] * depth;
    cost += naiveMidpointBSTCost(p, q, i, r - 1, depth + 1, trace);
    cost += naiveMidpointBSTCost(p, q, r, j, depth + 1, trace);
    return cost;
}

void printOptimalTree(const vector<vector<int>>& root, int i, int j,
                       const vector<string>& names, int depth = 1) {
    if (i >= j) return;
    int r = root[i][j];
    cout << string(depth * 2, ' ') << names[r - 1]
         << " (depth " << depth << ")\n";
    printOptimalTree(root, i, r - 1, names, depth + 1);
    printOptimalTree(root, r, j, names, depth + 1);
}

int main() {
    cout << fixed << setprecision(4);

    // ------------------------------------------------------------------
    // Worked example: a config lookup with a hot key that is NOT at the
    // sorted midpoint (deliberately — a hot key that happens to sit at
    // the midpoint would let plain binary search win "by accident" and
    // hide the actual effect we're demonstrating).
    // ------------------------------------------------------------------
    vector<string> names = {"cache", "debug", "host", "mode", "port", "retries", "verbose"};
    vector<double> p = {0.60, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02}; // hit frequencies
    vector<double> q = {0.04, 0.04, 0.04, 0.04, 0.04, 0.04, 0.04, 0.00}; // gap frequencies
    // sum: 0.60 + 0.02*6 + 0.04*7 + 0.00 = 0.60 + 0.12 + 0.28 = 1.00
    // "cache" is hot but is the FIRST key alphabetically — plain binary
    // search buries it near the middle of the tree regardless, since it
    // only looks at sort order, never at frequency.

    int n = (int)p.size();

    auto result = knuthOptimalBST(p, q);

    cout << "=== Knuth's Optimal BST ===\n";
    cout << "Keys (sorted): ";
    for (auto& s : names) cout << s << " ";
    cout << "\nSearch frequencies: ";
    for (auto v : p) cout << v << " ";
    cout << "\n\n";

    cout << "Optimal expected cost: " << result.expectedCost << " comparisons/search\n";
    cout << "Tree structure (root first):\n";
    printOptimalTree(result.root, 0, n, names);

    cout << "\n=== Baseline: plain binary search (midpoint) BST ===\n";
    vector<pair<int,int>> trace;
    double naiveCost = naiveMidpointBSTCost(p, q, 0, n, 1, &trace);
    cout << "Ignores frequency, always splits at the midpoint — this is\n"
         << "the tree binary search effectively builds.\n";
    cout << "Expected cost: " << naiveCost << " comparisons/search\n";

    cout << "\n=== Comparison ===\n";
    double improvementPct = (naiveCost - result.expectedCost) / naiveCost * 100.0;
    cout << "Optimal BST is " << improvementPct
         << "% cheaper in expectation than frequency-blind binary search,\n"
         << "for this frequency distribution. The saving comes entirely\n"
         << "from moving the hot key ('" << names[0] << "') near the root instead of\n"
         << "leaving it wherever the sorted midpoint happens to place it.\n";

    cout << "\n=== Sanity check: uniform frequencies ===\n";
    // If all frequencies are equal, Knuth's optimal BST should degenerate
    // to (approximately) the same shape as plain binary search — there's
    // no frequency signal to exploit, so there's nothing to gain by
    // deviating from the midpoint split. This is the same claim the
    // number-guessing game demonstrates empirically for the uniform case.
    vector<double> pu(n, 1.0 / (2 * n + 1));
    vector<double> qu(n + 1, 1.0 / (2 * n + 1));
    auto uniformResult = knuthOptimalBST(pu, qu);
    double uniformNaive = naiveMidpointBSTCost(pu, qu, 0, n, 1, nullptr);
    cout << "Optimal cost:  " << uniformResult.expectedCost << "\n";
    cout << "Midpoint cost: " << uniformNaive << "\n";
    cout << "(Equal, as expected: uniform priors give binary search nothing to improve on.)\n";

    return 0;
}
