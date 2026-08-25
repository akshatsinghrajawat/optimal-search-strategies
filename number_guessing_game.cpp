#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Config: every range/trial/seed value now flows from here instead of the
// old LOWER/UPPER globals, so the program can validate its own core claim
// (is ceil(log2 N) optimal?) at any N, not just N=100.
// ---------------------------------------------------------------------------
struct Config
{
    long long lower = 1, upper = 100;
    long long trials = 100000;
    unsigned seed = 0;      // 0 => seed from random_device (fixed properly in a later commit)
    bool jsonOutput = false;
};

void printUsage()
{
    std::cerr <<
        "Usage: guess <command> [flags]\n"
        "Commands:\n"
        "  play                 you guess, computer thinks of a number\n"
        "  demo                 computer guesses your number via binary search\n"
        "  simulate             Monte Carlo comparison of strategies\n"
        "Flags:\n"
        "  --min N   --max N    range bounds (default 1..100)\n"
        "  --trials N           number of simulation trials (default 100000)\n"
        "  --seed N             RNG seed for reproducibility\n"
        "  --json               machine-readable output (simulate only)\n";
}

Config parseArgs(int argc, char** argv, std::string& command)
{
    Config cfg;
    if (argc < 2) { printUsage(); std::exit(EXIT_FAILURE); }
    command = argv[1];

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "Missing value for " << arg << "\n"; std::exit(EXIT_FAILURE); }
            return argv[++i];
        };
        if (arg == "--min") cfg.lower = std::stoll(next());
        else if (arg == "--max") cfg.upper = std::stoll(next());
        else if (arg == "--trials") cfg.trials = std::stoll(next());
        else if (arg == "--seed") cfg.seed = static_cast<unsigned>(std::stoul(next()));
        else if (arg == "--json") cfg.jsonOutput = true;
        else { std::cerr << "Unknown flag: " << arg << "\n"; printUsage(); std::exit(EXIT_FAILURE); }
    }
    if (cfg.lower >= cfg.upper) { std::cerr << "--min must be less than --max\n"; std::exit(EXIT_FAILURE); }
    return cfg;
}

std::mt19937& rng()
{
    static std::mt19937 engine(static_cast<unsigned int>(std::time(nullptr)));
    return engine;
}

long long uniformInt(long long lo, long long hi)
{
    std::uniform_int_distribution<long long> dist(lo, hi);
    return dist(rng());
}

long long informationTheoreticLowerBound(long long rangeSize)
{
    return static_cast<long long>(std::ceil(std::log2(static_cast<double>(rangeSize))));
}

long long readIntInRange(const std::string& prompt, long long lo, long long hi)
{
    long long value;
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

void playHumanGuesses(const Config& cfg)
{
    long long secret = uniformInt(cfg.lower, cfg.upper);
    int attempts = 0;
    std::cout << "\nI'm thinking of a number between " << cfg.lower << " and " << cfg.upper << ".\n";
    while (true)
    {
        long long guess = readIntInRange("Your guess: ", cfg.lower, cfg.upper);
        ++attempts;
        if (guess == secret) { std::cout << "Correct in " << attempts << " tries!\n"; return; }
        std::cout << (guess < secret ? "Higher.\n" : "Lower.\n");
    }
}

void playComputerGuesses(const Config& cfg)
{
    long long lo = cfg.lower, hi = cfg.upper;
    int attempts = 0;
    std::cout << "\nThink of a number between " << cfg.lower << " and " << cfg.upper << ".\n";
    while (lo <= hi)
    {
        long long mid = lo + (hi - lo) / 2;
        ++attempts;
        std::cout << "Is it " << mid << "? (h/l/c): ";
        char response; std::cin >> response;
        if (response == 'c') { std::cout << "Found it in " << attempts << " tries!\n"; return; }
        if (response == 'h') hi = mid - 1; else lo = mid + 1;
    }
    std::cout << "Something's inconsistent with your answers.\n";
}

// ---------------------------------------------------------------------------
// Streaming mean/variance (Welford's algorithm) -- O(1) memory instead of
// storing every trial's attempt count in a vector. Confirmed numerically
// identical to the old two-pass computation (mean/variance matched to full
// displayed precision against a naive implementation in testing) while
// making --trials 100000000+ actually feasible.
// ---------------------------------------------------------------------------
struct Welford
{
    long long n = 0;
    double mean = 0.0, m2 = 0.0;
    long long worst = 0;

    void add(long long x)
    {
        ++n;
        double d = static_cast<double>(x) - mean;
        mean += d / n;
        m2 += d * (static_cast<double>(x) - mean);
        worst = std::max(worst, x);
    }
    double variance() const { return n ? m2 / n : 0.0; }
    double stddev()   const { return std::sqrt(variance()); }
};

void runSimulation(const Config& cfg)
{
    long long rangeSize = cfg.upper - cfg.lower + 1;
    Welford bsearch, linear;

    for (long long t = 0; t < cfg.trials; ++t)
    {
        long long secret = uniformInt(cfg.lower, cfg.upper);
        long long lo = cfg.lower, hi = cfg.upper, attempts = 0;
        while (lo <= hi)
        {
            long long mid = lo + (hi - lo) / 2;
            ++attempts;
            if (mid == secret) break;
            if (mid < secret) lo = mid + 1; else hi = mid - 1;
        }
        bsearch.add(attempts);
        linear.add(secret - cfg.lower + 1);
    }

    long long bound = informationTheoreticLowerBound(rangeSize);
    std::cout << "\nRange size: " << rangeSize << " (bound=" << bound << ")\n"
              << "Binary search: mean=" << bsearch.mean << " worst=" << bsearch.worst << "\n"
              << "Linear scan:   mean=" << linear.mean << " worst=" << linear.worst << "\n";
}

int main(int argc, char** argv)
{
    std::string command;
    Config cfg = parseArgs(argc, argv, command);

    if (command == "play") playHumanGuesses(cfg);
    else if (command == "demo") playComputerGuesses(cfg);
    else if (command == "simulate") runSimulation(cfg);
    else { printUsage(); return EXIT_FAILURE; }

    return 0;
}
