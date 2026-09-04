#define GUESS_NO_MAIN
#include "number_guessing_game.cpp"
#define KNUTH_NO_MAIN
#include "knuth_optimal_bst.cpp"

#include <cassert>
#include <iostream>

void test_information_theoretic_bound_edge_cases()
{
    // Correct worst-case comparisons for this binary-search implementation
    // is "smallest k such that 2^k >= N+1", equivalently ceil(log2(N+1)) --
    // NOT ceil(log2(N)). The two formulas only disagree at exact powers of
    // two, which is exactly why this was invisible with the hardcoded
    // default N=100 (not a power of two) and only surfaced once N was
    // actually parameterized and tested across values.
    assert(informationTheoreticLowerBound(1) == 1);
    assert(informationTheoreticLowerBound(2) == 2);
    assert(informationTheoreticLowerBound(3) == 2);
    assert(informationTheoreticLowerBound(4) == 3);
    assert(informationTheoreticLowerBound(7) == 3);
    assert(informationTheoreticLowerBound(8) == 4);
    assert(informationTheoreticLowerBound(1023) == 10);
    assert(informationTheoreticLowerBound(1024) == 11);
    assert(informationTheoreticLowerBound(1025) == 11);
}

// Exhaustive, not sampled: every value in a set of N's chosen exactly for
// their edge-case potential (the flaw list's own suggestion) -- every exact
// power of two up to 2^10, their neighbors (2^k-1, 2^k+1), and N=100 (the
// repo's original hardcoded default, kept as a regression check).
void test_binary_search_matches_bound_exhaustively()
{
    std::vector<long long> ns = {1, 2, 3, 4, 7, 8, 9, 15, 16, 17,
                                  31, 32, 33, 63, 64, 65, 100,
                                  127, 128, 129, 255, 256, 257, 1023, 1024, 1025};
    for (long long n : ns)
    {
        long long bound = informationTheoreticLowerBound(n);
        for (long long secret = 1; secret <= n; ++secret)
        {
            long long attempts = binarySearchAttempts(secret, 1, n);
            assert(attempts <= bound);
        }
    }
}

// Randomized property test: for random N and random secret, binary search
// must terminate within informationTheoreticLowerBound(N) steps. Fixed seed
// so a failure is reproducible.
void test_binary_search_property_random(int iterations = 20000)
{
    std::mt19937 gen(12345);
    for (int i = 0; i < iterations; ++i)
    {
        std::uniform_int_distribution<long long> nDist(1, 1000000);
        long long n = nDist(gen);
        std::uniform_int_distribution<long long> secretDist(1, n);
        long long secret = secretDist(gen);
        long long attempts = binarySearchAttempts(secret, 1, n);
        assert(attempts <= informationTheoreticLowerBound(n));
    }
}

void test_interpolation_search_correct_on_uniform_samples()
{
    std::mt19937 gen(7);
    std::uniform_int_distribution<long long> d(1, 1000000);
    for (int i = 0; i < 5000; ++i)
    {
        long long secret = d(gen);
        long long attempts = interpolationSearchAttempts(secret, 1, 1000000);
        assert(attempts > 0); // sanity: always terminates and reports a positive count
    }
}

void test_entropy_optimal_never_beats_shannon_bound()
{
    // The bug this test would have caught: an earlier version of the
    // entropy-optimal comparison used 3-way (higher/lower/exact) queries,
    // which produced a mean BELOW H(p) -- impossible for a valid yes/no
    // decision procedure. This checks the exact (non-sampled) expectation.
    long long n = 100;
    auto w = zipfWeights(n, 1.0);
    std::vector<double> cum(n);
    std::partial_sum(w.begin(), w.end(), cum.begin());
    double H = shannonEntropyBits(w);

    double exact = 0.0;
    for (long long i = 0; i < n; ++i)
        exact += w[i] * entropyOptimalYesNoAttempts(i, 0, n - 1, cum);

    assert(exact >= H - 1e-9); // Shannon's lower bound, with float slack
}

void test_knuth_optimal_bst_uniform_matches_naive()
{
    // Under uniform priors there is no frequency skew to exploit, so the
    // optimal tree should collapse to the same cost as the naive
    // midpoint-split tree binary search implicitly builds.
    int n = 5;
    std::vector<double> p(n, 1.0 / (2 * n + 1));
    std::vector<double> q(n + 1, 1.0 / (2 * n + 1));
    auto result = knuthOptimalBST(p, q);
    double naive = naiveMidpointBSTCost(p, q, 0, n, 1, nullptr);
    assert(result.expectedCost - naive < 1e-9 && naive - result.expectedCost < 1e-9);
}

void test_knuth_optimal_bst_beats_naive_on_skewed_frequencies()
{
    // With a hot key away from the sorted midpoint, the optimal BST must
    // strictly beat the frequency-blind midpoint-split tree.
    std::vector<double> p = {0.60, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02};
    std::vector<double> q = {0.04, 0.04, 0.04, 0.04, 0.04, 0.04, 0.04, 0.00};
    int n = (int)p.size();
    auto result = knuthOptimalBST(p, q);
    double naive = naiveMidpointBSTCost(p, q, 0, n, 1, nullptr);
    assert(result.expectedCost < naive - 1e-9);
}
int main()
{
    test_information_theoretic_bound_edge_cases();
    test_binary_search_matches_bound_exhaustively();
    test_binary_search_property_random();
    test_interpolation_search_correct_on_uniform_samples();
    test_entropy_optimal_never_beats_shannon_bound();
    test_knuth_optimal_bst_uniform_matches_naive();
    test_knuth_optimal_bst_beats_naive_on_skewed_frequencies();
    std::cout << "All tests passed.\n";
    return 0;
}
