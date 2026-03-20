// Minimal standalone ImGui test — equity trading broker auth (AngelOne)
// Build: add to CMakeLists.txt as a separate executable
// Links against imgui, glfw, opengl, nlohmann_json

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <string>

// AngelOne credential state
struct AngelOneCreds {
    char api_key[128]    = {};
    char client_code[64] = {};
    char pin[64]         = {};
    char totp_secret[64] = {};
    bool show_pin        = false;
    bool connected       = false;
    std::string status_msg;
};

static AngelOneCreds creds;
static int active_tab = 0; // 0=overview, 1=broker

void render_overview() {
    ImGui::Text("Equity Trading - Overview");
    ImGui::Separator();
    ImGui::Spacing();
    if (creds.connected) {
        ImGui::TextColored(ImVec4(0.1f,0.75f,0.35f,1), "Connected to AngelOne");
        ImGui::Text("Client: %s", creds.client_code);
    } else {
        ImGui::TextColored(ImVec4(0.9f,0.7f,0.1f,1), "Paper Trading Mode");
        ImGui::Spacing();
        ImGui::Text("Go to Broker Config tab to connect AngelOne.");
    }
}

void render_broker_config() {
    ImGui::Text("AngelOne Configuration");
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Auth: TOTP");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("API Key");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##ak", creds.api_key, sizeof(creds.api_key));

    ImGui::Text("Client Code");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##cc", creds.client_code, sizeof(creds.client_code));

    ImGui::Text("PIN");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##pin", creds.pin, sizeof(creds.pin),
                     creds.show_pin ? 0 : ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    if (ImGui::SmallButton(creds.show_pin ? "Hide" : "Show"))
        creds.show_pin = !creds.show_pin;

    ImGui::Text("TOTP Secret");
    ImGui::SameLine(120);
    ImGui::SetNextItemWidth(250);
    ImGui::InputText("##totp", creds.totp_secret, sizeof(creds.totp_secret));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!creds.connected) {
        if (ImGui::Button("Connect")) {
            if (creds.api_key[0] && creds.client_code[0]) {
                creds.connected = true;
                creds.status_msg = "Connected to AngelOne!";
            } else {
                creds.status_msg = "Fill API Key and Client Code first.";
            }
        }
    } else {
        ImGui::TextColored(ImVec4(0.1f,0.75f,0.35f,1), "CONNECTED");
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Disconnect")) {
            creds.connected = false;
            creds.status_msg = "Disconnected.";
        }
    }

    if (!creds.status_msg.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(
            creds.connected ? ImVec4(0.1f,0.75f,0.35f,1) : ImVec4(0.85f,0.25f,0.25f,1),
            "%s", creds.status_msg.c_str());
    }
}

int main() {
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(900, 600, "Equity Trading Test - AngelOne", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Fullscreen window
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // Nav buttons
        if (ImGui::Button("Overview")) active_tab = 0;
        ImGui::SameLine();
        if (ImGui::Button("Broker Config")) active_tab = 1;
        ImGui::Separator();
        ImGui::Spacing();

        switch (active_tab) {
            case 0: render_overview(); break;
            case 1: render_broker_config(); break;
        }

        ImGui::End();

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
