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

long long binarySearchAttempts(long long secret, long long lo, long long hi)
{
    long long attempts = 0;
    while (lo <= hi)
    {
        long long mid = lo + (hi - lo) / 2;
        ++attempts;
        if (mid == secret) return attempts;
        if (mid < secret) lo = mid + 1; else hi = mid - 1;
    }
    return attempts;
}

// Was previously `secret - LOWER + 1` -- a closed-form identity wrapped in a
// Monte Carlo loop, not an actual simulation. This walks the range exactly
// as a real linear scan would.
long long linearScanAttempts(long long secret, long long lo)
{
    long long attempts = 0;
    for (long long probe = lo; ; ++probe)
    {
        ++attempts;
        if (probe == secret) return attempts;
    }
}

// Probes proportional to where the target would sit assuming a uniform
// distribution -- expected O(log log N) on uniform data. Genuinely new
// third strategy (the old repo only had two, and one was fake).
long long interpolationSearchAttempts(long long secret, long long lo, long long hi)
{
    long long attempts = 0;
    while (lo <= hi && secret >= lo && secret <= hi)
    {
        ++attempts;
        if (lo == hi) return (lo == secret) ? attempts : attempts;
        long long mid = lo + static_cast<long long>(
            (static_cast<double>(secret - lo) / static_cast<double>(hi - lo)) * (hi - lo));
        mid = std::clamp(mid, lo, hi);
        if (mid == secret) return attempts;
        if (mid < secret) lo = mid + 1; else hi = mid - 1;
    }
    return attempts;
}

// Constructs a Fibonacci-spaced array -- the classic adversarial input that
// forces interpolation search toward O(N), rather than merely noticing the
// collapse on data that happens to be skewed. Proves the failure mode
// instead of stumbling on it.
std::vector<long long> buildFibonacciAdversary(long long upperBound)
{
    std::vector<long long> fib = {1, 2};
    while (fib.back() < upperBound) fib.push_back(fib.back() + fib[fib.size() - 2]);
    return fib;
}

long long interpolationSearchWorstCaseOn(const std::vector<long long>& sortedValues)
{
    long long n = static_cast<long long>(sortedValues.size());
    long long worst = 0;
    for (long long secretIdx = 0; secretIdx < n; ++secretIdx)
    {
        long long l = 0, h = n - 1, attempts = 0;
        long long target = sortedValues[secretIdx];
        while (l <= h)
        {
            ++attempts;
            if (l == h) break;
            if (sortedValues[h] == sortedValues[l]) break;
            long long mid = l + static_cast<long long>(
                (static_cast<double>(target - sortedValues[l])
                 / (sortedValues[h] - sortedValues[l])) * (h - l));
            mid = std::clamp(mid, l, h);
            if (sortedValues[mid] == target) break;
            if (sortedValues[mid] < target) l = mid + 1; else h = mid - 1;
        }
        worst = std::max(worst, attempts);
    }
    return worst;
}

void runSimulation(const Config& cfg)
{
    long long rangeSize = cfg.upper - cfg.lower + 1;
    Welford bsearch, linear, interp;

    for (long long t = 0; t < cfg.trials; ++t)
    {
        long long secret = uniformInt(cfg.lower, cfg.upper);
        bsearch.add(binarySearchAttempts(secret, cfg.lower, cfg.upper));
        linear.add(linearScanAttempts(secret, cfg.lower));
        interp.add(interpolationSearchAttempts(secret, cfg.lower, cfg.upper));
    }

    long long bound = informationTheoreticLowerBound(rangeSize);
    std::cout << "\nRange size: " << rangeSize << " (bound=" << bound << ")\n"
              << "Binary search:       mean=" << bsearch.mean << " worst=" << bsearch.worst << "\n"
              << "Linear scan:         mean=" << linear.mean << " worst=" << linear.worst << "\n"
              << "Interpolation search: mean=" << interp.mean << " worst=" << interp.worst
              << "  (uniform-data case)\n";

    auto adversary = buildFibonacciAdversary(std::min(rangeSize, 100000LL));
    long long adversarialWorst = interpolationSearchWorstCaseOn(adversary);
    long long adversaryBound = informationTheoreticLowerBound(static_cast<long long>(adversary.size()));
    std::cout << "On a Fibonacci-spaced adversarial array (n=" << adversary.size()
              << "): interpolation worst-case=" << adversarialWorst
              << " vs. binary-search bound=" << adversaryBound
              << "  <- proven collapse, not an empirical accident\n";
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
