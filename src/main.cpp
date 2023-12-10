#include <stdio.h>

#include <string_view>
#include <span>
#include <vector>
#include <functional>

using Tokens = std::span<std::string_view>;

std::vector<std::string_view> Split(const std::string_view str, const char delim = ',')
{   
    std::vector<std::string_view> result;

    int indexCommaToLeftOfColumn = 0;
    int indexCommaToRightOfColumn = -1;

    for (int i=0;i<static_cast<int>(str.size());i++)
    {
        if (str[i] == delim)
        {
            indexCommaToLeftOfColumn = indexCommaToRightOfColumn;
            indexCommaToRightOfColumn = i;
            int index = indexCommaToLeftOfColumn + 1;
            int length = indexCommaToRightOfColumn - index;

            std::string_view column(str.data() + index, length);
            result.push_back(column);
        }
    }
    const std::string_view finalColumn(str.data() + indexCommaToRightOfColumn + 1, str.size() - indexCommaToRightOfColumn - 1);
    result.push_back(finalColumn);
    return result;
}

template <typename T>
using OptParseFun = bool(Tokens, T*);

struct Argument {
    template<typename OptType>
    Argument(const char* _name, OptParseFun<OptType>* opt_parse_fun) {
        name = _name;
        parse_fun = [opt_parse_fun](Tokens args, void* opt_ptr) -> bool {
            OptType* opt = reinterpret_cast<OptType*>(opt_ptr);
            return (*opt_parse_fun)(args, opt);
        };
    };
    
    std::string_view name;
    std::function<bool(Tokens, void*)> parse_fun;
};

struct Context {
    template <typename OptType>
    bool ParseArgs(std::string_view arg_string, OptType& opt_type) {
        void* erased = (void*)&opt_type;
        
        tokens = Split(arg_string, ' ');
        std::vector<Tokens> arg_tokens;
        for (size_t i = 0; i < tokens.size();) {
            for (size_t j = i + 1; j < tokens.size(); ++j) {
                if (tokens[j].starts_with("--")) {
                    arg_tokens.push_back(Tokens(tokens.begin() + i, tokens.begin() + j));
                    i = j;
                    break;
                }
            }

            if (i == (tokens.size() - 1)) {
                arg_tokens.push_back(Tokens(tokens.begin() + i, tokens.begin() + i + 1));
                break;
            }
        }

        for (Tokens& arg_token : arg_tokens) {
            auto it = std::find_if(args.begin(), args.end(), [&](Argument const& arg) { return arg.name == arg_token[0]; });
            if (!it->parse_fun(arg_token.subspan(1), erased)) {
                return false;
            }
        }

        return true;
    };

    std::span<Argument> args;
    std::vector<std::string_view> tokens;
};


using CmdFun = bool(Context&);

struct ConsoleCommand {
    ConsoleCommand(const char* _name, const char* _desc, CmdFun* _fun) 
        : name(_name)
        , description(_desc)
        , cmd_fun(_fun)
    {}
    
    template<typename OptType>
    Argument& AddArgument(const char* name, OptParseFun<OptType>* opt_parse_fun) {
        Argument& arg = arguments.emplace_back(
            name, opt_parse_fun
        );

        return arg;
    }

    bool Invoke() {
        Context ctx;
        ctx.args = arguments;
        return (*cmd_fun)(ctx);
    }

    std::vector<Argument> arguments;

    std::string_view name;
    std::string_view description;
    CmdFun* cmd_fun;
};

struct ExampleOptions {
    bool enable = false;
    Tokens targets;
};

int main(int argc, char** argv) {
    printf("Hello, sailor\n");

    ConsoleCommand example("example", "An example command", [](Context& ctx) -> bool {
        ExampleOptions opts;
        ctx.ParseArgs("--targets a b c --enable", opts);
        printf("Command enabled %s\n", opts.enable ? "yes" : "no");
        printf("Command targets:\n");
        for (std::string_view target : opts.targets) {
            printf("  %.*s\n", static_cast<int>(target.length()), target.data());
        }
        return true;
    });

    example.AddArgument<ExampleOptions>("--targets", [](Tokens tokens, ExampleOptions* opt) -> bool {
        opt->targets = tokens;
        return true; 
    });

    example.AddArgument<ExampleOptions>("--enable", [](Tokens tokens, ExampleOptions* opt) -> bool {
        opt->enable = true;
        return true; 
    });

    example.Invoke();

    return 0;
}