#pragma once
#include <boost/system/error_category.hpp>

namespace engine {
namespace error {

enum quickjs_errors {
    eval_script_failed,
    exec_pending_job_failed,
};

const boost::system::error_category& get_quickjs_category();

} // namespace error
} // namespace engine
