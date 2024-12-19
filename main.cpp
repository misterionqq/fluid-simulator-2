#include <iostream>
#include <csignal>
#include <cstdio>

#include "fluid.h"
#include "flags-parser.h"

bool save_flag = false;
bool exit_flag = false;

// Signal handler for SIGINT to set the save flag
void my_handler(int nsig) {
    save_flag = true;
}

// Комментарии я делал по шаблону с прошлого курсе с помощью ChatGPT, надеюсь, это простительно

//==============================//
// Main program execution       //
//==============================//

int main(int argc, char* argv[]) {
    // Register the signal handler for SIGINT
    (void) signal(SIGINT, my_handler);

    parser optsParser(argc, argv);

    // Retrieve options for input/output files and types
    auto input_file = optsParser.get_option("--input-file");
    auto save_file = optsParser.get_option("--save-file");
    int p_type = get_type(optsParser.get_option("--p-type"));
    int v_type = get_type(optsParser.get_option("--v-type"));
    int v_flow_type = get_type(optsParser.get_option("--v-flow-type"));

    //==============================//
    // Work with files              //
    //==============================//

    std::ifstream input(input_file);
    if (!input.is_open()) {
        throw std::invalid_argument("Can't open file");
    }
    int N, M;
    input >> N >> M;
    input.seekg(0, ios::beg);

    // Create the fluid simulation object
    auto fluid = create_fluid(p_type, v_type, v_flow_type, N, M);

    std::ofstream saveFile(save_file);
    if (!input.is_open()) {
        throw std::invalid_argument("Can't open file");
    }

    fluid->load(input);

    //==============================//
    // Simulation loop              //
    //==============================//

    int T = 1'000'000;
    for (int i = 0; i < T; ++i) {
        // Check if a save has been requested
        if (save_flag) {
            std::cout << "\nSaving current position..." << std::endl;
            saveFile.close();
            saveFile.open(save_file, std::ios::trunc);
            fluid->save(saveFile);
            save_flag = false;
            std::cout << "Saved to " + save_file << std::endl;

            // Prompt user for next action
            char choice;
            do {
                std::cout << "Enter 'C' to continue or 'Q' to quit: ";
                std::cin >> choice;
                choice = std::tolower(choice);

                if (choice == 'q') {
                    exit_flag = true;
                    break;
                } else if (choice == 'c') {
                    break;
                } else {
                    std::cout << "Invalid input. Please enter 'C' or 'Q'." << std::endl;
                }
            } while (true);

            if (exit_flag) {
                break;
            }
        }

        fluid->next(std::cout);
    }

    // Cleanup and termination
    input.close();
    saveFile.close();
    return 0;
}