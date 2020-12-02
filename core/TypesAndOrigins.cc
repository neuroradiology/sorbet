#include "Types.h"
#include "common/sort.h"

using namespace std;
namespace sorbet::core {

// This sorts the underlying `origins`.
//
// `originForUninitialized` is some local state that we pipe in here for the
// sake of better error messages. Basically, when a variable takes on the type
// `NilClass` due to its being uninitialized, we will assign a `Loc` for its
// origin that corresponds to the enclosing `def` itself, rather than to a
// specific initializing statement. `originForUninitialized` is that loc, and
// we test for equality against it to decide whether to print a special error
// message. Note that it is reasonable to pass `Loc::none()` as
// `originForUninitialized` only if the information is truly not applicable
// (e.g. in "fake" dispatches used to generate signature suggestions). In other
// cases, passing `Loc::none()` will result in confusing error messages.
vector<ErrorLine> TypeAndOrigins::origins2Explanations(const GlobalState &gs, Loc originForUninitialized) const {
    vector<ErrorLine> result;

    auto compare = [originForUninitialized](Loc left, Loc right) {
        // Put "uninitialized" origins towards the end, since the overall
        // message reads better that way.
        if (originForUninitialized.exists()) {
            if (left == originForUninitialized && right != originForUninitialized) {
                return false;
            }
            if (left != originForUninitialized && right == originForUninitialized) {
                return true;
            }
        }

        if (left.file() != right.file()) {
            return left.file().id() < right.file().id();
        }
        if (left.beginPos() != right.beginPos()) {
            return left.beginPos() < right.beginPos();
        }
        if (left.endPos() != right.endPos()) {
            return left.endPos() < right.endPos();
        }
        return false;
    };
    auto sortedOrigins = origins;
    fast_sort(sortedOrigins, compare);
    Loc last;
    for (auto o : sortedOrigins) {
        if (o == last) {
            continue;
        }
        last = o;

        if (originForUninitialized.exists() && o == originForUninitialized) {
            result.emplace_back(ErrorLine::from(o, "Possibly uninitialized (`{}`) in:", "NilClass"));
        } else {
            result.emplace_back(o, "");
        }
    }
    return result;
}

TypeAndOrigins::~TypeAndOrigins() noexcept {
    histogramInc("TypeAndOrigins.origins.size", origins.size());
}

} // namespace sorbet::core
