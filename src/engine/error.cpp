#include "error.h"

namespace engine {
namespace error {

class quickjs_error_category : public boost::system::error_category {
public:
    const char* name() const BOOST_NOEXCEPT {
        return "engine.quickjs";
    }

    std::string message(int value) const {
        if (value == quickjs_errors::eval_script_failed)
            return "failed to evaluate script, exception accurred";
        if (value == quickjs_errors::exec_pending_job_failed)
            return "failed to execute pending jobs";
        return "asio.misc error";
    }
};

const boost::system::error_category& get_quickjs_category() {
    static quickjs_error_category category;
    return category;
}

} // namespace error
} // namespace engine