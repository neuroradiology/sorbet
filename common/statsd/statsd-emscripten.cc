#include "common/statsd/statsd.h"
using namespace std;

namespace sorbet {

bool StatsD::submitCounters(const CounterState &counters, string_view host, int port, string_view prefix) {
    return false;
}

void StatsD::addStandardMetrics() {}
void StatsD::addExtraTags(const map<string, string> &tags) {}

} // namespace sorbet
