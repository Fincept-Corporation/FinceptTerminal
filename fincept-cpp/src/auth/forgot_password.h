#pragma once
#include "core/app_screen.h"
#include <string>

namespace fincept::auth {

class ForgotPasswordScreen {
public:
    void render(fincept::AppScreen& next_screen);
    void reset();

private:
    enum Step { EMAIL, OTP_RESET, SUCCESS };
    Step step_ = EMAIL;

    char email_[256] = {};
    char otp_[64] = {};
    char new_password_[256] = {};
    char confirm_password_[256] = {};

    bool loading_ = false;
    std::string error_;
    std::string success_msg_;
};

} // namespace fincept::auth
