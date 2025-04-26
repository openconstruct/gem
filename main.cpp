#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib> // For std::getenv
#include <stdexcept>
#include <filesystem> // Requires C++17
#include <vector>
#include <limits> // Required for numeric_limits
#include <ios>    // Required for streamsize
#include <optional> // For returning optional path

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- Configuration ---
const std::string DEFAULT_MODEL = "models/gemini-1.5-flash-latest";
const std::string CONFIG_DIR_NAME = ".config";
const std::string APP_CONFIG_DIR_NAME = "gem";
const std::string CONFIG_FILE_NAME = "model.txt";

// Function to get API key securely (from environment variable)
std::string get_api_key() {
    const char* key = std::getenv("GEMINI_API_KEY");
    if (key == nullptr) {
        throw std::runtime_error("Error: GEMINI_API_KEY environment variable not set.");
    }
    return std::string(key);
}

// --- Configuration File Handling ---

// Function to get the path to the user's home directory
std::optional<fs::path> get_home_directory() {
    const char* home_env = nullptr;
    #ifdef _WIN32
        home_env = std::getenv("USERPROFILE");
    #else
        home_env = std::getenv("HOME");
    #endif

    if (home_env == nullptr) {
        std::cerr << "Warning: Could not determine home directory (HOME/USERPROFILE not set)." << std::endl;
        return std::nullopt;
    }
    return fs::path(home_env);
}

// Function to get the full path to the configuration file
std::optional<fs::path> get_config_file_path() {
    auto home_dir = get_home_directory();
    if (!home_dir) {
        return std::nullopt;
    }
    // Use ~/.config/gem/model.txt standard path
    return home_dir.value() / CONFIG_DIR_NAME / APP_CONFIG_DIR_NAME / CONFIG_FILE_NAME;
}

// Function to read the selected model from the config file
std::string read_selected_model_from_config() {
    auto config_path_opt = get_config_file_path();
    if (!config_path_opt) {
        return ""; // Cannot determine config path
    }
    fs::path config_path = config_path_opt.value();

    if (fs::exists(config_path)) {
        std::ifstream config_file(config_path);
        if (config_file) {
            std::string model_name;
            if (std::getline(config_file, model_name)) {
                // Basic validation: check if it's not empty and looks like a model name
                if (!model_name.empty() && model_name.find("models/") == 0) {
                    return model_name;
                } else if (!model_name.empty()) {
                    std::cerr << "Warning: Invalid model name found in config file '" << config_path.string() << "': " << model_name << std::endl;
                 }
            }
        } else {
             std::cerr << "Warning: Could not open config file for reading: " << config_path.string() << std::endl;
        }
    }
    return ""; // Return empty if file doesn't exist, is empty, invalid, or cannot be read
}

// Function to write the selected model to the config file
bool write_selected_model_to_config(const std::string& model_name) {
    auto config_path_opt = get_config_file_path();
     if (!config_path_opt) {
        std::cerr << "Error: Cannot write config file, home directory not found." << std::endl;
        return false;
    }
    fs::path config_path = config_path_opt.value();
    fs::path config_dir = config_path.parent_path();

    try {
        // Ensure the directory exists
        if (!fs::exists(config_dir)) {
            if (!fs::create_directories(config_dir)) {
                 std::cerr << "Error: Could not create config directory: " << config_dir.string() << std::endl;
                 return false;
            }
        }

        // Write the model name, overwriting the file
        std::ofstream config_file(config_path);
        if (!config_file) {
            std::cerr << "Error: Could not open config file for writing: " << config_path.string() << std::endl;
            return false;
        }
        config_file << model_name << std::endl;
        std::cout << "Set default model to: " << model_name << std::endl;
        std::cout << "Configuration saved to: " << config_path.string() << std::endl;
        return true;

    } catch (const fs::filesystem_error& e) {
         std::cerr << "Filesystem Error writing config: " << e.what() << std::endl;
         return false;
    } catch (const std::exception& e) {
        std::cerr << "Error writing config: " << e.what() << std::endl;
        return false;
    }
}


// --- Gemini API Interaction --- (get_generative_models and call_gemini_api remain the same)

// Helper function to fetch and filter models supporting "generateContent"
std::vector<std::string> get_generative_models(const std::string& api_key) {
    std::vector<std::string> model_names;
    std::string api_url = "https://generativelanguage.googleapis.com/v1beta/models?key=" + api_key;
    cpr::Response r = cpr::Get(cpr::Url{api_url});
    if (r.status_code != 200) throw std::runtime_error("API Error fetching models: Status Code " + std::to_string(r.status_code) + " - Response: " + r.text);
    try {
        json response_json = json::parse(r.text);
        if (response_json.contains("models") && response_json["models"].is_array()) {
            for (const auto& model : response_json["models"]) {
                bool supports_generate = false;
                if (model.contains("supportedGenerationMethods") && model["supportedGenerationMethods"].is_array()) {
                     for (const auto& method : model["supportedGenerationMethods"]) {
                         if (method.get<std::string>() == "generateContent") { supports_generate = true; break; }
                     }
                }
                if (supports_generate && model.contains("name")) { model_names.push_back(model["name"].get<std::string>()); }
            }
        } else { throw std::runtime_error("API response does not contain a 'models' array."); }
    } catch (json::parse_error& e) { throw std::runtime_error("JSON Parse Error fetching models: " + std::string(e.what()) + "\nResponse Text: " + r.text); }
    catch (json::type_error& e) { throw std::runtime_error("JSON Type Error fetching models: " + std::string(e.what()) + "\nResponse Text: " + r.text); }
    catch (const std::exception& e) { throw std::runtime_error("Error processing model list: " + std::string(e.what())); }
    return model_names;
}

// Function to call the Gemini API's generateContent endpoint (no changes needed)
std::string call_gemini_api(const std::string& prompt, const std::string& model_name, const std::string& api_key) {
    std::string api_url = "https://generativelanguage.googleapis.com/v1beta/" + model_name + ":generateContent?key=" + api_key;
    json request_body = { {"contents", {{ {"parts", {{ {"text", prompt} }}} }}} };
    std::cout << "Calling Gemini API (" << model_name << ")..." << std::endl;
    cpr::Response r = cpr::Post(cpr::Url{api_url}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Body{request_body.dump()});
    if (r.status_code != 200) throw std::runtime_error("API Error (" + model_name + "): Status Code " + std::to_string(r.status_code) + " - Response: " + r.text);
    try {
        json response_json = json::parse(r.text);
        if (response_json.contains("candidates") && response_json["candidates"].is_array() && !response_json["candidates"].empty()) {
            const auto& first_candidate = response_json["candidates"][0];
             if (first_candidate.contains("content") && first_candidate["content"].contains("parts") &&
                first_candidate["content"]["parts"].is_array() && !first_candidate["content"]["parts"].empty()) {
                const auto& first_part = first_candidate["content"]["parts"][0];
                if (first_part.contains("text")) return first_part["text"].get<std::string>();
             }
             if (first_candidate.contains("finishReason") && first_candidate["finishReason"] != "STOP") throw std::runtime_error("API Error (" + model_name + "): Generation finished due to " + first_candidate["finishReason"].get<std::string>());
        }
        if (response_json.contains("promptFeedback") && response_json["promptFeedback"].contains("blockReason")) throw std::runtime_error("API Error (" + model_name + "): Prompt blocked - Reason: " + response_json["promptFeedback"]["blockReason"].dump());
        throw std::runtime_error("API Error (" + model_name + "): Could not extract text content from response. Raw response: " + r.text);
    } catch (json::parse_error& e) { throw std::runtime_error("JSON Parse Error (" + model_name + "): " + std::string(e.what()) + "\nResponse Text: " + r.text); }
    catch (json::type_error& e) { throw std::runtime_error("JSON Type Error (" + model_name + "): " + std::string(e.what()) + "\nResponse Text: " + r.text); }
}

// --- File I/O --- (No changes needed here)
std::string read_file(const fs::path& file_path) { /* ... */ }
void write_file(const fs::path& file_path, const std::string& content) { /* ... */ }

// --- Action Functions --- (No changes needed here)
void create_file_with_gemini(const fs::path& file_path, const std::string& description, const std::string& model_name, const std::string& api_key) { /* ... */ }
void edit_file_with_gemini(const fs::path& file_path, const std::string& edit_instruction, const std::string& model_name, const std::string& api_key) { /* ... */ }
void explain_file_with_gemini(const fs::path& file_path, const std::string& model_name, const std::string& api_key) { /* ... */ }


// --- NEW listmodels Command Action ---
void list_and_select_model(const std::string& api_key) {
     std::cout << "Fetching available models..." << std::endl;
    std::vector<std::string> models = get_generative_models(api_key);

    if (models.empty()) {
        std::cerr << "Error: Could not fetch any available models supporting 'generateContent'." << std::endl;
        return;
    }

    std::cout << "Available models supporting 'generateContent':\n";
    std::cout << "-------------------------------------------\n";
    for (size_t i = 0; i < models.size(); ++i) {
        std::cout << (i + 1) << ". " << models[i] << "\n";
    }
    std::cout << "-------------------------------------------\n";
    std::cout << "Enter the number of the model to set as default (1-" << models.size() << "): ";

    std::string input_line;
    size_t choice = 0;

    while (true) { // Loop until valid input is received
        if (!std::getline(std::cin, input_line) || input_line.empty()) {
            std::cerr << "Selection cancelled or input error." << std::endl;
            return; // Exit if getline fails or input is empty
        }
        try {
            choice = std::stoull(input_line);
            if (choice >= 1 && choice <= models.size()) {
                break; // Valid choice, exit loop
            } else {
                std::cerr << "Invalid choice. Please enter a number between 1 and " << models.size() << ": ";
            }
        } catch (const std::invalid_argument& ) {
            std::cerr << "Invalid input. Please enter a number: ";
        } catch (const std::out_of_range& ) {
            std::cerr << "Input number too large. Please enter a valid number: ";
        }
        // Clear potential error flags on cin if needed, though getline is usually robust
        // std::cin.clear();
        // std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Less needed with getline
    }

    // Save the selected model to config
    write_selected_model_to_config(models[choice - 1]);
}


// --- Main Application Logic ---

void print_usage(const char* app_name) {
    std::cerr << "Usage: \n";
    std::cerr << "  " << app_name << " [-m <model_name>] create <filename> \"<description>\"\n";
    std::cerr << "  " << app_name << " [-m <model_name>] edit <filename> \"<instruction>\"\n";
    std::cerr << "  " << app_name << " [-m <model_name>] explain <filename>\n";
    std::cerr << "  " << app_name << " listmodels          # List models and set the persistent default\n";
    std::cerr << "  " << app_name << " [-h | --help]\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  -m <model_name> : Override the saved/default model for THIS command only.\n";
    std::cerr << "                     (e.g., models/gemini-1.5-pro-latest)\n";
    std::cerr << "  -h, --help      : Show this help message and exit.\n";
    std::cerr << "\nDescription:\n";
    std::cerr << "  Uses the model set by 'listmodels' by default.\n";
    std::cerr << "  Falls back to '" << DEFAULT_MODEL << "' if no model is set.\n";
     auto config_path = get_config_file_path();
     if(config_path) {
        std::cerr << "  Current default model is saved in: " << config_path.value().string() << "\n";
     }
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << app_name << " listmodels          # Select and save your preferred model\n";
    std::cerr << "  " << app_name << " create hello.py \"Print 'Hello'\" # Uses the saved model\n";
    std::cerr << "  " << app_name << " -m models/gemini-pro edit hello.py \"Add a comment\" # Uses gemini-pro once\n";
    std::cerr << "  " << app_name << " explain hello.py      # Uses the saved model again\n";
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string override_model = ""; // Stores model if -m is used
    std::vector<std::string> args; // Store non-flag arguments

    // --- Argument Parsing ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        // Special handling for listmodels as it's now an action, not just a flag
        if (arg == "listmodels") {
             if (argc != 2) { // listmodels should be the only argument
                 std::cerr << "Error: 'listmodels' command does not take other arguments or flags." << std::endl;
                 print_usage(argv[0]);
                 return 1;
             }
            try {
                std::string api_key = get_api_key();
                list_and_select_model(api_key);
                return 0; // listmodels action finished
            } catch (const std::exception& e) {
                std::cerr << "Error during model selection: " << e.what() << std::endl;
                return 1;
            }
        }
         else if (arg == "-m" || arg == "--model") {
            if (override_model.empty()){ // Only allow one -m
                if (i + 1 < argc) {
                    override_model = argv[++i];
                    if (override_model.find("models/") != 0) {
                        std::cerr << "Warning: Model name '" << override_model << "' might be missing the 'models/' prefix." << std::endl;
                    }
                } else {
                    std::cerr << "Error: " << arg << " option requires a model name." << std::endl;
                    print_usage(argv[0]);
                    return 1;
                }
            } else {
                 std::cerr << "Error: Cannot specify -m option multiple times." << std::endl;
                 print_usage(argv[0]);
                 return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
             print_usage(argv[0]);
             return 0;
        } else {
            // Assume it's part of the action command if not listmodels or a flag
            args.push_back(arg);
        }
    }

    // --- Action Processing (create, edit, explain) ---
    if (args.empty()) {
        // If we got here, no valid action was provided (listmodels handled above)
         std::cerr << "Error: No action specified (create, edit, explain)." << std::endl;
         print_usage(argv[0]);
        return 1;
    }

    std::string action = args[0];
    // Check if action is valid before proceeding
    if (action != "create" && action != "edit" && action != "explain") {
         std::cerr << "Error: Unknown action '" << action << "'. Use 'create', 'edit', 'explain', or 'listmodels'." << std::endl;
         print_usage(argv[0]);
         return 1;
    }

    try {
        std::string api_key = get_api_key();
        std::string model_to_use;

        // Determine which model to use
        if (!override_model.empty()) {
            // 1. Priority: -m flag override
            model_to_use = override_model;
             std::cout << "Using specified model (override): " << model_to_use << std::endl;
        } else {
            // 2. Try reading from config file
            model_to_use = read_selected_model_from_config();
            if (!model_to_use.empty()) {
                 std::cout << "Using configured default model: " << model_to_use << std::endl;
            } else {
                // 3. Fallback to hardcoded default
                model_to_use = DEFAULT_MODEL;
                 std::cout << "Using hardcoded default model: " << model_to_use << std::endl;
            }
        }

        // --- Execute the Action ---
        if (action == "create") {
            if (args.size() != 3) { std::cerr << "Error: 'create' requires <filename> \"<description>\"\n"; print_usage(argv[0]); return 1; }
            fs::path filename = args[1];
            std::string instruction = args[2];
            create_file_with_gemini(filename, instruction, model_to_use, api_key);
        } else if (action == "edit") {
             if (args.size() != 3) { std::cerr << "Error: 'edit' requires <filename> \"<instruction>\"\n"; print_usage(argv[0]); return 1; }
             fs::path filename = args[1];
             if (!fs::exists(filename)) { std::cerr << "Error: File '" << filename.string() << "' does not exist for editing.\n"; return 1; }
             std::string instruction = args[2];
             edit_file_with_gemini(filename, instruction, model_to_use, api_key);
        } else if (action == "explain") {
             if (args.size() != 2) { std::cerr << "Error: 'explain' requires <filename>\n"; print_usage(argv[0]); return 1; }
             fs::path filename = args[1];
             if (!fs::exists(filename)) { std::cerr << "Error: File '" << filename.string() << "' does not exist for explaining.\n"; return 1; }
             explain_file_with_gemini(filename, model_to_use, api_key);
        }
        // No else needed here because we validated the action earlier

    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}