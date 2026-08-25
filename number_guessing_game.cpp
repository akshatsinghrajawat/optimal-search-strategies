#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

constexpr int LOWER = 1;
constexpr int UPPER = 100;

std::mt19937& rng()
{
    static std::mt19937 engine(static_cast<unsigned int>(std::time(nullptr)));
    return engine;
}

int uniformInt(int lo, int hi)
{
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng());
}

int informationTheoreticLowerBound(int rangeSize)
{
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(rangeSize))));
}

int readIntInRange(const std::string& prompt, int lo, int hi)
{
    int value;
    while (true)
    {
        std::cout << prompt;
        std::cin >> value;
        if (std::cin.fail() || value < lo || value > hi)
        {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Please enter a number between " << lo << " and " << hi << ".\n";
            continue;
        }
        return value;
    }
}

void playHumanGuesses()
{
    int secret = uniformInt(LOWER, UPPER);
    int attempts = 0;
    std::cout << "\nI'm thinking of a number between " << LOWER << " and " << UPPER << ".\n";
    while (true)
    {
        int guess = readIntInRange("Your guess: ", LOWER, UPPER);
        ++attempts;
        if (guess == secret) { std::cout << "Correct in " << attempts << " tries!\n"; return; }
        std::cout << (guess < secret ? "Higher.\n" : "Lower.\n");
    }
}

void playComputerGuesses()
{
    int lo = LOWER, hi = UPPER, attempts = 0;
    std::cout << "\nThink of a number between " << LOWER << " and " << UPPER << ".\n";
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        ++attempts;
        std::cout << "Is it " << mid << "? (h/l/c): ";
        char response; std::cin >> response;
        if (response == 'c') { std::cout << "Found it in " << attempts << " tries!\n"; return; }
        if (response == 'h') hi = mid - 1; else lo = mid + 1;
    }
    std::cout << "Something's inconsistent with your answers.\n";
}

struct Stats { double mean = 0, variance = 0; int worstCase = 0; };

void runSimulation()
{
    int trials = 100000;
    std::vector<int> bsearchCounts(trials), linearCounts(trials);

    for (int t = 0; t < trials; ++t)
    {
        int secret = uniformInt(LOWER, UPPER);
        int lo = LOWER, hi = UPPER, attempts = 0;
        while (lo <= hi)
        {
            int mid = lo + (hi - lo) / 2;
            ++attempts;
            if (mid == secret) break;
            if (mid < secret) lo = mid + 1; else hi = mid - 1;
        }
        bsearchCounts[t] = attempts;
        linearCounts[t] = secret - LOWER + 1;
    }

    Stats bsearch, linear;
    bsearch.worstCase = *std::max_element(bsearchCounts.begin(), bsearchCounts.end());
    linear.worstCase = *std::max_element(linearCounts.begin(), linearCounts.end());
    double bsum = 0, lsum = 0;
    for (int a : bsearchCounts) bsum += a;
    for (int a : linearCounts) lsum += a;
    bsearch.mean = bsum / trials;
    linear.mean = lsum / trials;
    double bsq = 0, lsq = 0;
    for (int a : bsearchCounts) bsq += (a - bsearch.mean) * (a - bsearch.mean);
    for (int a : linearCounts) lsq += (a - linear.mean) * (a - linear.mean);
    bsearch.variance = bsq / trials;
    linear.variance = lsq / trials;

    int bound = informationTheoreticLowerBound(UPPER - LOWER + 1);
    std::cout << "\nBinary search: mean=" << bsearch.mean << " worst=" << bsearch.worstCase
              << " (bound=" << bound << ")\n"
              << "Linear scan:   mean=" << linear.mean << " worst=" << linear.worstCase << "\n";
}

int main()
{
    int mode = readIntInRange("1) Play  2) Computer guesses  3) Simulate: ", 1, 3);
    if (mode == 1) playHumanGuesses();
    else if (mode == 2) playComputerGuesses();
    else runSimulation();
    return 0;
}
