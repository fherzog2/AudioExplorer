// SPDX-License-Identifier: GPL-2.0-only
#include <string>
#include <vector>
#include <fstream>

std::vector<char> read_file(std::ifstream& fs)
{
    std::vector<char> result;

    fs.seekg(0, fs.end);
    std::streampos length = fs.tellg();
    fs.seekg(0, fs.beg);

    result.resize(length);

    fs.read(result.data(), length);

    return result;
}

std::string getNameFromFilepath(const std::string& path)
{
    const size_t last_slash_index = path.find_last_of("\\/");
    const size_t name_begin_index = last_slash_index == std::string::npos ? 0 : last_slash_index + 1;

    std::string name = path.substr(name_begin_index);

    for (char& c : name)
    {
        if (c == '.')
            c = '_';
        c = toupper(c);
    }

    return name;
}

struct OutputBuffer
{
    std::vector<std::string> names;
    std::string blobs;
};

bool compile_file_to_c(const std::string& filepath, OutputBuffer& output)
{
    std::ifstream fs(filepath, std::ios::in|std::ios::binary);
    if (!fs)
    {
        fprintf(stderr, "%s cannot be opened", filepath.data());
        return false;
    }

    std::vector<char> content = read_file(fs);

    std::string name = getNameFromFilepath(filepath);

    output.names.push_back(name);

    output.blobs += "const char DATA_" + name + "[] = {\n";

    size_t i = 0;
    for (char c : content)
    {
        char hi = (c & 0xF0) >> 4;
        char lo = c & 0xF;

        char hex_hi = hi < 10 ? '0' + hi : 'A' + hi - 10;
        char hex_lo = lo < 10 ? '0' + lo : 'A' + lo - 10;

        output.blobs += "0x";
        output.blobs += hex_hi;
        output.blobs += hex_lo;

        if(i+1 != content.size())
            output.blobs += ",";

        // keep each line short

        if (i % 16 == 15)
            output.blobs += "\n";

        ++i;
    }

    output.blobs += "\n};\n\n";
    return true;
}

void build_file_content(const OutputBuffer& content, std::string& header, std::string& cpp)
{
    header +=
        "#pragma once\n"
        "\n"
        "// THIS FILE WAS AUTOGENERATED\n"
        "\n"
        "namespace res {\n"
        "    struct data\n"
        "    {\n"
        "        const char* ptr;\n"
        "        size_t size;\n"
        "    };\n"
        "\n";
    for (const std::string& name : content.names)
    {
        header += "    data " + name + "();\n";
    }
    header += "} //res";

    cpp +=
        "#include <cstddef>\n"
        "#include \"compiled_resources.h\"\n"
        "\n"
        "// THIS FILE WAS AUTOGENERATED\n"
        "\n";
    cpp += content.blobs;
    cpp +=
        "namespace res {\n"
        "    inline data make_data(const char* ptr, size_t size) {\n"
        "        data d;\n"
        "        d.ptr = ptr;\n"
        "        d.size = size;\n"
        "        return d;\n"
        "    };\n"
        "\n";
    for (const std::string& name : content.names)
    {
        cpp += "    data " + name + "() { return make_data(DATA_" + name + ", sizeof(DATA_" + name + ")); }\n";
    }
    cpp += "} //res";
}

bool write_file_if_changed(const std::string& filepath, const std::string& content)
{
    {
        // check if writing the file is necessary

        std::ifstream fs(filepath, std::ios::in|std::ios::binary);
        if (fs)
        {
            std::vector<char> existing_content = read_file(fs);

            if (std::equal(content.begin(), content.end(), existing_content.begin(), existing_content.end()))
                return true;
        }
    }

    std::fstream fs(filepath, std::ios::out|std::ios::binary|std::ios::trunc);
    if (!fs)
    {
        fprintf(stderr, "%s cannot be opened", filepath.data());
        return false;
    }

    fs.write(content.data(), content.size());

    return true;
}

enum class ArgParserMode
{
    NONE,
    INPUT,
    OUTPUT,
};

int main(int argc, char* argv[])
{
    // Usage: ResCompiler -i [inputfile] [inputfile] -o [outputfile]

    std::vector<std::string> input_files;
    std::string output_file;

    ArgParserMode current_mode = ArgParserMode::NONE;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-i")
        {
            current_mode = ArgParserMode::INPUT;
        }
        else if (arg == "-o")
        {
            current_mode = ArgParserMode::OUTPUT;
        }
        else
        {
            switch (current_mode)
            {
            case ArgParserMode::NONE:
                break;
            case ArgParserMode::INPUT:
                input_files.push_back(arg);
                break;
            case ArgParserMode::OUTPUT:
                output_file = arg;
                break;
            }
        }
    }

    if (output_file.empty())
        return 1;

    std::string headerpath = output_file;
    auto found = headerpath.find_last_of('.');
    if (found != std::string::npos)
        headerpath.replace(found, std::string::npos, ".h");
    else
    {
        fprintf(stderr, "Cannot create matching header path for %s", output_file.data());
        return 1;
    }

    OutputBuffer output;
    for (const std::string& f : input_files)
    {
        if (!compile_file_to_c(f, output))
            return 1;
    }

    std::string header;
    std::string cpp;
    build_file_content(output, header, cpp);

    if (!write_file_if_changed(headerpath, header))
        return 1;
    if (!write_file_if_changed(output_file, cpp))
        return 1;

    return 0;
}