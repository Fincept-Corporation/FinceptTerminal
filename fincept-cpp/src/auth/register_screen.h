#pragma once
#include "app.h"
#include "utils/validators.h"
#include <string>

namespace fincept::auth {

class RegisterScreen {
public:
    void render(fincept::AppScreen& next_screen);
    void reset();

private:
    enum Step { FORM, OTP_VERIFICATION, COMPLETE };
    Step step_ = FORM;

    char first_name_[128] = {};
    char last_name_[128] = {};
    char email_[256] = {};
    char phone_[64] = {};
    char country_code_[16] = {'+', '1', '\0'};
    char password_[256] = {};
    char confirm_password_[256] = {};
    char otp_[64] = {};

    bool loading_ = false;
    std::string error_;

    utils::PasswordStrength pw_strength_;
    bool passwords_match_ = true;
};

} // namespace fincept::auth
