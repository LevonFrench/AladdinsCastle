// SPDX-License-Identifier: MIT
#pragma once
#include "runtime_host.hpp"

namespace acvr {
// Optional Windows provider. ACVR_MORE means the session is not yet RUNNING;
// ACVR_STOPPED means an orderly runtime exit. Neither creates an end obligation.
acvr_result make_openxr_gl_host(const char *application_name_utf8,const char *content_root_utf8,
                               const char *gun_model_id_utf8,std::unique_ptr<Host> &out);
}
