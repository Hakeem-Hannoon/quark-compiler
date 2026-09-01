#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>   // for WIFEXITED / WEXITSTATUS
#endif

#include "quark/Analyzer.h"
#include "quark/AstView.h"
#include "quark/CodeGen.h"
#include "quark/Error.h"
#include "quark/Lexer.h"
#include "quark/Parser.h"

namespace {

struct Options {
    std::string input;
    std::string output;
    bool emit_tokens = false;
    bool emit_ast = false;
    bool emit_dot = false;
    bool emit_svg = false;
    bool emit_c = false;
    bool keep_c = false;
    bool run = false;
};

void usage(std::ostream& stream) {
    stream
        << "Usage: quarkc <file.qr> [options]\n"
        << "\n"
        << "Options:\n"
        << "  -o <path>       Write the executable to <path>\n"
        << "  --emit-tokens   Print the token stream and stop\n"
        << "  --emit-ast      Draw the syntax tree as text and stop\n"
        << "  --emit-svg      Draw the syntax tree as an SVG and stop\n"
        << "  --emit-dot      Draw the syntax tree as Graphviz DOT and stop\n"
        << "  --emit-c        Print the generated C and stop\n"
        << "  --keep-c        Keep the generated C next to the executable\n"
        << "  --run           Run the executable after building it\n"
        << "  -h, --help      Show this message\n";
}

// Strips the directory and the extension: examples/1.qr -> 1
std::string stem(const std::string& path) {
    std::size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);

    std::size_t dot = base.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return base;
    }

    return base.substr(0, dot);
}

// The generated C is handed to the host compiler through the shell, so a path
// containing a quote would let the file name run commands. Reject those instead.
bool isShellSafe(const std::string& path) {
    return path.find('\'') == std::string::npos;
}

std::string shellQuote(const std::string& path) {
    return "'" + path + "'";
}

// The C compiler to shell out to: $CC if set, otherwise cc.
std::string hostCompiler() {
    const char* cc = std::getenv("CC");

    if (cc && *cc) {
        return cc;
    }

    return "cc";
}

bool parseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            usage(std::cout);
            return false;
        } else if (arg == "--emit-tokens") {
            options.emit_tokens = true;
        } else if (arg == "--emit-ast") {
            options.emit_ast = true;
        } else if (arg == "--emit-dot") {
            options.emit_dot = true;
        } else if (arg == "--emit-svg") {
            options.emit_svg = true;
        } else if (arg == "--emit-c") {
            options.emit_c = true;
        } else if (arg == "--keep-c") {
            options.keep_c = true;
        } else if (arg == "--run") {
            options.run = true;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("-o needs a path");
            }
            options.output = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("Unknown option: " + arg);
        } else if (options.input.empty()) {
            options.input = arg;
        } else {
            throw std::runtime_error("Unexpected argument: " + arg);
        }
    }

    if (options.input.empty()) {
        usage(std::cerr);
        throw std::runtime_error("No input file");
    }

    if (options.output.empty()) {
        options.output = stem(options.input);
    }

    return true;
}

std::string readFile(const std::string& path) {
    std::ifstream file(path.c_str());

    if (!file.is_open()) {
        throw std::runtime_error("Could not open " + path);
    }

    return std::string(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

void writeFile(const std::string& path, const std::string& contents) {
    std::ofstream file(path.c_str());

    if (!file.is_open()) {
        throw std::runtime_error("Could not write " + path);
    }

    file << contents;

    if (!file) {
        throw std::runtime_error("Could not write " + path);
    }
}

// std::system returns a wait status on POSIX; normalise it to an exit code.
int exitCodeOf(int status) {
    if (status == -1) {
        return -1;
    }

#ifdef WIFEXITED
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
#else
    return status;
#endif
}

} // namespace

int main(int argc, char** argv) {
    Options options;

    try {
        if (!parseArgs(argc, argv, options)) {
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << "quarkc: " << error.what() << "\n";
        return 2;
    }

    try {
        std::string source = readFile(options.input);

        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();

        if (options.emit_tokens) {
            for (const Token& token : tokens) {
                std::cout << tokenTypeToString(token.type)
                          << "(\"" << token.value << "\")"
                          << " @ line " << token.line << "\n";
            }
            return 0;
        }

        Parser parser(tokens);
        std::unique_ptr<Program> program(parser.parseProgram());

        Analyzer analyzer;
        analyzer.analyze(program.get());

        if (options.emit_ast || options.emit_dot || options.emit_svg) {
            const TreeNode view = buildAstView(program.get());

            if (options.emit_ast) {
                std::cout << renderTree(view);
            }
            if (options.emit_dot) {
                std::cout << renderDot(view);
            }
            if (options.emit_svg) {
                std::cout << renderSvg(view);
            }

            return 0;
        }

        CodeGen generator;
        std::string c_source = generator.generate(program.get());

        if (options.emit_c) {
            std::cout << c_source;
            return 0;
        }

        std::string c_path = options.output + ".c";

        if (!isShellSafe(c_path) || !isShellSafe(options.output)) {
            std::cerr << "quarkc: output path may not contain a quote\n";
            return 2;
        }

        writeFile(c_path, c_source);

        std::string command =
            hostCompiler() + " " + shellQuote(c_path) + " -o " + shellQuote(options.output);

        int status = exitCodeOf(std::system(command.c_str()));

        if (status != 0) {
            std::cerr << "quarkc: the C backend failed (" << command << ")\n";
            std::cerr << "quarkc: generated C left at " << c_path << "\n";
            return 1;
        }

        if (!options.keep_c) {
            std::remove(c_path.c_str());
        }

        if (options.run) {
            std::string program_path = options.output;

            // ./ so the shell looks in the current directory, not $PATH.
            if (program_path.find('/') == std::string::npos) {
                program_path = "./" + program_path;
            }

            return exitCodeOf(std::system(shellQuote(program_path).c_str()));
        }

        return 0;
    } catch (const CompileError& error) {
        std::cerr << options.input << ":" << error.line() << ": error: "
                  << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "quarkc: " << error.what() << "\n";
        return 1;
    }
}
