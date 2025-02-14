// Convert Mesh.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <algorithm>

struct Vertex {
    float x, y, z;
};

// Updated face structure to match PLY format
struct Face {
    std::vector<int> vertices;
    std::vector<float> texcoords;  // UV coordinates for texturing
    int texnumber;                 // Texture index
};

class PlyStlConverter {
private:
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<std::vector<int>> triangulated_faces;
    std::string input_file;
    std::string output_file;
    bool is_binary;

    bool readPlyHeader(std::ifstream& file, int& num_vertices, int& num_faces) {
        std::string line;
        bool vertex_section = false;
        bool face_section = false;

        // Read header
        std::getline(file, line);
        if (line.substr(0, 3) != "ply") {
            std::cerr << "Error: Not a PLY file" << std::endl;
            return false;
        }

        // Detect format
        std::getline(file, line);
        is_binary = (line.find("binary") != std::string::npos);
        std::cout << "File format: " << (is_binary ? "Binary" : "ASCII") << std::endl;

        while (std::getline(file, line)) {
            if (line.find("element vertex") != std::string::npos) {
                sscanf(line.c_str(), "element vertex %d", &num_vertices);
                vertex_section = true;
                std::cout << "Found " << num_vertices << " vertices" << std::endl;
            }
            else if (line.find("element face") != std::string::npos) {
                sscanf(line.c_str(), "element face %d", &num_faces);
                face_section = true;
                std::cout << "Found " << num_faces << " faces" << std::endl;
            }
            else if (line.find("end_header") != std::string::npos) {
                break;
            }
        }

        return vertex_section && face_section;
    }

    void readVertices(std::ifstream& file, int num_vertices) {
        vertices.reserve(num_vertices);
        std::cout << "Reading vertices..." << std::endl;

        if (is_binary) {
            for (int i = 0; i < num_vertices; i++) {
                Vertex v;
                file.read(reinterpret_cast<char*>(&v.x), sizeof(float));
                file.read(reinterpret_cast<char*>(&v.y), sizeof(float));
                file.read(reinterpret_cast<char*>(&v.z), sizeof(float));
                vertices.push_back(v);
            }
        }
        else {
            for (int i = 0; i < num_vertices; i++) {
                Vertex v;
                file >> v.x >> v.y >> v.z;
                vertices.push_back(v);
            }
        }
        std::cout << "Finished reading " << vertices.size() << " vertices" << std::endl;
    }

    void readFaces(std::ifstream& file, int num_faces) {
        faces.reserve(num_faces);
        std::cout << "Reading faces..." << std::endl;

        int max_vertices = vertices.size();
        int skipped_faces = 0;
        int processed_faces = 0;

        for (int i = 0; i < num_faces; i++) {
            if (file.peek() == EOF) {
                std::cout << "Reached end of file at face " << i << std::endl;
                break;
            }

            Face f;

            // Read number of vertices in face
            uint8_t num_vertices_in_face;
            file.read(reinterpret_cast<char*>(&num_vertices_in_face), sizeof(uint8_t));

            // Read vertex indices
            std::vector<int> vertex_indices(num_vertices_in_face);
            file.read(reinterpret_cast<char*>(vertex_indices.data()), num_vertices_in_face * sizeof(int));

            // Read number of texture coordinates
            uint8_t num_texcoords;
            file.read(reinterpret_cast<char*>(&num_texcoords), sizeof(uint8_t));

            // Read texture coordinates
            std::vector<float> texcoords(num_texcoords);
            file.read(reinterpret_cast<char*>(texcoords.data()), num_texcoords * sizeof(float));

            // Read texture number
            int texnumber;
            file.read(reinterpret_cast<char*>(&texnumber), sizeof(int));

            bool face_valid = true;
            for (int j = 0; j < num_vertices_in_face; j++) {
                if (vertex_indices[j] < 0 || vertex_indices[j] >= max_vertices) {
                    face_valid = false;
                    if (i < 10 || i > num_faces - 10) {
                        std::cout << "Invalid index at face " << i << "[" << j << "]: "
                            << vertex_indices[j] << " (max: " << max_vertices - 1 << ")" << std::endl;
                    }
                    break;
                }
            }

            if (face_valid) {
                f.vertices = vertex_indices;
                f.texcoords = texcoords;
                f.texnumber = texnumber;
                faces.push_back(f);
                processed_faces++;
            }
            else {
                skipped_faces++;
            }

            if (i % 10000 == 0) {
                std::cout << "\rProcessing face " << i << " of " << num_faces
                    << " (Valid: " << processed_faces
                    << ", Skipped: " << skipped_faces << ")" << std::flush;
            }
        }

        std::cout << "\n\nFace reading summary:" << std::endl;
        std::cout << "Total faces attempted: " << num_faces << std::endl;
        std::cout << "Valid faces read: " << processed_faces << std::endl;
        std::cout << "Skipped faces: " << skipped_faces << std::endl;
        std::cout << "Final face count: " << faces.size() << std::endl;
    }

    void triangulateFaces() {
        std::cout << "Triangulating faces..." << std::endl;
        triangulated_faces.reserve(faces.size() * 2);

        for (const Face& face : faces) {
            if (face.vertices.size() == 3) {
                triangulated_faces.push_back(face.vertices);
            }
            else if (face.vertices.size() > 3) {
                // Fan triangulation for polygons
                for (size_t i = 1; i < face.vertices.size() - 1; i++) {
                    triangulated_faces.push_back({
                        face.vertices[0],
                        face.vertices[i],
                        face.vertices[i + 1]
                        });
                }
            }
        }

        std::cout << "Created " << triangulated_faces.size() << " triangles" << std::endl;
    }

    void writeStlBinary(std::ofstream& file) {
        std::cout << "Writing STL file..." << std::endl;

        // Write header (80 bytes)
        char header[80] = "PLY to STL conversion";
        file.write(header, sizeof(header));

        // Write number of triangles
        uint32_t num_triangles = triangulated_faces.size();
        file.write(reinterpret_cast<char*>(&num_triangles), sizeof(num_triangles));

        // Prepare normal and attribute byte count
        float zero_normal[3] = { 0.0f, 0.0f, 0.0f };
        uint16_t attr_byte_count = 0;

        // Write each triangle
        for (const auto& triangle : triangulated_faces) {
            // Write normal (placeholder)
            file.write(reinterpret_cast<char*>(zero_normal), 12);

            // Write vertices
            for (int i = 0; i < 3; i++) {
                const Vertex& v = vertices[triangle[i]];
                file.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
                file.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
                file.write(reinterpret_cast<const char*>(&v.z), sizeof(float));
            }

            // Write attribute byte count
            file.write(reinterpret_cast<char*>(&attr_byte_count), sizeof(attr_byte_count));
        }

        std::cout << "Finished writing STL file" << std::endl;
    }

public:
    PlyStlConverter(const std::string& input, const std::string& output)
        : input_file(input), output_file(output) {
    }

    bool convert() {
        // Open input file
        std::ifstream ply_file(input_file, std::ios::binary);
        if (!ply_file.is_open()) {
            std::cerr << "Error: Could not open input file " << input_file << std::endl;
            return false;
        }

        // Read header
        int num_vertices = 0, num_faces = 0;
        if (!readPlyHeader(ply_file, num_vertices, num_faces)) {
            std::cerr << "Error: Invalid PLY header" << std::endl;
            return false;
        }

        // Process file
        readVertices(ply_file, num_vertices);
        readFaces(ply_file, num_faces);
        triangulateFaces();

        // Write output file
        std::ofstream stl_file(output_file, std::ios::binary);
        if (!stl_file.is_open()) {
            std::cerr << "Error: Could not open output file " << output_file << std::endl;
            return false;
        }

        writeStlBinary(stl_file);

        // Print final statistics
        std::cout << "\nConversion Statistics:" << std::endl;
        std::cout << "  Input vertices: " << vertices.size() << std::endl;
        std::cout << "  Input faces: " << faces.size() << std::endl;
        std::cout << "  Output triangles: " << triangulated_faces.size() << std::endl;

        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <input.ply> <output.stl>" << std::endl;
        return 1;
    }

    PlyStlConverter converter(argv[1], argv[2]);
    return converter.convert() ? 0 : 1;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
