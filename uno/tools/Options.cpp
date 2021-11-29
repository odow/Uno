#include <iostream>
#include <fstream>
#include <sstream>
#include "Options.hpp"
#include "Logger.hpp"

Options get_default_options(const std::string& file_name) {
   std::ifstream file;
   file.open(file_name);
   if (!file) {
      throw std::invalid_argument("The option file was not found");
   }
   else {
      // register the default options
      Options options;
      std::string key, value;
      std::string line;
      while (std::getline(file, line)) {
         if (!line.empty() && line.find('#') != 0) {
            std::istringstream iss;
            iss.str(line);
            iss >> key >> value;
            options[key] = value;
         }
      }
      file.close();
      return options;
   }
}

void find_preset(const std::string& preset, Options& options) {
   // shortcuts for state-of-the-art combinations
   if (preset == "ipopt") {
      options["mechanism"] = "LS";
      options["constraint-relaxation"] = "feasibility-restoration";
      options["strategy"] = "filter";
      options["subproblem"] = "IPM";
      options["filter_Beta"] = "0.99999";
      options["filter_Gamma"] = "1e-5";
      options["armijo_decrease_fraction"] = "1e-4";
      options["LS_backtracking_ratio"] = "0.5";
   }
   else if (preset == "filtersqp") {
      options["mechanism"] = "TR";
      options["constraint-relaxation"] = "feasibility-restoration";
      options["strategy"] = "filter";
      options["subproblem"] = "SQP";
   }
   else if (preset == "byrd") {
      options["mechanism"] = "LS";
      options["constraint-relaxation"] = "l1-relaxation";
      options["strategy"] = "l1-penalty";
      options["subproblem"] = "SQP";
      options["l1_relaxation_initial_parameter"] = "1";
      options["LS_backtracking_ratio"] = "0.5";
      options["armijo_decrease_fraction"] = "1e-8";
      options["l1_relaxation_epsilon1"] = "0.1";
      options["l1_relaxation_epsilon2"] = "0.1";
      options["tolerance"] = "1e-6";
   }
}

void get_command_line_options(int argc, char* argv[], Options& options) {
   // build the (argument, value) map
   int i = 1;
   while (i < argc - 1) {
      std::string argument_i = std::string(argv[i]);
      if (argument_i[0] == '-') {
         if (i < argc - 1) {
            const std::string name = argument_i.substr(1);
            const std::string value_i = std::string(argv[i + 1]);
            if (name == "preset") {
               find_preset(value_i, options);
            }
            else {
               std::cout << "(" << argument_i << ", " << value_i << ")\n";
               options[name] = value_i;
            }
            i += 2;
         }
      }
      else {
         std::cout << "Argument " << argument_i << " was ignored\n";
         i++;
      }
   }
}

void print_options(const Options& options) {
   for (const auto& [key, value]: options) {
      std::cout << "Option " << key << " = " << value << "\n";
   }
}

void set_logger(const std::string& logger_level) {
   try {
      if (logger_level == "ERROR") {
         Logger::logger_level = ERROR;
      }
      else if (logger_level == "WARNING") {
         Logger::logger_level = WARNING;
      }
      else if (logger_level == "INFO") {
         Logger::logger_level = INFO;
      }
      else if (logger_level == "DEBUG") {
         Logger::logger_level = DEBUG;
      }
   }
   catch (std::out_of_range&) {
   }
}
