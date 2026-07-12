#pragma once
#include <fstream>
#include <string>
#include <vector>

namespace TinyPDB {
    struct Atom {
        std::string element;
        float x, y, z;
    };

    inline std::vector<Atom> parse(const std::string& filepath, bool omit_hydrogen = true) {
        std::vector<Atom> atoms;
        std::ifstream file(filepath);
        if (!file.is_open()) return atoms;

        std::string line;
        while (std::getline(file, line)) {
            // Fix Quirks 2: Stop parsing after the first NMR model to avoid overlapping proteins
            if (line.compare(0, 6, "ENDMDL") == 0) {
                break;
            }

            if (line.size() >= 54 && (line.compare(0, 4, "ATOM") == 0 || line.compare(0, 6, "HETATM") == 0)) {

                // Fix Quirk 1: Handle Alternate Locations. Column 17 (index 16).
                // Only accept default (' ') or the primary conformation ('A'). Skip 'B', 'C', etc.
                if (line.size() > 16) {
                    char alt_loc = line[16];
                    if (alt_loc != ' ' && alt_loc != 'A' && alt_loc != '1') {
                        continue;
                    }
                }

                // Extract Element (Columns 77-78)
                std::string element = (line.size() >= 78) ? line.substr(76, 2) : "";
                element.erase(0, element.find_first_not_of(" "));
                element.erase(element.find_last_not_of(" ") + 1);

                // Fallback to name field if element column is blank
                if (element.empty() && line.size() >= 14) {
                    element = line.substr(12, 2);
                    element.erase(0, element.find_first_not_of(" "));
                }

                // Omit Hydrogen variants (H, D=Deuterium, T=Tritium)
                if (omit_hydrogen && (element == "H" || element == "h" || element == "D" || element == "T")) {
                    continue;
                }

                try {
                    Atom atom;
                    atom.element = element;
                    atom.x = std::stof(line.substr(30, 8));
                    atom.y = std::stof(line.substr(38, 8));
                    atom.z = std::stof(line.substr(46, 8));
                    atoms.push_back(atom);
                } catch (...) {
                    continue; // Skip malformed rows safely
                }
            }
        }
        return atoms;
    }
}