#include "crypto_trading_screen.h"
#include "crypto_internal.h"
#include "ui/theme.h"
#include "trading/exchange_service.h"
#include "storage/database.h"
#include "core/logger.h"
#include <imgui.h>
#include <cstring>

namespace fincept::crypto {

using namespace theme::colors;


void CryptoTradingScreen::load_exchange_credentials() {
    std::string service = "exchange_" + exchange_id_;
    auto opt = db::ops::get_credential_by_service(service);
    if (opt) {
        std::strncpy(cred_api_key_, opt->api_key.value_or("").c_str(), sizeof(cred_api_key_) - 1);
        std::strncpy(cred_api_secret_, opt->api_secret.value_or("").c_str(), sizeof(cred_api_secret_) - 1);
        std::strncpy(cred_password_, opt->password.value_or("").c_str(), sizeof(cred_password_) - 1);

        has_credentials_ = (cred_api_key_[0] != '\0' && cred_api_secret_[0] != '\0');

        // Push to ExchangeService
        if (has_credentials_) {
            trading::ExchangeCredentials ec;
            ec.api_key = cred_api_key_;
            ec.secret = cred_api_secret_;
            ec.password = cred_password_;
            trading::ExchangeService::instance().set_credentials(ec);
        }
    } else {
        cred_api_key_[0] = '\0';
        cred_api_secret_[0] = '\0';
        cred_password_[0] = '\0';
        has_credentials_ = false;
    }
    credentials_loaded_ = true;
}

void CryptoTradingScreen::save_exchange_credentials() {
    std::string service = "exchange_" + exchange_id_;

    db::Credential c;
    c.service_name = service;
    c.api_key = std::string(cred_api_key_);
    c.api_secret = std::string(cred_api_secret_);
    c.password = std::string(cred_password_);
    db::ops::save_credential(c);

    has_credentials_ = (cred_api_key_[0] != '\0' && cred_api_secret_[0] != '\0');

    // Push to ExchangeService
    if (has_credentials_) {
        trading::ExchangeCredentials ec;
        ec.api_key = cred_api_key_;
        ec.secret = cred_api_secret_;
        ec.password = cred_password_;
        trading::ExchangeService::instance().set_credentials(ec);

        // Trigger initial live data fetch
        if (trading_mode_ == TradingMode::Live) {
            async_fetch_live_balance();
            async_fetch_live_positions();
            async_fetch_live_orders();
        }
    }

    LOG_INFO(TAG, "Credentials saved for %s (has_key=%d)", exchange_id_.c_str(), has_credentials_);
}

bool CryptoTradingScreen::has_credentials() const {
    return has_credentials_;
}

void CryptoTradingScreen::render_credentials_popup() {
    if (!show_credentials_popup_) return;

    ImGui::SetNextWindowSize(ImVec2(420, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Exchange API Credentials", &show_credentials_popup_,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking)) {

        // Exchange name header
        char header[128];
        std::snprintf(header, sizeof(header), "Configure API Key for: %s", exchange_id_.c_str());
        ImGui::TextColored(ACCENT, "%s", header);
        ImGui::Separator();
        ImGui::Spacing();

        float input_w = ImGui::GetContentRegionAvail().x - 16;

        // API Key
        ImGui::TextColored(TEXT_DIM, "API Key");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##cred_api_key", cred_api_key_, sizeof(cred_api_key_));
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // API Secret
        ImGui::TextColored(TEXT_DIM, "API Secret");
        ImGui::PushItemWidth(input_w);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
        ImGui::InputText("##cred_api_secret", cred_api_secret_, sizeof(cred_api_secret_),
                         ImGuiInputTextFlags_Password);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();

        ImGui::Spacing();

        // Password (for exchanges that require it: OKX, Kucoin, etc.)
        bool needs_password = (exchange_id_ == "okx" || exchange_id_ == "kucoin" ||
                               exchange_id_ == "gate" || exchange_id_ == "bitget");
        if (needs_password) {
            ImGui::TextColored(TEXT_DIM, "Passphrase / Password");
            ImGui::PushItemWidth(input_w);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, BG_INPUT);
            ImGui::InputText("##cred_password", cred_password_, sizeof(cred_password_),
                             ImGuiInputTextFlags_Password);
            ImGui::PopStyleColor();
            ImGui::PopItemWidth();
            ImGui::Spacing();
        }

        // Status
        if (has_credentials_) {
            ImGui::TextColored(MARKET_GREEN, "Credentials configured");
        } else {
            ImGui::TextColored(TEXT_DIM, "No credentials stored for this exchange");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        float btn_w = (input_w - 12) / 3;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        if (ImGui::Button("Save", ImVec2(btn_w, 28))) {
            save_exchange_credentials();
            show_credentials_popup_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 6);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        if (ImGui::Button("Clear", ImVec2(btn_w, 28))) {
            cred_api_key_[0] = '\0';
            cred_api_secret_[0] = '\0';
            cred_password_[0] = '\0';
            save_exchange_credentials();
            has_credentials_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine(0, 6);

        ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        if (ImGui::Button("Cancel", ImVec2(btn_w, 28))) {
            show_credentials_popup_ = false;
        }
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        // Security note
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        ImGui::TextWrapped("Credentials are stored encrypted in the local database. "
                           "Use read-only API keys when possible.");
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace fincept::crypto
