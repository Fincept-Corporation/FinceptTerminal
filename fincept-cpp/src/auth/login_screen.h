#pragma once
#include "app.h"
#include <string>

namespace fincept::auth {

class LoginScreen {
public:
    void render(fincept::AppScreen& next_screen);
    void reset();

private:
    char email_[256] = {};
    char password_[256] = {};
    char mfa_code_[64] = {};

    bool show_mfa_ = false;
    bool show_force_login_ = false;
    bool loading_ = false;
    std::string error_;
    std::string saved_email_;
};

} // namespace fincept::auth
